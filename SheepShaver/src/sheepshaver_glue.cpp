/*
 *  sheepshaver_glue.cpp - Glue Kheperix CPU to SheepShaver CPU engine interface
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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
#include "main.h"
#include "prefs.h"
#include "xlowmem.h"
#include "emul_op.h"
#include "rom_patches.h"
#include "macos_util.h"
#include "sigsegv.h"
#include "ppc-bitfields.hpp"
#include "ppc-cpu.hpp"
#include "thunks.h"

// Used for NativeOp trampolines
#include "video.h"
#include "name_registry.h"
#include "serial.h"
#include "ether.h"
#include "timer.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef USE_SDL_VIDEO
#include <SDL_events.h>
#endif

#define DEBUG 0
#include "debug.h"

extern "C" {
#include "dis-asm.h"
}

// Emulation time statistics
#ifndef EMUL_TIME_STATS
#define EMUL_TIME_STATS 0
#endif

static void enter_mon(void)
{
}

// From main_*.cpp
extern uintptr SignalStackBase();

// From rsrc_patches.cpp
extern "C" void check_load_invoc(uint32 type, int16 id, uint32 h);
extern "C" void named_check_load_invoc(uint32 type, uint32 name, uint32 h);

// PowerPC EmulOp to exit from emulation looop
const uint32 POWERPC_EXEC_RETURN = POWERPC_EMUL_OP | 1;

// Enable Execute68k() safety checks?
#define SAFE_EXEC_68K 1

// Save FP state in Execute68k()?
#define SAVE_FP_EXEC_68K 1

// Interrupts in EMUL_OP mode?
#define INTERRUPTS_IN_EMUL_OP_MODE 1

// Interrupts in native mode?
#define INTERRUPTS_IN_NATIVE_MODE 1

// Pointer to Kernel Data
static KernelData * kernel_data;

// SIGSEGV handler
sigsegv_return_t sigsegv_handler(sigsegv_address_t, sigsegv_address_t);

void powerpc_cpu::Reset() {
	tinyppc.SetMemoryPtr((uint8 *)VMBaseDiff);
	tinyppc.Reset();
}

uint32 spcflags_mask;
spinlock_t spcflags_lock;

bool check_spcflags(TinyPPC *tinyppc)
{
	if (spcflags_test(SPCFLAG_CPU_EXEC_RETURN)) {
		spcflags_clear(SPCFLAG_CPU_EXEC_RETURN);
		return false;
	}
	if (spcflags_test(SPCFLAG_CPU_HANDLE_INTERRUPT)) {
		spcflags_clear(SPCFLAG_CPU_HANDLE_INTERRUPT);
		static bool processing_interrupt = false;
		if (!processing_interrupt) {
			processing_interrupt = true;
			tinyppc->Interrupt();
			processing_interrupt = false;
		}
	}
	if (spcflags_test(SPCFLAG_CPU_TRIGGER_INTERRUPT)) {
		spcflags_clear(SPCFLAG_CPU_TRIGGER_INTERRUPT);
		spcflags_set(SPCFLAG_CPU_HANDLE_INTERRUPT);
	}
	return true;
}

void powerpc_cpu::execute(uint32 entry)
{
	pc() = entry;
	execute_depth++;
	tinyppc.Execute();
	--execute_depth;
}

void powerpc_cpu::execute()
{
	execute(pc());
}

void powerpc_cpu::invalidate_cache_range(uintptr start, uintptr end)
{
}


/**
 *		PowerPC emulator glue with special 'sheep' opcodes
 **/

