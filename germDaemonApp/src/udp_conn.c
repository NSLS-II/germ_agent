/**
 * File: udp_conn.c
 *
 * Functionality: UDP interface to FPGA, provides register read/write and 
 *                high speed data interface.
 *
 *-----------------------------------------------------------------------------
 * 
 * Revisions:
 *
 *   v1.0
 *     - Author: Ji Li
 *     - Date  : Dec 2022
 *     - Brief : Separated from germ_xx.c;
 *               Implemented as a thread.
 *   v0.1
 *     - Author: J. Kuczewski 
 *     - Date  : September 2015
 *     - Brief : Functionality included in germ_xx.c
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
//#include "ezca.h"

#include "germ.h"
//#include "udp.h"
#include "udp_conn.h"


//event data buffer for a frame
//uint16_t evtdata[500000000];
//uint32_t evtdata[20000000];
frame_buff_t frame_buff[NUM_FRAME_BUFF];

// indicate which bufferto write/read
unsigned char write_buff;
unsigned char read_buff;


/* arrays for energy and time spectra */
unsigned int mca[384][4096];
unsigned int tdc[384][1024];
int evnt;

extern pv_obj_t  pv[NUM_PVS];


extern char gige_ip_addr[16];
const char *GIGE_ERROR_STRING[] = { "",
    "Client asserted register read failure", 
    "Client asserted register write failure" };
    


extern uint32_t reg1_val;

extern unsigned char read_buff, write_buff;

char filename[1024];
int runno;
unsigned long filesize;
 
char        udp_conn_thread_ready = 0;
extern char exp_mon_thread_ready;



//=======================================================
const char *gige_strerr(int code)
{
    if (code < 0)
        return strerror(errno);
    
    return GIGE_ERROR_STRING[code];
}


//=======================================================
struct sockaddr_in *find_addr_from_iface(char *iface)
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    

    printf("[%s]: find address from iface %s\n", __func__, iface);

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        //printf("[%s]: IP address of %s is %s\n", __func__, ifa->ifa_name);
        if (ifa->ifa_addr->sa_family==AF_INET)
        {
            if ( 0 == strcmp(iface, ifa->ifa_name))
            {
                sa = (struct sockaddr_in *) ifa->ifa_addr;
                return sa;
            }
        }
    }
    
    freeifaddrs(ifap);
    return NULL;
}  


