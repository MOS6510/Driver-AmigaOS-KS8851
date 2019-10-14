#ifndef __ETHERBRIDGE_VERSION__
#define __ETHERBRIDGE_VERSION__

#define DEVICE_VERSION 2
#define DEVICE_REVISION 0
#define DEVICE_BETA_STATE "11"
#if DEBUG > 0
#define COMPILED_TIME " " __TIME__
#else
#define COMPILED_TIME
#endif

#define DEVICE_NAME "etherbridge.device"

#if defined(__mc68060__)
#define CPUTYPE "68060"
#elif defined(__mc68040__)
#define CPUTYPE "68040"
#elif defined(__mc68030__)
#define CPUTYPE "68030"
#elif defined(__mc68020__)
#define CPUTYPE "68020"
#else
#define CPUTYPE "68000"
#endif

#if DEBUG > 0
#define DEBUG_TEXT "DEBUG "
#else
#define DEBUG_TEXT ""
#endif



/**
 * It seems that the date should be in format (dd Mmm yyyy).
 * Only than AmigaOS (3.x) version command can extract it.
 */

//                       " BETA " DEVICE_BETA_STATE " "

#define VERSION_STRING "$VER " DEVICE_NAME \
                       " 2.0 (06 Jan 2019)" \
                       DEBUG_TEXT \
                       "(" CPUTYPE ")" \
                       " (c) by Heiko Pruessing " COMPILED_TIME \
                       "\r\n\0"
#endif

