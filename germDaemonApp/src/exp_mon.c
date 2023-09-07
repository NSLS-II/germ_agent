#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

//#include <tsDefs.h>
#include <cadef.h>
#include <ezca.h>
//#include "ezca.h"

#include "germ.h"
//#include "udp.h"
#include "exp_mon.h"
#include "log.h"

extern unsigned char count;
extern char          filename[MAX_FILENAME_LEN];
extern char          datafile_run[MAX_FILENAME_LEN];
extern char          spectrafile_run[MAX_FILENAME_LEN];
extern unsigned long filesize;
extern unsigned long runno;

extern pv_obj_t  pv[NUM_PVS];
extern unsigned int  nelm;
extern unsigned int  monch;

extern unsigned char tsen_proc, chen_proc;
extern unsigned char tsen_ctrl, chen_ctrl;
extern char          tsen[MAX_NELM], chen[MAX_NELM];

extern char ca_dtype[7][11];
extern char exp_mon_thread_ready;
extern char gige_ip_addr[16];
//========================================================================
// Generate names of datafile and spectrafile with FNAM and RUNNO.
// data_proc_thread will append segno to funfile as the data file name
// used to save detector data.
//------------------------------------------------------------------------
void file_name_gen(void)
{
    char run[32];
    char spectra[32];

    log("filename is %s, runno is %u\n", filename, runno);

    memset(datafile_run, 0, sizeof(datafile_run));
    memset(spectrafile_run, 0, sizeof(spectrafile_run));
    memset(run, 0, sizeof(run));

    strcpy(datafile_run, filename);
    log("filename is %s, datafile_run is %s\n", filename, datafile_run);
    sprintf(run, ".%010ld", runno);
    memcpy(datafile_run+strlen(datafile_run), run, strlen(run));
    log("new data file name (w/o seg) is %s\n", datafile_run);

    strcpy(spectrafile_run, filename);
    sprintf(spectra, "_spectra_.%010ld", runno);
    memcpy(spectrafile_run+strlen(spectrafile_run), spectra, strlen(spectra));
    log("new spectra file name is %s\n", spectrafile_run);
}


//------------------------------------------------------------------------
void print_en(char* en)
{
    int width = 10;
    unsigned int i, j, I, J;
    I = nelm/width;
    J = nelm % width;

    printf("  ");
    for(j=0; j<width; j++)
        printf("%6d", j);
    printf("\n");
    
    for(i=0; i<6*width+2; i++)
        printf("-");
    printf("\n");

    for(i=0; i<I; i++)
    {
        //printf("");
        printf("%2d", i);
        for(j=0; j<width; j++)
            printf("%6d", en[i*width+j]);
        printf("\n");
    }
    printf("%2d", i);
    for(j=0; j<J; j++)
        printf("%6d", en[i*width+j]);
    printf("\n\n");
}
        

//========================================================================
// Enables/disables the objects: test pulse, channel.
//========================================================================
void en_array_proc( unsigned char pv_proc,      // Can be PV_TSEN_PROC or PV_TSEN_PROC.
                    unsigned char pv_ctrl,      // Can be PV_TSEN_CTRL or PV_CHEN_CTRL.
                    unsigned char pv_en,        // Can be PV_TSEN or PV_CHEN.
                    unsigned char en_val)       // Value to enable the target. 1 for PV_TSEN, 0 for PV_CHEN.
{
    unsigned char dis_val;

    log("processing %s...\n", pv[pv_en].my_name);
    log("original:\n");
    print_en((char*)(pv[pv_en].my_var_p));
//    print_en(tsen);

    //for(i=0; i<nelm; i++)
    //{
    //    printf("%d ", *((unsigned char*)(pv[pv_en].my_var_p)+i));
    //}
    //printf("\n\n");

    switch(*(unsigned char*)(pv[pv_ctrl].my_var_p))
    {
        //--------------------------------------------

        case EN_CTRL_ENABLE:
            log("enable %d\n", monch);
            *(((unsigned char*)(pv[pv_en].my_var_p))+monch) = en_val;
            break;

        //--------------------------------------------

        case EN_CTRL_ENABLE_ALL:
            log("enable all.\n");
            memset(pv[pv_en].my_var_p, en_val, nelm);
            break;

        //--------------------------------------------

        case EN_CTRL_DISABLE:
            log("disable %d\n", monch);
            dis_val = 1 - en_val;
            *((unsigned char*)(pv[pv_en].my_var_p)+monch) = dis_val;
            break;

        //--------------------------------------------

        case EN_CTRL_DISABLE_ALL:
            log("disable all.\n");
            dis_val = 1 - en_val;
            memset(pv[pv_en].my_var_p, dis_val, nelm);
            break;

        //--------------------------------------------

        default:
            log("invalid option (%d).\n", *(unsigned char*)(pv[pv_ctrl].my_var_p));
    }

    log("after change:\n");
    print_en((char*)(pv[pv_en].my_var_p));

    pvs_put(pv_en, nelm);

    *(unsigned char*)(pv[pv_proc].my_var_p) = 0;
    pv_put(pv_proc);
}

