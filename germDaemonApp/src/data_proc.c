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


extern frame_buff_t frame_buff[NUM_FRAME_BUFF];
    
extern pv_obj_t pv[NUM_PVS];

extern char         datafile_run[MAX_FILENAME_LEN];
extern char         spectrafile_run[MAX_FILENAME_LEN];
extern char         datafile[MAX_FILENAME_LEN];
extern char         spectrafile[MAX_FILENAME_LEN];

extern unsigned int runno;
extern uint16_t     filesize;  // in Megabyte

extern unsigned char read_buff, write_buff;
 
extern int evnt;

extern uint16_t mca[NUM_MCA_ROW * NUM_MCA_COL];
extern uint16_t tdc[NUM_TDC_ROW * NUM_TDC_COL];
extern unsigned int nelm;

extern char udp_conn_thread_ready;

//=======================================================     
void write_data_file( unsigned int segno,
                      uint16_t*    src,
                      unsigned int num_words )
{
    char           seg[20];
//    char           fn[64];
    struct timeval tv_begin, tv_end;
    FILE*          fp;

    sprintf(seg, ".%05d.bin", segno);
    log("segno is %d, seg is %s\n", segno, seg);
    strcpy(datafile, datafile_run);
    memcpy(datafile+strlen(datafile), seg, strlen(seg));

    pv_put(PV_DATA_FILENAME);

    fp = fopen(datafile, "a");
    if(NULL == fp)
    {
        log("ERROR!!! Failed to open file %s\n", datafile);
        return;
    }

    gettimeofday(&tv_begin, NULL);
    fwrite(src, num_words, sizeof(uint16_t), fp);
    gettimeofday(&tv_end, NULL);

    log("wrote %6.2f MB to data file %s in %f sec\n",
            num_words*2/1e6,
            datafile,
            (float)(time_elapsed(tv_begin, tv_end)/1e6) );

    fclose(fp);
}