class sheepshaver_cpu
	: public powerpc_cpu
{
public:
	void execute_sheep(uint32 opcode);

	// Constructor
	sheepshaver_cpu();

	// CR & XER accessors
	uint32 get_cr() const		{ return tinyppc.cr; }
	void set_cr(uint32 v)		{ tinyppc.cr = v; }
	uint32 get_xer() const		{ return tinyppc.xer; }
	void set_xer(uint32 v)		{ tinyppc.xer = v; }
	
	// Execute NATIVE_OP routine
	void execute_native_op(uint32 native_op);
	static void call_execute_native_op(powerpc_cpu * cpu, uint32 native_op);

	// Execute EMUL_OP routine
	void execute_emul_op(uint32 emul_op);
	static void call_execute_emul_op(powerpc_cpu * cpu, uint32 emul_op);

	// Execute 68k routine
	void execute_68k(uint32 entry, M68kRegisters *r);

	// Execute ppc routine
	void execute_ppc(uint32 entry);

	// Execute MacOS/PPC code
	uint32 execute_macos_code(uint32 tvect, int nargs, uint32 const *args);

	// Resource manager thunk
	void get_resource(uint32 old_get_resource);
	static void call_get_resource(powerpc_cpu * cpu, uint32 old_get_resource);

	// Handle MacOS interrupt
	void interrupt(uint32 entry);

	// Make sure the SIGSEGV handler can access CPU registers
	friend sigsegv_return_t sigsegv_handler(sigsegv_info_t *sip);
};

sheepshaver_cpu::sheepshaver_cpu()
{
}

/*		NativeOp instruction format:
		+------------+-------------------------+--+-----------+------------+
		|      6     |                         |FN|    OP     |      2     |
		+------------+-------------------------+--+-----------+------------+
		 0         5 |6                      18 19 20      25 26        31
*/

typedef bit_field< 19, 19 > FN_field;
typedef bit_field< 20, 25 > NATIVE_OP_field;
typedef bit_field< 26, 31 > EMUL_OP_field;

void sheepshaver_cpu::call_execute_emul_op(powerpc_cpu * cpu, uint32 emul_op) {
	static_cast<sheepshaver_cpu *>(cpu)->execute_emul_op(emul_op);
}

// Execute EMUL_OP routine
void sheepshaver_cpu::execute_emul_op(uint32 emul_op)
{
	M68kRegisters r68;
	WriteMacInt32(XLM_68K_R25, gpr(25));
	WriteMacInt32(XLM_RUN_MODE, MODE_EMUL_OP);
	for (int i = 0; i < 8; i++)
		r68.d[i] = gpr(8 + i);
	for (int i = 0; i < 7; i++)
		r68.a[i] = gpr(16 + i);
	r68.a[7] = gpr(1);
	uint32 saved_cr = get_cr() & 0xff9fffff; // mask_operand::compute(11, 8)
	uint32 saved_xer = get_xer();
	EmulOp(&r68, gpr(24), emul_op);
	set_cr(saved_cr);
	set_xer(saved_xer);
	for (int i = 0; i < 8; i++)
		gpr(8 + i) = r68.d[i];
	for (int i = 0; i < 7; i++)
		gpr(16 + i) = r68.a[i];
	gpr(1) = r68.a[7];
	WriteMacInt32(XLM_RUN_MODE, MODE_68K);
}

// Execute SheepShaver instruction
void sheepshaver_cpu::execute_sheep(uint32 opcode)
{
	assert((((opcode >> 26) & 0x3f) == 6) && OP_MAX <= 64 + 3);
	switch (opcode & 0x3f) {
	case 0:		// EMUL_RETURN
		QuitEmulator();
		break;

	case 1:		// EXEC_RETURN
		spcflags_set(SPCFLAG_CPU_EXEC_RETURN);
		break;

	case 2:		// EXEC_NATIVE
		execute_native_op(NATIVE_OP_field::extract(opcode));
		if (FN_field::test(opcode))
			pc() = lr();
		else
			pc() += 4;
		break;

	default:	// EMUL_OP
		execute_emul_op(EMUL_OP_field::extract(opcode) - 3);
		pc() += 4;
		break;
	}
}

