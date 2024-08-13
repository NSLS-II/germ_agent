#ifndef __GERM__
#define __GERM__

#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>

//#include <iostream>

#include <cadef.h>
#include "log.h"

#define RETRY_ON_FAILURE  5
#define MAX_FILENAME_LEN  255
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
#define PV_COUNT               4
#define PV_TMP_DATAFILE_DIR    5
#define PV_DATAFILE_DIR        6
#define PV_FILENAME            7
#define PV_RUNNO               8
#define PV_FILESIZE            9
#define PV_IPADDR             10
#define PV_NELM               11
#define PV_MONCH              12
#define PV_TSEN_PROC          13
#define PV_CHEN_PROC          14
#define PV_TSEN_CTRL          15
#define PV_CHEN_CTRL          16
#define PV_TSEN               17
#define PV_CHEN               18
#define PV_RESTART            19
// Write only
#define PV_FILENAME_RBV       20
#define PV_RUNNO_RBV          21
#define PV_FILESIZE_RBV       22
#define PV_IPADDR_RBV         23
#define PV_NELM_RBV           24
#define PV_MONCH_RBV          25

//-----------------------------------------------------------
// Read/written by data_write_thread.
//-----------------------------------------------------------
#define PV_DATA_FILENAME      26

//-----------------------------------------------------------
// Read/written by data_proc_thread.
//-----------------------------------------------------------
#define PV_MCA                27
#define PV_TDC                28
#define PV_SPEC_FILENAME      29


//===========================================================
// Some PV related constants
//===========================================================

#define MAX_PV_NAME_LEN      256
#define MAX_NELM             384

#define NUM_PVS               30

#define FIRST_MAIN_PV          0
#define LAST_MAIN_PV           3

#define FIRST_EXP_MON_PV       4
#define LAST_EXP_MON_PV       25

#define FIRST_EXP_MON_RD_PV    4
#define LAST_EXP_MON_RD_PV    19

#define FIRST_DATA_WRITE_PV   26
#define LAST_DATA_WRITE_PV    26

#define FIRST_DATA_PROC_PV    27
#define LAST_DATA_PROC_PV     29

#define FIRST_ENV_PV          PV_PID
#define LAST_ENV_PV           PV_DIR

#define FIRST_RUN_PV          PV_TMP_DATAFILE_DIR
#define LAST_RUN_PV           PV_FILESIZE

#define FIRST_RUN_RBV_PV      PV_FILENAME_RBV
#define LAST_RUN_RBV_PV       PV_FILESIZE_RBV


//###########################################################

#define SOF_MARKER       0xfeedface
#define SOF_MARKER_UPPER 0xfeed
#define SOF_MARKER_LOWER 0xface
#define EOF_MARKER       0xdecafbad
#define EOF_MARKER_UPPER 0xdeca
#define EOF_MARKER_LOWER 0xfbad


//#define NUM_FRAME_BUFF       2
#define NUM_PACKET_BUFF                     16    // must be power of 2
#define PACKET_BUFF_MASK   (NUM_PACKET_BUFF-1) 
#define MAX_PACKET_LENGTH                 2048

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
    pthread_mutex_t     mutex;
    uint8_t             status;
    uint16_t            length;
    uint32_t            runno;
    uint8_t             packet[MAX_PACKET_LENGTH];
} packet_buff_t;

#define DATA_PROCCED   0x01
#define DATA_WRITTEN   0x02
#define DATA_PROCCING  0x10
#define DATA_WRITING   0x20

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

//###########################################################

extern char ca_dtype[7][11];

#define ezca_type_to_dbr(t)   (t==ezcaByte)   ? DBR_CHAR   :( \
                              (t==ezcaString) ? DBR_STRING :( \
                              (t==ezcaShort)  ? DBR_SHORT  :( \
                              (t==ezcaLong)   ? DBR_LONG   :( \
                              (t==ezcaFloat)  ? DBR_FLOAT  :( \
                              (t==ezcaDouble) ? DBR_DOUBLE :( \
                              999 ))))))

//###########################################################
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

//###########################################################

#define mutex_lock(mutex, buff_num)        log("wait on buff[%d]\n", buff_num);  \
                                           log("pthread_mutex_lock returns %d\n", pthread_mutex_lock(mutex));            \
                                           log("buff[%d] locked\n", buff_num)
    
#define mutex_unlock(mutex, buff_num)      log("releasing buff[%d]\n", buff_num); \
                                           pthread_mutex_unlock(mutex);           \
                                           log("buff[%d] released\n", buff_num)


// Some functions

long int time_elapsed(struct timeval time_i, struct timeval time_f);

//========================================================================
// Read a mutex protected string.
//========================================================================
inline void read_protected_string( char * src,
                                   char * dest,
                                   unsigned char len,
                                   pthread_mutex_t * mutex_p )
{
    memset(dest, 0, len);
    log("lock mutex for read.\n");
    pthread_mutex_lock(mutex_p);
    strcpy(dest, src);
    pthread_mutex_unlock(mutex_p);
    log("mutex unlocked\n");
}

//========================================================================
// Write a mutex protected string.
//========================================================================
inline void write_protected_string( char * src,
                                    char * dest,
                                    unsigned char len,
                                    pthread_mutex_t * mutex_p )
{
    log("lock mutex for write.\n");
    pthread_mutex_lock(mutex_p);
    memset(dest, 0, len);
    strcpy(dest, src);
    pthread_mutex_unlock(mutex_p);
    log("mutex unlocked\n");
}

void lock_buff_read(uint8_t idx, char check_val, const char* caller);
void lock_buff_write(uint8_t idx, char check_val, const char* caller);
void unlock_buff(uint8_t, const char* caller);

void create_channel(const char* thread, unsigned int first, unsigned int last_pv);


#endif
