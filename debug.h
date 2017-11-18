 /**
 * \file debug.h
 * \author GoodMan <2757364047@qq.com>
 * \date 2015/07/12
 *
 * This file is for debug...
 * Copyright (C) 2015 GoodMan.
 *
 */

#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h>
#include "clog.h"

#if 1
#define dprintf(level, fmt,arg ...) do{ \
    clog(WIS_DEBUG, "vc -"fmt,  arg); \
    }while(0)


#define err_msg(fmt, arg...) do{    \
    clog(WIS_ERROR, "vc-[err]\t, %s %d " fmt,__FILE__, __LINE__, ## arg);    \
    } while (0)

#define info_msg(fmt, arg...) do{   \
        clog(WIS_INFO, "vc-[info] \t%s %d " fmt,__FILE__,__LINE__, ## arg);    \
    } while (0)

#define warn_msg(fmt, arg...) do {  \
        clog(WIS_WARNING, "vc-[warn]\t%s %d " fmt,__FILE__,__LINE__, ## arg); \
    } while (0)

#define debug_msg(fmt, arg...) do { \
        clog(WIS_DEBUG, "vc-[debug]\t%s %d " fmt, __FILE__, __LINE__, ## arg);   \
    } while (0)
#else
#define dprintf(level, fmt, arg...)     if (mal_test_dbg_level >= level) \
        printf("vc-[DEBUG]\t%s:%d " fmt, __FILE__, __LINE__, ## arg)

#define err_msg(fmt, arg...) do { if (mal_test_dbg_level >= 1)		\
    printf("vc-[ERR]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
    printf("vc-[ERR]\t" fmt, ## arg);	\
	} while (0)

#define info_msg(fmt, arg...) do { if (mal_test_dbg_level >= 2)		\
    printf("vc-[INFO]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
    printf("vc-[INFO]\t" fmt, ## arg);	\
	} while (0)

#define warn_msg(fmt, arg...) do { if (mal_test_dbg_level >= 3)		\
    printf("vc-[WARN]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
    printf("vc-[WARN]\t" fmt, ## arg);	\
	} while (0)

#define debug_msg(fmt, arg...) do { if (mal_test_dbg_level >= 4)	\
    printf("vc-[DEBUG]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
    printf("vc-[DEBUG]\t" fmt, ## arg);	\
	} while (0)
#endif


#endif //DEBUG_H