// Handle MacOS interrupt
void sheepshaver_cpu::interrupt(uint32 entry)
{
#if EMUL_TIME_STATS
	ppc_interrupt_count++;
	const clock_t interrupt_start = clock();
#endif

	// Save program counters and branch registers
	uint32 saved_pc = pc();
	uint32 saved_lr = lr();
	uint32 saved_ctr= ctr();
	uint32 saved_sp = gpr(1);

	// Initialize stack pointer to SheepShaver alternate stack base
	gpr(1) = SignalStackBase() - 64;

	// Build trampoline to return from interrupt
	SheepVar32 trampoline = POWERPC_EXEC_RETURN;

	// Prepare registers for nanokernel interrupt routine
	WriteMacInt32(KERNEL_DATA_BASE + 0x004, gpr(1));
	WriteMacInt32(KERNEL_DATA_BASE + 0x018, gpr(6));

	gpr(6) = ReadMacInt32(KERNEL_DATA_BASE + 0x65c);
	assert(gpr(6) != 0);
	WriteMacInt32(gpr(6) + 0x13c, gpr(7));
	WriteMacInt32(gpr(6) + 0x144, gpr(8));
	WriteMacInt32(gpr(6) + 0x14c, gpr(9));
	WriteMacInt32(gpr(6) + 0x154, gpr(10));
	WriteMacInt32(gpr(6) + 0x15c, gpr(11));
	WriteMacInt32(gpr(6) + 0x164, gpr(12));
	WriteMacInt32(gpr(6) + 0x16c, gpr(13));

	gpr(1)  = KernelDataAddr;
	gpr(7)  = ReadMacInt32(KERNEL_DATA_BASE + 0x660);
	gpr(8)  = 0;
	gpr(10) = trampoline.addr();
	gpr(12) = trampoline.addr();
	gpr(13) = get_cr();

	tinyppc.sheepi();
	gpr(11) = 0xf072; // MSR (SRR1)
	set_cr((gpr(11) & 0x0fff0000) | (get_cr() & ~0x0fff0000));

	// Enter nanokernel
	execute(entry);

	// Restore program counters and branch registers
	pc() = saved_pc;
	lr() = saved_lr;
	ctr()= saved_ctr;
	gpr(1) = saved_sp;

#if EMUL_TIME_STATS
	interrupt_time += (clock() - interrupt_start);
#endif
}

// Execute 68k routine
void sheepshaver_cpu::execute_68k(uint32 entry, M68kRegisters *r)
{
#if EMUL_TIME_STATS
	exec68k_count++;
	const clock_t exec68k_start = clock();
#endif

#if SAFE_EXEC_68K
	if (ReadMacInt32(XLM_RUN_MODE) != MODE_EMUL_OP)
		printf("FATAL: Execute68k() not called from EMUL_OP mode\n");
#endif

	// Save program counters and branch registers
	uint32 saved_pc = pc();
	uint32 saved_lr = lr();
	uint32 saved_ctr= ctr();
	uint32 saved_cr = get_cr();

	// Create MacOS stack frame
	// FIXME: make sure MacOS doesn't expect PPC registers to live on top
	uint32 sp = gpr(1);
	gpr(1) -= 56;
	WriteMacInt32(gpr(1), sp);

	// Save PowerPC registers
	uint32 saved_GPRs[19];
	memcpy(&saved_GPRs[0], &gpr(13), sizeof(uint32)*(32-13));
#if SAVE_FP_EXEC_68K
	double saved_FPRs[18];
	memcpy(&saved_FPRs[0], &fpr(14), sizeof(double)*(32-14));
#endif

	// Setup registers for 68k emulator
	set_cr(CR_SO_field<2>::mask());			// Supervisor mode
	for (int i = 0; i < 8; i++)					// d[0]..d[7]
	  gpr(8 + i) = r->d[i];
	for (int i = 0; i < 7; i++)					// a[0]..a[6]
	  gpr(16 + i) = r->a[i];
	gpr(23) = 0;
	gpr(24) = entry;
	gpr(25) = ReadMacInt32(XLM_68K_R25);		// MSB of SR
	gpr(26) = 0;
	gpr(28) = 0;								// VBR
	gpr(29) = ReadMacInt32(KERNEL_DATA_BASE + 0x1074);		// Pointer to opcode table
	gpr(30) = ReadMacInt32(KERNEL_DATA_BASE + 0x1078);		// Address of emulator
	gpr(31) = KernelDataAddr + 0x1000;

	// Push return address (points to EXEC_RETURN opcode) on stack
	gpr(1) -= 4;
	WriteMacInt32(gpr(1), XLM_EXEC_RETURN_OPCODE);
	
	// Rentering 68k emulator
	WriteMacInt32(XLM_RUN_MODE, MODE_68K);

	// Set r0 to 0 for 68k emulator
	gpr(0) = 0;

	// Execute 68k opcode
	uint32 opcode = ReadMacInt16(gpr(24));
	gpr(27) = (int32)(int16)ReadMacInt16(gpr(24) += 2);
	gpr(29) += opcode * 8;
	execute(gpr(29));

	// Save r25 (contains current 68k interrupt level)
	WriteMacInt32(XLM_68K_R25, gpr(25));

	// Reentering EMUL_OP mode
	WriteMacInt32(XLM_RUN_MODE, MODE_EMUL_OP);

	// Save 68k registers
	for (int i = 0; i < 8; i++)					// d[0]..d[7]
	  r->d[i] = gpr(8 + i);
	for (int i = 0; i < 7; i++)					// a[0]..a[6]
	  r->a[i] = gpr(16 + i);

	// Restore PowerPC registers
	memcpy(&gpr(13), &saved_GPRs[0], sizeof(uint32)*(32-13));
#if SAVE_FP_EXEC_68K
	memcpy(&fpr(14), &saved_FPRs[0], sizeof(double)*(32-14));
#endif

	// Cleanup stack
	gpr(1) += 56;

	// Restore program counters and branch registers
	pc() = saved_pc;
	lr() = saved_lr;
	ctr()= saved_ctr;
	set_cr(saved_cr);

#if EMUL_TIME_STATS
	exec68k_time += (clock() - exec68k_start);
#endif
}

