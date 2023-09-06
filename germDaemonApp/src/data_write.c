/**
 * File: data_write.c
 *
 * Functionality: Save raw data.
 *
 *-----------------------------------------------------------------------------
 * 
 * Revisions:
 *
 *   v1.1
 *     - By    : Ji Li
 *     - Date  : Sep 2023
 *     - Brief : Read packets from buffer and save frames to files.
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

/*
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
*/

//=======================================================     
// test code for gige_reg_t 
void* data_write_thread(void* arg)
{
    packet_buff_t * buff_p;
    unsigned char read_buff = 0;
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
    //static uint64_t words_to_full = 0;  // how many words to write to the file to reach filesize
    //static uint64_t words_written = 0;  // how many words already written to the file
    
    static unsigned long next_frame = 0;

    uint32_t event, et_event;
    uint32_t adr, de, dt;

    unsigned long int i, j;
    FILE * fp;

    uint8_t start_of_frame = 0;
    uint8_t end_of_frame = 0;

    uint32_t packet_length, packet_counter;
    uint64_t frame_size;
    char datafile_name[MAX_FILENAME_LEN];

    struct timespec t1, t2;

    log("########## Initializing data_proc_thread ##########\n");

    SEVCHK( ca_context_create(ca_disable_preemptive_callback),
            "ca_context_create @data_proc_thread" );

    create_channel(__func__, FIRST_DATA_PROC_PV, LAST_DATA_PROC_PV);

    t1.tv_sec  = 0;
    t1.tv_nsec = 600;

    do
    {
        nanosleep(&t1, &t2);
    } while(0 == udp_conn_thread_ready);

    while(1)
    {
        //et_event       = 0;
        frame_size     = 0;
        num_packets    = 0;
        num_events     = 0;
        start_of_frame = 0;
        end_of_frame   = 0;

        // look for a whole frame
        while ( 0 == end_of_frame )
        {
            buff_p = &packet_buff[read_buff];
            pthread_mutex_lock(&(buff_p->lock));
            log("packet_buff[%d] locked for reading\n", read_buff);

            packet_length = buff_p->length;
            packet        = buff_p->packet;
            frame_size   += packet_length;
            num_packets++;

            packet_counter = (ntohs(packet[0]) << 16 | ntohs(packet[1]));

            //misc_len = 4;

            if (ntohs(packet[4]) == SOF_MARKER_UPPER &&
                ntohs(packet[5]) == SOF_MARKER_LOWER)
            {
                gettimeofday(&tv_begin, NULL);
                //total_data = 0;
                cnt = packet_counter;
                first_packetnum = packet_counter;
                frame_num = (ntohs(packet[6]) << 16) | ntohs(packet[7]);
                if (next_frame != buff_p->frame_num)
                {
                    err("%u frames lost.\n", frame_num - next_frame);
                }
                next_frame = frame_num + 1;

                strcpy(datafile, filename);
                sprintf(run, ".010ld", frame_num);
                memcpy(datafile+strlen(datafile), run, strlen(run));

                fp = fopen(datafile, "a");
                if(NULL == fp)
                {
                    err("Failed to open data file %s\n", datafile);
                    continue;
                }

                start_of_frame = 1;
                payload_length = (packet_length - 24) / 8;
                log("got Start of Frame\n");
            }
            else
            {
                if(0 == start_of_frame)
                {
                    err("missed Start of Frame\n");
                }

                // if last word is 0xDECAFBAD, this is end of frame
                if ( ntohs(packet[(packet_length/sizeof(uint16_t))-2]) == EOF_MARKER_UPPER &&
                     ntohs(packet[(packet_length/sizeof(uint16_t))-1]) == EOF_MARKER_LOWER )
                {
                    num_lost_event = ntohs(packet[n/sizeof(uint16_t)-4]) << 16 |
                                     ntohs(mesg[n/sizeof(uint16_t)-3]);
                    end_of_frame = 1;
                    payload_length = packet_length - 16;
                    log("got End of Frame\n");
                }
                else
                {
                    payload_length = packet_length - 8;
                }
            }
            buff_p->flag++;

            pthread_mutex_unlock(&(buff_p->lock));
            log("buff[%d] released\n", read_buff);
            read_buff++;
            read_buff %= NUM_PACKET_BUFF;
            
            if(NULL != fp)
            {
                fwrite(packet, packet_length, 1, fp);
            }
        }

        if(NULL != fp)
        {
            fclose(fp);
            gettimeofday(&tv_end, NULL);
            log("datafile (new run) written\n"); 
        }
        log( "frame %lu (%lu packets / %lu events / %lu bytes) processed in %f sec\n",
             frame_num, num_packets, num_events, frame_size,
             time_elapsed(tv_begin, tv_end)/1e6);

        if (packet_counter != cnt-1)
        {
            err("missed %u packets\n", packet_counter - cnt);
        }
        else
        {
            log("    all packets received\n");
        }

        if (0!= buff_p->num_lost_event)
        {
            err("%u events lost due to UDP Tx FIFO overflow.\n", buff_p->num_lost_event);
        }
        else
        {
            log("    no overflow detected in UDP Tx FIFO\n");
        }
   
    }
    
}