//=======================================================
gige_reg_t *gige_reg_init(uint16_t reb_id, char *iface)
{
    int rc = 0;
    struct sockaddr_in *iface_addr;
    gige_reg_t *ret;
    char my_ip_addr[16];
    
    ret = malloc(sizeof(gige_reg_t));
    if (ret == NULL)
        return NULL;
    
    // IP Address based off of ID
    sprintf(ret->client_ip_addr, "%s", gige_ip_addr); /*GIGE_CLIENT_IP);*/
    printf( "[%s]: init with IP address %s\n",
            __func__,
            ret->client_ip_addr);

    
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
        printf("[%s]: listening on %s\n", __func__, inet_ntoa(ret->si_recv.sin_addr));
    }
    else {
        //fprintf(stderr, "[%s]: listening on any address\n", __func__);
        printf("[%s]: listening on any address\n", __func__);
        ret->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    // Bind to Register RX Port
    ret->si_recv.sin_port = htons(GIGE_REGISTER_RX_PORT);
    rc = bind( ret->sock,
               (struct sockaddr *)&ret->si_recv, 
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


//=======================================================
void gige_reg_close(gige_reg_t *reg)
{
    close(reg->sock);
    free(reg);
}


//=======================================================
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
        printf("[%s]: select error!\n", __func__);
        return -1;
    }
    else if ( ret == 0 ) {
        printf("[%s]: socket timeout\n", __func__);
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


//=======================================================
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
        printf("[%s]: select error!\n", __func__);
        return -1;
    }
    else if ( ret == 0 ) {
        printf("[%s]: socket timeout\n", __func__);
        return -1;
    }
    
    ssize_t n = recvfrom(reg->sock, msg , 10, 0, 
                         (struct sockaddr *)&si_other, &len);
    if (n < 0) {
        printf("[%s]: incorrect number of bytes received (%ld)\n", __func__, n);
        perror(__func__);
        return -1;
    }
    
    printf("[%s]: Len = %d\t Msg = %x\n", __func__, (int)n, ntohl(msg[1]));
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


//=======================================================
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
    printf("[%s]: init with IP=%s\n", __func__, ret->client_ip_addr);    
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
        //fprintf(stderr, "[%s]: listening on any address\n", __func__);
        ret->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    int size = 145000000;
    if (setsockopt(ret->sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)) == -1) {
        fprintf(stderr, "Error setting socket opts: %s\n", strerror(errno));
        printf("[%s]: error setting socket opts: %s\n", __func__, strerror(errno));
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

//=======================================================
void gige_data_close(gige_reg_t *dat)
{
    close(dat->sock);
    free(dat);
}


//=======================================================
uint64_t gige_data_recv(gige_data_t *dat, frame_buff_t* buff_p)
{
    struct sockaddr_in cliaddr;
    struct timeval tv_begin, tv_end;
    socklen_t len;
    uint16_t mesg[4096];
    int n = 0, total_sz = 0, total_data = 0;
    uint32_t first_packetnum, packet_counter;
    //int src = 0, dest =0;
    int cnt = 0, end_of_frame = 0, start_of_frame = 0;
    //int i = 0;
    //int word, adr, de, dt, j;
    uint16_t event_start;   // start position of event
    uint16_t misc_len;      // length of packet header/tail
    uint16_t* data;

    unsigned long next_frame = 0;

    data = buff_p->evtdata;
    
    evnt=0;
    while ( ! end_of_frame) {
        n = recvfrom( dat->sock, mesg, sizeof(mesg), 0, 
                      (struct sockaddr *)&cliaddr, &len);
        total_sz += n;
        //printf("Received %d bytes\n",total_sz);

        if (n < 0) {
            perror(__func__);
            return n;
        }

        /* first word always packet count */
        packet_counter = (ntohs(mesg[0]) << 16 | ntohs(mesg[1])); 

        //for(i=0;i<n/2;i=i+2) 
        //   printf("%d:\t%4x\n",i,(ntohs(mesg[i]) << 16 | ntohs(mesg[i+1])));  

        //printf("Packet Counter: %d\n",packet_counter);
        //for (i=0;i<n;i++)
        //    printf("%d:  %d\n",i,ntohs(mesg[i]));

        //src = dest = 2;

	event_start = 4;
	misc_len    = 4;
        /* Second word is padding */
        /* if third word is 0xFEEDFACE, this is first packet of new frame */
        if (ntohs(mesg[4]) == SOF_MARKER_UPPER &&
            ntohs(mesg[5]) == SOF_MARKER_LOWER)
	{
            gettimeofday(&tv_begin, NULL);
            total_data = 0;
            cnt = packet_counter;
            first_packetnum = packet_counter;
	    buff_p->frame_num = (ntohs(mesg[6]) << 16) | ntohs(mesg[7]);
	    if (next_frame != buff_p->frame_num)
	    {
	        printf("[%s]: ERROR! %ld frames lost.\n", __func__, buff_p->frame_num - next_frame);
	    }
	    next_frame = buff_p->frame_num + 1;
            //words 0,1 = packet counter
            //words 2,3 = 0xfeedface
            //words 4,5 = 0xframenum
            //src = dest = 4; //5; //2;  
	    event_start = 8;
	    misc_len    = 8;
            start_of_frame = 1;
            printf("[%s]: got Start of Frame\n", __func__);

	   /* clear result array */
           // memset(mca,0,384*4096*sizeof(unsigned int));
           // memset(tdc,0,384*1024*sizeof(unsigned int));
            /*for(i=0;i<384;i++){
                for(j=0;j<4096;j++){     
                    mca[i][j]=0;
                }
            }
            for(i=0;i<384;i++){
                for(j=0;j<1024;j++){     
                    tdc[i][j]=0; 
                }
            }*/
        }
	else
	{
	    if (0 == start_of_frame)
	    {
                fprintf(stderr, "ERROR: EOF before SOF!\n");
	    }

            /* if last word is 0xDECAFBAD, this is end of frame */
            if (ntohs(mesg[(n/sizeof(uint16_t))-2]) == EOF_MARKER_UPPER &&
                ntohs(mesg[(n/sizeof(uint16_t))-1]) == EOF_MARKER_LOWER)
            {
                gettimeofday(&tv_end, NULL);
                buff_p->num_lost_event = ntohs(mesg[n/sizeof(uint16_t)-4]) << 16 | ntohs(mesg[n/sizeof(uint16_t)-3]);
                if (0 != buff_p->num_lost_event)
                {
                    printf("[%s]: %d events lost due to overflow.\n", __func__, buff_p->num_lost_event);
                }
                misc_len = 8;
                end_of_frame = 1;
                //dest = 2;
                printf("[%s]: got End of Frame\n", __func__);
            }
	}

        /* append packet data to frame data */
        //memcpy( &data[total_data/sizeof(uint16_t)],
	//        &mesg[src],
	//	n-(dest*sizeof(uint16_t)));
        //total_data += n-(dest*sizeof(uint16_t)); /* index into full frame data */
        memcpy( &data[total_data/sizeof(uint16_t)],
	        &mesg[event_start],
		n-(misc_len*sizeof(uint16_t)));
        total_data += n-(misc_len*sizeof(uint16_t)); /* index into full frame data */
        cnt++;
        printf("[%s]: %i bytes in packet\n", __func__, n);
    }
    
    if (packet_counter != cnt-1) {
        fprintf( stderr,
	         "ERROR: Dropped a packet! Missed %i packets\n",
		 packet_counter - cnt);
        return 0;
    }
    
 
    printf( "[%s]: total packets = %d,\t total Data = %4.2f MB\n",
            __func__,
	    packet_counter-first_packetnum,
	    total_data/1e6 );
    dat->bitrate = total_sz/(1.0*time_elapsed(tv_begin, tv_end));
    //dat->n_pixels = total_data*8/16;
    printf( "[%s]: throughput is %4.2f MB, %f MB/s\n",
            __func__,
	    total_sz/1e6,
	    dat->bitrate );
    //printf("%i pixels, %i bytes\n", total_data*8/16, total_data);
    
    buff_p->num_words = total_data/sizeof(uint16_t);
    return total_data/sizeof(uint16_t);   //return number of 16 bit words  
}


//=======================================================
double gige_get_bitrate(gige_data_t *dat)
{
    return dat->bitrate;
}


//=======================================================
int gige_get_n_pixels(gige_data_t *dat)
{
    return dat->n_pixels;
}


//=======================================================
void* udp_conn_thread(void* arg)
{
    int rc = 0;
    uint32_t value;

    frame_buff_t * buff_p;

    struct timespec t1, t2;

    t1.tv_sec  = 1;
    t1.tv_nsec = 0;

    printf("#####################################################\n");
    printf("[%s]: Initializing udp_conn_thread...\n", __func__);

    do  
    {   
        nanosleep(&t1, &t2);
    } while(0 == exp_mon_thread_ready);

    printf("[%s]: the IP address of the UDP port on the detector is %s\n",
            __func__,
            (char*)pv[PV_IPADDR].my_var_p);
    pv_put(PV_IPADDR_RBV);
    //gige_reg_t *reg = gige_reg_init(150, NULL);
    gige_reg_t *reg = gige_reg_init(150, "eno1");
    gige_data_t *dat;

    printf("[%s]: writing 0x%x to Register 0x01...\n", __func__, reg1_val);
    //rc = gige_reg_write(reg, 0x00000001, 0x1);
    rc = gige_reg_write(reg, 0x00000001, reg1_val);
    if (rc != 0) {
        fprintf(stderr, "Error: %s\n", gige_strerr(rc));
        printf("[%s]: status returned from gige_reg_write() - %d (%s)\n", __func__, rc, gige_strerr(rc));
    }
    
    printf("[%s]: reading Register 0x01...\n", __func__);
    rc = gige_reg_read(reg, 0x00000001, &value);
    printf("[%s]: readVal from register 0x01: 0x%x\n", __func__, value);
    if (rc != 0)
    {
        fprintf(stderr, "Error: %s\n", gige_strerr(rc));
        printf("[%s]: status returned from gige_reg_read() - %d (%s)\n", __func__, rc, gige_strerr(rc));
    }

    if(reg1_val != value)
    {
        printf("[%s]: ERROR!!! Read value 0x%x from register 1 doesn't equal to written value 0x%x.\n", __func__, value, reg1_val);
    }

    for(int i=0; i<NUM_FRAME_BUFF; i++)
    {
        if (pthread_mutex_init(&frame_buff[i].lock, NULL) != 0)
        {
            printf("\n[%s]: mutex init failed!\n", __func__);
            pthread_exit(NULL);
        }
    }

    udp_conn_thread_ready = 1;

    printf("[%s]: receiving Data...\n", __func__); 
    while (1)
    { 
        dat = gige_data_init(150, NULL);

        buff_p = &frame_buff[write_buff];
        pthread_mutex_lock(&buff_p->lock);
        printf("[%s]: buff[%d] locked for writing\n", __func__, read_buff);

        //buff_p->num_words = gige_data_recv(dat, buff_p->evtdata);
        gige_data_recv(dat, buff_p);

        /* Just write. Let the reader detect overflow. */
        read_buff = write_buff;

        pthread_mutex_unlock(&buff_p->lock);

        write_buff++;
        write_buff %= NUM_FRAME_BUFF;
        printf("[%s]: buff[%d] released\n", __func__, read_buff);
    }

    gige_reg_close(reg);
    return 0;
}
