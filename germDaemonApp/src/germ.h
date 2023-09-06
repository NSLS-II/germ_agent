#ifndef __GERM__
#define __GERM__

#include <pthread.h>
#include <time.h>
#include <stdint.h>

#include <cadef.h>

#define RETRY_ON_FAILURE  5
#define MAX_FILENAME_LEN  64
#define PREFIX_CFG_FILE  "prefix.cfg"


//###########################################################

//===========================================================
// PV definitions
//===========================================================

//-----------------------------------------------------------
// Written by main.
//-----------------------------------------------------------
#define PV_PID                 0
#define PV_HOSTNAME            1
#define PV_DIR                 2
// health
#define PV_WATCHDOG            3

//-----------------------------------------------------------
// Read/written by exp_mon_thread.
//-----------------------------------------------------------
#define PV_FILENAME            4
#define PV_RUNNO               5
#define PV_FILESIZE            6
#define PV_IPADDR              7
#define PV_NELM                8
#define PV_MONCH               9
#define PV_TSEN_PROC          10
#define PV_CHEN_PROC          11
#define PV_TSEN_CTRL          12
#define PV_CHEN_CTRL          13
#define PV_TSEN               14
#define PV_CHEN               15
// Write only
#define PV_FILENAME_RBV       16
#define PV_RUNNO_RBV          17
#define PV_FILESIZE_RBV       18
#define PV_IPADDR_RBV         19
#define PV_NELM_RBV           20
#define PV_MONCH_RBV          21

//-----------------------------------------------------------
// Read/written by data_proc_thread.
//-----------------------------------------------------------
#define PV_MCA                22
#define PV_TDC                23
#define PV_DATA_FILENAME      24
#define PV_SPEC_FILENAME      25


//===========================================================
// Some PV related constants
//===========================================================

#define MAX_PV_NAME_LEN       64
//#define MAX_NELM             384
#define MAX_NELM             192
//#define MAX_NELM              96

#define NUM_PVS               26

#define FIRST_MAIN_PV          0
#define LAST_MAIN_PV           3

#define FIRST_EXP_MON_PV       4
#define LAST_EXP_MON_PV       21

#define FIRST_EXP_MON_RD_PV    4
#define LAST_EXP_MON_RD_PV    15

#define FIRST_DATA_PROC_PV    22
#define LAST_DATA_PROC_PV     25

#define FIRST_ENV_PV          PV_PID
#define LAST_ENV_PV           PV_DIR

#define FIRST_RUN_PV          PV_FILENAME
#define LAST_RUN_PV           PV_FILESIZE

#define FIRST_RUN_RBV_PV      PV_FILENAME_RBV
#define LAST_RUN_RBV_PV       PV_FILESIZE_RBV


//###########################################################

#define NUM_FRAME_BUFF   2
#define NUM_PACKET_BUFF  8

typedef struct
{
    pthread_mutex_t  lock;
    uint32_t         frame_num;
    uint16_t         num_lost_event;
    uint64_t         num_words;
    uint16_t         evtdata[500000000];
} frame_buff_t;

typedef struct
{
    pthread_mutex_t  lock;
//    uint32_t         frame_num;
//    uint16_t         num_lost_event;
    uint8_t         flag;
    uint16_t        length;
    uint8_t         packet[1024];
} packet_buff_t;

typedef struct
{
    char           my_name[64];
    unsigned char  my_dtype;
    unsigned int   my_nelm;
    void          *my_var_p;
    chid           my_chid;
    evid           my_evid;
} pv_obj_t;

//###########################################################

// mca/tdc dimensions
#define NUM_MCA_ROW   MAX_NELM
#define NUM_MCA_COL       4096
#define NUM_TDC_ROW   MAX_NELM
#define NUM_TDC_COL       1024

//===========================================================

//#define TRACE_CA
//#define USE_EZCA
#define EZCA_DEBUG

//###########################################################

#ifdef USE_EZCA
extern char ca_dtype[6][11];
#else
extern char ca_dtype[7][11];

#define ezca_type_to_dbr(t)   (t==ezcaByte)   ? DBR_CHAR   :( \
                              (t==ezcaString) ? DBR_STRING :( \
                              (t==ezcaShort)  ? DBR_SHORT  :( \
                              (t==ezcaLong)   ? DBR_LONG   :( \
                              (t==ezcaFloat)  ? DBR_FLOAT  :( \
                              (t==ezcaDouble) ? DBR_DOUBLE :( \
                              999 ))))))
#endif

//###########################################################

//===========================================================
#ifdef USE_EZCA
//-----------------------------------------------------------
#ifdef TRACE_CA
#define pv_get(i)        printf("[%s]: read %s as %s\n", __func__, pv[i].my_name, ca_dtype[pv[i].my_dtype]);\
                         ezcaGet(pv[i].my_name, pv[i].my_dtype, 1, pv[i].my_var_p);sleep(1)
