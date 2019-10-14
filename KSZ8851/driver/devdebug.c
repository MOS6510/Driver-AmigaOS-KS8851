/*
 * Copyright (C) 1997 - 1999, 2018 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <clib/debug_protos.h>

int debugLevel = 0;               //0: No debug, > 0 debug level

#if DEBUG > 0

//In Debug.a file but not in header file??
VOID vkprintf( CONST_STRPTR formatString, CONST APTR values );

void printdebug(short dbLevel, char * format, ...)
{
   if (debugLevel >= dbLevel)
   {
      Disable();
      va_list args;
      va_start(args,format);
      vkprintf(format, args);
      va_end( args );
      Enable();
   }
}

void print(short dbLevel, char * text)
{
   Disable();
   if (debugLevel >= dbLevel)
   {
      while(*text) {
         KPutChar(*(text++));
      }
   }
   Enable();
}

static char text[]="0x00000000\n\0";
static const char confstr[]="0123456789abcdef";

void printi(short dbLevel,unsigned long wert1)
{
   if (debugLevel >= dbLevel)
   {
      int i;
      ULONG wert2=wert1;
   
      for(i=9;i>=2;i--)
      {
        text[i]=confstr[wert2 & 0xf];
        wert2=wert2>>4;
      }
      print(dbLevel,text);
   } 
}

#endif