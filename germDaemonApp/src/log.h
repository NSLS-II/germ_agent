#ifndef _LOG_
#define _LOG_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define _DBG_

#ifdef _DBG_
#define log(a, ...)   {char* _str_ptr_ = malloc(strlen(a)+100);      \
                       strcpy(_str_ptr_, "[%s]: ");                 \
                       strcat(_str_ptr_, a);                        \
                       printf(_str_ptr_, __func__, ##__VA_ARGS__);  \
                       free(_str_ptr_);                            }
#else
#define log(a,...)
#endif

#define err(a, ...)   {char* _str_ptr_ = malloc(strlen(a)+100);      \
                       strcpy(_str_ptr_, "!!!ERROR!!! [%s]: ");        \
                       strcat(_str_ptr_, a);                        \
                       printf(_str_ptr_, __func__, ##__VA_ARGS__);  \
                       free(_str_ptr_);                            }
#endif