//========================================================================
// Update on data file name related PV updates.
//    typedef struct event_handler_args {
//        void            *usr;   /* user argument supplied with request */
//        chanId          chid;   /* channel id */
//        long            type;   /* the type of the item returned */
//        long            count;  /* the element count of the item returned */
//        const void      *dbr;   /* a pointer to the item returned */
//        int             status; /* ECA_XXX status of the requested op from the server */
//    } evargs;
//------------------------------------------------------------------------
void pv_update(struct event_handler_args eha)
{
    if (ECA_NORMAL != eha.status)
    {
        printf( "[%s]: CA subscription status is %d instead of %d (ECA_NORMAL)!\n",
                __func__,
                eha.status,
                ECA_NORMAL);
        return;
    }

    log("updating %s.\n", ca_name(eha.chid));
    //printf("[%s]: type is %ld, count is %ld\n", eha.type, eha.count);

	// monch
    if ((unsigned long)eha.chid == (unsigned long)(pv[PV_MONCH].my_chid))
    {
        monch = *(unsigned int*)eha.dbr;
        //printf("[%s]: new monch is %d\n", monch);
        pv_put(PV_MONCH_RBV);
    }
    // tsen_proc
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_TSEN_PROC].my_chid))
    {
        if (1 == *(unsigned char*)eha.dbr)
            en_array_proc(PV_TSEN_PROC, PV_TSEN_CTRL, PV_TSEN, 1);
    }
    // chen_proc
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_CHEN_PROC].my_chid))
    {
        en_array_proc(PV_CHEN_PROC, PV_CHEN_CTRL, PV_CHEN, 0);
    }
    // tsen
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_TSEN].my_chid))
    {
        memcpy(tsen, eha.dbr, nelm);
//        print_en((char*)eha.dbr);
    }
    // chen
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_CHEN].my_chid))
    {
        memcpy(chen, eha.dbr, nelm);
//        print_en((char*)eha.dbr);
    }
    // tsen_ctrl
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_TSEN_CTRL].my_chid))
    {
        tsen_ctrl = *(char*)eha.dbr;
    }
    // chen_ctrl
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_CHEN_CTRL].my_chid))
    {
        chen_ctrl = *(char*)eha.dbr;
    }
    // filesize
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_FILESIZE].my_chid))
    {
        filesize = *(unsigned long*)eha.dbr;
        //printf("[%s]: new file size is %ld\n", filesize);
        pv_put(PV_FILESIZE_RBV);
    }
    //filename
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_FILENAME].my_chid))
    {
        memset(filename, 0, MAX_FILENAME_LEN);
        strcpy(filename, eha.dbr);
        log("new filename is %s\n", filename);
        pv_put(PV_FILENAME_RBV);
        file_name_gen();
    }
    // runno
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_RUNNO].my_chid))
    {
        runno = *(unsigned long*)eha.dbr;
        //printf("[%s]: new runno is %ld\n", runno);
        pv_put(PV_RUNNO_RBV);
        file_name_gen();
    }
    // ipaddr
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_IPADDR].my_chid))
    {
        strcpy(gige_ip_addr, eha.dbr);
        //printf("[%s]: new IP address is %s\n", gige_ip_addr);
        pv_put(PV_IPADDR_RBV);
    }
    // nelm
    else if ((unsigned long)eha.chid == (unsigned long)(pv[PV_NELM].my_chid))
    {
        nelm = *(unsigned int*)eha.dbr;
        //printf("[%s]: new nelm is %d\n", nelm);
        pv_put(PV_NELM_RBV);
    }
