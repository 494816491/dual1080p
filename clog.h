#ifndef WIS_LOG_H
#define WIS_LOG_H

/*
#define CL_DEBUG		0
#define CL_VERBOSE		1
#define CL_INFO			2
#define CL_WARN			3
#define CL_ERR			4
#define CL_ALARM		5
*/

#if __cplusplus
extern "C" {
#endif

#define WIS_DEBUG   0 
#define WIS_INFO  	1
#define WIS_WARNING 2
#define WIS_ALARM   3
#define WIS_ERROR   4

void clog(int level, char* fmt, ...);
void clog_init(char *path);
void clog_exit();




#define trace(lvl, fmt...) \
do{\
	char __buf1[64], __buf2[1024];\
    snprintf(__buf1, sizeof(__buf1), "[%s:%d-%s] ", __FILE__, __LINE__, __FUNCTION__);\
    snprintf(__buf2, sizeof(__buf2), fmt);\
    clog(lvl, "%s%s", __buf1, __buf2);\
} while(0)


#define Try_Call(express, name)\
    do{\
        int func_ret = express;\
        if (0 != func_ret)\
        {\
            trace(WIS_ERROR, "%s ERR %#x", name, func_ret);\
            return func_ret;\
        }\
    }while(0)

#define AssertNull(express,name) \
    do{\
        void *func_ret = express; \
        if(NULL != func_ret) \
        { \
           trace(WIS_ERROR, "%s ERR %#x", name, func_ret);\
        } \
    }while(0)

#define AssertZero(express,name) \
    do{\
        int func_ret = express; \
        if(0 != func_ret) \
        { \
           trace(WIS_ERROR, "%s ERR %#x", name, func_ret);\
        } \
    }while(0)

#if __cplusplus
}
#endif

#endif

