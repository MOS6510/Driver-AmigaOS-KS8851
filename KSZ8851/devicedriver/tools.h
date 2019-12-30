/*
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This file contains some useful tools for debugging purpose. Some are valid during release time.

#ifndef __TOOLS_H__
#define __TOOLS_H__

#include <exec/types.h>

//Our own main alert number :-)
#define AN_DEVICE   0x07200000

#ifdef DEBUG
//Checking something at runtime. Should be only active during development...
#define RUNTIME_ASSERT(a) \
   if (!(a)) { \
      DEBUGOUT((VERBOSE_HW,"ASSERT: %s %d\n", __FILE__ , __LINE__ )); \
      Alert( AT_DeadEnd | AG_BadParm | AN_DEVICE ); \
   }
#else
#define RUNTIME_ASSERT(a)
#endif

#define ALERT_CODE_LIBRARY_NOT_OPEN (AT_DeadEnd | AG_OpenDev | AN_DEVICE )

#define ALERT_ASSERT(a) if (!(a)){ Alert(DEADEND_ALERT | AN_DEVICE | 0x20); }

/**
 * Checks that the library is really open.
 */
#define CHECK_LIBRARY_IS_OPEN(libBasePointer) \
   if (!libBasePointer) \
      Alert(ALERT_CODE_LIBRARY_NOT_OPEN);

extern void CHECK_ALREADY_QUEUED(struct List *, struct Node*);


#endif /* TOOLS_H_ */
