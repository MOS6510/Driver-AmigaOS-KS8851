/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#include "configfile.h"

#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <dos/dos.h> 
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <dos/notify.h>
#include <dos/filehandler.h>
#include <dos/exall.h>
#include <clib/dos_protos.h>
#include <exec/alerts.h>

#include <strings.h>
#include "devdebug.h"


#define LINE_MAX_LEN 255

static ULONG fh = 0;
static char linebuff[LINE_MAX_LEN+3] = {0};
static struct RDArgs *rdargs = NULL;
static BOOL configFileOpen = FALSE;


void RegistryInit(const char * File) {

   if (fh) {
      Close(fh);
   }

   fh = Open((char*) File, MODE_OLDFILE);
   rdargs = (struct RDArgs*) AllocDosObject( DOS_RDARGS, NULL);

   if (fh && rdargs) {
      rdargs->RDA_ExtHelp = 0;
      configFileOpen = TRUE;
   } else {
      configFileOpen = FALSE;
   }
}

void RegistryDestroy() {
   if (fh) {
      Close(fh);
      fh = (ULONG)NULL;
   };
   if (rdargs) {
      FreeDosObject( DOS_RDARGS, rdargs);
      rdargs = NULL;
   }
}

//TODO: This is very slow! Bad alg. Needs to be fixed!
char * ReadKeyStr( const char * key, char * def )
{
    char * result;
    LONG args[] = {0};  // only 1 Argument

    //Alles ok
    if (!configFileOpen) return def;

    Seek(fh, 0, OFFSET_BEGINNING);

    //Read line by line... => bad!
    while(FGets( fh, linebuff, LINE_MAX_LEN))
    {
        if(linebuff[0] == '#') // Skip comment lines
           continue;
        if(linebuff[0] == ' ') // Skip space lines
           continue;
        if(linebuff[0] == 0xa) // Skip empty lines
           continue;

        rdargs->RDA_Source.CS_Buffer = (unsigned char *)linebuff;
        rdargs->RDA_Source.CS_Length = 256;
        rdargs->RDA_Source.CS_CurChr = 0;
        rdargs->RDA_Buffer           = NULL;
        rdargs->RDA_BufSiz           = 0;

        // ReadArgs() requires that the line be CR'd and null-terminated
        // or funny things happen.
        USHORT strlength = strlen(linebuff);
        linebuff[ strlength-1 ] = '\n';
        linebuff[ strlength-0 ] = 0;

        if(ReadArgs( (char*) key ,args, rdargs))
        {
           if(args[0])
           {
              result = (char*)args[0];
              FreeArgs( rdargs );
              return result;
           } else {
              FreeArgs( rdargs );
           }
        }
    }
    return def;
}


long ReadKeyInt(const char * Key, long def )
{
    LONG args[] = {0};  // 1 Argument
    char TempKey[80];

    strcpy(TempKey, Key);
    strcat(TempKey, "/N");

    //Alles ok ??
    if (!configFileOpen) return def;

    Seek(fh, 0, OFFSET_BEGINNING);

    while(FGets( fh, linebuff, LINE_MAX_LEN))
    {
        if(linebuff[0] == '#') // Skip comment lines
           continue;
        if(linebuff[0] == ' ') // Skip space lines
           continue;
        if(linebuff[0] == 0xa) // Skip empty lines
           continue;

        rdargs->RDA_Source.CS_Buffer = (unsigned char *)linebuff;
        rdargs->RDA_Source.CS_Length = 256;
        rdargs->RDA_Source.CS_CurChr = 0;
        rdargs->RDA_Buffer           = NULL;
        rdargs->RDA_BufSiz           = 0;

        // ReadArgs() requires that the line be CR'd and null-terminated
        // or funny things happen.
        USHORT strlength = strlen(linebuff);
        linebuff[strlength - 1] = '\n';
        linebuff[strlength - 0] = 0;


        if(ReadArgs( TempKey ,args, rdargs))
        {
           long result;

           if(args[0])
           {
              result = (*(long*)args[0]);
              FreeArgs( rdargs );
              return result;
           }
           FreeArgs( rdargs );
        }
    }
    return def;
}

static uint32_t hexStringToU32(char * str) {
   uint32_t val = 0;
   while (*str) {
      val <<= 4;
      if (*str >= '0' && *str <= '9') {
         val = val | (*str - '0');
      } else if (*str >= 'a' && *str <= 'f') {
         val = val | (*str - 'a' + 10);
      } else if (*str >= 'A' && *str <= 'F') {
         val = val | (*str - 'A' + 10);
      }

      ++str;
   }
   return val;
}

/**
 * Reads a MAC address from configuration string...
 * @param key
 * @param dstMacAddress
 * @param defaultAddress
 * @return
 */
void ReadKeyMacAddress(const char * key, uint8_t * dstMacAddress, const uint8_t * defaultAddress ) {
   const char * DELIM = ": ";
   char * macString = ReadKeyStr(key, NULL);
   if (macString) {
      char * pch = strtok(macString, DELIM);
      for (uint8_t i = 0; (i < 6) && pch != NULL; i++) {
         dstMacAddress[i] = hexStringToU32(pch);
         pch = strtok(NULL, DELIM);
      }
   } else {
      for (uint8_t i = 0; i < 6; i++) {
         dstMacAddress[i] = defaultAddress[i];
      }
   }
}
