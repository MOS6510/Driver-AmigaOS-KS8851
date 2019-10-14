/* 
 * Copyright (C) 1997 - 1999 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef CONFFILE_H
#define CONFFILE_H

#include <exec/types.h>

void RegistryInit( const char * File);
void RegistryDestroy();
char * ReadKeyStr( const char * Key, char * def );
long   ReadKeyInt( const char * Key, long def );

#endif

