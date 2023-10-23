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
#include <pthread.h>
#include <stdatomic.h>

#include <cadef.h>
#include <ezca.h>
//#include "ezca.h"

#include "germ_atom.h"
//#include "udp.h"
#include "udp_conn_atom.h"
#include "log.h"


//event data buffer for a frame
//uint16_t evtdata[500000000];
//uint32_t evtdata[20000000];
//frame_buff_t frame_buff[NUM_FRAME_BUFF];
extern packet_buff_t packet_buff[NUM_PACKET_BUFF];

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

extern atomic_char exp_mon_thread_ready;
extern atomic_char udp_conn_thread_ready;

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
    log("gige_ip_addr = %s\n", gige_ip_addr);
    log("init with IP address %s\n", ret->client_ip_addr);
    
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
    ret->si_recv.sin_port = htons(GIGE_REGISTER_RX_PORT);
    //ret->si_recv.sin_port = htons(32000);
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
    ret->si_read.sin_port = htons(GIGE_REGISTER_READ_TX_PORT);
    //ret->si_read.sin_port = htons(GIGE_DATA_RX_PORT);
    
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
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    
    // Setup for select call
    FD_ZERO(&fds);
    FD_SET(reg->sock, &fds);
    
    // Wait for Socket data ready
    ret = select((reg->sock + 1), &fds, NULL, NULL, &timeout);
    
    // Detect timeout
    if ( ret < 0 ) {
        log("select error: %d! Abort\n", errno);
        return -1;
    }
    
    if ( ret == 0 ) {
        log("socket timeout from select(). Abort.\n");
        return -1;
    }
    
    ssize_t n = recvfrom(reg->sock, msg , 10, 0, 
                         (struct sockaddr *)&si_other, &len);
    if (n < 0) {
        log("incorrect number of bytes received (%ld). Abort.\n", n);
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
    //sprintf(ret->client_ip_addr, "%s", "172.16.0.215"); /*GIGE_CLIENT_IP);*/ 
    log("gige_ip_addr = %s\n", gige_ip_addr);
    log("init with IP address %s\n", ret->client_ip_addr);    

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



int8_t gige_data_recv(gige_data_t *dat, packet_buff_t* buff_p)
{
    struct sockaddr_in cliaddr;
    struct timeval tv_begin, tv_end;
    socklen_t len;
    
    gettimeofday(&tv_begin, NULL);
    buff_p->length = recvfrom( dat->sock, buff_p->packet, MAX_PACKET_LENGTH, 0,
                               (struct sockaddr *)&cliaddr, &len);
                
    if ( buff_p->length < 0 )
    {
        perror(__func__);
                return -1;
    }

    gettimeofday(&tv_end, NULL);
    log( "received %u bytes\n",
         buff_p->length );
//    log( "received %u bytes in %f sec\n",
//         buff_p->length,
//         (float)(time_elapsed(tv_begin, tv_end)/1e6) );

    buff_p->runno = runno;
    tv_begin = tv_end;

    return 0;
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

    packet_buff_t * buff_p;
    unsigned char   write_buff = 0;

    struct timespec t1, t2;

    t1.tv_sec  = 0;
    t1.tv_nsec = 300;

    do
    {
        nanosleep(&t1, &t2);
    } while(0 == atomic_load(&exp_mon_thread_ready));

    log("########## Initializing udp_conn_thread ##########\n");

//    for(int i=0; i<NUM_PACKET_BUFF; i++)
//    {
//        if (pthread_mutex_init(&packet_buff[i].lock, NULL) != 0)
//        {
//            log("\nmutex init failed!\n");
//            pthread_exit(NULL);
//        }
//        packet_buff[i].flag = 3;  // preset the flags
//        log("buff[%d].flag = %d\n", i, packet_buff[i].flag);
//    }

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

    dat = gige_data_init(150, NULL);


    for (int i=0; i<NUM_PACKET_BUFF; i++)
    {
        log("buff[%d].status = 0x%x\n", i, packet_buff[i].status);
    }

    atomic_store(&udp_conn_thread_ready, 1);

    log("receiving Data...\n"); 
    while (1)
    { 
        buff_p = &(packet_buff[write_buff]);

        log("write to buff[%d]\n", write_buff);
        //lock_buff_write(write_buff, DATA_WRITTEN|DATA_PROCCED, __func__);
        lock_buff_write(write_buff, DATA_WRITTEN, __func__);

        while(gige_data_recv(dat, buff_p) ); // loop until receive is successful
        
        buff_p->status = 0;
        unlock_buff(write_buff, __func__);
        log("buff[%d] released\n", write_buff);

        write_buff++;
        write_buff &= PACKET_BUFF_MASK;
    }

    gige_reg_close(reg);
    return 0;
}
