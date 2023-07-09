/*
 *  spcflags.hpp - CPU special flags
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
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

#ifndef SPCFLAGS_H
#define SPCFLAGS_H

/**
 *		Basic special flags
 **/

enum {
	SPCFLAG_CPU_EXEC_RETURN			= 1 << 0,	// Return from emulation loop
	SPCFLAG_CPU_TRIGGER_INTERRUPT	= 1 << 1,	// Trigger user interrupt
	SPCFLAG_CPU_HANDLE_INTERRUPT	= 1 << 2,	// Call user interrupt handler
	SPCFLAG_CPU_ENTER_MON			= 1 << 3,	// Enter cxmon
	SPCFLAG_JIT_EXEC_RETURN			= 1 << 4,	// Return from compiled code
};

extern uint32 spcflags_mask;
extern spinlock_t spcflags_lock;

static inline void spcflags_init() {
	spcflags_mask = 0; spcflags_lock = SPIN_LOCK_UNLOCKED;
}
static inline bool spcflags_empty() {
	return !spcflags_mask;
}
static inline bool spcflags_test(uint32 v) {
	return spcflags_mask & v;
}
static inline void spcflags_set(uint32 v) {
	spin_lock(&spcflags_lock); spcflags_mask |= v; spin_unlock(&spcflags_lock);
}
static inline void spcflags_clear(uint32 v) {
	spin_lock(&spcflags_lock); spcflags_mask &= ~v; spin_unlock(&spcflags_lock);
}

#endif /* SPCFLAGS_H */
