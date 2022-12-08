/**
 * @file    gige.c
 * @author  J. Kuczewski 
 * @date    September 2015
 * @version 0.1
 * @brief   UDP interface to FPGA, provides register read/write and high speed 
 *          data interface.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <time.h>
#include <cadef.h>
#include <ezca.h>

#include "gige_test.h"
#include "epics.h"


//event data buffer for a frame
uint16_t evtdata[500000000];
//uint32_t evtdata[20000000];

/* arrays for energy and time spectra */
unsigned int mca[384][4096];
unsigned int tdc[384][1024];
int evnt;

char gige_ip_addr[16];
const char *GIGE_ERROR_STRING[] = { "",
    "Client asserted register read failure", 
    "Client asserted register write failure" };
    
const char *gige_strerr(int code)
{
    if (code < 0)
        return strerror(errno);
    
    return GIGE_ERROR_STRING[code];
}

struct sockaddr_in *find_addr_from_iface(char *iface)                                               
{                                                                                                   
   struct ifaddrs *ifap, *ifa;                                                                      
   struct sockaddr_in *sa;                                                                          
   
   getifaddrs (&ifap);                                                                              
   for (ifa = ifap; ifa; ifa = ifa->ifa_next) {                                                     
      if (ifa->ifa_addr->sa_family==AF_INET) {                                                      
         sa = (struct sockaddr_in *) ifa->ifa_addr;                                                 
         if ( 0 == strcmp(iface, ifa->ifa_name)) {                                                  
            return sa;                                                                              
         }                                                                                          
      }                                                                                             
   }                                                                                                
   
   freeifaddrs(ifap);                                                                               
   return NULL;                                                                                     
}  


