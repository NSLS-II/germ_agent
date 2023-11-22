/**
 * File: germ.c
 *
 * Functionality: Main program of UDP daemon for Germanium detector.
 *
 *-----------------------------------------------------------------------------
 *
 * Revisions:
 *
 *   v1.0
 *     - Author: Ji Li
 *     - Date  : Dec 2022
 *     - Brief : Create threads and offload the functionalities to threads.
 *
 *   v0.1
 *     - Author: J. Kuczewski
 *     - Date  : September 2015
 *     - Brief : germ_getudpevents.c
 *               UDP interface to FPGA, provides register read/write and high
 *               speed data interface.
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
#include <sched.h>

#include <cadef.h>

#ifdef USE_EZCA
#include <ezca.h>
#endif

#include "germ_atom.h"
#include "exp_mon.h"
#include "udp_conn_atom.h"
#include "data_write_atom.h"
#include "data_proc_atom.h"
#include "mover.h"
#include "log.h"


//event data buffer for a frame
//uint16_t evtdata[500000000];
//uint32_t evtdata[20000000];
//

/* arrays for energy and time spectra */
//int evnt;
packet_buff_t packet_buff[NUM_PACKET_BUFF];
    
uint32_t reg1_val = 0x1;  // value to be written to FPGA register 1

pv_obj_t pv[NUM_PVS];

char ca_dtype[7][11] = { "DBR_STRING",
                         "DBR_SHORT",
                         "DBR_FLOAT",
                         "DBR_ENUM",
                         "DBR_CHAR",
                         "DBR_LONG",
                         "DBR_DOUBLE" };

uint16_t mca[NUM_MCA_ROW * NUM_MCA_COL];
uint16_t tdc[NUM_TDC_ROW * NUM_TDC_COL];

atomic_char   count = ATOMIC_VAR_INIT(0);
char          tmp_datafile_dir[MAX_FILENAME_LEN];
char          datafile_dir[MAX_FILENAME_LEN];
char          filename[MAX_FILENAME_LEN];
char          datafile_run[MAX_FILENAME_LEN];
char          spectrafile_run[MAX_FILENAME_LEN];
char          datafile[MAX_FILENAME_LEN];
char          spectrafile[MAX_FILENAME_LEN];
unsigned long filesize = 0;
unsigned long runno = 0;

unsigned int  nelm = 0;
unsigned int  monch = 0;

unsigned char tsen_proc, chen_proc;
unsigned char tsen_ctrl, chen_ctrl;
char          tsen[MAX_NELM], chen[MAX_NELM];

char          gige_ip_addr[16];

char          hostname[32];
unsigned long pid = 0;
char          directory[64];

unsigned int  watchdog = 0;

atomic_char exp_mon_thread_ready    = ATOMIC_VAR_INIT(0);
atomic_char udp_conn_thread_ready   = ATOMIC_VAR_INIT(0);
atomic_char data_write_thread_ready = ATOMIC_VAR_INIT(0);

//========================================================================
// Calculate elapsed time.
//------------------------------------------------------------------------
long int time_elapsed(struct timeval time_i, struct timeval time_f)
{
    return (1000000*(time_f.tv_sec-time_i.tv_sec) + (time_f.tv_usec-time_i.tv_usec));
}


//========================================================================
// Initialize packet buffer.
//========================================================================
void buff_init(void)    //packet_buff_t buff_p;
{
    for(int i=0; i<NUM_PACKET_BUFF; i++)
    {
    //    packet_buff_t *buff_p = &packet_buff[i];
    //    packet_buff[idx].lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    //    packet_buff[idx].flag = DATA_WRITE_MASK | DATA_PROC_MASK;
    //    packet_buff[i].lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&packet_buff[i].mutex, NULL);
        //if (pthread_mutex_init(&packet_buff[i].lock, NULL) != 0)
        //{
        //    log("\nmutex init failed!\n");
        //    return(-1);
        //}
        //packet_buff[i].flag = DATA_WRITE_MASK | DATA_PROC_MASK;  // preset the flags
        //atomic_init(&packet_buff[i].flag, DATA_WRITE_MASK | DATA_PROC_MASK);  // preset the flags
        //pthread_spin_init(&packet_buff[i].spinlock, PTHREAD_PROCESS_SHARED);
        //packet_buff[i].flag = (atomic_flag)ATOMIC_FLAG_INIT;
        //log("buff[%d].flag = %d\n", i, packet_buff[i].flag);
        packet_buff[i].status = DATA_WRITTEN | DATA_PROCCED;
        log("buff[%d].status = %d\n", i, packet_buff[i].status);
    } 
}