// Call MacOS PPC code
uint32 sheepshaver_cpu::execute_macos_code(uint32 tvect, int nargs, uint32 const *args)
{
#if EMUL_TIME_STATS
	macos_exec_count++;
	const clock_t macos_exec_start = clock();
#endif

	// Save program counters and branch registers
	uint32 saved_pc = pc();
	uint32 saved_lr = lr();
	uint32 saved_ctr= ctr();

	// Build trampoline with EXEC_RETURN
	SheepVar32 trampoline = POWERPC_EXEC_RETURN;
	lr() = trampoline.addr();

	gpr(1) -= 64;								// Create stack frame
	uint32 proc = ReadMacInt32(tvect);			// Get routine address
	uint32 toc = ReadMacInt32(tvect + 4);		// Get TOC pointer

	// Save PowerPC registers
	uint32 regs[8];
	regs[0] = gpr(2);
	for (int i = 0; i < nargs; i++)
		regs[i + 1] = gpr(i + 3);

	// Prepare and call MacOS routine
	gpr(2) = toc;
	for (int i = 0; i < nargs; i++)
		gpr(i + 3) = args[i];
	execute(proc);
	uint32 retval = gpr(3);

	// Restore PowerPC registers
	for (int i = 0; i <= nargs; i++)
		gpr(i + 2) = regs[i];

	// Cleanup stack
	gpr(1) += 64;

	// Restore program counters and branch registers
	pc() = saved_pc;
	lr() = saved_lr;
	ctr()= saved_ctr;

#if EMUL_TIME_STATS
	macos_exec_time += (clock() - macos_exec_start);
#endif

	return retval;
}

// Execute ppc routine
inline void sheepshaver_cpu::execute_ppc(uint32 entry)
{
	// Save branch registers
	uint32 saved_lr = lr();

	SheepVar32 trampoline = POWERPC_EXEC_RETURN;
	WriteMacInt32(trampoline.addr(), POWERPC_EXEC_RETURN);
	lr() = trampoline.addr();

	execute(entry);

	// Restore branch registers
	lr() = saved_lr;
}

void sheepshaver_cpu::call_get_resource(powerpc_cpu * cpu, uint32 old_get_resource) {
	static_cast<sheepshaver_cpu *>(cpu)->get_resource(old_get_resource);
}

// Resource Manager thunk
inline void sheepshaver_cpu::get_resource(uint32 old_get_resource)
{
	uint32 type = gpr(3);
	int16 id = gpr(4);

	// Create stack frame
	gpr(1) -= 56;

	// Call old routine
	execute_ppc(old_get_resource);

	// Call CheckLoad()
	uint32 handle = gpr(3);
	check_load_invoc(type, id, handle);
	gpr(3) = handle;

	// Cleanup stack
	gpr(1) += 56;
}


