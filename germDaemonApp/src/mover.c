#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <cadef.h>

#ifdef USE_EZCA
#include <ezca.h>
#endif

#include "germ.h"
#include "mover.h"
#include "log.h"

#define _COPY_DELETE_  // Define this macro when the source and destination directories
                       // are in different storage, e.g., RAM disk and network drive

extern atomic_char count;
extern char        tmp_datafile_dir[MAX_FILENAME_LEN];
extern char        datafile_dir[MAX_FILENAME_LEN];
extern pthread_mutex_t tmp_datafile_dir_lock;
extern pthread_mutex_t datafile_dir_lock;
extern pthread_mutex_t filename_lock;

void* mover_thread(void* arg)
{
    uint8_t num_files;
    struct dirent **namelist;

    char count_status = 0;

    char  src_file[MAX_FILENAME_LEN];
    char  dest_file[MAX_FILENAME_LEN];

    char buff[4194304];
    size_t bytes_read, bytes_write;

    FILE *src_fp, *dest_fp;

    char tmp_datafile_dir_val[MAX_FILENAME_LEN];
    char datafile_dir_val[MAX_FILENAME_LEN];

    while(1)
    {
        read_protected_string(tmp_datafile_dir, tmp_datafile_dir_val, &tmp_datafile_dir_lock);
        if(strlen(tmp_datafile_dir_val)==0)
            continue;

        count_status = atomic_load(&count);
        num_files = scandir(tmp_datafile_dir_val, &namelist, 0, alphasort);

        //printf("%d items found in %s.\n", num_files, tmp_datafile_dir_val);
        //for (int i=0; i<num_files; i++)
        //{
        //    printf("%d - %s\n", i+1, namelist[i]->d_name);
        //}

        if( num_files > 3 )
        {
            if( count_status == 1 )
            {
                free(namelist[--num_files]);  // leave the last file when counting
            }

            read_protected_string(datafile_dir, datafile_dir_val, &datafile_dir_lock);

            while (num_files>2)  // 
            {
                memset(src_file, 0, MAX_FILENAME_LEN);
                memset(dest_file, 0, MAX_FILENAME_LEN);
    
                strcat(src_file, tmp_datafile_dir_val);
                strcat(src_file, "/");

                printf("src_file = %s\n", src_file);
                printf("filename = %s\n", namelist[--num_files]->d_name);
                strcat(src_file, namelist[num_files]->d_name);
    
                strcat(dest_file, datafile_dir_val);
                strcat(dest_file, "/");
                strcat(dest_file, namelist[num_files]->d_name);
    
                //printf("Moving %s\n", namelist[--num_files]->d_name);
                printf("Moving %s\n", src_file);
    
#ifdef _COPY_DELETE_
                src_fp = fopen(src_file, "rb");
                if(NULL == src_fp)
                {
                    perror("Error opening source file");
                    break;
                }
    
                dest_fp = fopen(dest_file, "wb");
                if(NULL == src_fp)
                {
                    perror("Error opening source file");
                    break;
                }
    
                while(1)
                {
                    bytes_read = fread(buff, 1, sizeof(buff), src_fp);
                    log("Read %d bytes\n", bytes_read);
                    if(bytes_read <= 0)
                    {   
                        break;
                    }   
                    bytes_write = fwrite(buff, 1, bytes_read, dest_fp);
                    log("Wrote %d bytes\n", bytes_write);
                }
 
                fclose(dest_fp);
                fclose(src_fp);
    
                if(remove(src_file) != 0)
                {
                    perror("Error deleting source file");
                }
#else
                int result = rename(src_file, dest_file);
                if(result!=0)
                {
                    perror("failed to move file\n");
                }
#endif
                free(namelist[num_files]);
            }

            free(namelist[0]); // free '.'
            free(namelist[1]); // free '..'

            free(namelist);
            pthread_yield();
        }

        pthread_yield();
    }

}
