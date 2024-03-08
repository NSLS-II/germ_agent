#ifndef _LOG_
#define _LOG_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */


//#define _DBG_
#define _INFO_

#ifdef _DBG_
#define log(a, ...)   { char* _str_ptr_ = malloc(strlen(a)+100);     \
                        strcpy(_str_ptr_, "[%s]: ");                 \
                        strcat(_str_ptr_, a);                        \
                        printf(_str_ptr_, __func__, ##__VA_ARGS__);  \
                        free(_str_ptr_);                            }
#else
#define log(a,...)
#endif

#ifdef _INFO_
#define info(a, ...)   { char* _str_ptr_ = malloc(strlen(a)+100);     \
                         strcpy(_str_ptr_, "[%s]: ");                 \
                         strcat(_str_ptr_, a);                        \
                         printf(_str_ptr_, __func__, ##__VA_ARGS__);  \
                         free(_str_ptr_);                            }
#else
#define info(a,...)
#endif

#define err(a, ...)   { char* _str_ptr_ = malloc(strlen(a)+100);                        \
                        time_t raw_time;                                                \
                        struct tm * timeinfo;                                           \
                        time( &raw_time);                                               \
                        timeinfo = localtime( &raw_time );                              \
                        strcpy(_str_ptr_, "ERROR: @%s    [%s]: ");                      \
                        strcat(_str_ptr_, a);                                           \
                        printf(_str_ptr_, asctime(timeinfo), __func__, ##__VA_ARGS__);  \
                        free(_str_ptr_);                            }

#define warn(a, ...)   { char* _str_ptr_ = malloc(strlen(a)+100);                        \
                         time_t raw_time;                                                \
                         struct tm * timeinfo;                                           \
                         time( &raw_time);                                               \
                         timeinfo = localtime( &raw_time );                              \
                         strcpy(_str_ptr_, "WARNING: @%s    [%s]: ");                    \
                         strcat(_str_ptr_, a);                                           \
                         printf(_str_ptr_, asctime(timeinfo), __func__, ##__VA_ARGS__);  \
                         free(_str_ptr_);                            }


#endif