/**
 *		SheepShaver CPU engine interface
 **/

// PowerPC CPU emulator
sheepshaver_cpu *ppc_cpu = NULL;

void execute_sheep(uint32 opcode) {
	ppc_cpu->execute_sheep(opcode);
}

void FlushCodeCache(uintptr start, uintptr end)
{
	D(bug("FlushCodeCache(%08x, %08x)\n", start, end));
	ppc_cpu->invalidate_cache_range(start, end);
}

// Dump PPC registers
static void dump_registers(void)
{
//	ppc_cpu->dump_registers();
}

// Dump log
static void dump_log(void)
{
//	ppc_cpu->dump_log();
}

static int read_mem(bfd_vma memaddr, bfd_byte *myaddr, int length, struct disassemble_info *info)
{
	Mac2Host_memcpy(myaddr, memaddr, length);
	return 0;
}

static void dump_disassembly(const uint32 pc, const int prefix_count, const int suffix_count)
{
	struct disassemble_info info;
	INIT_DISASSEMBLE_INFO(info, stderr, fprintf);
	info.read_memory_func = read_mem;

	const int count = prefix_count + suffix_count + 1;
	const uint32 base_addr = pc - prefix_count * 4;
	for (int i = 0; i < count; i++) {
		const bfd_vma addr = base_addr + i * 4;
		fprintf(stderr, "%s0x%8llx:  ", addr == pc ? " >" : "  ", addr);
		print_insn_ppc(addr, &info);
		fprintf(stderr, "\n");
	}
}

sigsegv_return_t sigsegv_handler(sigsegv_info_t *sip)
{
#if ENABLE_VOSF
	// Handle screen fault
	extern bool Screen_fault_handler(sigsegv_info_t *sip);
	if (Screen_fault_handler(sip))
		return SIGSEGV_RETURN_SUCCESS;
#endif

	const uintptr addr = (uintptr)sigsegv_get_fault_address(sip);
#if HAVE_SIGSEGV_SKIP_INSTRUCTION
	// Ignore writes to ROM
	if ((addr - (uintptr)ROMBaseHost) < ROM_SIZE)
		return SIGSEGV_RETURN_SKIP_INSTRUCTION;

	// Get program counter of target CPU
	sheepshaver_cpu * const cpu = ppc_cpu;
	const uint32 pc = cpu->pc();
	
	// Fault in Mac ROM or RAM?
	bool mac_fault = (pc >= ROMBase && pc < (ROMBase + ROM_AREA_SIZE)) || (pc >= RAMBase && pc < (RAMBase + RAMSize)) || (pc >= DR_CACHE_BASE && pc < (DR_CACHE_BASE + DR_CACHE_SIZE));
	if (mac_fault) {

		// "VM settings" during MacOS 8 installation
		if (pc == ROMBase + 0x488160 && cpu->gpr(20) == 0xf8000000)
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
	
		// MacOS 8.5 installation
		else if (pc == ROMBase + 0x488140 && cpu->gpr(16) == 0xf8000000)
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
	
		// MacOS 8 serial drivers on startup
		else if (pc == ROMBase + 0x48e080 && (cpu->gpr(8) == 0xf3012002 || cpu->gpr(8) == 0xf3012000))
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
	
		// MacOS 8.1 serial drivers on startup
		else if (pc == ROMBase + 0x48c5e0 && (cpu->gpr(20) == 0xf3012002 || cpu->gpr(20) == 0xf3012000))
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
		else if (pc == ROMBase + 0x4a10a0 && (cpu->gpr(20) == 0xf3012002 || cpu->gpr(20) == 0xf3012000))
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
	
		// MacOS 8.6 serial drivers on startup (with DR Cache and OldWorld ROM)
		else if ((pc - DR_CACHE_BASE) < DR_CACHE_SIZE && (cpu->gpr(16) == 0xf3012002 || cpu->gpr(16) == 0xf3012000))
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
		else if ((pc - DR_CACHE_BASE) < DR_CACHE_SIZE && (cpu->gpr(20) == 0xf3012002 || cpu->gpr(20) == 0xf3012000))
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;

		// Ignore writes to the zero page
		else if ((uint32)(addr - SheepMem::ZeroPage()) < (uint32)SheepMem::PageSize())
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;

		// Ignore all other faults, if requested
		if (PrefsFindBool("ignoresegv"))
			return SIGSEGV_RETURN_SKIP_INSTRUCTION;
		ppc_cpu->tinyppc.StopTrace();
	}
#else
#error "FIXME: You don't have the capability to skip instruction within signal handlers"
#endif

	fprintf(stderr, "SIGSEGV\n");
	fprintf(stderr, "  pc %p\n", sigsegv_get_fault_instruction_address(sip));
	fprintf(stderr, "  ea %p\n", sigsegv_get_fault_address(sip));
	dump_registers();
	dump_log();
	dump_disassembly(pc, 8, 8);

	enter_mon();
	QuitEmulator();

	return SIGSEGV_RETURN_FAILURE;
}

