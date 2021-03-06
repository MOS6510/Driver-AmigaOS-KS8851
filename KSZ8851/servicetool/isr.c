/*
 * KSZ8851 Amiga Network Driver. This file contains AmigaOS ISR stuff.
 */

#include "isr.h"

#include "ksz8851.h"

#if !defined(REG)
#define REG(reg,arg) arg __asm(#reg)
#endif

//Amiga Color register 0
#define COLORREG00 *((uint16_t*)0xdff180)

#if DEBUG > 0
#define SetBackgroundColor(x) COLORREG00 = x
#else
#define SetBackgroundColor(x)
#endif


// The priority of our AmigaOS interrupt server handler
#define INTERRUPT_SERVER_PRIOIRY 0 //121

 /*
  * The interrupt service routine ("Amiga Interrupt Server handler")
  */
 static volatile uint8_t isr( REG(a1, NetInterface * interface) )
 {
    Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;

    //Is ISR handling enabled?
    if (0 != context->intDisabledCounter) {
       //ISR is disabled, DO NOT TOUCH ANY NIC REGISTERS!
       SetBackgroundColor(2);
       //=> We can't be the source of the interrupt request!
       return (uint8_t)0;
    }

    //Call the handler. Check if the interrupt was from our hardware...
    if (ksz8851IrqHandler(interface)) {
       SetBackgroundColor(0xa);
       //Signal the service task
       Signal(context->signalTask, (1 << context->sigNumber));
       context->signalCounter++;

       //Stop the interrupt chain. Signal that we processed this interrupt.
       return (uint8_t)1;
    }

    //Signal AmigaOS that out hardware was not the source of the interrupt request...
    return (uint8_t)0;
 }

 BOOL installInterruptHandler(NetInterface * interface)
 {
    Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;

    if (context->signalTask) {
       TRACE_DEBUG("Interrupt is already installed...\n");
       return TRUE;
    }

    //Support only current task..
    context->signalTask = FindTask(0l);
    context->sigNumber  = AllocSignal(-1);

    TRACE_INFO("ISR: Task = '%s', signal=0x%lx (mask=0x%lx)\n", context->signalTask->tc_Node.ln_Name, context->sigNumber, (1 << context->sigNumber));

    if (context->sigNumber == -1) {
       TRACE_DEBUG("No free signal!\n");
       context->signalTask = NULL;
       return FALSE;
    }

    context->AmigaInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    context->AmigaInterrupt.is_Node.ln_Name = "KSZ8851 SANA2 network driver interrupt";
    context->AmigaInterrupt.is_Node.ln_Pri  = INTERRUPT_SERVER_PRIOIRY;
    context->AmigaInterrupt.is_Data         = interface; //will be called in ISR with A1 register
    context->AmigaInterrupt.is_Code         = (void*)(&isr);

    //Network chip uses (INT6) which is "external interrupt" on AmigaOS.
    AddIntServer(INTB_EXTER, &context->AmigaInterrupt);

    return TRUE;
 }

 void uninstallInterruptHandler(NetInterface * interface)
 {
    Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;
    if(context->signalTask)
    {
       //Free interrupt and signal number...
       RemIntServer(INTB_EXTER, &context->AmigaInterrupt);
       context->signalTask = NULL;
       FreeSignal(context->sigNumber);
       context->sigNumber= -1;
    } else {
       TRACE_DEBUG("Trying to uninstall ISR but it was not installed. Ignored.\n");
    }
 }

