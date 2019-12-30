/*
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef HELPER_H
#define HELPER_H

#include <exec/initializers.h>

//only used when compiled with baserel option but want to get rid of the compiler warning...
#if defined(BASEREL)
#define SAVEDS __saveds
#else
#define SAVEDS
#endif

#endif
