/*
 *  basilisk_glue.cpp - Glue UAE CPU to Basilisk II CPU engine interface
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "emul_op.h"
#include "timer.h"
#include "spcflags.h"

#include "Tiny68020.h"
Tiny68020 tiny68020;

// RAM and ROM pointers
uint32 RAMBaseMac = 0;		// RAM base (Mac address space) gb-- initializer is important
uint8 *RAMBaseHost;			// RAM base (host address space)
uint32 RAMSize;				// Size of RAM
uint32 ROMBaseMac;			// ROM base (Mac address space)
uint8 *ROMBaseHost;			// ROM base (host address space)
uint32 ROMSize;				// Size of ROM

#if !REAL_ADDRESSING
// Mac frame buffer
uint8 *MacFrameBaseHost;	// Frame buffer base (host address space)
uint32 MacFrameSize;		// Size of frame buffer
int MacFrameLayout;			// Frame buffer layout
#endif

#if DIRECT_ADDRESSING
uintptr MEMBaseDiff;		// Global offset between a Mac address and its Host equivalent
#endif

uae_u32 spcflags;
B2_mutex *spcflags_lock = NULL;

// From newcpu.cpp
extern int quit_program;
void m68k_execute(void);

/*
 *  Initialize 680x0 emulation, CheckROM() must have been called first
 */

bool Init680x0(void)
{
	spcflags_lock = B2_create_mutex();
#if REAL_ADDRESSING
	// Mac address space = host address space
	RAMBaseMac = (uintptr)RAMBaseHost;
	ROMBaseMac = (uintptr)ROMBaseHost;
#elif DIRECT_ADDRESSING
	// Mac address space = host address space minus constant offset (MEMBaseDiff)
	// NOTE: MEMBaseDiff is set up in main_unix.cpp/main()
	RAMBaseMac = 0;
	ROMBaseMac = Host2MacAddr(ROMBaseHost);
#else
	// Initialize UAE memory banks
	RAMBaseMac = 0;
	switch (ROMVersion) {
		case ROM_VERSION_64K:
		case ROM_VERSION_PLUS:
		case ROM_VERSION_CLASSIC:
			ROMBaseMac = 0x00400000;
			break;
		case ROM_VERSION_II:
			ROMBaseMac = 0x00a00000;
			break;
		case ROM_VERSION_32:
			ROMBaseMac = 0x40800000;
			break;
		default:
			return false;
	}
	memory_init();
#endif
	return true;
}


/*
 *  Deinitialize 680x0 emulation
 */

void Exit680x0(void)
{
}


/*
 *  Initialize memory mapping of frame buffer (called upon video mode change)
 */

void InitFrameBufferMapping(void)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	memory_init();
#endif
}

/*
 *  Reset and start 680x0 emulation (doesn't return)
 */

void Start680x0(void)
{
	tiny68020.Reset();
	m68k_execute();
}


/*
 *  Trigger interrupt
 */

void TriggerInterrupt(void)
{
	idle_resume();
	SPCFLAGS_SET( SPCFLAG_INT );
}

void TriggerNMI(void)
{
}


/*
 *  Get 68k interrupt level
 */

int intlev(void)
{
	return InterruptFlags ? 1 : 0;
}


/*
 *  Execute MacOS 68k trap
 *  r->a[7] and r->sr are unused!
 */

void Execute68kTrap(uint16 trap, struct M68kRegisters *r)
{
	tiny68020.execsub(trap, *r, true);
}


/*
 *  Execute 68k subroutine
 *  The executed routine must reside in UAE memory!
 *  r->a[7] and r->sr are unused!
 */

void Execute68k(uint32 addr, struct M68kRegisters *r)
{
	tiny68020.execsub(addr, *r, false);
}


int quit_program = 0;

void m68k_emulop_return(void)
{
	SPCFLAGS_SET( SPCFLAG_BRK );
	quit_program = 1;
}

void m68k_emulop(uae_u32 opcode)
{
	struct M68kRegisters r;
	tiny68020.exportRegs(r);
	EmulOp(opcode, &r);
	tiny68020.importRegs(r);
}

int m68k_do_specialties(void)
{
	if (SPCFLAGS_TEST(SPCFLAG_DOTRACE)) {
		SPCFLAGS_CLEAR(SPCFLAG_DOTRACE);
		tiny68020.Trace();
	}
	if (SPCFLAGS_TEST(SPCFLAG_TRACE)) {
		SPCFLAGS_CLEAR(SPCFLAG_TRACE);
		SPCFLAGS_SET(SPCFLAG_DOTRACE);
	}
	if (SPCFLAGS_TEST( SPCFLAG_DOINT )) {
		SPCFLAGS_CLEAR( SPCFLAG_DOINT );
		int intr = intlev();
		if (intr != -1)
			tiny68020.IRQ(intr);
	}

	if (SPCFLAGS_TEST( SPCFLAG_INT )) {
		SPCFLAGS_CLEAR( SPCFLAG_INT );
		SPCFLAGS_SET( SPCFLAG_DOINT );
	}

	if (SPCFLAGS_TEST( SPCFLAG_BRK )) {
		SPCFLAGS_CLEAR( SPCFLAG_BRK );
		return 1;
	}

	return 0;
}

void m68k_execute (void)
{
setjmpagain:
	TRY(prb) {
		for (;;) {
			if (quit_program > 0) {
				if (quit_program == 1)
					break;
				quit_program = 0;
			}
			tiny68020.Execute();
		}
	}
	CATCH(prb) {
		goto setjmpagain;
	}
}