//========================================================================
// Lock a buffer for read.
//========================================================================
void lock_buff_read(uint8_t idx, char check_val, const char* caller)
{
    struct timespec t1, t2;
    t1.tv_sec  = 0;
    t1.tv_nsec = 10;
    while(1)
    {
        log("%s - locking buff[%d]...\n", caller, idx);
        //while(atomic_flag_test_and_set(&packet_buff[idx].flag))
        //{
        //    sched_yield();
        //}
        //pthread_spin_lock(&packet_buff[idx].spinlock);
        pthread_mutex_lock(&packet_buff[idx].mutex);

        log("%s - buff[%d] data check...\n", caller, idx)
        log("%s - buff[%d] status is 0x%x against check value 0x%x\n", caller, idx, packet_buff[idx].status, check_val);
        if( !(packet_buff[idx].status & check_val) )
        {
            log("%s - buff[%d] ready for read\n", caller, idx);
            break;
        }
        //atomic_flag_clear(&packet_buff[idx].flag);
        //pthread_spin_unlock(&packet_buff[idx].spinlock);
        pthread_mutex_unlock(&packet_buff[idx].mutex);
        log("%s - buffer doesn't have new data\n", caller, idx);
        //sched_yield();
        //pthread_yield();
        nanosleep(&t1, &t2);
    }
    log("%s - buff[%d] locked\n", caller, idx);
}


//========================================================================
// Lock a buffer for write.
//========================================================================
void lock_buff_write(uint8_t idx, char check_val, const char* caller)
{
    struct timespec t1, t2;
    t1.tv_sec  = 0;
    t1.tv_nsec = 10;

    while(1)
    {
        log("%s - locking buff[%d]...\n", caller, idx);
        //while(atomic_flag_test_and_set(&packet_buff[idx].flag))
        //{
        //    sched_yield();
        //}
        //pthread_spin_lock(&packet_buff[idx].spinlock);
        pthread_mutex_lock(&packet_buff[idx].mutex);

        log("%s - buff[%d] data check...\n", caller, idx)
        log("%s - buff[%d] status is 0x%x against check value 0x%x\n",
            caller, idx, packet_buff[idx].status, check_val);
        if( (packet_buff[idx].status & check_val) == check_val )
        {
            log("%s - buff[%d] ready for write\n", caller, idx);
            break;
        }
        //atomic_flag_clear(&packet_buff[idx].flag);
        //pthread_spin_unlock(&packet_buff[idx].spinlock);
        pthread_mutex_unlock(&packet_buff[idx].mutex);
        err("%s - buff[%d] hasn't been read\n", caller, idx);
        //sched_yield();
        //pthread_yield();
        nanosleep(&t1, &t2);
    }
    log("%s - buff[%d] locked\n", caller, idx);
}


//========================================================================
// Unlock a buffer.
//========================================================================
void unlock_buff(uint8_t idx, const char* caller)
{
    //atomic_flag_clear(packet_buff[idx].flag);
    //pthread_spin_unlock(packet_buff[idx].spinlock);
    pthread_mutex_unlock(&packet_buff[idx].mutex);
    log("%s - buff[%d] unlocked\n", caller, idx);
}

//========================================================================
// Create channels for PVs.
//========================================================================
void create_channel(const char * thread, unsigned int first_pv, unsigned int last_pv)
{
    int status;
    for (int i=first_pv; i<=last_pv; i++)
    {   
        info("creating CA channel for %s (%s)...\n", pv[i].my_name, thread);
        //for (int j=0; j<8; j++)
        //{
            status = ca_create_channel(pv[i].my_name, NULL, NULL, 0, &pv[i].my_chid);
        //    if (1==ECA_NORMAL)
        //    {
        //        break;
        //    }
        //    sleep(1);
        //}
        SEVCHK(status, "Create channel failed");
        //for (int j=0; j<8; j++)
        //{
            status = ca_pend_io(1.0);
        //    if (1==ECA_NORMAL)
        //    {
        //        break;
        //    }
        //    sleep(1);
        //}
        SEVCHK(status, "Channel connection failed");
    }   
    //printf("[%s]: finished creating CA channels.\n", thread);
}

