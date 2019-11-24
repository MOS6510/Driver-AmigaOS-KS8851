/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef CONFFILE_H
#define CONFFILE_H

/**
 * Initialize config reader.
 * @param File path to the config file
 */
void RegistryInit( const char * File);

/**
 * Destroy config reader
 */
void RegistryDestroy(void);

/**
 * Read a string value from an key.
 * @param Key string of the key
 * @param def default string value if not configurations was saved for that
 * @return config value or @param def
 */
char * ReadKeyStr( const char * Key, char * def );

/**
 * Read a numeric value
 * @param Key the key
 * @param def default value if not set
 */
long  ReadKeyInt( const char * Key, long def );

#endif