gige_reg_t *gige_reg_init(uint16_t reb_id, char *iface)
{
    int rc = 0;
    struct sockaddr_in *iface_addr;
    gige_reg_t *ret;
    
    ret = malloc(sizeof(gige_reg_t));
    if (ret == NULL)
        return NULL;
    
    // IP Address based off of ID
    sprintf(ret->client_ip_addr, "%s", gige_ip_addr); /*GIGE_CLIENT_IP);*/
    printf("Init: IP=%s\n",ret->client_ip_addr);    

    
    // Recv socket
    ret->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ret->sock == -1) {
        perror(__func__);
        return NULL;
    }
    
    // Recv Port Setup
    bzero(&ret->si_recv, sizeof(ret->si_recv));
    ret->si_recv.sin_family = AF_INET;
    
    // Lookup "iface" or default to any address
    if (iface != NULL && 
        (iface_addr = find_addr_from_iface(iface)) != NULL) {
        ret->si_recv.sin_addr.s_addr = iface_addr->sin_addr.s_addr;
    }
    else {
        //fprintf(stderr, "%s: listening on any address\n", __func__);
        ret->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    // Bind to Register RX Port
    ret->si_recv.sin_port = htons(GIGE_REGISTER_RX_PORT);
    rc = bind(ret->sock, (struct sockaddr *)&ret->si_recv, 
         sizeof(ret->si_recv));
    if (rc < 0) {
        perror(__func__);
        return NULL;
    }
    
    // Setup client READ TX
    bzero(&ret->si_read, sizeof(ret->si_read));
    ret->si_read.sin_family = AF_INET;
    ret->si_read.sin_port = htons(GIGE_REGISTER_READ_TX_PORT);
    
    if (inet_aton(ret->client_ip_addr , &ret->si_read.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
    }
    ret->si_lenr = sizeof(ret->si_read);
    
    // Setup client WRITE TX
    bzero(&ret->si_write, sizeof(ret->si_write));
    ret->si_write.sin_family = AF_INET;
    ret->si_write.sin_port = htons(GIGE_REGISTER_WRITE_TX_PORT);
    
    if (inet_aton(ret->client_ip_addr , &ret->si_write.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
    }
    ret->si_lenw = sizeof(ret->si_write);
    
    return ret;
}

void gige_reg_close(gige_reg_t *reg)
{
    close(reg->sock);
    free(reg);
}

int gige_reg_read(gige_reg_t *reg, uint32_t addr, uint32_t *value)
{
    struct sockaddr_in si_other;
    socklen_t len;
    uint32_t msg[10];
    int            ret;
    fd_set         fds;
    struct timeval timeout;
    
    bzero(&si_other, sizeof(si_other));
    
    msg[0] = htonl(GIGE_KEY);
    msg[1] = htonl(addr);
    
    if (sendto(reg->sock, msg, 2*4, 0 , 
        (struct sockaddr *) &reg->si_read, reg->si_lenr)==-1) {
        perror("sendto()");
    }
    
    // 3 second timeout
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    
    // Setup for select call
    FD_ZERO(&fds);
    FD_SET(reg->sock, &fds);
    
    // Wait for Socket data ready
    ret = select((reg->sock + 1), &fds, NULL, NULL, &timeout);
    
    // Detect timeout
    if ( ret < 0 ) {
        printf("%s: Select error!\n", __func__);
        return -1;
    }
    else if ( ret == 0 ) {
        printf("%s: Socket timeout\n", __func__);
        return -1;
    }
    
    ssize_t n = recvfrom(reg->sock, msg , 10, 0, 
                         (struct sockaddr *)&si_other, &len);
    if (n < 0) {
        perror(__func__);
        return -1;
    }
    
    // Detect FPGA register access failure
    if (ntohl(msg[1]) == REG_ACCESS_FAIL && (ntohl(msg[0]) >> 24) == 0xff) {
        return REGISTER_READ_FAIL;
    }
    
    *value = (uint32_t)htonl(msg[1]);
    
    return 0;
}

int gige_reg_write(gige_reg_t *reg, uint32_t addr, uint32_t value)
{
    struct sockaddr_in si_other;
    socklen_t len;
    uint32_t msg[10];
    int            ret;
    fd_set         fds;
    struct timeval timeout;
    
    bzero(&si_other, sizeof(si_other));
    
    msg[0] = htonl(GIGE_KEY);
    msg[1] = htonl(addr);
    msg[2] = htonl(value);
    
    if (sendto(reg->sock, msg, 3*4, 0 , 
        (struct sockaddr *) &reg->si_write, reg->si_lenw)==-1) {
        perror("sendto()");
    }
    
    // 3 second timeout
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    
    // Setup for select call
    FD_ZERO(&fds);
    FD_SET(reg->sock, &fds);
    
    // Wait for Socket data ready
    ret = select((reg->sock + 1), &fds, NULL, NULL, &timeout);
    
    // Detect timeout
    if ( ret < 0 ) {
        printf("%s: Select error!\n", __func__);
        return -1;
    }
    else if ( ret == 0 ) {
        printf("%s: Socket timeout\n", __func__);
        return -1;
    }
    
    ssize_t n = recvfrom(reg->sock, msg , 10, 0, 
                         (struct sockaddr *)&si_other, &len);
    if (n < 0) {
        perror(__func__);
        return -1;
    }
    
    printf("Len: %d\t Msg: %x\n",(int)n,ntohl(msg[1]));
    // Detect FPGA register access failure
    if (ntohl(msg[1]) == REG_ACCESS_FAIL && (ntohl(msg[0]) >> 24) == 0xff) {
        return REGISTER_WRITE_FAIL;
    }
    
    // Make sure we wrote the register
    if (ntohl(msg[1]) == REG_ACCESS_OKAY) {
        return 0;
    }
    
    return -1;
}


gige_data_t *gige_data_init(uint16_t reb_id, char *iface)
{
    int rc = 0;
    struct sockaddr_in *iface_addr;
    gige_data_t *ret;
    
    ret = malloc(sizeof(gige_reg_t));
    if (ret == NULL)
        return NULL;
    
    // IP Address based off of ID
    sprintf(ret->client_ip_addr, "%s", gige_ip_addr); /*GIGE_CLIENT_IP);*/ 
    printf("Init: IP=%s\n",ret->client_ip_addr);    
    // Recv socket
    ret->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ret->sock == -1) {
        perror(__func__);
        return NULL;
    }
    
    // Recv Port Setup
    bzero(&ret->si_recv, sizeof(ret->si_recv));
    ret->si_recv.sin_family = AF_INET;
    
    // Lookup "iface" or default to any address
    if (iface != NULL && 
        (iface_addr = find_addr_from_iface(iface)) != NULL) {
        ret->si_recv.sin_addr.s_addr = iface_addr->sin_addr.s_addr;
    }
    else {
        //fprintf(stderr, "%s: listening on any address\n", __func__);
        ret->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    int size = 145000000;
    if (setsockopt(ret->sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)) == -1) {
        fprintf(stderr, "Error setting socket opts: %s\n", strerror(errno));
    }
    
    // Bind to Register RX Port
    ret->si_recv.sin_port = htons(GIGE_DATA_RX_PORT);
    rc = bind(ret->sock, (struct sockaddr *)&ret->si_recv, 
         sizeof(ret->si_recv));
    if (rc < 0) {
        perror(__func__);
        return NULL;
    }
    
    return ret;
}

void gige_data_close(gige_reg_t *dat)
{
    close(dat->sock);
    free(dat);
}


long int time_elapsed(struct timeval time_i, struct timeval time_f)
{
  return (1000000*(time_f.tv_sec-time_i.tv_sec) + (time_f.tv_usec-time_i.tv_usec));
}



uint64_t gige_data_recv(gige_data_t *dat, uint16_t *data)
{
    struct sockaddr_in cliaddr;
    struct timeval tvBegin, tvEnd;
    socklen_t len;
    uint16_t mesg[4096];
    int n = 0, total_sz = 0, total_data = 0;
    uint32_t first_packetnum, packet_counter;
    int src = 0, dest =0 , cnt = 0, end_of_frame = 0, start_of_frame = 0;
    int i = 0;
    int word, adr, de, dt, j;
    
    evnt=0;
    while ( ! end_of_frame) {
        n = recvfrom(dat->sock, mesg, sizeof(mesg), 0, 
                     (struct sockaddr *)&cliaddr, &len);
        total_sz += n;
        //printf("Received %d bytes\n",total_sz);

        if (n < 0) {
            perror(__func__);
            return n;
        }
        
        packet_counter = (ntohs(mesg[0]) << 16 | ntohs(mesg[1])); /* first word always packet count */

        //for(i=0;i<n/2;i=i+2) 
        //   printf("%d:\t%4x\n",i,(ntohs(mesg[i]) << 16 | ntohs(mesg[i+1])));  

        //printf("Packet Counter: %d\n",packet_counter);
        //for (i=0;i<n;i++)
        //    printf("%d:  %d\n",i,ntohs(mesg[i]));

        src = dest = 2;
        /* Second word is padding */
        /* if third word is 0xFEEDFACE, this is first packet of new frame */
        if (ntohs(mesg[4]) == SOF_MARKER_UPPER &&
            ntohs(mesg[5]) == SOF_MARKER_LOWER) {
            gettimeofday(&tvBegin, NULL);
            total_data = 0;
            cnt = packet_counter;
            first_packetnum = packet_counter;
            //words 0,1 = packet counter
            //words 2,3 = 0xfeedface
            //words 4,5 = 0xframenum
	    src = dest = 4; //5; //2;  
            start_of_frame = 1;
            printf("Got Start of Frame\n");

	       /* clear result array */
//	 memset(mca,0,384*4096*sizeof(unsigned int));
//	 memset(tdc,0,384*1024*sizeof(unsigned int));
	  for(i=0;i<384;i++){
	    for(j=0;j<4096;j++){     
		  mca[i][j]=0;
   		  }
  		}
	 for(i=0;i<384;i++){
	   for(j=0;j<1024;j++){     
		  tdc[i][j]=0; 
	          }
	   }
	 
        }

	/* if last word is 0xDECAFBAD, this is end of frame */
        if (ntohs(mesg[(n/sizeof(uint16_t))-2]) == EOF_MARKER_UPPER &&
            ntohs(mesg[(n/sizeof(uint16_t))-1]) == EOF_MARKER_LOWER) {
            gettimeofday(&tvEnd, NULL);
            end_of_frame = 1;
            dest = 2;
            if ( ! start_of_frame) 
                fprintf(stderr, "ERROR: EOF before SOF!\n");
            printf("Got End of Frame\n");
        }
	/* append packet data to frame data */
       memcpy(&data[total_data/sizeof(uint16_t)], &mesg[src], n-(dest*sizeof(uint16_t)));
       total_data += n-(dest*sizeof(uint16_t)); /* index into full frame data */
       cnt++;
       printf("Bytes in packet: %i\n",n);
       
       /* parse events in packet */
       for(i=4;i<n-4*end_of_frame;i+=2){ /* if this is last packet, don't
                                            interpret last few words */
       word=(ntohs(mesg[i]) << 16 | ntohs(mesg[i+1]));
       if(!(word & 0x80000000)){ /* data has 0 in MSB, timestamp has 1 */
           evnt++;
           adr = (word >> 22) & 0x1ff;
           dt = (word >> 12) & 0x3ff;
           de = (word >> 0) & 0xfff;
           if(adr>=384) adr=383;
           if(dt>=1024) dt=1023;
           if(de>=4096) de=4095;
           
           mca[adr][de]+=1;
           tdc[adr][dt]+=1;
	/* skip timestamp for now */
	   }
	 }
       
    }
    
    if (packet_counter != cnt-1) {
        fprintf(stderr, "ERROR: Dropped a packet! Missed %i packets\n", packet_counter - cnt);
        return 0;
    }
    
 
    printf("Total Packets=%d\tTotal Data=%4.2f MB\n",packet_counter-first_packetnum,total_data/1e6);
    dat->bitrate = total_sz/(1.0*time_elapsed(tvBegin, tvEnd));
    //dat->n_pixels = total_data*8/16;
    printf("Throughput: %4.2f MB,  %f MB/s\n", total_sz/1e6, dat->bitrate);
    //printf("%i pixels, %i bytes\n", total_data*8/16, total_data);
    
    return total_data/sizeof(uint16_t);   //return number of 16 bit words  
}




double gige_get_bitrate(gige_data_t *dat)
{
    return dat->bitrate;
}

int gige_get_n_pixels(gige_data_t *dat)
{
    return dat->n_pixels;
}


char filename[1024];
int runno;
unsigned long filesize;
 
// test code for gige_reg_t 
int main(void)
{
    int rc = 0;
    uint32_t value;
    char run[64];
    ezcaGet("det1.IPADDR", ezcaString,1,&gige_ip_addr);
    gige_reg_t *reg = gige_reg_init(150, NULL);
    gige_data_t *dat = gige_data_init(150, NULL);
    int i,j, checkval,chkerr=0;
    FILE *fp;
    struct timeval tvBegin, tvEnd;
    
    pthread_t tid[2];
    int status;

    printf("Creating thread...\n");
    while(1)
    {
        status = pthread_create(&tid[0], NULL, &epics_thread, NULL);
        if ( 0 == status)
        {
            printf("[%s]: germ_test_thread created.\n", __func__);
            break;
        }

        printf("[%s]: ERROR!!! Can't create thread germ_test_thread: [%s]\n", __func__, strerror(status));
    }

   
    printf("Writing Register...\n");
    rc = gige_reg_write(reg, 0x00000001, 0x1);
    if (rc != 0) {
        fprintf(stderr, "Error: %s\n", gige_strerr(rc));
    }
    
    printf("Reading Register...\n");
    rc = gige_reg_read(reg, 0x00000001, &value);
    printf("ReadVal: %x\n",value);
    if (rc != 0)
        fprintf(stderr, "Error: %s\n", gige_strerr(rc));


    printf("Receiving Data...\n"); 
    while (1) {
      //get the data from a frame
      int numwords = gige_data_recv(dat,evtdata);
      //printf("Numwords in Frame=%d\n",numwords);
      //printf("\n");


    //print all data
    //for(i=0;i<=numwords;i=i+2) 
    //  printf("%d:\t%4x\n",i,(ntohs(evtdata[i]) << 16 | ntohs(evtdata[i+1])));  

    printf("Numwords in Frame=%d\n",numwords);
    printf("Numevents in Frame = %d\n",(numwords-8)/4);

    //check data 
    printf("Checking Data...\n");
    for(i=0;i<numwords-4;i=i+2) {
         //last 4 16bit words are fifofullcnt and EOF
         //printf("%d:\t%4x\n",i,ntohs(evtdata[i]));
         if (i == 0) {
            printf("SOF: %x\n",(ntohs(evtdata[i]) << 16 | ntohs(evtdata[i+1])));
            checkval = 0;
         }
         else if (i == 2) {
            printf("FrameNum: %d\n",(ntohs(evtdata[i]) << 16 | ntohs(evtdata[i+1])));
            checkval = 0;
         }
         else 
            if (checkval != (ntohs(evtdata[i]) << 16 | ntohs(evtdata[i+1]))) {
               printf("Error at i=%d\tval=%x\t should be=%x\n",i,(ntohs(evtdata[i]) << 16 | ntohs(evtdata[i+1])),checkval);
               chkerr = 1;
               break;  
            }
            else           
               checkval++;
      }

      printf("Events lost to Overflow: %d\n",(ntohs(evtdata[numwords-4]) << 16 | ntohs(evtdata[numwords-3])));
      printf("EOF: %x\n",(ntohs(evtdata[numwords-2]) << 16 | ntohs(evtdata[numwords-1])));
      if (chkerr == 0)
         printf("Checking Complete.  No errors :)\n");
	printf("Events: %i\n",evnt);
	
      //save to disk
      printf("Saving File...\n");
      //ezcaGet("det1.FNAM",ezcaString,1,&filename);
      //ezcaGet("det1.RUNNO",ezcaLong,1,&runno);
      sprintf(run,"_%i.bin",runno);
      strncat(filename,run,strlen(run));
//      sprintf(filename, "testdata.bin");
      fp = fopen(filename, "w");
      gettimeofday(&tvBegin, NULL);
      fwrite(evtdata,numwords,sizeof(uint16_t),fp);
      gettimeofday(&tvEnd, NULL);
      printf("Wrote %4.2f MB to %s in %f sec\n", numwords*2/1e6, filename, (float)(time_elapsed(tvBegin, tvEnd)/1e6));
      fclose(fp);
      printf("\n");
            /* open file for spectra */
      sprintf(run,"_spectra_%i.dat",runno);
      //ezcaGet("det1.FNAM",ezcaString,1,&filename);
      strncat(filename,run,strlen(run));
      fp=fopen(filename,"w");
      if(fp!=NULL) printf("Output file opened OK\n");
      fprintf(fp,"\t#name: spect\n");
      fprintf(fp,"\t#type: matrix\n");
      fprintf(fp,"\t#rows: 384\n");
      fprintf(fp,"\t#columns: 4096\n");

      for(i=0;i<384;i++){
       for(j=0;j<4096;j++){
        fprintf(fp,"%u  ",mca[i][j]);
       }
       fprintf(fp,"\n");
      }
      fprintf(fp,"\n");
      fprintf(fp,"\t#name: tot\n");
      fprintf(fp,"\t#type: matrix\n");
      fprintf(fp,"\t#rows: 384\n");
      fprintf(fp,"\t#columns: 1024\n");

      for(i=0;i<384;i++){
       for(j=0;j<1024;j++){
        fprintf(fp,"%u  ",tdc[i][j]);
       }
       fprintf(fp,"\n");
      }
      fprintf(fp,"\n");
      printf("Output file written OK\n");
      fclose(fp);
   }
     
    gige_reg_close(reg);
    return 0;
}