#define pv_put(i)        printf("[%s]: write %s as %s\n", __func__, pv[i].my_name, ca_dtype[pv[i].my_dtype]);\
                         ezcaPut(pv[i].my_name, pv[i].my_dtype, 1, pv[i].my_var_p);sleep(1)
#define pvs_get(i,n)     printf("[%s]: read %s as %ld %s\n", __func__, pv[i].my_name, n, ca_dtype[pv[i].my_dtype]);\
                         ezcaGet(pv[i].my_name, pv[i].my_dtype, n, pv[i].my_var_p);sleep(1)
#define pvs_put(i,n)     printf("[%s]: write %s as %ld %s\n", __func__, pv[i].my_name, n, ca_dtype[pv[i].my_dtype]);\
                         ezcaPut(pv[i].my_name, pv[i].my_dtype, n, pv[i].my_var_p);sleep(1)
//-----------------------------------------------------------
#else
#define pv_get(i)        ezcaGet(pv[i].my_name, pv[i].my_dtype, 1, pv[i].my_var_p)
#define pv_put(i)        ezcaPut(pv[i].my_name, pv[i].my_dtype, 1, pv[i].my_var_p)
#define pvs_get(i, n)    ezcaGet(pv[i].my_name, pv[i].my_dtype, n, pv[i].my_var_p)
#define pvs_put(i, n)    ezcaPut(pv[i].my_name, pv[i].my_dtype, n, pv[i].my_var_p)
//-----------------------------------------------------------
#endif // ifdef TRACE_CA
//===========================================================
#else
//-----------------------------------------------------------
#ifdef TRACE_CA
#define pv_get(i)                                                                               \
    printf("[%s]: read %s as %s\n", __func__, pv[i].my_name, ca_dtype[pv[i].my_dtype]);         \
    SEVCHK(ca_get(pv[i].my_dtype, pv[i].my_chid, pv[i].my_var_p), "Get failed");                \
    SEVCHK(ca_pend_io(1.0), "I/O failed"); sleep(1)

#define pv_put(i)                                                                               \
    printf("[%s]: write %s as %s\n", __func__, pv[i].my_name, ca_dtype[pv[i].my_dtype]);        \
    SEVCHK(ca_put(pv[i].my_dtype, pv[i].my_chid, pv[i].my_var_p), "Put failed");                \
    ca_flush_io(); sleep(1)

#define pvs_get(i, n)                                                                           \
    printf("[%s]: read %s as %d %s\n", __func__, pv[i].my_name, n, ca_dtype[pv[i].my_dtype]);   \
    SEVCHK(ca_array_get(pv[i].my_dtype, n, pv[i].my_chid, pv[i].my_var_p), "Get failed");       \
    SEVCHK(ca_pend_io(1.0), "I/O failed"); sleep(1)

#define pvs_put(i, n)                                                                           \
    printf("[%s]: write %s as %d %s\n", __func__, pv[i].my_name, n, ca_dtype[pv[i].my_dtype]);  \
    SEVCHK(ca_array_put(pv[i].my_dtype, n, pv[i].my_chid, pv[i].my_var_p), "Put failed");       \
    ca_flush_io(); sleep(1)
//-----------------------------------------------------------
#else
#define pv_get(i)                                                                               \
    SEVCHK(ca_get(pv[i].my_dtype, pv[i].my_chid, pv[i].my_var_p), "Get failed");                \
    SEVCHK(ca_pend_io(1.0), "I/O failed"); sleep(1)

#define pv_put(i)                                                                               \
    SEVCHK(ca_put(pv[i].my_dtype, pv[i].my_chid, pv[i].my_var_p), "Put failed");                \
    ca_flush_io(); sleep(1)

#define pvs_get(i, n)                                                                           \
    SEVCHK(ca_array_get(pv[i].my_dtype, n, pv[i].my_chid, pv[i].my_var_p), "Get failed");       \
    SEVCHK(ca_pend_io(1.0), "I/O failed"); sleep(1)

#define pvs_put(i, n)                                                                           \
    SEVCHK(ca_array_put(pv[i].my_dtype, n, pv[i].my_chid, pv[i].my_var_p), "Put failed");       \
    ca_flush_io(); sleep(1)
//-----------------------------------------------------------
#endif // ifdef TRACE_CA
//===========================================================
#endif // ifdef USE_EZCA

//###########################################################
// Some functions

long int time_elapsed(struct timeval time_i, struct timeval time_f);

void create_channel(const char* thread, unsigned int first, unsigned int last_pv);


#endif
