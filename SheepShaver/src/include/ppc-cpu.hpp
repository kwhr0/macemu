/*
 *  ppc-cpu.hpp - PowerPC CPU definition
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

#ifndef PPC_CPU_H
#define PPC_CPU_H

#include "TinyPPC.h"
#include "spcflags.hpp"

struct powerpc_cpu {
	TinyPPC tinyppc;
	int execute_depth;
	uint32 & gpr(int i)			{ return tinyppc.gpr[i]; }
	uint32 gpr(int i) const		{ return tinyppc.gpr[i]; }
	double & fpr(int i)			{ return tinyppc.fpr[i].f; }
	double fpr(int i) const		{ return tinyppc.fpr[i].f; }
	uint32 & lr()				{ return tinyppc.lr; }
	uint32 lr() const			{ return tinyppc.lr; }
	uint32 & ctr()				{ return tinyppc.ctr; }
	uint32 ctr() const			{ return tinyppc.ctr; }
	uint32 & pc()				{ return tinyppc.pc; }
	uint32 pc() const			{ return tinyppc.pc; }
	powerpc_cpu() { spcflags_init(); }
	void Reset();
	void execute();
	void execute(uint32 entry);
	void invalidate_cache_range(uintptr start, uintptr end);
	void trigger_interrupt() {
		spcflags_set(SPCFLAG_CPU_TRIGGER_INTERRUPT);
	}
};

#endif /* PPC_CPU_H */
