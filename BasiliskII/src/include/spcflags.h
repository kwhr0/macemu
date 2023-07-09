 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68000 emulation
  *
  * Copyright 1995 Bernd Schmidt
  */

#ifndef SPCFLAGS_H
#define SPCFLAGS_H

enum {
	SPCFLAG_STOP			= 0x01,
	SPCFLAG_INT	            = 0x02,
	SPCFLAG_BRK				= 0x04,
	SPCFLAG_TRACE			= 0x08,
	SPCFLAG_DOTRACE			= 0x10,
	SPCFLAG_DOINT			= 0x20,
	SPCFLAG_VBL			= 0x100,
	SPCFLAG_MFP			= 0x200,
	SPCFLAG_INT3		= 0x800,
	SPCFLAG_INT5		= 0x1000,
	SPCFLAG_SCC		= 0x2000,
	SPCFLAG_ALL			= SPCFLAG_STOP
					| SPCFLAG_INT
					| SPCFLAG_BRK
					| SPCFLAG_TRACE
					| SPCFLAG_DOTRACE
					| SPCFLAG_DOINT
					| SPCFLAG_INT3
					| SPCFLAG_VBL
					| SPCFLAG_INT5
					| SPCFLAG_SCC
					| SPCFLAG_MFP
};

extern uae_u32 spcflags;

#define SPCFLAGS_TEST(m) \
	((spcflags & (m)) != 0)

/* Macro only used in m68k_reset() */
#define SPCFLAGS_INIT(m) do { \
	spcflags = (m); \
} while (0)

#include "main.h"
extern B2_mutex *spcflags_lock;

#define SPCFLAGS_SET(m) do { 				\
	B2_lock_mutex(spcflags_lock);			\
	spcflags |= (m);					\
	B2_unlock_mutex(spcflags_lock);		\
} while (0)

#define SPCFLAGS_CLEAR(m) do {				\
	B2_lock_mutex(spcflags_lock);			\
	spcflags &= ~(m);					\
	B2_unlock_mutex(spcflags_lock);         \
} while (0)

#endif /* SPCFLAGS_H */
