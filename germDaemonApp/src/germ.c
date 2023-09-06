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

#include <cadef.h>
//#include "ezca.h"
#include <ezca.h>

#include "germ.h"
//#include "udp.h"
#include "udp_conn.h"
#include "data_proc.h"
#include "exp_mon.h"
//#include "test.h"
#include "log.h"


//event data buffer for a frame
//uint16_t evtdata[500000000];
//uint32_t evtdata[20000000];
//

//// indicate which bufferto write/read
//unsigned char write_buff;
//unsigned char read_buff;

/* arrays for energy and time spectra */
//int evnt;

    
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

//========================================================================
// Calculate elapsed time.
//------------------------------------------------------------------------
long int time_elapsed(struct timeval time_i, struct timeval time_f)
{
    return (1000000*(time_f.tv_sec-time_i.tv_sec) + (time_f.tv_usec-time_i.tv_usec));
}


//========================================================================
// Create channels for PVs.
//========================================================================
void create_channel(const char * thread, unsigned int first_pv, unsigned int last_pv)
{
    int status;
    for (int i=first_pv; i<=last_pv; i++)
    {   
        log("creating CA channel for %s...\n", thread, pv[i].my_name);
        status = ca_create_channel(pv[i].my_name, NULL, NULL, 0, &pv[i].my_chid);
        SEVCHK(status, "Create channel failed");
        status = ca_pend_io(1.0);
        SEVCHK(status, "Channel connection failed");
        //sleep(1);
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
        err("ERROR!!! Failed to open %s\n", PREFIX_CFG_FILE);
        return -1;
    }   

    if(1 != fscanf(fp, "%s", prefix))
    {
        err("ERROR!!! Incorrect data in %s. Make sure it contains prefix only.\n",
                __func__,
                PREFIX_CFG_FILE );
        fclose(fp);
        return -1;
    }   

    prefix_len = strlen(prefix);
    if(0 == prefix_len)
    {   
        err("ERROR!!! Incorrect data in %s. Make sure it contains prefix only.\n",
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

        printf("%s\n", pv[i].my_name);
    }

    //--------------------------------------------------
    // Pointers to variables
    //--------------------------------------------------
    pv[PV_FILENAME].my_var_p      = (void*)filename;
    pv[PV_FILENAME_RBV].my_var_p  = (void*)filename;
    pv[PV_FILESIZE].my_var_p      = (void*)(&filesize);
    pv[PV_FILESIZE_RBV].my_var_p  = (void*)(&filesize);
    pv[PV_RUNNO].my_var_p         = (void*)(&runno);
    pv[PV_RUNNO_RBV].my_var_p     = (void*)(&runno);
    pv[PV_IPADDR].my_var_p        = (void*)(&gige_ip_addr);
    pv[PV_IPADDR_RBV].my_var_p    = (void*)(&gige_ip_addr);
    pv[PV_NELM].my_var_p          = (void*)(&nelm);
    pv[PV_NELM_RBV].my_var_p      = (void*)(&nelm);
    pv[PV_MONCH].my_var_p         = (void*)(&monch);
    pv[PV_MONCH_RBV].my_var_p     = (void*)(&monch);
    pv[PV_PID].my_var_p           = (void*)(&pid);
    pv[PV_HOSTNAME].my_var_p      = (void*)hostname;
    pv[PV_DIR].my_var_p           = (void*)directory;
    pv[PV_WATCHDOG].my_var_p      = (void*)(&watchdog);
    pv[PV_MCA].my_var_p           = (void*)(&mca);
    pv[PV_TDC].my_var_p           = (void*)(&tdc);
    pv[PV_TSEN_PROC].my_var_p     = (void*)(&tsen_proc);
    pv[PV_CHEN_PROC].my_var_p     = (void*)(&chen_proc);
    pv[PV_TSEN_CTRL].my_var_p     = (void*)(&tsen_ctrl);
    pv[PV_CHEN_CTRL].my_var_p     = (void*)(&chen_ctrl);
    pv[PV_TSEN].my_var_p          = (void*)(&tsen);
    pv[PV_CHEN].my_var_p          = (void*)(&chen);
    pv[PV_DATA_FILENAME].my_var_p = (void*)datafile;
    pv[PV_SPEC_FILENAME].my_var_p = (void*)spectrafile;

    //--------------------------------------------------
    // Data types
    //--------------------------------------------------
#ifdef USE_EZCA
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

    t1.tv_sec  = 1;
    t1.tv_nsec = 0;

    log("initializing PV objects...\n");
    if(0 != pv_array_init())
    {
        log("failed to initialize PV objects.\n");
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

        err("ERROR!!! Can't create exp_mon: [%s]\n",
    	        __func__,
		strerror(status));
    }

    // Create udp_conn_thread to configure FPGA and receive data through
    // UDP connection.
    log("creating udp_conn_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[1], NULL, &udp_conn_thread, NULL);
        if ( 0 == status)
        {
            log("udp_conn_thread created.\n");
            break;
        }

        err("ERROR!!! Can't create udp_conn_thread: [%s]\n",
	            __func__,
		strerror(status));
    }

    // Create data_write_thread to save raw data to files.
    log("creating data_write_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[2], NULL, &data_write_thread, NULL);
        if ( 0 == status)
        {
            log("data_write_thread created.\n");
            break;
        }

        err("ERROR!!! Can't create data_write_thread: [%s]\n",
	            __func__,
		strerror(status));
    }

    // Create data_proc_thread to process data and save files.
    log("creating data_proc_thread...\n");
    while(1)
    {
        status = pthread_create(&tid[3], NULL, &data_proc_thread, NULL);
        if ( 0 == status)
        {
            log("data_proc_thread created.\n");
            break;
        }

        err("ERROR!!! Can't create data_proc_thread: [%s]\n",
	            __func__,
		strerror(status));
    }

    log("finished initialization.\n");

    //-----------------------------------------------------------
    // Feed the watchdog.
    //-----------------------------------------------------------
    while(1)
    {
        nanosleep(&t1, &t2);
        pv_put(PV_WATCHDOG);
    }

}