//========================================================================
// PV initialization
//------------------------------------------------------------------------
int pv_array_init(void)
{
    char          prefix[40];
    unsigned char prefix_len;
    char          pv_suffix[NUM_PVS][64];
    unsigned char i;
    FILE*         fp;
    
    //--------------------------------------------------
    // Get prefix
    //--------------------------------------------------
    memset(prefix, 0, sizeof(prefix));

    fp = fopen(PREFIX_CFG_FILE, "r");
    if(NULL == fp) 
    {   
        err("Failed to open %s\n", PREFIX_CFG_FILE);
        return -1;
    }   

    if(1 != fscanf(fp, "%s", prefix))
    {
        err("Incorrect data in %s. Make sure it contains prefix only.\n",
                __func__,
                PREFIX_CFG_FILE );
        fclose(fp);
        return -1;
    }   

    prefix_len = strlen(prefix);
    if(0 == prefix_len)
    {   
        err("Incorrect data in %s. Make sure it contains prefix only.\n",
                __func__,
                PREFIX_CFG_FILE );
        fclose(fp);
        return -1;
    }   

    fclose(fp);

    log("prefix is %s\n", prefix);



    //--------------------------------------------------
    // Define suffixes.
    //--------------------------------------------------
    memset(pv_suffix, 0, sizeof(pv_suffix));
    memset(pv_suffix, 0, sizeof(pv_suffix));
    memcpy(pv_suffix[PV_COUNT],            ".CNT",                 4);
    memcpy(pv_suffix[PV_DATAFILE_DIR],     ":DATAFILE_DIR",       12);
    memcpy(pv_suffix[PV_FILENAME],         ".FNAM",                5);
    memcpy(pv_suffix[PV_FILENAME_RBV],     ":FNAM_RBV",            9);
    memcpy(pv_suffix[PV_FILESIZE],         ":FSIZ",                5);
    memcpy(pv_suffix[PV_FILESIZE_RBV],     ":FSIZ_RBV",            9);
    memcpy(pv_suffix[PV_RUNNO],            ".RUNNO",               6);
    memcpy(pv_suffix[PV_RUNNO_RBV],        ":RUNNO_RBV",          10);
    memcpy(pv_suffix[PV_IPADDR],           ".IPADDR",              7);
    memcpy(pv_suffix[PV_IPADDR_RBV],       ":IPADDR_RBV",         11);
    memcpy(pv_suffix[PV_NELM],             ".NELM",                5);
    memcpy(pv_suffix[PV_NELM_RBV],         ":NELM_RBV",            9);
    memcpy(pv_suffix[PV_MONCH],            ".MONCH",               6);
    memcpy(pv_suffix[PV_MONCH_RBV],        ":MONCH_RBV",          10);
    memcpy(pv_suffix[PV_PID],              ":PID",                 4);
    memcpy(pv_suffix[PV_HOSTNAME],         ":HOSTNAME",            9);
    memcpy(pv_suffix[PV_DIR],              ":DIR",                 4);
    memcpy(pv_suffix[PV_WATCHDOG],         ":WATCHDOG",            9);
    memcpy(pv_suffix[PV_MCA],              ":MCA",                 4);
    memcpy(pv_suffix[PV_TDC],              ":TDC",                 4);
    memcpy(pv_suffix[PV_TSEN_PROC],        ":TSEN_PROC",          10);
    memcpy(pv_suffix[PV_CHEN_PROC],        ":CHEN_PROC",          10);
    memcpy(pv_suffix[PV_TSEN_CTRL],        ":TSEN_CTRL",          10);
    memcpy(pv_suffix[PV_CHEN_CTRL],        ":CHEN_CTRL",          10);
    memcpy(pv_suffix[PV_TSEN],             ".TSEN",                5);
    memcpy(pv_suffix[PV_CHEN],             ".CHEN",                5);
    memcpy(pv_suffix[PV_DATA_FILENAME],    ":DATA_FILENAME",      14);
    memcpy(pv_suffix[PV_SPEC_FILENAME],    ":SPEC_FILENAME",      14);

    //--------------------------------------------------
    // PV name assembly
    //--------------------------------------------------
    memset(pv, 0, sizeof(pv));
    for (i=0; i<NUM_PVS; i++)
    {
        memcpy (pv[i].my_name,
                prefix,
                prefix_len);

        memcpy( pv[i].my_name+prefix_len,
                pv_suffix[i],
                strlen(pv_suffix[i]));

        log("%s\n", pv[i].my_name);
    }

    //--------------------------------------------------
    // Pointers to variables
    //--------------------------------------------------
    //pv[PV_COUNT].my_var_p            = (void*)count;
    pv[PV_TMP_DATAFILE_DIR].my_var_p = (void*)tmp_datafile_dir;
    pv[PV_DATAFILE_DIR].my_var_p     = (void*)datafile_dir;
    pv[PV_FILENAME].my_var_p         = (void*)filename;
    pv[PV_FILENAME_RBV].my_var_p     = (void*)filename;
    pv[PV_FILESIZE].my_var_p         = (void*)(&filesize);
    pv[PV_FILESIZE_RBV].my_var_p     = (void*)(&filesize);
    pv[PV_RUNNO].my_var_p            = (void*)(&runno);
    pv[PV_RUNNO_RBV].my_var_p        = (void*)(&runno);
    pv[PV_IPADDR].my_var_p           = (void*)(&gige_ip_addr);
    pv[PV_IPADDR_RBV].my_var_p       = (void*)(&gige_ip_addr);
    pv[PV_NELM].my_var_p             = (void*)(&nelm);
    pv[PV_NELM_RBV].my_var_p         = (void*)(&nelm);
    pv[PV_MONCH].my_var_p            = (void*)(&monch);
    pv[PV_MONCH_RBV].my_var_p        = (void*)(&monch);
    pv[PV_PID].my_var_p              = (void*)(&pid);
    pv[PV_HOSTNAME].my_var_p         = (void*)hostname;
    pv[PV_DIR].my_var_p              = (void*)directory;
    pv[PV_WATCHDOG].my_var_p         = (void*)(&watchdog);
    pv[PV_MCA].my_var_p              = (void*)(&mca);
    pv[PV_TDC].my_var_p              = (void*)(&tdc);
    pv[PV_TSEN_PROC].my_var_p        = (void*)(&tsen_proc);
    pv[PV_CHEN_PROC].my_var_p        = (void*)(&chen_proc);
    pv[PV_TSEN_CTRL].my_var_p        = (void*)(&tsen_ctrl);
    pv[PV_CHEN_CTRL].my_var_p        = (void*)(&chen_ctrl);
    pv[PV_TSEN].my_var_p             = (void*)(&tsen);
    pv[PV_CHEN].my_var_p             = (void*)(&chen);
    pv[PV_DATA_FILENAME].my_var_p    = (void*)datafile;
    pv[PV_SPEC_FILENAME].my_var_p    = (void*)spectrafile;

    //--------------------------------------------------
    // Data types
    //--------------------------------------------------
#ifdef USE_EZCA
    pv[PV_COUNT].my_dtype            = ezcaByte;
    pv[PV_TMP_DATAFILE_DIR].my_dtype = ezcaString;
    pv[PV_DATAFILE_DIR].my_dtype     = ezcaString;
    pv[PV_FILENAME].my_dtype         = ezcaString;
    pv[PV_FILENAME_RBV].my_dtype     = ezcaString;
    pv[PV_FILESIZE].my_dtype         = ezcaLong;
    pv[PV_FILESIZE_RBV].my_dtype     = ezcaLong;
    pv[PV_RUNNO].my_dtype            = ezcaLong;
    pv[PV_RUNNO_RBV].my_dtype        = ezcaLong;
    pv[PV_IPADDR].my_dtype           = ezcaString;
    pv[PV_IPADDR_RBV].my_dtype       = ezcaString;
    pv[PV_NELM].my_dtype             = ezcaShort;
    pv[PV_NELM_RBV].my_dtype         = ezcaShort;
    pv[PV_MONCH].my_dtype            = ezcaLong;
    pv[PV_MONCH_RBV].my_dtype        = ezcaLong;
    pv[PV_PID].my_dtype              = ezcaLong;
    pv[PV_HOSTNAME].my_dtype         = ezcaString;
    pv[PV_DIR].my_dtype              = ezcaString;
    pv[PV_WATCHDOG].my_dtype         = ezcaShort;
    pv[PV_MCA].my_dtype              = ezcaLong;
    pv[PV_TDC].my_dtype              = ezcaLong;
    pv[PV_TSEN_PROC].my_dtype        = ezcaByte;
    pv[PV_CHEN_PROC].my_dtype        = ezcaByte;
    pv[PV_TSEN_CTRL].my_dtype        = ezcaByte;
    pv[PV_CHEN_CTRL].my_dtype        = ezcaByte;
    pv[PV_TSEN].my_dtype             = ezcaByte;
    pv[PV_CHEN].my_dtype             = ezcaByte;
    pv[PV_DATA_FILENAME].my_dtype    = eacaString;
    pv[PV_SPEC_FILENAME].my_dtype    = eacaString;
#else
    pv[PV_COUNT].my_dtype            = DBR_CHAR;
    pv[PV_TMP_DATAFILE_DIR].my_dtype = DBR_STRING;
    pv[PV_DATAFILE_DIR].my_dtype     = DBR_STRING;
    pv[PV_FILENAME].my_dtype         = DBR_STRING;
    pv[PV_FILENAME_RBV].my_dtype     = DBR_STRING;
    pv[PV_FILESIZE].my_dtype         = DBR_LONG;
    pv[PV_FILESIZE_RBV].my_dtype     = DBR_LONG;
    pv[PV_RUNNO].my_dtype            = DBR_LONG;
    pv[PV_RUNNO_RBV].my_dtype        = DBR_LONG;
    pv[PV_IPADDR].my_dtype           = DBR_STRING;
    pv[PV_IPADDR_RBV].my_dtype       = DBR_STRING;
    pv[PV_NELM].my_dtype             = DBR_LONG;
    pv[PV_NELM_RBV].my_dtype         = DBR_LONG;
    pv[PV_MONCH].my_dtype            = DBR_LONG;
    pv[PV_MONCH_RBV].my_dtype        = DBR_LONG;
    pv[PV_PID].my_dtype              = DBR_LONG;
    pv[PV_HOSTNAME].my_dtype         = DBR_STRING;
    pv[PV_DIR].my_dtype              = DBR_STRING;
    pv[PV_WATCHDOG].my_dtype         = DBR_SHORT;
    pv[PV_MCA].my_dtype              = DBR_LONG;
    pv[PV_TDC].my_dtype              = DBR_LONG;
    pv[PV_TSEN_PROC].my_dtype        = DBR_CHAR;
    pv[PV_CHEN_PROC].my_dtype        = DBR_CHAR;
    pv[PV_TSEN_CTRL].my_dtype        = DBR_CHAR;
    pv[PV_CHEN_CTRL].my_dtype        = DBR_CHAR;
    pv[PV_TSEN].my_dtype             = DBR_CHAR;
    pv[PV_CHEN].my_dtype             = DBR_CHAR;
    pv[PV_DATA_FILENAME].my_dtype    = DBR_STRING;
    pv[PV_SPEC_FILENAME].my_dtype    = DBR_STRING;
#endif
   
//    for(i = 0; i<NUM_PVS; i++)
//    {
//        pv[i].my_nelm = 1;  //this of xxEN will be changed to nelm later
//    }

    return 0;
}