/*
 *  Initialize CPU emulation
 */

void init_emul_ppc(void)
{
	// Get pointer to KernelData in host address space
	kernel_data = (KernelData *)Mac2HostAddr(KERNEL_DATA_BASE);

	// Initialize main CPU emulator
	ppc_cpu = new sheepshaver_cpu();
	ppc_cpu->Reset();
	WriteMacInt32(XLM_RUN_MODE, MODE_68K);
}

/*
 *  Deinitialize emulation
 */

void exit_emul_ppc(void)
{
	delete ppc_cpu;
	ppc_cpu = NULL;
}

/*
 *  Emulation loop
 */

void emul_ppc(uint32 entry)
{
	// start emulation loop and enable code translation or caching
	ppc_cpu->execute(entry);
}

/*
 *  Handle PowerPC interrupt
 */

void TriggerInterrupt(void)
{
	idle_resume();
  // Trigger interrupt to main cpu only
  if (ppc_cpu)
	  ppc_cpu->trigger_interrupt();
}

void HandleInterrupt(RegTmp *r)
{
#ifdef USE_SDL_VIDEO
	// We must fill in the events queue in the same thread that did call SDL_SetVideoMode()
	SDL_PumpEvents();
#endif

	// Do nothing if interrupts are disabled
	if (int32(ReadMacInt32(XLM_IRQ_NEST)) > 0)
		return;

	// Update interrupt count
#if EMUL_TIME_STATS
	interrupt_count++;
#endif

	// Interrupt action depends on current run mode
	switch (ReadMacInt32(XLM_RUN_MODE)) {
	case MODE_68K:
		// 68k emulator active, trigger 68k interrupt level 1
		WriteMacInt16(ReadMacInt32(KERNEL_DATA_BASE + 0x67c), 1);
		r->cr |= ReadMacInt32(KERNEL_DATA_BASE + 0x674);
		break;
    
#if INTERRUPTS_IN_NATIVE_MODE
	case MODE_NATIVE:
		// 68k emulator inactive, in nanokernel?
		if (r->gpr[1] != KernelDataAddr) {

			// Prepare for 68k interrupt level 1
			WriteMacInt16(ReadMacInt32(KERNEL_DATA_BASE + 0x67c), 1);
			WriteMacInt32(ReadMacInt32(KERNEL_DATA_BASE + 0x658) + 0xdc,
						  ReadMacInt32(ReadMacInt32(KERNEL_DATA_BASE + 0x658) + 0xdc)
						  | ReadMacInt32(KERNEL_DATA_BASE + 0x674));
      
			// Execute nanokernel interrupt routine (this will activate the 68k emulator)
			DisableInterrupt();
			if (ROMType == ROMTYPE_NEWWORLD)
				ppc_cpu->interrupt(ROMBase + 0x312b1c);
			else
				ppc_cpu->interrupt(ROMBase + 0x312a3c);
		}
		break;
#endif
    
#if INTERRUPTS_IN_EMUL_OP_MODE
	case MODE_EMUL_OP:
		// 68k emulator active, within EMUL_OP routine, execute 68k interrupt routine directly when interrupt level is 0
		if ((ReadMacInt32(XLM_68K_R25) & 7) == 0) {
#if EMUL_TIME_STATS
			const clock_t interrupt_start = clock();
#endif
#if 1
			// Execute full 68k interrupt routine
			M68kRegisters r;
			uint32 old_r25 = ReadMacInt32(XLM_68K_R25);	// Save interrupt level
			WriteMacInt32(XLM_68K_R25, 0x21);			// Execute with interrupt level 1
			static const uint8 proc_template[] = {
				0x3f, 0x3c, 0x00, 0x00,			// move.w	#$0000,-(sp)	(fake format word)
				0x48, 0x7a, 0x00, 0x0a,			// pea		@1(pc)			(return address)
				0x40, 0xe7,						// move		sr,-(sp)		(saved SR)
				0x20, 0x78, 0x00, 0x064,		// move.l	$64,a0
				0x4e, 0xd0,						// jmp		(a0)
				M68K_RTS >> 8, M68K_RTS & 0xff	// @1
			};
			BUILD_SHEEPSHAVER_PROCEDURE(proc);
			Execute68k(proc, &r);
			WriteMacInt32(XLM_68K_R25, old_r25);		// Restore interrupt level
#else
			// Only update cursor
			if (HasMacStarted()) {
				if (InterruptFlags & INTFLAG_VIA) {
					ClearInterruptFlag(INTFLAG_VIA);
					ADBInterrupt();
					ExecuteNative(NATIVE_VIDEO_VBL);
				}
			}
#endif
#if EMUL_TIME_STATS
			interrupt_time += (clock() - interrupt_start);
#endif
		}
		break;
#endif
	}
}

