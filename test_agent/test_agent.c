#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <cadef.h>
#include <ezca.h>

#include "test_agent.h"

char  pv[NUM_PVS][MAX_PV_LEN];
char  pv_dtype[NUM_PVS];
unsigned long nelm;
unsigned long monch;

/*===================================================
 * PV initialization
 *===================================================*/
void pv_array_init(void)
{
    char          prefix[40];
    unsigned char prefix_len;
    char          pv_suffix[NUM_PVS][64];

    FILE* fp;


    memset(pv, 0, sizeof(pv));

    fp = fopen(PREFIX_CFG_FILE, "r");
    if( NULL==fp )
    {
        printf("[%s]: ERROR! Failed to open %s\n", __func__, PREFIX_CFG_FILE);
        return;
    }

    fscanf(fp, "%s", prefix);
    if(0==strlen(prefix))
    {
        printf( "[%s]: ERROR! Incorrect data in %s. Make sure it contains prefix only.\n",
                __func__,
                PREFIX_CFG_FILE );
        fclose(fp);
        return;
    }
    fclose(fp);

    prefix_len = strlen(prefix);
    printf("prefix is %s, length is %d\n", prefix, prefix_len);

    memset(pv_suffix, 0, sizeof(pv_suffix));
    memcpy(pv_suffix[PV_TSEN_PROC],  "TSEN_PROC", 	9);
    memcpy(pv_suffix[PV_CHEN_PROC],  "CHEN_PROC", 	9);
    memcpy(pv_suffix[PV_TSEN_CTRL],  "TSEN_CTRL", 	9);
    memcpy(pv_suffix[PV_CHEN_CTRL],  "CHEN_CTRL", 	9);
    memcpy(pv_suffix[PV_TSEN],       "TSEN",      	4);
    memcpy(pv_suffix[PV_CHEN],       "CHEN",		4);
    memcpy(pv_suffix[PV_MONCH],      "MONCH",     	5);
    memcpy(pv_suffix[PV_NELM],       "NELM",           	4);
    memcpy(pv_suffix[PV_WATCHDOG],   "TEST:WATCHDOG",  13);
    
    pv_dtype[PV_TSEN_PROC] = ezcaByte;
    pv_dtype[PV_CHEN_PROC] = ezcaByte;
    pv_dtype[PV_TSEN_CTRL] = ezcaByte;
    pv_dtype[PV_CHEN_CTRL] = ezcaByte;
    pv_dtype[PV_TSEN]      = ezcaByte;
    pv_dtype[PV_CHEN]      = ezcaByte;
    pv_dtype[PV_MONCH]     = ezcaLong;
    pv_dtype[PV_NELM]      = ezcaLong;
    pv_dtype[PV_WATCHDOG]  = ezcaLong;
    
    for (int i=0; i<NUM_PVS; i++)
    {
    }

    for (int i=0; i<NUM_PVS; i++)
    {
        memcpy(pv[i], prefix, prefix_len);
        memcpy(pv[i]+prefix_len, pv_suffix[i], strlen(pv_suffix[i]));
        printf("[%s]: PV = %s\n", __func__, pv[i]);
    }
}


/*===================================================
 * Enables/disables the target: test pulse, channel.
 *===================================================*/