//======================================================================== 

int main(int argc, char* argv[])
{
//    char test_en = 0;

    pthread_t tid[4];
    int status;

    struct timespec t1, t2;

    int opt;

    //-----------------------------------------------------------

    char* op_string = "t::d::h::";

    while ((opt = getopt(argc, argv, op_string))!= -1)
    {
        switch (opt)
        {
            case 't':
//                test_en = 1;
                log("test mode enabled.\n");
                break;
            case 'd':
                reg1_val = 0x3;
                log("start in test mode. FPGA will send test data.\n");
                break;
            case 'h':
                printf("Usage:\n");
                printf("    germ_udp_daemon [-t] [-d]\n");
                printf("        -t  : enable test mode.\n");
                printf("        -d  : enable dummy data.\n");
                break;
            default:
                break;
        }
    }

    //-----------------------------------------------------------

    log("starting Germanium Daemon...\n");

    memset(mca, 0, sizeof(mca));
    memset(tdc, 0, sizeof(tdc));
    //memset(filename, 0, sizeof(filename));
    //memset(datafile, 0, sizeof(datafile));
    //memset(spectrafile, 0, sizeof(datafile));
    memset(gige_ip_addr, 0, sizeof(gige_ip_addr));
    memset(hostname, 0, sizeof(hostname));
    memset(directory, 0, sizeof(directory));

    t1.tv_sec  = 0;
    t1.tv_nsec = 1000;

    log("initializing PV objects...\n");
    if(0 != pv_array_init())
    {
        err("failed to initialize PV objects.\n");
        return -1;
    }

    //--------------------------------------------------
    // Create channels for the PVs.
    //--------------------------------------------------
    create_channel(__func__, FIRST_MAIN_PV, LAST_MAIN_PV);

    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create @main");

    //-----------------------------------------------------------
    // Send environment information. 
    //-----------------------------------------------------------

    status = gethostname(hostname, 32);
    if (0 != status)
    {
        err("ERROR (%d)! gethostname() error. Exit the thread.\n", errno);
        return -1;
    }
    log("hostname is %s\n", hostname);

    if (NULL == getcwd(directory, sizeof(directory)))
    {
        err("ERROR (%d)! getcwd() error", errno);
        return -1;
    }
    log("current working dir is %s\n", directory);

    pid = getpid();
    log("pid is %ld\n", pid);

    for (int i=FIRST_ENV_PV; i<=LAST_ENV_PV; i++)
    {   
        pv_put(i);
    }   

    //-----------------------------------------------------------
    // Initialize packet buffers.
    //-----------------------------------------------------------
    buff_init();

    //-----------------------------------------------------------
    // Create threads.
    //-----------------------------------------------------------

    // Create exp_mon_thread to receive configuration
    // information from IOCs and report status.
    log("creating exp_mon...\n");
    while(1)
    {
        status = pthread_create(&tid[0], NULL, &exp_mon_thread, NULL);
        if ( 0 == status)
        {
            log("exp_mon created.\n");
            break;
        }

        err("Can't create exp_mon: [%s]\n",
    	        __func__,
		strerror(status));
    }

    //-------------------------------------------------------------------
    // Create udp_conn_thread to configure FPGA and receive data through
    // UDP connection.
    //do
    //{
    //    nanosleep(&t1, &t2);
    //} while(0 == exp_mon_thread_ready);

    log("creating udp_conn_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[1], NULL, &udp_conn_thread, NULL);
        if ( 0 == status)
        {
            log("udp_conn_thread created.\n");
            break;
        }

        err("Can't create udp_conn_thread: [%s]\n",
	            __func__,
		strerror(status));
    }

    //-------------------------------------------------------------------
    // Create data_write_thread to save raw data files.
    //do
    //{
    //    nanosleep(&t1, &t2);
    //} while(0 == udp_conn_thread_ready);

    log("creating data_write_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[2], NULL, &data_write_thread, NULL);
        if ( 0 == status)
        {
            log("data_write_thread created.\n");
            break;
        }

        err("Can't create data_write_thread: [%s]\n",
	            __func__,
		strerror(status));
    }

    //-------------------------------------------------------------------
    // Create mover_thread to save raw data files.
    //do
    //{
    //    nanosleep(&t1, &t2);
    //} while(0 == udp_conn_thread_ready);

    log("creating mover_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[2], NULL, &mover_thread, NULL);
        if ( 0 == status)
        {
            log("mover_thread created.\n");
            break;
        }

        err("Can't create mover_thread: [%s]\n",
	            __func__,
		strerror(status));
    }

    //-------------------------------------------------------------------
    // Create data_proc_thread to calculate and save spectra files.
    //do
    //{
    //    nanosleep(&t1, &t2);
    //} while(0 == data_write_thread_ready);
/*
    log("creating data_proc_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[3], NULL, &data_proc_thread, NULL);
        if ( 0 == status)
        {
            log("data_proc_thread created.\n");
            break;
        }

        err("Can't create data_proc_thread: [%s]\n",
	            __func__,
		strerror(status));
    }
*/
    log("finished initialization.\n");

    //-----------------------------------------------------------
    // Feed the watchdog.
    //-----------------------------------------------------------
    do
    {
        nanosleep(&t1, &t2);
    } while(0 == atomic_load(&data_write_thread_ready));


    while(1)
    {
        nanosleep(&t1, &t2);
        pv_put(PV_WATCHDOG);
    }

}
