#ifndef _KSZ8851_AMIGA_DRIVER_H
#define _KSZ8851_AMIGA_DRIVER_H

#include "ksz8851.h"

/**
 * Installs the Amiga Interrupt server. Function must be called from right Task!
 * @param interface
 * @param task
 * @param sigNumber
 */
BOOL installInterruptHandler(NetInterface * interface);

/**
 * Uninstalls the AmigaOS Interrupt Server
 * @param interface
 */
void uninstallInterruptHandler(NetInterface * interface);

#endif
