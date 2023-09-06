/**
 * File: udp_data.c
 *
 * Functionality: Spectra calculatation and data saving.
 *
 *-----------------------------------------------------------------------------
 * 
 * Revisions:
 *
 *   v1.0
 *     - By    : Ji Li
 *     - Date  : Dec 2022
 *     - Brief : Separated from germ_xx.c;
 *               Added spectra calculation;
 *               Implemented as a thread.
 *
 *               Data file names are in the format of
 *                   filename.runno.segno.bin
 *               where
 *                   . filename is from an EPICS PV;
 *                   . runno is from an EPICS PV;
 *                   . segno is created according to filesize
 *                     which is from an EPICS PV.
 *
 *   v0.1
 *     - By    : J. Kuczewski 
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

#include <cadef.h>
#include <ezca.h>
//#include "ezca.h"

#include "germ.h"
//#include "udp.h"
#include "data_proc.h"
#include "log.h"


extern packet_buff_t packet_buff[NUM_PACKET_BUFF];
    
extern pv_obj_t pv[NUM_PVS];

extern char         datafile_run[MAX_FILENAME_LEN];
extern char         spectrafile_run[MAX_FILENAME_LEN];
extern char         datafile[MAX_FILENAME_LEN];
extern char         spectrafile[MAX_FILENAME_LEN];

extern unsigned int runno;
extern uint16_t     filesize;  // in Megabyte

 
extern int evnt;

extern uint16_t mca[NUM_MCA_ROW * NUM_MCA_COL];
extern uint16_t tdc[NUM_TDC_ROW * NUM_TDC_COL];
extern unsigned int nelm;

extern char udp_conn_thread_ready;


//=======================================================     
void* data_proc_thread(void* arg)
{
    packet_buff_t * buff_p;
    uint16_t     * evtdata_p;
    unsigned char read_buff;

    /* arrays for energy and time spectra */
    //unsigned int mca[384][4096];
    //unsigned int tdc[384][1024];
    //int evnt;
    //
    unsigned long int num_words;

//    unsigned char chkerr;
//    int checkval;

//    char         fn[1024];
//    char         run[64];
    unsigned int prev_runno    = 0;
    unsigned int segno         = 0;  // file segment number, always start from 0 for a new run
    static uint64_t     words_to_full = 0;  // how many words to write to the file to reach filesize
    static uint64_t     words_written = 0;  // how many words already written to the file
    
    static unsigned long next_frame = 0;

    uint32_t event, et_event;
    uint32_t adr, de, dt;

    unsigned long int i, j;
    FILE * fp;

    struct timespec t1, t2;

    log("#####################################################\n");
    log("Initializing data_proc_thread...\n");

    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create @data_proc_thread");

    create_channel(__func__, FIRST_DATA_PROC_PV, LAST_DATA_PROC_PV);

    t1.tv_sec  = 1;
    t1.tv_nsec = 0;

    do
    {
        nanosleep(&t1, &t2);
    } while(0 == udp_conn_thread_ready);

    while(1)
    {
        memset(mca, 0, sizeof(mca));
        memset(tdc, 0, sizeof(tdc));
//        memset(fn,  0, sizeof(fn));
        et_event = 0;

        start_of_frame = 0;
		end_of_frame   = 0;
		et_event = 0;

		while (0 == end_of_frame)
		{
            buff_p = &packet_buff[read_buff];
            pthread_mutex_lock(&(buff_p->lock));
            log("buff[%d] locked for reading\n", read_buff);

			start = 1;
			end = 0;
      
            if (ntohs(packet[4]) == SOF_MARKER_UPPER &&
                ntohs(packet[5]) == SOF_MARKER_LOWER)
	        {
			    frame_num = (ntohs(packet[6]) << 16) | ntohs(packet[7]);
			    start = 3;
		    }
			else
			{
                if ( ntohs(packet[(packet_length/sizeof(uint16_t))-2]) == EOF_MARKER_UPPER &&
                     ntohs(packet[(packet_length/sizeof(uint16_t))-1]) == EOF_MARKER_LOWER )
			    {
				    end = 1;
			}

        //--------------------------------------------------------------------------
        // Calculate spectra
        // A event data consists of a 32-bit ET/PA event followed
        // by a 32-bit timestamp.
        // Currently only ET events are parsed.
        for(i=start; i<buff_p->length; i+=8)
        {
            event = (((ntohs(buff_p->packet[i]) << 8 | ntohs(buff_p->packet[i+1]))) << 8 |
                    (ntohs(buff_p->packet[i+2]))) << 8 | ntohs(buff_p->packet[i+1]);
            if(!(event & 0x80000000))    // E/T event
            {
                et_event++;
                adr = (event >> 22) & 0x1ff;
                dt  = (event >> 12) & 0x3ff;
                de  = (event >> 0)  & 0xfff;

                if(adr >= MAX_NELM)
                    adr = MAX_NELM-1;
                if(dt >= 1024)
                    dt=1023;
                if(de >= 4096)
                    de=4095;

                mca[adr*NUM_MCA_COL + de] += 1;
                mca[adr*NUM_TDC_COL + dt] += 1;
            }
        }

        pvs_put(PV_MCA, nelm);
        pvs_put(PV_TDC, nelm);
  
        //--------------------------------------------------------------------------
        // write spectra file
        //memset(fn, 0, sizeof(fn));
        //strcpy(fn, filename);
        //sprintf(run, "_spectra_%i.dat", runno);
        //strncat(fn, run, strlen(run));
        //memcpy(fn+strlen(fn), run, strlen(run));

        strcpy(spectrafile, filename);
		strcpy(spectrafile+strlen(spectrafile), ".spectra");
		sprintf(run, ".010ld", frame_num
        pv_put(PV_SPEC_FILENAME);

        fp = fopen(spectrafile, "a");
        if(fp == NULL)
		{
		    err("failed to open spectra file %s\n", spectrafile);
            continue;
		}

        fprintf(fp, "\t#name: spect\n");
        fprintf(fp, "\t#type: matrix\n");
        fprintf(fp, "\t#rows: %d\n", MAX_NELM);
        fprintf(fp, "\t#columns: 4096\n");

        //for(i=0; i<384; i++)
        for(i=0; i<MAX_NELM; i++)
        {
            for(j=0;j<4096;j++)
            {
                fprintf(fp, "%u  ", mca[i*NUM_MCA_COL + j]);
            }
            fprintf(fp, "\n");
        }
        fprintf(fp,"\n");
        
        fprintf(fp, "\t#name: tot\n");
        fprintf(fp, "\t#type: matrix\n");
        fprintf(fp, "\t#rows: %d\n", MAX_NELM);
        fprintf(fp, "\t#columns: 1024\n");

        //for(i=0; i<384; i++)
        for(i=0; i<MAX_NELM; i++)
        {
            for(j=0; j<1024; j++)
            {
                fprintf(fp, "%u  ", tdc[i*NUM_TDC_COL + j]);
            }
            fprintf(fp, "\n");
        }
        fprintf(fp,"\n");

        fclose(fp);
        log("spectra file %s written OK\n", spectrafile);

        pthread_mutex_unlock(&(buff_p->lock));
        log("buff[%d] released\n", read_buff);
		
        read_buff++;
        read_buff %= NUM_PACKET_BUFF;
    }
}