/*
    switch ((unsigned long)eha.chid)
    {
        // monch
        case (unsigned long)(pv[PV_MONCH].my_chid):
            monch = *(unsigned int*)eha.dbr;
            //printf("[%s]: new monch is %d\n", monch);
            pv_put(PV_MONCH_RBV);
            break;
    
        // tsen_proc
        case (unsigned long)(pv[PV_TSEN_PROC].my_chid):
            if (1 == *(unsigned char*)eha.dbr)
                en_array_proc(PV_TSEN_PROC, PV_TSEN_CTRL, PV_TSEN, 1);
				break;
    
        // chen_proc
        case (unsigned long)(pv[PV_CHEN_PROC].my_chid):
            en_array_proc(PV_CHEN_PROC, PV_CHEN_CTRL, PV_CHEN, 0);
			break;
    
    
        // tsen
        case (unsigned long)(pv[PV_TSEN].my_chid):
            memcpy(tsen, eha.dbr, nelm);
            //print_en((char*)eha.dbr);
            break;
    
        // chen
        case (unsigned long)(pv[PV_CHEN].my_chid):
            memcpy(chen, eha.dbr, nelm);
            //print_en((char*)eha.dbr);
            break;
    
        // tsen_ctrl
        case (unsigned long)(pv[PV_TSEN_CTRL].my_chid):
            tsen_ctrl = *(char*)eha.dbr;
	    	break;
        
        // chen_ctrl
        case (unsigned long)(pv[PV_CHEN_CTRL].my_chid):
            chen_ctrl = *(char*)eha.dbr;
	    	break;
    
        // filesize
        case (unsigned long)(pv[PV_FILESIZE].my_chid):
            filesize = *(unsigned long*)eha.dbr;
            //printf("[%s]: new file size is %ld\n", filesize);
            pv_put(PV_FILESIZE_RBV);
	    	break;
    
        //filename
        case (unsigned long)(pv[PV_FILENAME].my_chid):
            log("eha.dbr is %s\n", eha.dbr);
            strcpy(filename, eha.dbr);
            log("new filename is %s\n", filename);
            pv_put(PV_FILENAME_RBV);
            filename_gen();
	    	break;

        // runno
        case (unsigned long)(pv[PV_RUNNO].my_chid):
            runno = *(unsigned long*)eha.dbr;
            //printf("[%s]: new runno is %ld\n", runno);
            pv_put(PV_RUNNO_RBV);
            filename_gen();
	    	break;
    
	    // ipaddr
        case (unsigned long)(pv[PV_IPADDR].my_chid):
            strcpy(gige_ip_addr, eha.dbr);
            //printf("[%s]: new IP address is %s\n", gige_ip_addr);
            pv_put(PV_IPADDR_RBV);
	    	break;
    
        // nelm
        case (unsigned long)(pv[PV_NELM].my_chid):
            nelm = *(unsigned int*)eha.dbr;
            //printf("[%s]: new nelm is %d\n", nelm);
            pv_put(PV_NELM_RBV);
	    	break;
    }
*/
}
//------------------------------------------------------------------------


//========================================================================

void pv_subscribe(unsigned char i)
{
    SEVCHK(ca_create_subscription(pv[i].my_dtype,
                                  0, //pv[i].my_nelm,
                                  pv[i].my_chid,
                                  DBE_VALUE,
                                  pv_update,
                                  &pv[i],
                                  &pv[i].my_evid),
            "ca_create_subscription");

}

//========================================================================

void* exp_mon_thread(void * arg)
{
    printf("#####################################################\n");
    log("Initializing exp_mon_thread...\n");

    create_channel(__func__, FIRST_EXP_MON_PV, LAST_EXP_MON_PV);

    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create @exp_mon_thread");

    // Get it for udp_conn_thread to initialize UDP connection
    pv_get(PV_IPADDR);
    //printf("[%s]: IP address is %s\n", gige_ip_addr);
    pv_put(PV_IPADDR_RBV);

//    // Get it for xxEN to be correctly subscribed
//    pv_get(PV_NELM);
//    log("NELM is %d\n", nelm);
//    pv_put(PV_NELM_RBV);
//    pv[PV_TSEN].my_nelm = nelm;
//    pv[PV_CHEN].my_nelm = nelm;
    

    for (int i=FIRST_EXP_MON_RD_PV; i<=LAST_EXP_MON_RD_PV; i++)
    {
        pv_subscribe(i);
    }
    //printf("[%s]: TSEN has %ld elements\n", ca_element_count(pv[PV_TSEN].my_chid)); 

    exp_mon_thread_ready = 1;

    log("exp_mon_thread initialization finished.\n");
    log("monitoring PV changes...\n");

    printf("=====================================================\n");

    SEVCHK(ca_pend_event(0.0),"ca_pend_event @exp_mon_thread");

    return NULL;
}
