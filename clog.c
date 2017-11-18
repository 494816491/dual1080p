#include "clog.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


static int minlevel = WIS_INFO;

static FILE* logfile = NULL;



#define CLF_MAXSIZE		(512 * 1024)
#define CLF_NUM			4

//VC log =  syslog0.txt
//NT log =  syslog1.txt
//IF  log =  syslog2.txt
//P1 log =  syslog4.txt 

//#define CLF_TEMPLATE	"/tmp/syslog%d.txt"
#define USB_MASS   	    "Generic"

static char CLF_TEMPLATE[64];

int IsUSBOk()
{

	FILE *stream;
	char buf[1024];
	char *find;
	memset(buf,0,sizeof(buf));
    stream = popen("lsscsi","r");
	fread(buf,sizeof(char),sizeof(buf),stream);
	pclose(stream);
	find = strstr(buf,USB_MASS);
    if(find != NULL ) return 1;
    return 0;
	
}

static void TruncateLogfile()
{
	char lfname[64];
	char lfn2[64];
	int i;
	
	if (logfile)
	{
		fclose(logfile);
		sprintf(lfname, CLF_TEMPLATE, CLF_NUM - 1);
		remove(lfname);
		for (i = CLF_NUM - 1; i > 0; i--)
		{
			sprintf(lfname, CLF_TEMPLATE, i);
			sprintf(lfn2, CLF_TEMPLATE, i - 1);
			rename(lfn2, lfname);
		}
		//sprintf(lfname, CLF_TEMPLATE, 0);
		logfile = fopen(lfn2, "a+");
		if (NULL == logfile)
		{
            printf("[%s-%d:%s] Open log file error \n", __FILE__, __LINE__, __FUNCTION__);
			return;
		}

		if (IsUSBOk())
		{
            system("cp /tmp/syslog*.txt /mnt/video2");
		}
	}
}


void clog_init(char *path)
{

	char lfname[64];
    memset(CLF_TEMPLATE,0,sizeof(CLF_TEMPLATE));
    strcpy(CLF_TEMPLATE,path);
	sprintf(lfname, CLF_TEMPLATE, 0);
	logfile = fopen(lfname, "a+");
	if (NULL == logfile)
	{
        printf("[%s-%d:%s] Open log file error \n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
	fseek(logfile, 0, SEEK_END);
	if (ftell(logfile) >= CLF_MAXSIZE)
	{
		TruncateLogfile();
	}
}


void clog_exit()
{
	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
	
	if (IsUSBOk())
	{
		system("cp /tmp/syslog*.txt /mnt/video2/");
	}
}


void clog(int level, char* fmt, ...)
{
	va_list ap;
	char tb[32], nf[512], line[1024];
	struct tm tm;
	time_t t;
	int len;

	va_start(ap, fmt);
	time(&t);
	localtime_r(&t, &tm);
	strftime(tb, sizeof(tb), "%Y.%m.%d-%H:%M:%S", &tm);
	snprintf(nf, sizeof(nf), "%s %d[%d]: %s\r\n", tb, tm.tm_wday, level, fmt);
	len = vsnprintf(line, sizeof(line), nf, ap);
	va_end(ap);
	fputs(line, stderr);
	if (logfile && (level >= minlevel))
	{
		fputs(line, logfile);

		if (ftell(logfile) >= CLF_MAXSIZE)
		{
			TruncateLogfile();
		}
		else
			fflush(logfile);
	}
}