void array_proc( unsigned char pv_proc,             // ID of the PV in array pv[]. Can be PV_TSEN_PROC or PV_CHEN_PROC
                 unsigned char pv_ctrl,             // ID of the PV in array pv[]. Can be PV_TSEN_CTRL or PV_CHEN_CTRL
                 unsigned char pv_en,               // ID of the PV in array pv[]. Can be PV_TSEN or PV_CHEN
                 unsigned char en_val)              // Value to enable the target. 1 for PV_TSEN, 0 for PV_CHEN
{
    //unsigned long monch;
    unsigned char proc, ctrl;
    char s[MAX_NELM];
    unsigned char dis_val;

    int i;

    pv_get(pv_proc, &proc);
    if (0 == proc)
    {
        return;
    }

    printf("[%s]: processing %s...\n", __func__, pv[pv_en]);

    pvs_get(pv_en, nelm, s);

    pv_get(pv_ctrl, &ctrl);

    switch(ctrl)
    {
        //--------------------------------------------
	
        case ENABLE:
            pv_get(PV_MONCH, &monch);
            printf("[%s]: enable %ld\n", __func__, monch);

            for(i=0; i<nelm; i++)
            {
                printf("%d ", s[i]);
            }
            printf("\n\n");
            s[monch] = en_val;
            for(i=0; i<nelm; i++)
            {
                printf("%i ", s[i]);
            }
            printf("\n\n");  
            break;

        //--------------------------------------------
        
	case ENABLE_ALL:
            printf("[%s]: enable all.\n", __func__);

            memset(s, en_val, nelm);
	    printf("s after change:\n");
	    for (i=0; i<nelm; i++)
	        printf("%d  ", s[i]);
	    printf("\n\n");
            break;

        //--------------------------------------------
	
        case DISABLE:
            pv_get(PV_MONCH, &monch);
            printf("[%s]: disable %d\n", __func__, monch);
            
            for(i=0; i<nelm; i++)
            {
                printf("%i ", s[i]);
            }
            printf("\n\n");
            dis_val = 1 - en_val;
            s[monch] = dis_val;
            for(i=0; i<nelm; i++)
            {
                printf("%i ", s[i]);
            }
            printf("\n\n");  
            break;
	
        //--------------------------------------------
	
        case DISABLE_ALL:
            printf("[%s]: disable all.\n", __func__);
            dis_val = 1 - en_val;
            memset(s, dis_val, nelm);
            break;
	
        //--------------------------------------------
	
	default:
	    printf("[%s]: invalid option (%d).\n", ctrl);
    }
    
    pvs_put(pv_en, nelm, s);

    proc = 0;
    pv_set(pv_proc, &proc);
}

/*===================================================
 * Thread definition.
 *===================================================*/
void * germ_test_thread(void * arg)
{
    FILE * fp;
    unsigned char i;
    struct timespec t1, t2;
    unsigned long  watchdog;

    printf("#####################################################\n");
    printf("[%s]: Initializing test_thread...\n", __func__);

    t1.tv_sec  = 1;
    t1.tv_nsec = 0;

    printf("[%s]: Initializing PVs...\n", __func__);
    pv_array_init();

    printf("[%s]: epics_thread initialization finished.\n", __func__);
    printf("=====================================================\n");

    pv_get(PV_NELM, &nelm);
    printf("[%s]: %ld elements in total.\n", __func__, nelm);

    watchdog  = 0;
    pv_set(PV_WATCHDOG, &watchdog);
    
    while(1)
    {
        nanosleep(&t1, &t2);
        
	printf("[%s]: checking TSEN...\n", __func__);
        array_proc(PV_TSEN_PROC, PV_TSEN_CTRL, PV_TSEN, 1);

	printf("[%s]: checking CHEN...\n", __func__);
        array_proc(PV_CHEN_PROC, PV_CHEN_CTRL, PV_CHEN, 0);

        pv_set(PV_WATCHDOG, &watchdog);
    }
}


int main(int argc, char **argv)
{
    pthread_t tid[2];

    struct timespec t1, t2; 
    t1.tv_sec = 5;
    t1.tv_nsec = 0;

    int status;

    while(1)
    {   
        status = pthread_create(&tid[0], NULL, &germ_test_thread, NULL);
        if ( 0 == status)
        {
            printf("[%s]: germ_test_thread created.\n", __func__);
            break;
        }

        printf("[%s]: ERROR!!! Can't create thread germ_test_thread: [%s]\n", __func__, strerror(status));
    }   

    while(1)
    {   
        nanosleep(&t1, &t2);
    }   

}
