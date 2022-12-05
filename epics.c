#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

//#include <tsDefs.h>
#include <cadef.h>
#include <ezca.h>

#include "epics.h"

extern char          filename[64];
extern unsigned long filesize;
extern unsigned long runno;

void * epics_thread(void * arg)
{
    char prefix[40];
    unsigned char prefix_len;
    FILE * fp;

    unsigned char scan_status;
    //char          filename[64];
    //unsigned long filesize;
    //unsigned long runno;
    char          hostname[32];
    unsigned long heartbeat;
    unsigned int  watchdog;
    unsigned long pid;
    char          directory[64];

    char  pv[NUM_PVS][64];
    char  pv_suffix[NUM_PVS][64];
    void* pv_var_p[NUM_PVS];
    char  pv_dtype[NUM_PVS];

    chid chan;
    int status;
    enum thread_state_t {IDLE, STAGE, WAIT_FOR_SCAN, SCAN} thread_state;

    unsigned char i;

    struct timespec t1, t2;

    printf("#####################################################\n");
    printf("%s: Initializing epics_thread...\n", __func__);

    t1.tv_sec  = 1;
    t1.tv_nsec = 0;

    /***** Get running environment *****/
    pid = getpid();
    printf("%s: pid is %ld\n", __func__, pid);
    
    status = gethostname(hostname, 32);
    if (0 != status)
    {
        printf("%s: ERROR (%d)! gethostname() error.\n", __func__, status);
        return;
    }
    printf("%s: hostname is %s\n", __func__, hostname);

    status = getcwd(directory, sizeof(directory));
    if (NULL == status)
    {
        printf("%s: ERROR (%d)! getcwd() error", __func__, status);
        return;
    }
    printf("%s: current working dir is %s\n", __func__, directory);

    /***** PV initialization *****/
    memset(pv_suffix, 0, sizeof(pv_suffix));
    memcpy(pv_suffix[PV_SCANSTATUS],   "ScanStatus",         10);
    memcpy(pv_suffix[PV_FILENAME],     "Data:Filename",      13);
    memcpy(pv_suffix[PV_FILESIZE],     "Data:Filesize",      13);
    memcpy(pv_suffix[PV_RUNNO],        "Data:Runno",         10);
    memcpy(pv_suffix[PV_PID],          "Data:AgentPid",      13);
    memcpy(pv_suffix[PV_HOSTNAME],     "Data:AgentHostname", 18);
    memcpy(pv_suffix[PV_DIR],          "Data:AgentDir",      13);
    memcpy(pv_suffix[PV_HEARTBEAT],    "Data:Heartbeat",     14);
    memcpy(pv_suffix[PV_WATCHDOG],     "Data:Watchdog",      13);
    memcpy(pv_suffix[PV_FILENAME_ACK], "Data:Filename_ACK",  17);
    memcpy(pv_suffix[PV_FILESIZE_ACK], "Data:Filesize_ACK",  17);
    memcpy(pv_suffix[PV_RUNNO_ACK],    "Data:Runno_ACK",     14); 

    pv_var_p[PV_SCANSTATUS]   = (void*)(&scan_status);
    pv_var_p[PV_FILENAME]     = (void*)filename;
    pv_var_p[PV_FILESIZE]     = (void*)(&filesize);
    pv_var_p[PV_RUNNO]        = (void*)(&runno);
    pv_var_p[PV_PID]          = (void*)(&pid);
    pv_var_p[PV_HOSTNAME]     = (void*)hostname;
    pv_var_p[PV_DIR]          = (void*)directory;
    pv_var_p[PV_HEARTBEAT]    = (void*)(&heartbeat);
    pv_var_p[PV_WATCHDOG]     = (void*)(&watchdog);
    pv_var_p[PV_FILENAME_ACK] = (void*)filename;
    pv_var_p[PV_FILESIZE_ACK] = (void*)(&filesize);
    pv_var_p[PV_RUNNO_ACK]    = (void*)(&runno);

    pv_dtype[PV_SCANSTATUS]   = ezcaShort;
    pv_dtype[PV_FILENAME]     = ezcaString;
    pv_dtype[PV_FILESIZE]     = ezcaLong;
    pv_dtype[PV_RUNNO]        = ezcaLong;
    pv_dtype[PV_PID]          = ezcaLong;
    pv_dtype[PV_HOSTNAME]     = ezcaString;
    pv_dtype[PV_DIR]          = ezcaString;
    pv_dtype[PV_HEARTBEAT]    = ezcaLong;
    pv_dtype[PV_WATCHDOG]     = ezcaShort;
    pv_dtype[PV_FILENAME_ACK] = ezcaString;
    pv_dtype[PV_FILESIZE_ACK] = ezcaLong;
    pv_dtype[PV_RUNNO_ACK]    = ezcaLong;

    memset(pv, 0, sizeof(pv));
    
    fp = fopen(PREFIX_CFG_FILE, "r");
    if(NULL==fp)
    {
        printf("%s: ERROR! Failed to open %s\n", __func__, PREFIX_CFG_FILE);
        return;
    }

    fscanf(fp, "%s", prefix);
    if(0==strlen(prefix))
    {
        printf( "%s: ERROR! Incorrect data in %s. Make sure it contains prefix only.\n",
	        __func__,
                PREFIX_CFG_FILE );
        fclose(fp);
        return;
    }
    fclose(fp);

//    printf("Prefix is %s\n", prefix);
    prefix_len = strlen(prefix);
//    printf("prefix_len = %d\n", prefix_len);

    for (i=0; i<NUM_PVS; i++)
    {
        memcpy(pv[i], prefix, prefix_len);
        memcpy(pv[i]+prefix_len, pv_suffix[i], strlen(pv_suffix[i]));
        //printf("%s\n", pv[i]);
    }

    printf("%s: send environment information...\n", __func__);
    for (i=NUM_READ_PVS; i<PV_HEARTBEAT; i++)
    {
    //    printf("%s: %s is %s\n", __func__, pv[i], *pv_var_p[i]);
        pv_set(i);
    }

    /*for (i=0; i<NUM_READ_PVS; i++)
        status = ezcaSetMonitor(pv[i], pv_dtype[i], 1);
    */

    printf("%s: epics_thread initialization finished.\n", __func__);

    printf("=====================================================\n");

    /***** Update *****/
    heartbeat = 0;
    watchdog  = 0;
    //pv_set(PV_WATCHDOG);
    printf("%s: wait for stage...\n", __func__);
    while(1)
    {
	nanosleep(&t1, &t2);

	pv_set(PV_HEARTBEAT);
	pv_set(PV_WATCHDOG);
	heartbeat++;

	pv_get(PV_SCANSTATUS);
	//printf("%s: scan_status is %d\n", __func__, scan_status);

        switch(thread_state)
	{
            //------------------------------------------------------

	    case IDLE:
	        if (SCAN_STATUS_STAGE == scan_status)
		{
	            printf("%s: start to stage...\n", __func__);
		    thread_state = STAGE;
		}
		//else
	        //    printf("%s: wait for stage...\n", __func__);

		break;
	    
            //------------------------------------------------------

	    case STAGE:
                for(i=0; i<NUM_READ_PVS; i++)
	        {
	            pv_get(i);
	        }

	        /*
	        printf("%s: filename: %s\n", __func__, filename);
	        printf("%s: filesize: %d\n", __func__, filesize);
	        printf("%s: runno: %d\n",    __func__, runno);
                */

	        for (i=PV_HEARTBEAT+1; i<NUM_PVS; i++)
	        {
		    pv_set(i);
	            //ezcaPut(pv[i], pv_dtype[i], 1, pv_var_p[i]);
	        }

	        printf("%s: stage. Wait for scan...\n", __func__);
		thread_state = WAIT_FOR_SCAN;

		break;

            //------------------------------------------------------

	    case WAIT_FOR_SCAN:
	        if (SCAN_STATUS_SCAN == scan_status)
		{
	            printf("%s: scan in process...\n", __func__);
		    thread_state = SCAN;
		}
		else
	            if (SCAN_STATUS_IDLE == scan_status)
		    {
	                printf("%s: wait for stage...\n", __func__);
		        thread_state = IDLE;
                    }

		break;

            //------------------------------------------------------

            case SCAN:
	        if (SCAN_STATUS_IDLE == scan_status)
		{
	            printf("%s: scan completed.\n", __func__);
	            printf("%s: wait for stage...\n", __func__);
		    thread_state = IDLE;
		}

		break;

            //------------------------------------------------------

	    default:
	        break;
	}
	/*
        printf("=========================================\n");
        do
	{
	    printf("%s: scan_status is %d\n", __func__, scan_status);
	} while(SCAN_STATUS_STAGE != scan_status);


        printf("-----------------------------------------\n");
	printf("%s: wait for scan...\n", __func__);
        do
	{
	    nanosleep(&t1, &t2);
	    pv_get(PV_SCANSTATUS);
	    printf("%s: scan_status is %d\n", __func__, scan_status);
	} while(SCAN_STATUS_SCAN != scan_status);

        printf("-----------------------------------------\n");
	printf("%s: wait for scan to complete...\n", __func__);
        do
	{
	    nanosleep(&t1, &t2);
	    pv_get(PV_SCANSTATUS);
	    printf("%s: scan_status is %d\n", __func__, scan_status);
	} while(SCAN_STATUS_SCAN != scan_status);
	*/
    }
}
