
/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.

 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.

 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
*/

#ifndef _DRAND48_H
#define _DRAND48_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef HAVE_DRAND48

#define	RAND48_SEED_0	(0x330e)
#define	RAND48_SEED_1	(0xabcd)
#define	RAND48_SEED_2	(0x1234)
#define	RAND48_MULT_0	(0xe66d)
#define	RAND48_MULT_1	(0xdeec)
#define	RAND48_MULT_2	(0x0005)
#define	RAND48_ADD	    (0x000b)

void _dorand48(unsigned short xseed[3]);

double erand48(unsigned short xseed[3]);

double drand48(void);

#endif // HAVE_DRAND48

#endif  // _DRAND48_H

