#ifndef _TEST_AGENT_T
#define _TEST_AGENT_T


#define PREFIX_CFG_FILE   "prefix.cfg"

#define NUM_PVS        9
#define PV_TSEN_PROC   0
#define PV_CHEN_PROC   1
#define PV_TSEN_CTRL   2
#define PV_CHEN_CTRL   3
#define PV_TSEN        4
#define PV_CHEN        5
#define PV_MONCH       6
#define PV_NELM        7
#define PV_WATCHDOG    8

#define MAX_PV_LEN    64
#define MAX_NELM     384

#define ENABLE         0
#define ENABLE_ALL     1
#define DISABLE        2
#define DISABLE_ALL    3 

#define pv_get(i, d)        ezcaGet(pv[i], pv_dtype[i], 1, d)
#define pvs_get(i, n, d)    ezcaGet(pv[i], pv_dtype[i], n, d)
#define pv_set(i, d)        ezcaPut(pv[i], pv_dtype[i], 1, d)
#define pvs_put(i, n, d)    ezcaPut(pv[i], pv_dtype[i], n, d)


/*===================================================
 * PV initialization
 *===================================================*/
void pv_array_init(void);

/*===================================================
 * Enables/disables the target: test pulse, channel.
 *===================================================*/
void array_proc( unsigned char pv_proc,             // ID of the PV in array pv[]. Can be PV_TSEN_PROC or PV_CHEN_PROC
                 unsigned char pv_ctrl,             // ID of the PV in array pv[]. Can be PV_TSEN_CTRL or PV_CHEN_CTRL
                 unsigned char pv_en,               // ID of the PV in array pv[]. Can be PV_TSEN or PV_CHEN
                 unsigned char en_val);             // Value to enable the target. 1 for PV_TSEN, 0 for PV_CHEN

/*===================================================
 * Thread definition.
 *===================================================*/
void * germ_test_thread(void * arg);

#endif