void sheepshaver_cpu::call_execute_native_op(powerpc_cpu * cpu, uint32 selector) {
	static_cast<sheepshaver_cpu *>(cpu)->execute_native_op(selector);
}

// Execute NATIVE_OP routine
void sheepshaver_cpu::execute_native_op(uint32 selector)
{
#if EMUL_TIME_STATS
	native_exec_count++;
	const clock_t native_exec_start = clock();
#endif

	switch (selector) {
	case NATIVE_PATCH_NAME_REGISTRY:
		DoPatchNameRegistry();
		break;
	case NATIVE_VIDEO_INSTALL_ACCEL:
		VideoInstallAccel();
		break;
	case NATIVE_VIDEO_VBL:
		VideoVBL();
		break;
	case NATIVE_VIDEO_DO_DRIVER_IO:
		gpr(3) = (int32)(int16)VideoDoDriverIO(gpr(3), gpr(4), gpr(5), gpr(6), gpr(7));
		break;
	case NATIVE_ETHER_AO_GET_HWADDR:
		AO_get_ethernet_address(gpr(3));
		break;
	case NATIVE_ETHER_AO_ADD_MULTI:
		AO_enable_multicast(gpr(3));
		break;
	case NATIVE_ETHER_AO_DEL_MULTI:
		AO_disable_multicast(gpr(3));
		break;
	case NATIVE_ETHER_AO_SEND_PACKET:
		AO_transmit_packet(gpr(3));
		break;
	case NATIVE_ETHER_IRQ:
		EtherIRQ();
		break;
	case NATIVE_ETHER_INIT:
		gpr(3) = InitStreamModule((void *)gpr(3));
		break;
	case NATIVE_ETHER_TERM:
		TerminateStreamModule();
		break;
	case NATIVE_ETHER_OPEN:
		gpr(3) = ether_open((queue_t *)gpr(3), (void *)gpr(4), gpr(5), gpr(6), (void*)gpr(7));
		break;
	case NATIVE_ETHER_CLOSE:
		gpr(3) = ether_close((queue_t *)gpr(3), gpr(4), (void *)gpr(5));
		break;
	case NATIVE_ETHER_WPUT:
		gpr(3) = ether_wput((queue_t *)gpr(3), (mblk_t *)gpr(4));
		break;
	case NATIVE_ETHER_RSRV:
		gpr(3) = ether_rsrv((queue_t *)gpr(3));
		break;
	case NATIVE_NQD_SYNC_HOOK:
		gpr(3) = NQD_sync_hook(gpr(3));
		break;
	case NATIVE_NQD_UNKNOWN_HOOK:
		gpr(3) = NQD_unknown_hook(gpr(3));
		break;
	case NATIVE_NQD_BITBLT_HOOK:
		gpr(3) = NQD_bitblt_hook(gpr(3));
		break;
	case NATIVE_NQD_BITBLT:
		NQD_bitblt(gpr(3));
		break;
	case NATIVE_NQD_FILLRECT_HOOK:
		gpr(3) = NQD_fillrect_hook(gpr(3));
		break;
	case NATIVE_NQD_INVRECT:
		NQD_invrect(gpr(3));
		break;
	case NATIVE_NQD_FILLRECT:
		NQD_fillrect(gpr(3));
		break;
	case NATIVE_SERIAL_NOTHING:
	case NATIVE_SERIAL_OPEN:
	case NATIVE_SERIAL_PRIME_IN:
	case NATIVE_SERIAL_PRIME_OUT:
	case NATIVE_SERIAL_CONTROL:
	case NATIVE_SERIAL_STATUS:
	case NATIVE_SERIAL_CLOSE: {
		typedef int16 (*SerialCallback)(uint32, uint32);
		static const SerialCallback serial_callbacks[] = {
			SerialNothing,
			SerialOpen,
			SerialPrimeIn,
			SerialPrimeOut,
			SerialControl,
			SerialStatus,
			SerialClose
		};
		gpr(3) = serial_callbacks[selector - NATIVE_SERIAL_NOTHING](gpr(3), gpr(4));
		break;
	}
	case NATIVE_GET_RESOURCE:
		get_resource(ReadMacInt32(XLM_GET_RESOURCE));
		break;
	case NATIVE_GET_1_RESOURCE:
		get_resource(ReadMacInt32(XLM_GET_1_RESOURCE));
		break;
	case NATIVE_GET_IND_RESOURCE:
		get_resource(ReadMacInt32(XLM_GET_IND_RESOURCE));
		break;
	case NATIVE_GET_1_IND_RESOURCE:
		get_resource(ReadMacInt32(XLM_GET_1_IND_RESOURCE));
		break;
	case NATIVE_R_GET_RESOURCE:
		get_resource(ReadMacInt32(XLM_R_GET_RESOURCE));
		break;
	case NATIVE_MAKE_EXECUTABLE:
		MakeExecutable(0, gpr(4), gpr(5));
		break;
	case NATIVE_CHECK_LOAD_INVOC:
		check_load_invoc(gpr(3), gpr(4), gpr(5));
		break;
	case NATIVE_NAMED_CHECK_LOAD_INVOC:
		named_check_load_invoc(gpr(3), gpr(4), gpr(5));
		break;
	default:
		printf("FATAL: NATIVE_OP called with bogus selector %d\n", selector);
		QuitEmulator();
		break;
	}

#if EMUL_TIME_STATS
	native_exec_time += (clock() - native_exec_start);
#endif
}

/*
 *  Execute 68k subroutine (must be ended with EXEC_RETURN)
 *  This must only be called by the emul_thread when in EMUL_OP mode
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68k(uint32 pc, M68kRegisters *r)
{
	ppc_cpu->execute_68k(pc, r);
}

/*
 *  Execute 68k A-Trap from EMUL_OP routine
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68kTrap(uint16 trap, M68kRegisters *r)
{
	SheepVar proc_var(4);
	uint32 proc = proc_var.addr();
	WriteMacInt16(proc, trap);
	WriteMacInt16(proc + 2, M68K_RTS);
	Execute68k(proc, r);
}

/*
 *  Call MacOS PPC code
 */

uint32 call_macos(uint32 tvect)
{
	return ppc_cpu->execute_macos_code(tvect, 0, NULL);
}

uint32 call_macos1(uint32 tvect, uint32 arg1)
{
	const uint32 args[] = { arg1 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos2(uint32 tvect, uint32 arg1, uint32 arg2)
{
	const uint32 args[] = { arg1, arg2 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos3(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3)
{
	const uint32 args[] = { arg1, arg2, arg3 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos4(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos5(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4, arg5 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos6(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos7(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6, uint32 arg7)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
	return ppc_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}
