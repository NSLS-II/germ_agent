#ifndef _EPICS_
#define _EPICS_

/****** PV definitions ******/
#define NUM_PVS           12
#define NUM_READ_PVS       4

/* The PVs are put in order as:
 *   read-only PVs
 *   write-only PVs (written only once)
 *   write-only PVs (written multiple times)
 */
// Read-only
#define PV_SCANSTATUS      0
#define PV_FILENAME        1
#define PV_FILESIZE        2
#define PV_RUNNO           3
// Write only once
#define PV_PID             4
#define PV_HOSTNAME        5
#define PV_DIR             6
// Write multiple times  
#define PV_HEARTBEAT       7
#define PV_WATCHDOG        8
#define PV_FILENAME_ACK    9
#define PV_FILESIZE_ACK   10
#define PV_RUNNO_ACK      11
 
#define MAX_PV_NAME_LEN   60
 
#define SCAN_STATUS_IDLE   0
#define SCAN_STATUS_STAGE  1
#define SCAN_STATUS_SCAN   2

/************************/
#define pv_get(i) ezcaGet(pv[i], pv_dtype[i], 1, pv_var_p[i])
#define pv_set(i) ezcaPut(pv[i], pv_dtype[i], 1, pv_var_p[i])

#define PREFIX_CFG_FILE "prefix.cfg"

void * epics_thread(void* arg);

#endif
