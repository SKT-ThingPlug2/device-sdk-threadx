/**
 * @file SKTtpDebug.c
 *
 * @brief Log print
 *
 * Copyright (C) 2016. SK Telecom, All Rights Reserved.
 * Written 2016, by SK Telecom
 */

#include <SimpleAPI/include/SKTtpDebug.h>

#include "NTPClient/NTPClient.h"
#include "sf_comms_api.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

BOOLEAN_TYPE_E  gSKTtpDebugEnable   = False;
LOG_LEVEL_E     gSKTtpDebugLogLevel = (LOG_LEVEL_E)LOG_LEVEL_NONE;
extern const sf_comms_instance_t g_sf_comms0;
extern unsigned int ntp_time(void);
char gStopConsole  = 0;

/*
 * SKTtpDebugLogLevelString
 */
void SKTtpDebugLogLevelString(LOG_LEVEL_E level, char *buffer)
{
    switch(level) {
        case LOG_LEVEL_VERBOSE: {
            memcpy(buffer, "VERBOSE", strlen("VERBOSE"));
        }
        break;

        case LOG_LEVEL_DEBUG: {
            memcpy(buffer, "DEBUG", strlen("DEBUG"));
        }
        break;

        case LOG_LEVEL_INFO: {
            memcpy(buffer, "INFO", strlen("INFO"));
        }
        break;

        case LOG_LEVEL_WARN: {
            memcpy(buffer, "WARN", strlen("WARN"));
        }
        break;

        case LOG_LEVEL_ERROR: {
            memcpy(buffer, "ERROR", strlen("ERROR"));
        }
        break;
        case LOG_LEVEL_ATCOM: {
                memcpy(buffer, "ATCOM", strlen("ATCOM"));
        }
        break;
        default: {
            memcpy(buffer, "NONE", strlen("NONE"));
        }
        break;
    }
}

/*
 * DebugInit
 */
void SKTtpDebugInit(BOOLEAN_TYPE_E enable, LOG_LEVEL_E level)
{
    gSKTtpDebugEnable   = enable;
    gSKTtpDebugLogLevel = level;
}

/*
 * TimeToString
 */
char* TimeToString(struct tm *t) {
  static char s[20];

  snprintf(s,20, "%04d-%02d-%02d %02d:%02d:%02d",
              t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
              t->tm_hour, t->tm_min, t->tm_sec
          );

  return s;
}

/*
 * DebugPrint
 */
void SKTtpDebugPrintf(LOG_LEVEL_E level, const char *format, ...)    
{
    if(gSKTtpDebugEnable == False ) {
        return;
    }

    if(gSKTtpDebugLogLevel == LOG_LEVEL_NONE) {
        return;
    }


    if(gSKTtpDebugLogLevel > level) {
        return;
    }

    char stringBuffer[30];
    memset(stringBuffer, 0, 30);
    SKTtpDebugLogLevelString(level, stringBuffer);


    struct tm *t;
    time_t timer;

    timer = ntp_time();
    timer = timer + get_npt_offset() + 32400;
    t = localtime(&timer);

    char buffer[2048];
    snprintf(buffer,2048, "[%s] [%s]: ", TimeToString(t), stringBuffer);
    if(!gStopConsole){
        g_sf_comms0.p_api->write(g_sf_comms0.p_ctrl, buffer,  strlen(buffer) ,TX_NO_WAIT);
        memset(buffer,0,2048);
        va_list argp;
        va_start(argp, format);
        vsprintf(buffer, format, argp);
        va_end(argp);
        g_sf_comms0.p_api->write(g_sf_comms0.p_ctrl, buffer,  strlen(buffer) ,TX_NO_WAIT);
        g_sf_comms0.p_api->write(g_sf_comms0.p_ctrl, "\r\n",  strlen("\r\n") ,TX_NO_WAIT);
    }
}

