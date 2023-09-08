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
 *   v1.1
 *     - Author: Ji Li
 *     - Date  : Oct 2023
 *     - Passing single packets instead of frames
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
#include "log.h"


//event data buffer for a frame
//uint16_t evtdata[500000000];
//uint32_t evtdata[20000000];
//frame_buff_t frame_buff[NUM_FRAME_BUFF];
packet_buff_t packet_buff[NUM_PACKET_BUFF];

/* arrays for energy and time spectra */
extern uint16_t mca[NUM_MCA_ROW][NUM_MCA_COL];
extern uint16_t tdc[NUM_TDC_ROW][NUM_TDC_COL];

extern pv_obj_t  pv[NUM_PVS];


extern char  gige_ip_addr[16];
const char  *GIGE_ERROR_STRING[] = { "",
    "Client asserted register read failure", 
    "Client asserted register write failure" };
    


extern uint32_t reg1_val;

//extern unsigned char read_buff, write_buff;

extern char     filename[MAX_FILENAME_LEN];
extern uint32_t runno;
extern uint32_t filesize;
 
extern uint8_t  udp_conn_thread_ready;

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
    

    log("find address from iface %s\n", iface);

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        //log("IP address of %s is %s\n", ifa->ifa_name);
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
//    char my_ip_addr[16];
    
    ret = malloc(sizeof(gige_reg_t));
    if (ret == NULL)
        return NULL;
    
    // IP Address based off of ID
    sprintf(ret->client_ip_addr, "%s", gige_ip_addr); /*GIGE_CLIENT_IP);*/
    log( "init with IP address %s\n",
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
        log("listening on %s\n", inet_ntoa(ret->si_recv.sin_addr));
    }
    else {
        //fprintf(stderr, "listening on any address\n");
        log("listening on any address\n");
        ret->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    // Bind to Register RX Port
    //ret->si_recv.sin_port = htons(GIGE_REGISTER_RX_PORT);
    ret->si_recv.sin_port = htons(32000);
    rc = bind( ret->sock,
               (struct sockaddr *)&ret->si_recv, 
               sizeof(ret->si_recv));
    if (rc < 0) {
        perror(__func__);
        log("failed to bind.\n");
        return NULL;
    }
    
    // Setup client READ TX
    bzero(&ret->si_read, sizeof(ret->si_read));
    ret->si_read.sin_family = AF_INET;
    //ret->si_read.sin_port = htons(GIGE_REGISTER_READ_TX_PORT);
    ret->si_read.sin_port = htons(GIGE_DATA_RX_PORT);
    
    if (inet_aton(ret->client_ip_addr , &ret->si_read.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
    }
    ret->si_lenr = sizeof(ret->si_read);
    
    // Setup client WRITE TX
    bzero(&ret->si_write, sizeof(ret->si_write));
    ret->si_write.sin_family = AF_INET;
    ret->si_write.sin_port = htons(GIGE_REGISTER_WRITE_TX_PORT);
    
    if (inet_aton(ret->client_ip_addr , &ret->si_write.sin_addr) == 0) {
        err("inet_aton() failed\n");
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
        log("select error!\n");
        return -1;
    }
    else if ( ret == 0 ) {
        log("socket timeout\n");
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
        log("select error!\n");
        return -1;
    }
    else if ( ret == 0 ) {
        log("socket timeout\n");
        return -1;
    }
    
    ssize_t n = recvfrom(reg->sock, msg , 10, 0, 
                         (struct sockaddr *)&si_other, &len);
    if (n < 0) {
        log("incorrect number of bytes received (%ld)\n", n);
        perror(__func__);
        return -1;
    }
    
    log("Len = %d\t Msg = %x\n", (int)n, ntohl(msg[1]));
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
    log("init with IP=%s\n", ret->client_ip_addr);    
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
        //fprintf(stderr, "listening on any address\n");
        ret->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    int size = 145000000;
    if (setsockopt(ret->sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)) == -1) {
        err("Error setting socket opts: %s\n", strerror(errno));
        //log("error setting socket opts: %s\n", strerror(errno));
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



uint8_t gige_data_recv(gige_data_t *dat, packet_buff_t* buff_p)
{
    struct sockaddr_in cliaddr;
    struct timeval tv_begin, tv_end;
    socklen_t len;
    
	buff_p->length = recvfrom( dat->sock, buff_p->packet, MAX_PACKET_LENGTH, 0,
                               (struct sockaddr *)&cliaddr, &len);
	        
    if ( buff_p->length < 0 )
    {
        perror(__func__);
		return -1;
    }

    gettimeofday(&tv_end, NULL);
    log( "received %u bytes in %f sec\n",
         buff_p->length,
         (float)(time_elapsed(tv_begin, tv_end)/1e6) );

    buff_p->runno = runno;
    tv_begin = tv_end;

	return 0;
}

/*
//=======================================================
uint64_t gige_data_recv(gige_data_t *dat, frame_buff_t* buff_p)
{
    struct sockaddr_in cliaddr;
    struct timeval tv_begin, tv_end;
    socklen_t len;
    uint16_t mesg[4096];
    unsigned int n = 0;
    unsigned long total_sz = 0, total_data = 0;
    uint32_t first_packetnum, packet_counter;
    //int src = 0, dest =0;
    int cnt = 0, end_of_frame = 0, start_of_frame = 0;
    //int i = 0;
    //int word, adr, de, dt, j;
//    uint16_t event_start;   // start position of event
    uint16_t misc_len;      // length of packet header/tail
    uint16_t* data;
    uint16_t sod;           // start of data in a packet
//    uint16_t eod = 4;       // end of data in a packet
    uint16_t payload_len;
    
    static unsigned int next_frame = 0;

    data = buff_p->evtdata;
    
//    log("receiving UDP data...\n");

    while ( ! end_of_frame)
    {
        n = recvfrom( dat->sock, mesg, sizeof(mesg), 0, 
                      (struct sockaddr *)&cliaddr, &len);
        total_sz += n;
        log("received %lu bytes\n", total_sz);

        if (n < 0)
        {
            perror(__func__);
            return n;
        }

        // first word always packet count
        packet_counter = (ntohs(mesg[0]) << 16 | ntohs(mesg[1])); 

		sod      = 4;
		misc_len = 4;

        // Second word is padding
        // if third word is 0xFEEDFACE, this is first packet of new frame
        if (ntohs(mesg[4]) == SOF_MARKER_UPPER &&
            ntohs(mesg[5]) == SOF_MARKER_LOWER)
		{
            gettimeofday(&tv_begin, NULL);
            //total_data = 0;
            cnt = packet_counter;
            first_packetnum = packet_counter;
			buff_p->frame_num = (ntohs(mesg[6]) << 16) | ntohs(mesg[7]);
			if (next_frame != buff_p->frame_num)
			{
			    err("%u frames lost.\n", buff_p->frame_num - next_frame);
			}
			next_frame = buff_p->frame_num + 1;
            //words 0,1 = packet counter
            //words 2,3 = 0x00000000
            //words 4,5 = 0xfeedface
            //words 6,7 = 0xframenum
            //src = dest = 4; //5; //2;  
			//event_start = 8;
			//misc_len    = 8;

            // First packet of a frame has a header length of 8
            //sod = 8;
            //misc_len = 8;

            start_of_frame = 1;
            log("got Start of Frame\n");
        }
		else
		{

            // if last word is 0xDECAFBAD, this is end of frame
            if (ntohs(mesg[(n/sizeof(uint16_t))-2]) == EOF_MARKER_UPPER &&
                ntohs(mesg[(n/sizeof(uint16_t))-1]) == EOF_MARKER_LOWER)
            {
                gettimeofday(&tv_end, NULL);
                buff_p->num_lost_event = ntohs(mesg[n/sizeof(uint16_t)-4]) << 16 | ntohs(mesg[n/sizeof(uint16_t)-3]);
                //misc_len = 8;
                end_of_frame = 1;
                //dest = 2;
                log("got End of Frame\n");
            }
		}

        // append packet data to frame data
        //memcpy( &data[total_data/sizeof(uint16_t)],
		//        &mesg[src],
		//	n-(dest*sizeof(uint16_t)));
        //total_data += n-(dest*sizeof(uint16_t)); // index into full frame data
        payload_len = n - (misc_len*sizeof(uint16_t));
        memcpy( &data[total_data/sizeof(uint16_t)],
				&mesg[sod],
				payload_len);
        total_data += payload_len; // index into full frame data
        cnt++;
        log("%u bytes in payload of  packet %u\n", payload_len, packet_counter);
    }

    log("==================================================\n");

    // Summary
    log("Reception of frame %u:\n", buff_p->frame_num);

    dat->bitrate = total_sz/(1.0*time_elapsed(tv_begin, tv_end));
    //dat->n_pixels = total_data*8/16;
    log("    throughput is %4.2f MB (including packet headers), %f MB/s\n",
        total_sz/1e6,
        dat->bitrate );
    //log("%i pixels, %i bytes\n", total_data*8/16, total_data);

    if (0 == start_of_frame)
    {
        err("Missed SOF!\n");
    }

    log("    expecting %d packets, received %u packets)\n",
         packet_counter-first_packetnum+1,
         cnt-first_packetnum);
   
    if (packet_counter != cnt-1)
    {
        err("Missed %u packets\n", packet_counter - cnt);
    }
    else
    {
        log("    all packets received\n");
    }
    
    if (0!= buff_p->num_lost_event)
    {
        err("%d events lost due to UDP Tx FIFO overflow.\n", buff_p->num_lost_event);
    }
    else
    {
        log("    no overflow detected in UDP Tx FIFO\n");
    }

    log("    payload received including SOF/EOF) = %lu bytes / %4.2f MB\n", total_data, total_data/1e6);

    log("==================================================\n");
    
    buff_p->num_words = total_data/sizeof(uint16_t);
    return total_data/sizeof(uint16_t);   //return number of 16 bit words  
}
*/

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

    packet_buff_t * buff_p;
    unsigned char   write_buff = 0;

    //struct timespec t1, t2;

    //t1.tv_sec  = 1;
    //t1.tv_nsec = 0;

    uint8_t i = 0;

    log("########## Initializing udp_conn_thread ##########\n");

    log("the IP address of the UDP port on the detector is %s\n",
            (char*)pv[PV_IPADDR].my_var_p);
    pv_put(PV_IPADDR_RBV);
    gige_reg_t *reg = gige_reg_init(150, NULL);
    gige_data_t *dat;

    log("writing 0x%x to Register 0x01...\n", reg1_val);
    //rc = gige_reg_write(reg, 0x00000001, 0x1);
    rc = gige_reg_write(reg, 0x00000001, reg1_val);
    if (rc != 0) {
        //fprintf(stderr, "Error: %s\n", gige_strerr(rc));
        log("gige_reg_write() returned %d (%s)\n", rc, gige_strerr(rc));
    }
    
    log("reading Register 0x01...\n");
    rc = gige_reg_read(reg, 0x00000001, &value);
    log("register 0x01 read 0x%x\n", value);
    if (rc != 0)
    {
        //fprintf(stderr, "Error: %s\n", gige_strerr(rc));
        log("gige_reg_read() returned %d (%s)\n", rc, gige_strerr(rc));
    }

    if(reg1_val != value)
    {
        err("register 1 value 0x%x doesn't equal to written value 0x%x.\n", value, reg1_val);
    }

    for(int i=0; i<NUM_PACKET_BUFF; i++)
    {
        if (pthread_mutex_init(&packet_buff[i].lock, NULL) != 0)
        {
            log("\nmutex init failed!\n");
            pthread_exit(NULL);
        }
        packet_buff[i].flag = 0;  // reset the flags
    }

    dat = gige_data_init(150, NULL);

    buff_p = &packet_buff[write_buff];
    pthread_mutex_lock(&buff_p->lock);
    log("buff[%d] locked for writing\n", write_buff);

    udp_conn_thread_ready = 1;

//    log("receiving Data...\n"); 
    while (1)
    { 
        if (gige_data_recv(dat, buff_p) == 1)
        {
            buff_p->flag = 1;
            pthread_mutex_unlock(&buff_p->lock);
            log("buff[%d] released\n", write_buff);

            write_buff++;
            write_buff %= NUM_PACKET_BUFF;

            buff_p = &packet_buff[write_buff];
            pthread_mutex_lock(&buff_p->lock);

            // The flag is incremented by the data writing thread and
            // spectra calculation thread, so the value should be 2.
            if (i<NUM_PACKET_BUFF)
            {
                i++;
            }
            if( (buff_p->flag != 2) && (i>NUM_PACKET_BUFF) )
            {
                err("buffer overflow detected. Data file or spectra file may be missing\n");
            }
            log("buff[%d] locked for writing\n", write_buff);
        }
		else
		{
		    // Error in Rx, continue to use the current buff
		    continue;  
		}

    }

    gige_reg_close(reg);
    return 0;
}
