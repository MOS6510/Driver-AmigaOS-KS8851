/*
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef HELPER_H
#define HELPER_H

//only used when compiled with baserel option but want to get rid of the compiler warning...
#if baserel
#define SAVEDS __saveds__
#else
#define SAVEDS
#endif

#endif
