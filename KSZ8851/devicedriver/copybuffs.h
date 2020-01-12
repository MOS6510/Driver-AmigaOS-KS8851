/*
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef __COPY_BUFFS__H__
#define __COPY_BUFFS__H__

#include <exec/types.h>
#include <utility/hooks.h>
#include <devices/sana2.h>
#include "helper.h"

#if !defined(REG)
#define REG(reg,arg) arg __asm(#reg)
#endif

/*
** Typedef's for the SANA-II callback functions.
*/
typedef BOOL  (*SANA2_CFB)(APTR to, APTR from, LONG length);
typedef BOOL  (*SANA2_CTB)(APTR to, APTR from, LONG length);
typedef ULONG (*HOOK_FUNC)(struct Hook *hook,struct IOSana2Req * io,APTR message);

#define CopyFromBuffer(to,from,len)      CopyBuf(bm->bm_CopyFromBuffer  ,to,from,len)
#define CopyToBuffer(to,from,len)        CopyBuf(bm->bm_CopyToBuffer    ,to,from,len)

/**
 * Main Copy Buffer function.
 * @param funcPtr
 * @param to
 * @param from
 * @param len
 * @return
 */
SAVEDS ULONG CopyBuf(APTR funcPtr, APTR to, APTR from, ULONG len);

/**
 * Main filter function
 * @param hook
 * @param ioreq
 * @param rawPktData
 * @return
 */
SAVEDS ULONG CallFilterHook(struct Hook * hook, struct IOSana2Req * ioreq, APTR rawPktData);


/**
 * SANA2 V3 extension: DMA copy
 */

//V3 extension DMA:
#define CopyFromBufferDMA(io_data)      CopyFromOrToBufferDMA(bm->bm_CopyFromBufferDMA, io_data)
#define CopyToBufferDMA(io_data)        CopyFromOrToBufferDMA(bm->bm_CopyToBufferDMA,   io_data)
SAVEDS APTR CopyFromOrToBufferDMA( REG(a1, APTR funcPtr), REG(a0, APTR io_data_buffer) );

#endif
