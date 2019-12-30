/*
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the device startup code. This file should be the first object linked into the device
// It's our "device startup code".

// ------------------- INCLUDES ------------------------

#include <proto/exec.h>
#include <proto/utility.h>
#include <exec/types.h>
#include <exec/resident.h>

//From old libnix:
#include <libinit.h>

#include "devdebug.h"
#include "device.h"
#include "helper.h"
#include "tools.h"
#include "version.h"


// ------------------- Globals ---------------------------

extern const APTR InitTab[4];

// ------------------- Prototypes ------------------------

SAVEDS struct Library * asmDevInit(REG(a0,BPTR SegListPointer),
                                   REG(d0,struct Library * dev),
                                   REG(a6,struct Library * exec_base));

// ------------------- IMPLEMENTATION ------------------------

/**
 * First function of the device. Dummy function. "libnix" needs "exit()" to be referenced.
 * Also this is save way to prevent execution from CLI.
 */
LONG asmDevNullFunction( void )
{
   return 0;
}
ALIAS(exit, asmDevNullFunction);


static const struct Resident RomTag __attribute__((used)) =
{
  RTC_MATCHWORD,
  (struct Resident *)&RomTag,
  (struct Resident *)&RomTag+1,
  RTF_AUTOINIT,
  DEVICE_VERSION,
  NT_DEVICE,
  0,
  (char *)DevName,
  (char *)DevIdString,
  (APTR)&InitTab
};

const APTR InitTab[4] =
{
  (APTR)sizeof(struct DeviceDriver),
  (APTR)&__FuncTable__[1],
  (APTR)NULL,           //=> initialize device base by hand (in function "DevInit()" )
  (APTR)&asmDevInit
};

// ------------------------------ dealing with baserel comoilation ------------------------------------------

//When compiled with baserel or baserl32
#if defined(BASEREL)

static inline APTR __GetDataSeg(void)
{ APTR res;

  __asm("lea ___a4_init,%0" : "=a" (res));
  return res;
}

//const unsigned short __restore_a4[]; //= { 0x286e, OFFSET(LibraryHeader, dataSeg), 0x4e75 }; // "move.l a6@(dataSeg:w),a4;rts"

void __restore_a4(void)
{
   __asm("lea   ___a4_init,a4");
   //__asm("rts");
}

APTR dataSegment = NULL;
#endif





/**
 * AmigaOS entry point "DevInit".
 *
 * @param REG(a0,BPTR SegListPointer)
 * @param REG(d0,struct Library * dev)
 * @param REG(a6,struct Library * exec_base)
 * @return D0 Library...
 */
SAVEDS
struct Library * asmDevInit(REG(a0,BPTR SegListPointer),
                            REG(d0,struct Library * dev),
                            REG(a6,struct Library * exec_base))
{
   //Pick up the exec base...
   SysBase = (struct ExecBase *)exec_base;

#if defined(BASEREL)
   dataSegment = __GetDataSeg();
#endif

   return DeviceInit(SegListPointer, dev, exec_base );
}

/**
 * AmigaOS entry point "Open Device".
 *
 * @param REG(d0, ULONG unit)
 * @param REG(a1, APTR iorq)
 * @param REG(d1, ULONG flags)
 * @param REG(a6, struct Library * dev)
 */
SAVEDS
VOID asmDevOpen(REG(d0, ULONG unit),
              REG(a1, struct IOSana2Req * iorq),
              REG(d1, ULONG flags),
              REG(a6, struct Library * dev))
{
    DeviceOpen(iorq, unit, flags, dev);
}

/**
 * AmigaOS entry point "Close Device".
 *
 * @param REG(a1,APTR iorq)
 * @param REG(a6,struct Library * DevBase)
 * @return
 */
SAVEDS
BPTR asmDevClose(REG(a1,APTR iorq),REG(a6,struct Library * DevBase))
{
   return DeviceClose(iorq, DevBase);
}

/**
 * AmigaOS entry point "Expunge device".
 *
 * @param REG(a6,struct Library * DevBase)
 * @return
 */
SAVEDS
BPTR asmDevExpunge(REG(a6,struct Library * DevBase))
{
   return DeviceExpunge(DevBase);
}

/**
 * AmigaOS entry point "BeginIO".
 *
 * @param REG(a1,struct IOSana2Req *ios2)
 * @param REG(a6,struct Library * dev)
 */
SAVEDS
void asmDevBeginIO(REG(a1,struct IOSana2Req * ios2), REG(a6,struct Library * deviceBase))
{
   DeviceBeginIO(ios2,deviceBase);
}

/**
 * AmigaOS entry point "AbortIO".
 *
 * @param REG(a1,struct IOSana2Req *ios2)
 * @param REG(a6,struct Library * dev)
 * @return
 */
SAVEDS
void asmDevAbortIO(REG(a1,struct IOSana2Req * ios2), REG(a6,struct Library * deviceBase))
{
   DeviceAbortIO(ios2,deviceBase);
}

/**
 * Now add all "Device Functions" to the Library/Device.
 * WARNING: All functions must exist! If not no warning is generated and the ADD2LIST is not called.
 * Errors here would reorder the function table and funny things happen when using the library!
 */
ADD2LIST(asmDevOpen,         __FuncTable__,22);
ADD2LIST(asmDevClose,        __FuncTable__,22);
ADD2LIST(asmDevExpunge,      __FuncTable__,22);
ADD2LIST(asmDevNullFunction, __FuncTable__,22); // => Unused AmigaOS function (but must be present in table!)
ADD2LIST(asmDevBeginIO,      __FuncTable__,22);
ADD2LIST(asmDevAbortIO,      __FuncTable__,22);
ADDTABL_END();