//=======================================================     
// test code for gige_reg_t 
void* data_proc_thread(void* arg)
{
    frame_buff_t * buff_p;
    uint16_t     * evtdata_p;

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
//        chkerr   = 0;
//        checkval = 0;
        memset(mca, 0, sizeof(mca));
        memset(tdc, 0, sizeof(tdc));
//        memset(fn,  0, sizeof(fn));
        et_event = 0;

        buff_p = &frame_buff[read_buff];
        pthread_mutex_lock(&(buff_p->lock));
        log("buff[%d] locked for reading\n", read_buff);
      
        if (buff_p->frame_num != next_frame)
        {
            log( "[%s:] %ld frame(s) dropped.\n",
                    __func__,
                    buff_p->frame_num-next_frame);
        }
      
        next_frame = buff_p->frame_num + 1;
        num_words  = buff_p->num_words;
        evtdata_p  = buff_p->evtdata;

        log("%lu words / %lu events in frame.\n", num_words, (num_words-8)/4);

        /*
        //--------------------------------------------------------------------------
        //check data
        log("checking data...\n");
        for(i=0; i<num_words; i=i+2)
        {
            //last 4 16bit words are fifofullcnt and EOF
            //printf("%d:\t%4x\n",i,ntohs(evtdata_p[i]));
            if (i == 0) {
                log( "[%s:] SOF = 0x%x\n",
            __func__,
                  (ntohs(evtdata_p[i]) << 16 | ntohs(evtdata_p[i+1])) );
                checkval = 0;
            }
            else
            {
                if (i == 2)
                {
                          log("FrameNum = %d\n",
                            (ntohs(evtdata_p[i]) << 16 | ntohs(evtdata_p[i+1])) );
                          checkval = 0;
                }
                else
                {
                    if (checkval != (ntohs(evtdata_p[i]) << 16 | ntohs(evtdata_p[i+1])))
                    {
                        log("error at i = %d\tval = 0x%x\t should be 0x%x\n",
                          __func__,
                          i,
                          (ntohs(evtdata_p[i]) << 16 | ntohs(evtdata_p[i+1])),
                          checkval );
                        chkerr = 1;
                        break;
                    }
                    else
                       checkval++;
                }
            }
        }

        log("events lost to Overflow: %d\n",
                __func__,
                (ntohs(evtdata_p[num_words-4]) << 16 | ntohs(evtdata_p[num_words-3])) );

        log("EOF: 0x%x\n",
                __func__,
          (ntohs(evtdata_p[num_words-2]) << 16 | ntohs(evtdata_p[num_words-1])));

        if (chkerr == 0)
            log("checking complete. No errors.)\n");
        log("events %i\n", evnt);
  */


        log("saving files...\n");
        //--------------------------------------------------------------------------
        // Write event data file.
        // File size limited by filesize
        //memset(fn, 0, sizeof(fn));
        //strcpy(fn, filename);

        //sprintf(run, ".%5d", runno);
        //strncat(fn, run, strlen(run));
        //memcpy(fn+strlen(fn), run, strlen(run));

        log("%ld words in buffer.\n", num_words);
        segno = 0;
        write_data_file(segno, evtdata_p, num_words);
        log("datafile (new run) written\n"); 
/*
        if (prev_runno != runno) // starting a new run
        {
            segno = 0;
      
            write_data_file(segno, evtdata_p, num_words);
            log("datafile (new run) written\n"); 
            prev_runno = runno;
            words_to_full = (filesize << 5) - num_words;  // filesize in MB
            words_written = num_words;
        }
        else
        {
            if ( (runno == 0) && (segno == 0) && (words_written == 0))  // first ever
            {
                words_to_full = filesize << 5;
            }
      
            if (words_to_full > num_words)
            {
                write_data_file(segno, evtdata_p, num_words);
                log("datafile of run %d seg %d written\n", runno, segno);
                log("1\n");
                words_to_full -= num_words;
            }
            else
            {
                // fill the current segment/file
                write_data_file(segno, evtdata_p, words_to_full);
                log("datafile of run %d seg %d written\n", runno, segno);
                log("2\n");
            
                segno++;
            
                if(words_to_full == num_words)  // filled the file exactly
                {
                    words_written = 0;
                    words_to_full = filesize << 5;
                }
                else
                {
                    // write the rest of the data to the next segment/file
                    write_data_file(segno, evtdata_p+num_words-words_to_full, num_words-words_to_full);
                    log("datafile of run %d seg %d written\n", runno, segno);
                log("3\n");
                    words_written = num_words - words_to_full;
                    words_to_full = (filesize << 5) - words_written;
                }
            }
        }
*/

        //--------------------------------------------------------------------------
        // Calculate spectra
        // A event data consists of a 32-bit ET/PA event followed
        // by a 32-bit timestamp.
        // Currently only ET events are parsed.
        for(i=0; i<buff_p->num_words; i+=4)
        {
            event = (ntohs(evtdata_p[i]) << 16 | ntohs(evtdata_p[i+1]));
            if(!(event & 0x80000000))    // E/T event
            {
                et_event++;
                adr = (event >> 22) & 0x1ff;
                dt  = (event >> 12) & 0x3ff;
                de  = (event >> 0)  & 0xfff;
                //if(adr >= 384)
                //    adr = 383;
                if(adr >= MAX_NELM)
                    adr = MAX_NELM-1;
                if(dt >= 1024)
                    dt=1023;
                if(de >= 4096)
                    de=4095;

                // mca[adr][de]+=1;
                // tdc[adr][dt]+=1;
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

        strcpy(spectrafile, spectrafile_run);
        pv_put(PV_SPEC_FILENAME);

        fp = fopen(spectrafile, "a");
        if(fp != NULL)
            log("spectra file %s opened OK\n", spectrafile);

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

        log("spectra file %s written OK\n", spectrafile);
        fclose(fp);

        pthread_mutex_unlock(&(buff_p->lock));
        log("buff[%d] released\n", read_buff);
		
        read_buff++;
        read_buff %= NUM_FRAME_BUFF;
    }
}
