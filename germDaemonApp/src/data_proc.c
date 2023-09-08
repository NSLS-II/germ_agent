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

extern char         filename[MAX_FILENAME_LEN];

extern unsigned int runno;
extern uint16_t     filesize;  // in Megabyte

 
extern int evnt;

extern uint16_t mca[NUM_MCA_ROW * NUM_MCA_COL];
extern uint16_t tdc[NUM_TDC_ROW * NUM_TDC_COL];
extern unsigned int nelm;

extern uint8_t  data_write_thread_ready;

//=======================================================     
void* data_proc_thread(void* arg)
{
    packet_buff_t *buff_p;
    uint32_t      *packet;
    unsigned char read_buff = 0;

    uint16_t packet_length;

    uint32_t event;
    uint32_t adr, de, dt;

    unsigned long int i, j;
    FILE * fp;

    struct timespec t1, t2;

    uint8_t end_of_frame;
    uint32_t frame_num;

    uint8_t start, end;

    char   run[32];
    char   spectrafile[MAX_FILENAME_LEN];

    log("########## Initializing data_proc_thread ##########\n");

    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create @data_proc_thread");

    create_channel(__func__, FIRST_DATA_PROC_PV, LAST_DATA_PROC_PV);

    do
    {
        nanosleep(&t1, &t2);
    } while(0 == data_write_thread_ready);


    buff_p = &(packet_buff[read_buff]);
    mutex_lock(&(buff_p->lock), read_buff);

    while(1)
    {
        memset(mca, 0, sizeof(mca));
        memset(tdc, 0, sizeof(tdc));

        end_of_frame   = 0;

        while (0 == end_of_frame)
        {
            packet = (uint32_t*)(buff_p->packet);
            packet_length = buff_p->length >> 2;

            //for(int i=0; i<packet_length; i++)
            //{
            //    printf("0x%08x\n", ntohl(packet[i]));
            //}

            start = 2;
            end = 0;
      
            frame_num = buff_p->runno;

            if (ntohl(packet[2]) == SOF_MARKER)
            {
                //frame_num = ntohl(packet[3]);
                start = 4;
            }
            else
            {
                if ( ntohl(packet[packet_length-1]) == EOF_MARKER )
                {
                    end = 2;
                }
                end_of_frame = 1;
            }

            //--------------------------------------------------------------------------
            // Calculate spectra
            // A event data consists of a 32-bit ET/PA event followed
            // by a 32-bit timestamp.
            // Currently only ET events are parsed.
            for(i=start; i<packet_length-end; i+=2)
            {
                event = ntohl(packet[i]);
    
                if(!(event & 0x80000000))    // E/T event
                {
                    //et_event++;
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
        }

        mutex_unlock(&(buff_p->lock), read_buff);

        read_buff++;
        read_buff %= NUM_PACKET_BUFF;
        
        buff_p = &(packet_buff[read_buff]);
        mutex_lock(&(buff_p->lock), read_buff);

        pvs_put(PV_MCA, nelm);
        pvs_put(PV_TDC, nelm);
  
        //--------------------------------------------------------------------------
        // write spectra file

        strcpy(spectrafile, filename);
        strcpy(spectrafile+strlen(spectrafile), ".spectra");
        sprintf(run, ".%010u", frame_num);
        strcpy(spectrafile+strlen(spectrafile), run);
        strcpy(spectrafile+strlen(spectrafile), ".dat");

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
        for(i=0; i<nelm; i++)
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

    }
}
