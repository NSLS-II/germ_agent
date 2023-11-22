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
#include <stdatomic.h>

#include <cadef.h>

#ifdef USE_EZCA
#include <ezca.h>
#endif

#include "germ_atom.h"
//#include "udp.h"
#include "data_write_atom.h"
#include "log.h"


extern packet_buff_t packet_buff[NUM_PACKET_BUFF];
    
extern pv_obj_t pv[NUM_PVS];

extern char         filename[MAX_FILENAME_LEN];

extern unsigned int runno;
extern uint16_t     filesize;  // in Megabyte

extern atomic_char udp_conn_thread_ready;
extern atomic_char data_write_thread_ready;

void create_datafile_name(char * datafile, uint32_t run_num, uint32_t file_segment)
{
    char run[32];
    char file_seg[32];

    memset(datafile, 0, MAX_FILENAME_LEN);

    // filename from PV
    strcpy(datafile, filename);
    
    // run number
    sprintf(run, ".%010u", run_num);
    memcpy(datafile+strlen(datafile), run, strlen(run));

    // file segment number
    sprintf(file_seg, ".%010u", file_segment);
    memcpy(datafile+strlen(datafile), file_seg, strlen(file_seg));
    
    strcpy(datafile+strlen(datafile), ".bin");

    return;
}


//=======================================================     
void* data_write_thread(void* arg)
{
    packet_buff_t * buff_p;
    unsigned char read_buff = 0;

    FILE * fp = NULL;
    char   datafile[MAX_FILENAME_LEN];

    uint32_t  file_segment = 0;
    uint64_t file_written = 0;
    //uint16_t file_written_mb = 0;

    uint8_t start_of_frame = 0;
    uint8_t end_of_frame = 0;

    uint32_t run_num;
    uint8_t  first_run = 1;  // a flag indicating receipient of first frame with any frame_num

    uint32_t num_packets;
    uint16_t num_events;
    uint16_t num_lost_events;
    uint16_t packet_length;
    uint32_t packet_counter;
    uint16_t payload_length;
    uint32_t first_packetnum = 0;
    uint32_t frame_num = 0;
    uint32_t last_frame_num = 0;
    uint32_t frame_size;

    uint32_t *packet;

    struct timespec t1, t2;

    struct timeval tv_begin, tv_end;

    t1.tv_sec  = 0;
    t1.tv_nsec = 100;

    log("########## Initializing data_write_thread ##########\n");

    SEVCHK( ca_context_create(ca_disable_preemptive_callback),
            "ca_context_create @data_write_thread");
    create_channel(__func__, FIRST_DATA_WRITE_PV, LAST_DATA_WRITE_PV);

    do
    {
        nanosleep(&t1, &t2);
    } while(0 == atomic_load(&udp_conn_thread_ready));

    atomic_store(&data_write_thread_ready, 1);

    info("ready to read data...\n");

    while(1)
    {
        frame_size      = 0;
        num_packets     = 0;
        num_events      = 0;
        num_lost_events = 0;
        start_of_frame  = 0;
        end_of_frame    = 0;

        // look for a whole frame
        while ( 0 == end_of_frame )
        {
            buff_p = &(packet_buff[read_buff]);
            log("lock buff[%d] for read\n", read_buff);
            lock_buff_read(read_buff, DATA_WRITTEN, __func__);

            log("%d bytes in buff[%d]\n", buff_p->length, read_buff);
            packet_length = (buff_p->length) >> 2;  // process data as 4-byte words
            packet        = (uint32_t*)(buff_p->packet);
            frame_size   += packet_length << 2;
            num_packets++;

            packet_counter = ntohl(packet[0]);

	    //for(int i=0; i<packet_length; i++)
	    //{
  		//    printf("0x%08x\n", ntohl(packet[i]));
	    //}

            //-------------------------------------------------
            // check if it's the 1st or last packet of a frame
            if (ntohl(packet[2]) == SOF_MARKER) // first packet
            {
                gettimeofday(&tv_begin, NULL);
                first_packetnum = packet_counter;
                frame_num = ntohl(packet[3]);

                file_segment = 0;
                file_written = 0;

                run_num = frame_num;

                start_of_frame = 1;
                payload_length = packet_length - 4;
                info("got Start of Frame\n");
            }
            else
            {
                if(0 == start_of_frame)
                {
                    err("missed Start of Frame\n");
                }

                if ( ntohl(packet[packet_length-1]) == EOF_MARKER ) // last packet
                {
                    num_lost_events = ntohl(packet[packet_length-2]);
                    end_of_frame = 1;
                    payload_length = packet_length - 4;
                    info("got End of Frame\n");
                }
                else
                {
                    payload_length = packet_length - 2;
                }
                run_num = buff_p->runno;
            }

            //-------------------------------------------------
            // write data to file
            //
            // New file segment for the current run if the
            // file size limit has been reached.
            
            // open file if it was unsuccessful for the previous packet
            if(!fp) 
            {
                create_datafile_name(datafile, run_num, file_segment);
                fp = fopen(datafile, "a");
            }
            
            if(fp)
            {
                //printf("Writing %u from buff[%d] to file...\n", packet_length<<2, read_buff);
                fwrite(packet, packet_length << 2, 1, fp);
                file_written += packet_length << 2;

                if (((file_written+1008)>>20) > filesize)
                {
                    fclose(fp);
                    info("last data file is %s\n", datafile);
                    file_segment++;
                    file_written = 0;
                    create_datafile_name(datafile, run_num, file_segment);
                    fp = fopen(datafile, "a");
                }
            }
            else
            {
                err("failed to open datafile for runno %u\n", run_num);
            }

            num_events += payload_length >> 1;

            buff_p->status |= DATA_WRITTEN;
            unlock_buff(read_buff, __func__);
            log("buff[%d] unlocked\n", read_buff);

            //-------------------------------------------------
            // move to next buffer
            read_buff++;
            read_buff &= PACKET_BUFF_MASK;

            if (1 == end_of_frame)
            {
                break;
            }
        } // loop until a frame has been received

        if(fp)
        {
            fclose(fp);
            pv_put(PV_DATA_FILENAME);
            fp = NULL;
            gettimeofday(&tv_end, NULL);
            log("datafile (new run) written\n"); 

            file_segment = 0;
            file_written = 0;
            pv_put(PV_DATA_FILENAME);
        }
        if(first_run == 0)
        {
            if ((frame_num-last_frame_num) != 1)
            {
                err("%u frames lost\n", frame_num-last_frame_num-1);
            }
        }
        else
        {
            first_run = 0;
        }
        last_frame_num = frame_num;

        info( "frame %lu (%lu packets / %lu events / %lu bytes) processed in %f sec\n",
              frame_num, num_packets, num_events, frame_size,
              time_elapsed(tv_begin, tv_end)/1e6);

        packet_counter -= (first_packetnum-1);

        if (packet_counter != num_packets )
        {
            err("missed %u packets\n", packet_counter - num_packets);
        }
        else
        {
            log("    all packets received\n");
        }

        if (0!= num_lost_events)
        {
            err("%u events lost due to UDP Tx FIFO overflow.\n", num_lost_events);
        }
        else
        {
            info("    no overflow detected in UDP Tx FIFO\n");
        }
   
    }
    
}
