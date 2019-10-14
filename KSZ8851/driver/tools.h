/*
 * tools.h
 *
 *  Created on: 12.04.2018
 *      Author: heiko
 */

#ifndef TOOLS_H_
#define TOOLS_H_

#include <exec/types.h>


//Our own Etherbridge main alert number :-)
#define AN_Etherbridge   0x07200000

#ifdef DEBUG
//Checking something at runtime. Should be only active during development...
#define RUNTIME_ASSERT(a) \
   if (!(a)) { \
      DEBUGOUT((VERBOSE_HW,"ASSERT: %s %d\n", __FILE__ , __LINE__ )); \
      Alert( AT_DeadEnd | AG_BadParm | AN_Etherbridge ); \
   }
#else
#define RUNTIME_ASSERT(a)
#endif

#define ALERT_CODE_LIBRARY_NOT_OPEN (AT_DeadEnd | AG_OpenDev | AN_Etherbridge )

#define ALERT_ASSERT(a) if (!(a)){ Alert(DEADEND_ALERT | AN_Etherbridge | 0x20); }

/**
 * Checks that the library is really open.
 */
#define CHECK_LIBRARY_IS_OPEN(libBasePointer) \
   if (!libBasePointer) \
      Alert(ALERT_CODE_LIBRARY_NOT_OPEN);


extern void CHECK_ALREADY_QUEUED(struct List *, struct Node*);




#endif /* TOOLS_H_ */
