// Tiny68020
// Copyright 2021-2025 © Yasuo Kuwahara
// MIT License

#include "Tiny68020.h"
#include <cstring>

// ---- for BasiliskII

#include "sysdeps.h"
#include "spcflags.h"
extern int quit_program;
extern uint32_t ROMBaseMac;
int m68k_do_specialties(void);
void m68k_execute(void);
void m68k_emulop(uint32_t);
void m68k_emulop_return(void);

// ---- end

#define P(x)			(cnv.pmf = &Tiny68020::x, cnv.p)
#define PI(x, i)		(cnv.pmf = &Tiny68020::x<i>, cnv.p)
#define PIS(x, i, s)	(cnv.pmf = &Tiny68020::x<i, s>, cnv.p)
#define PM(i, j, s)		(cnv.pmf = &Tiny68020::move_ea_ea<i, j, s>, cnv.p)

#define MODE(op, mask, x) {\
a(op | 0x00, mask | 0x38, PI(x, 0));\
a(op | 0x08, mask | 0x38, PI(x, 1));\
a(op | 0x10, mask | 0x38, PI(x, 2));\
a(op | 0x18, mask | 0x38, PI(x, 3));\
a(op | 0x20, mask | 0x38, PI(x, 4));\
a(op | 0x28, mask | 0x38, PI(x, 5));\
a(op | 0x30, mask | 0x38, PI(x, 6));\
a(op | 0x38, mask | 0x3f, PI(x, 8));\
a(op | 0x39, mask | 0x3f, PI(x, 9));\
a(op | 0x3a, mask | 0x3f, PI(x, 10));\
a(op | 0x3b, mask | 0x3f, PI(x, 11));\
a(op | 0x3c, mask | 0x3f, PI(x, 12));\
a(op | 0x1f, mask | 0x3f, PI(x, 13));\
a(op | 0x27, mask | 0x3f, PI(x, 14));\
}
#define MODES(op, mask, x, s) {\
a(op | 0x00, mask | 0x38, PIS(x, 0, s));\
a(op | 0x08, mask | 0x38, PIS(x, 1, s));\
a(op | 0x10, mask | 0x38, PIS(x, 2, s));\
a(op | 0x18, mask | 0x38, PIS(x, 3, s));\
a(op | 0x20, mask | 0x38, PIS(x, 4, s));\
a(op | 0x28, mask | 0x38, PIS(x, 5, s));\
a(op | 0x30, mask | 0x38, PIS(x, 6, s));\
a(op | 0x38, mask | 0x3f, PIS(x, 8, s));\
a(op | 0x39, mask | 0x3f, PIS(x, 9, s));\
a(op | 0x3a, mask | 0x3f, PIS(x, 10, s));\
a(op | 0x3b, mask | 0x3f, PIS(x, 11, s));\
a(op | 0x3c, mask | 0x3f, PIS(x, 12, s));\
a(op | 0x1f, mask | 0x3f, PIS(x, 13, s));\
a(op | 0x27, mask | 0x3f, PIS(x, 14, s));\
}
#define S3(op, mask, x) {\
a(op | 0x00, mask, PI(x, 0));\
a(op | 0x40, mask, PI(x, 1));\
a(op | 0x80, mask, PI(x, 2));\
}
#define S03(op, mask, x) {\
a(op | 0x00, mask, PIS(x, 0, 0));\
a(op | 0x40, mask, PIS(x, 0, 1));\
a(op | 0x80, mask, PIS(x, 0, 2));\
}
#define IS3(op, mask, x) {\
MODES(op | 0x00, mask, x, 0);\
MODES(op | 0x40, mask, x, 1);\
MODES(op | 0x80, mask, x, 2);\
}
#define ME(op, mask, i, s) {\
a(op | 0x00, mask | 0x38, PM(0, i, s));\
a(op | 0x08, mask | 0x38, PM(1, i, s));\
a(op | 0x10, mask | 0x38, PM(2, i, s));\
a(op | 0x18, mask | 0x38, PM(3, i, s));\
a(op | 0x20, mask | 0x38, PM(4, i, s));\
a(op | 0x28, mask | 0x38, PM(5, i, s));\
a(op | 0x30, mask | 0x38, PM(6, i, s));\
a(op | 0x38, mask | 0x3f, PM(8, i, s));\
a(op | 0x39, mask | 0x3f, PM(9, i, s));\
a(op | 0x3a, mask | 0x3f, PM(10, i, s));\
a(op | 0x3b, mask | 0x3f, PM(11, i, s));\
a(op | 0x3c, mask | 0x3f, PM(12, i, s));\
a(op | 0x1f, mask | 0x3f, PM(13, i, s));\
a(op | 0x27, mask | 0x3f, PM(14, i, s));\
}
#define MEE(op, s) {\
ME(op | 0x0000, 0xf1c0, 0, s);\
ME(op | 0x0040, 0xf1c0, 1, s);\
ME(op | 0x0080, 0xf1c0, 2, s);\
ME(op | 0x00c0, 0xf1c0, 3, s);\
ME(op | 0x0100, 0xf1c0, 4, s);\
ME(op | 0x0140, 0xf1c0, 5, s);\
ME(op | 0x0180, 0xf1c0, 6, s);\
ME(op | 0x01c0, 0xffc0, 8, s);\
ME(op | 0x03c0, 0xffc0, 9, s);\
ME(op | 0x05c0, 0xffc0, 10, s);\
ME(op | 0x07c0, 0xffc0, 11, s);\
ME(op | 0x09c0, 0xffc0, 12, s);\
ME(op | 0x0ec0, 0xffc0, 13, s);\
ME(op | 0x0f00, 0xffc0, 14, s);\
}
#define CC(op, mask, x) {\
a(op | 0x000, mask, PI(x, 0));\
a(op | 0x100, mask, PI(x, 1));\
a(op | 0x200, mask, PI(x, 2));\
a(op | 0x300, mask, PI(x, 3));\
a(op | 0x400, mask, PI(x, 4));\
a(op | 0x500, mask, PI(x, 5));\
a(op | 0x600, mask, PI(x, 6));\
a(op | 0x700, mask, PI(x, 7));\
a(op | 0x800, mask, PI(x, 8));\
a(op | 0x900, mask, PI(x, 9));\
a(op | 0xa00, mask, PI(x, 10));\
a(op | 0xb00, mask, PI(x, 11));\
a(op | 0xc00, mask, PI(x, 12));\
a(op | 0xd00, mask, PI(x, 13));\
a(op | 0xe00, mask, PI(x, 14));\
a(op | 0xf00, mask, PI(x, 15));\
}
#define BCC(op, mask, s) {\
a(op | 0x000, mask, PIS(bcc, 0, s));\
a(op | 0x100, mask, PIS(bcc, 1, s));\
a(op | 0x200, mask, PIS(bcc, 2, s));\
a(op | 0x300, mask, PIS(bcc, 3, s));\
a(op | 0x400, mask, PIS(bcc, 4, s));\
a(op | 0x500, mask, PIS(bcc, 5, s));\
a(op | 0x600, mask, PIS(bcc, 6, s));\
a(op | 0x700, mask, PIS(bcc, 7, s));\
a(op | 0x800, mask, PIS(bcc, 8, s));\
a(op | 0x900, mask, PIS(bcc, 9, s));\
a(op | 0xa00, mask, PIS(bcc, 10, s));\
a(op | 0xb00, mask, PIS(bcc, 11, s));\
a(op | 0xc00, mask, PIS(bcc, 12, s));\
a(op | 0xd00, mask, PIS(bcc, 13, s));\
a(op | 0xe00, mask, PIS(bcc, 14, s));\
a(op | 0xf00, mask, PIS(bcc, 15, s));\
}

static struct Insn {
	using pmf_t = void (Tiny68020::*)(uint16_t);
	using pf_t = void (*)(Tiny68020 *, uint16_t);
	Insn() {
		union { pmf_t pmf; pf_t p; } cnv; // not portable
		for (int i = 0; i < 0x10000; i++) fn[i] = P(undef);
		IS3(0x0000, 0xffc0, ori_ea);
		a(0x003c, 0xffff, P(ori_ccr));
		a(0x007c, 0xffff, P(ori_sr));
		a(0x00c0, 0xf9c0, P(x00c0));
		MODE(0x0100, 0xf1c0, btst);
		MODE(0x0140, 0xf1c0, bchg);
		MODE(0x0180, 0xf1c0, bclr);
		MODE(0x01c0, 0xf1c0, bset);
		a(0x0108, 0xf138, P(movep)); // overwrite bitop
		IS3(0x0200, 0xffc0, andi_ea);
		a(0x023c, 0xffff, P(andi_ccr));
		a(0x027c, 0xffff, P(andi_sr));
		IS3(0x0400, 0xffc0, subi);
		IS3(0x0600, 0xffc0, addi);
		MODE(0x0800, 0xffc0, btst_i);
		MODE(0x0840, 0xffc0, bchg_i);
		MODE(0x0880, 0xffc0, bclr_i);
		a(0x08c0, 0xf9c0, P(x08c0));
		MODE(0x08c0, 0xffc0, bset_i); // overwrite x08c0
		MODES(0x0ac0, 0xffc0, cas, 0); // overwrite x08c0
		MODES(0x0cc0, 0xffc0, cas, 1); // overwrite x08c0
		MODES(0x0ec0, 0xffc0, cas, 2); // overwrite x08c0
		IS3(0x0a00, 0xffc0, eori_ea);
		a(0x0a3c, 0xffff, P(eori_ccr));
		a(0x0a7c, 0xffff, P(eori_sr));
		IS3(0x0c00, 0xffc0, cmpi);
		MEE(0x1000, 0);
		MEE(0x2000, 2);
		MODE(0x2040, 0xf1c0, movea_l);
		MEE(0x3000, 1);
		MODE(0x3040, 0xf1c0, movea_w);
		IS3(0x4000, 0xffc0, negx);
		MODE(0x40c0, 0xffc0, move_sr_ea);
		MODE(0x4100, 0xf1c0, chk_l);
		MODE(0x4180, 0xf1c0, chk_w);
		MODE(0x41c0, 0xf1c0, lea);
		IS3(0x4200, 0xffc0, clr);
		MODE(0x42c0, 0xffc0, move_ccr_ea);
		IS3(0x4400, 0xffc0, neg);
		MODE(0x44c0, 0xffc0, move_ea_ccr);
		IS3(0x4600, 0xffc0, _not);
		MODE(0x46c0, 0xffc0, move_ea_sr);
		MODE(0x4800, 0xffc0, nbcd);
		a(0x4808, 0xfff8, P(link_l)); // overwrite nbcd
		MODE(0x4840, 0xffc0, pea);
		a(0x4840, 0xfff8, P(swap)); // overwrite pea
		a(0x4848, 0xfff8, P(bkpt)); // overwrite pea
		MODES(0x4880, 0xffc0, movem_rm, 1);
		a(0x4880, 0xfff8, P(ext_w)); // overwrite movem_rm
		MODES(0x48c0, 0xffc0, movem_rm, 2);
		a(0x48c0, 0xfff8, P(ext_l));
		a(0x49c0, 0xfff8, P(ext_bl));
		IS3(0x4a00, 0xffc0, tst);
		MODE(0x4ac0, 0xffc0, tas);
		MODE(0x4c00, 0xffc0, mul_l);
		MODE(0x4c40, 0xffc0, div_l);
		MODES(0x4c80, 0xffc0, movem_mr, 1);
		MODES(0x4cc0, 0xffc0, movem_mr, 2);
		a(0x4e40, 0xfff0, P(trap));
		a(0x4e50, 0xfff8, P(link_w));
		a(0x4e58, 0xfff8, P(unlk));
		a(0x4e68, 0xfff8, P(move_usp_an));
		a(0x4e60, 0xfff8, P(move_an_usp));
		a(0x4e70, 0xffff, P(reset));
		a(0x4e71, 0xffff, P(nop));
		a(0x4e73, 0xffff, P(rte));
		a(0x4e74, 0xffff, P(rtd));
		a(0x4e75, 0xffff, P(rts));
		a(0x4e76, 0xffff, P(trapv));
		a(0x4e77, 0xffff, P(rtr));
		a(0x4e7a, 0xfffe, P(movec));
		MODE(0x4e80, 0xffc0, jsr);
		MODE(0x4ec0, 0xffc0, jmp);
		IS3(0x5000, 0xf1c0, addq);
		a(0x5008, 0xf138, P(addqa));
		IS3(0x5100, 0xf1c0, subq);
		a(0x5108, 0xf138, P(subq_a));
		MODES(0x50c0, 0xffc0, scc, 0);
		MODES(0x51c0, 0xffc0, scc, 1);
		MODES(0x52c0, 0xffc0, scc, 2);
		MODES(0x53c0, 0xffc0, scc, 3);
		MODES(0x54c0, 0xffc0, scc, 4);
		MODES(0x55c0, 0xffc0, scc, 5);
		MODES(0x56c0, 0xffc0, scc, 6);
		MODES(0x57c0, 0xffc0, scc, 7);
		MODES(0x58c0, 0xffc0, scc, 8);
		MODES(0x59c0, 0xffc0, scc, 9);
		MODES(0x5ac0, 0xffc0, scc, 10);
		MODES(0x5bc0, 0xffc0, scc, 11);
		MODES(0x5cc0, 0xffc0, scc, 12);
		MODES(0x5dc0, 0xffc0, scc, 13);
		MODES(0x5ec0, 0xffc0, scc, 14);
		MODES(0x5fc0, 0xffc0, scc, 15);
		CC(0x50c8, 0xfff8, dbcc); // overwrite scc
		CC(0x50fa, 0xfffe, trapcc);
		CC(0x50fc, 0xffff, trapcc);
		BCC(0x6000, 0xff00, 0);
		BCC(0x6000, 0xffff, 2);
		BCC(0x60ff, 0xffff, 4);
		a(0x6100, 0xff00, PI(bsr, 0)); // overwrite bcc
		a(0x6100, 0xffff, PI(bsr, 2)); // overwrite bcc
		a(0x61ff, 0xffff, PI(bsr, 4)); // overwrite bcc
		a(0x7000, 0xf100, P(moveq));
		a(0x7100, 0xf100, P(emulop)); // BasiliskII
		IS3(0x8000, 0xf1c0, or_ea_d);
		MODE(0x80c0, 0xf1c0, divu_w);
		IS3(0x8100, 0xf1c0, or_d_ea);
		a(0x8100, 0xf1f0, P(sbcd_r)); // overwrite or_d_ea
		a(0x8108, 0xf1f8, P(sbcd_m)); // overwrite or_d_ea
		MODE(0x81c0, 0xf1c0, divs_w);
		IS3(0x9000, 0xf1c0, sub_ea_d);
		MODE(0x90c0, 0xf1c0, suba_w);
		IS3(0x9100, 0xf1c0, sub_d_ea);
		S3(0x9100, 0xf1f8, subx_r); // overwrite sub_d_ea
		S3(0x9108, 0xf1f8, subx_m); // overwrite sub_d_ea
		MODE(0x91c0, 0xf1c0, suba_l);
		a(0xa000, 0xf000, P(a_line));
		IS3(0xb000, 0xf1c0, cmp);
		MODE(0xb0c0, 0xf1c0, cmpa_w);
		IS3(0xb100, 0xf1c0, eor_d_ea);
		S3(0xb108, 0xf1f8, cmpm); // overwrite eor_d_ea
		MODE(0xb1c0, 0xf1c0, cmpa_l);
		IS3(0xc000, 0xf1c0, and_ea_d);
		MODE(0xc0c0, 0xf1c0, mulu_w);
		IS3(0xc100, 0xf1c0, and_d_ea);
		a(0xc140, 0xf1f8, P(exg_dd));
		a(0xc148, 0xf1f8, P(exg_aa));
		a(0xc188, 0xf1f8, P(exg_da));
		a(0xc100, 0xf1f8, P(abcd_r)); // overwrite and_d_ea
		a(0xc108, 0xf1f8, P(abcd_m)); // overwrite and_d_ea
		MODE(0xc1c0, 0xf1c0, muls_w);
		IS3(0xd000, 0xf1c0, add_ea_d);
		MODE(0xd0c0, 0xf1c0, adda_w);
		IS3(0xd100, 0xf1c0, add_d_ea);
		MODE(0xd1c0, 0xf1c0, adda_l);
		S3(0xd100, 0xf1f8, addx_r); // overwrite add_d_ea
		S3(0xd108, 0xf1f8, addx_m); // overwrite add_d_ea
		S03(0xe000, 0xf1d8, asr);
		S03(0xe008, 0xf1d8, lsr);
		S03(0xe010, 0xf1d8, roxr);
		S03(0xe018, 0xf1d8, ror);
		S03(0xe100, 0xf1d8, asl);
		S03(0xe108, 0xf1d8, lsl);
		S03(0xe110, 0xf1d8, roxl);
		S03(0xe118, 0xf1d8, rol);
		MODES(0xe0c0, 0xf7c0, asr, 3);
		MODES(0xe2c0, 0xf7c0, lsr, 3);
		MODES(0xe4c0, 0xf7c0, roxr, 3);
		MODES(0xe6c0, 0xf7c0, ror, 3);
		MODES(0xe1c0, 0xf7c0, asl, 3);
		MODES(0xe3c0, 0xf7c0, lsl, 3);
		MODES(0xe5c0, 0xf7c0, roxl, 3);
		MODES(0xe7c0, 0xf7c0, rol, 3);
		MODE(0xe8c0, 0xf8c0, bitfield);
		a(0xf000, 0xf000, P(f_line));
		a(0xf280, 0xffff, P(xf280)); // for booting KT7.5.3
	}
	void a(uint16_t op, uint16_t mask, pf_t f) {
		int lim = (op & 0xf000) + 0x1000;
		for (int i = op & 0xf000; i < lim; i++)
			if ((i & mask) == op) fn[i] = f;
	}
	static void exec1(Tiny68020 *mpu, uint16_t op) { fn[op](mpu, op); }
	static inline pf_t fn[0x10000];
} insn;

Tiny68020::Tiny68020() : m(nullptr) {
#if TINY68020_TRACE
	memset(tracebuf, 0, sizeof(tracebuf));
	tracep = tracebuf;
#endif
}

void Tiny68020::Reset() {
	memset(d, 0, sizeof(d));
	memset(a, 0, sizeof(a));
	memset(cr, 0, sizeof(cr));
	sr = MS | MI;
	trace_pc = 0;
	// BasiliskII
	a[7] = 0x2000;
	pc = ROMBaseMac + 0x2a;
	SPCFLAGS_INIT(0);
}

// RW: 0...address only 1...read 2...write 3...modify
// M: 0-6:0-6 7:none 8:7-0 9:7-1 10:7-2 11:7-3 12:7-4 13:3,REG7 14:4,REG7 15:none
// S: 0...byte 1...word 2...long
template<int RW, int M, int S, typename F> Tiny68020::u32 Tiny68020::ea(int reg, F func) {
	auto exmode = [&](u32 base) {
		u16 ext = fetch2();
		u32 t;
		if constexpr (MPU_TYPE >= 68020) {
			auto idx = [&]{
				u32 o = (ext & 0x8000 ? a : d)[ext >> 12 & 7];
				return (ext & 0x800 ? o : (s16)o) << (ext >> 9 & 3);
			};
			auto disp = [&](int sw)->u32 {
				switch (sw & 3) {
					case 2: return fetch2();
					case 3: return fetch4();
				}
				return 0;
			};
			if (ext & 0x100) { // full extension
				t = ext & 0x80 ? 0 : base; // BS
				if (ext & 0x40) // IS=1
					t = ext & 7 ? get4(t + disp(ext >> 4)) + disp(ext) : t + disp(ext >> 4) + disp(ext);
				else { // IS=0
					t += disp(ext >> 4);
					t = (ext & 7 ? ext & 4 ? get4(t) + idx() : get4(t + idx()) : t + idx()) + disp(ext);
				}
			}
			else t = base + idx() + (s8)ext; // brief extension
		}
		else {
			t = (ext & 0x8000 ? a : d)[ext >> 12 & 7];
			t = ext & 0x800 ? t : (s16)t;
			t += base + (s8)ext;
		}
		return t;
	};
	u32 adr = 0, data = 0;
	reg &= 7;
	if constexpr (M == 2) adr = a[reg];
	if constexpr (M == 3 || M == 13) { // (An)+
		adr = a[reg];
		if constexpr (RW != 0) {
			if constexpr (M == 13 && S == 0) a[reg] += 2;
			else a[reg] += 1 << S;
		}
	}
	if constexpr (M == 4 || M == 14) { // -(An)
		if constexpr (RW != 0)
			if constexpr (M == 14 && S == 0) adr = a[reg] -= 2;
			else adr = a[reg] -= 1 << S;
		else adr = a[reg];
	}
	if constexpr (M == 5) adr = a[reg] + fetch2(); // (d16,An)
	if constexpr (M == 6) adr = exmode(a[reg]);
	if constexpr (M == 8) adr = fetch2(); // (abs).W
	if constexpr (M == 9) adr = fetch4(); // (abs).L
	if constexpr (M == 10) { adr = pc; adr += fetch2(); } // (d16,PC)
	if constexpr (M == 11) adr = exmode(pc); // (d8,PC,Xn)
//
	if constexpr (RW & 1) {
		if constexpr (M == 0) data = d[reg]; // Dn
		else if constexpr (M == 1) data = a[reg]; // An
		else if constexpr (M == 12) data = getImm<S>(); // #imm
		else data = ld<S>(adr);
	}
	if constexpr (RW == 1) func(data);
	if constexpr (RW == 2) data = func();
	if constexpr (RW == 3) data = func(data);
	if constexpr ((RW & 2) == 2) {
		if constexpr (M == 0) stD<S>(reg, data); // dst Dn
		else if constexpr (M == 1) stA(reg, data); // dst An
		else st<S>(adr, data);
	}
	return adr;
}

template<int M, int S> void Tiny68020::movem_rm(u16 op) {
	const u32 ofs = S << 1, list = fetch2();
	u32 i, adr = ea<0, M, S>(op, []{});
	if constexpr (M == 4 || M == 14) {
		for (i = 0; i < 8; i++)
			if (list & 1 << i) {
				/*if constexpr (MPU_TYPE >= 68020) { a[R0] = adr -= ofs; st<S>(adr, a[7 - i]); }
				else */st<S>(adr -= ofs, a[7 - i]);
			}
		for (i = 0; i < 8; i++)
			if (list & 0x100 << i) st<S>(adr -= ofs, d[7 - i]);
		a[R0] = adr;
	}
	else {
		for (i = 0; i < 8; i++)
			if (list & 1 << i) { st<S>(adr, d[i]); adr += ofs; }
		for (i = 0; i < 8; i++)
			if (list & 0x100 << i) { st<S>(adr, a[i]); adr += ofs; }
	}
}

template<int M, int S> void Tiny68020::movem_mr(u16 op) {
	const u32 ofs = S << 1, list = fetch2();
	u32 i, adr = ea<0, M, S>(op, []{});
	if constexpr (S == 1) {
		for (i = 0; i < 8; i++)
			if (list & 1 << i) { stD<2>(i, (s16)ld2(adr)); adr += ofs; }
		for (i = 0; i < 8; i++)
			if (list & 0x100 << i) { stA(i, (s16)ld2(adr)); adr += ofs; };
	}
	else {
		for (i = 0; i < 8; i++)
			if (list & 1 << i) { stD<2>(i, ld4(adr)); adr += ofs; };
		for (i = 0; i < 8; i++)
			if (list & 0x100 << i) { stA(i, ld4(adr)); adr += ofs; };
	}
	if constexpr (M == 3 || M == 13) a[R0] = adr; // write back if (An)+
}

void Tiny68020::movec(u16 op) {
	if (!(sr & MS)) Trap(8);
	u16 t = fetch2(), x = t & 0xfff;
	if (x >= 8) x -= 0x7f8;
	if (x < 16) {
		if (op & 1) {
			cr[x] = (t & 0x8000 ? a : d)[t >> 12 & 7];
			if (x == 2) cr[2] &= 0x80008000;
		}
		else {
			cr[sr & MM ? 11 : 12] = a[7];
			(t & 0x8000 ? a : d)[t >> 12 & 7] = cr[x];
		}
	}
}

void Tiny68020::movep(u16 op) {
	u32 t, adr = a[R0] + fetch2();
	switch (op & 0xc0) {
		case 0: // movep.w (d16,Ay),Dx
			t = ld1(adr) << 8;
			stD<1>(R9, t | ld1(adr + 2));
			break;
		case 0x40: // movep.l (d16,Ay),Dx
			t = ld1(adr) << 24;
			t |= ld1(adr + 2) << 16;
			t |= ld1(adr + 4) << 8;
			stD<2>(R9, t | ld1(adr + 6));
			break;
		case 0x80: // movep.w Dx,(d16,Ay)
			t = d[R9];
			st1(adr, t >> 8);
			st1(adr + 2, t);
			break;
		default: // movep.l Dx,(d16,Ay)
			t = d[R9];
			st1(adr, t >> 24);
			st1(adr + 2, t >> 16);
			st1(adr + 4, t >> 8);
			st1(adr + 6, t);
			break;
	}
}

template<int M, typename T32, typename T64> void Tiny68020::mul_l(u16 op, u16 op2) {
	ea<1, M, 2>(op, [&](T32 v) {
		T64 t = (T32)d[op2 >> 12 & 7] * (T64)v;
		if (op2 & 0x400) fmul<3>(t);
		else fmul<2, T32>(t);
		stD<2>(op2 >> 12 & 7, t);
		if (op2 & 0x400) stD<2>(op2 & 7, t >> 32);
	});
}

template<int M, typename T16, typename T32> void Tiny68020::div_w(u16 op) {
	ea<1, M, 1>(op, [&](T16 v) {
		if (v) {
			T32 r = d[R9], q = r / v;
			if (q == (T16)q) stD<2>(R9, flogic<1>(r % v << 16 | (q & 0xffff)));
			else fset<V1>();
		}
		else { fset<C0>(); Trap(5); }
	});
}

template<int M, typename T32, typename T64> void Tiny68020::div_l(u16 op, u16 op2) {
	ea<1, M, 2>(op, [&](T32 v) {
		if (v) {
			T64 r = op2 & 0x400 ? d[op2 >> 12 & 7] | (T64)d[op2 & 7] << 32 : (T32)d[op2 >> 12 & 7], q = r / v;
			if (q == (T32)q) {
				stD<2>(op2 & 7, r % v);
				stD<2>(op2 >> 12 & 7, flogic<2>((T32)q));
			}
			else fset<V1>();
		}
		else { fset<C0>(); Trap(5); }
	});
}

template<int W, int M, typename F> void Tiny68020::bitop(u16 op, F func) {
	u32 m;
	if constexpr ((W & 2) != 0) m = d[R9];
	else m = fetch2();
	if constexpr (!M) m = 1 << (m & 0x1f);
	else m = 1 << (m & 7);
	if constexpr (W & 1) { ea<3, M, M ? 0 : 2>(op, [&](u32 v) { fbtst<M ? 0 : 2>(v & m); return func(v, m); }); }
	else ea<1, M, M ? 0 : 2>(op, [&](u32 v) { fbtst<M ? 0 : 2>(v & m); });
}

template<int M> void Tiny68020::bitfield(u16 op) { // M68000PRM.pdf page 1-29
	u32 i, adr = 0, t = fetch2(), width = ((t & 0x20 ? d[t & 7] : t) - 1 & 0x1f) + 1, bofs = 0x40 - width;
	s32 ofs = t & 0x800 ? d[t >> 6 & 7] : t >> 6 & 0x1f;
	u64 data;
	if constexpr (M != 0) {
		adr = ea<0, M, 0>(op, []{}) + (ofs >> 3);
		data = (u64)ld4(adr) << 32 | ld4(adr + 4);
		bofs -= ofs & 7;
	}
	else {
		data = (u64)d[R0] << 32;
		bofs -= ofs & 0x1f;
	}
	auto ext = [&]{ return fbf(u32(data >> bofs & (1LL << width) - 1), width); };
	auto mask = [&]{ return ((1LL << width) - 1) << bofs; };
	switch (op & 0x700) {
		default: ext(); return; // bftst
		case 0x100: stD<2>(t >> 12 & 7, ext()); return; // bfextu
		case 0x200: ext(); data ^= mask(); break; // bfchg
		case 0x300: stD<2>(t >> 12 & 7, s32(ext() << (32 - width)) >> (32 - width)); return; // bfexts
		case 0x400: ext(); data &= ~mask(); break; // bfclr
		case 0x500: // bfffo
			i = 0;
			for (u32 d1 = ext(), m1 =  1 << (width - 1); i < width && !(d1 & m1); i++, m1 >>= 1)
				;
			d[t >> 12 & 7] = ofs + i;
			return;
		case 0x600: ext(); data |= mask(); break; // bfset
		case 0x700: data = (data & ~mask()) | ((u64)fbf(d[t >> 12 & 7], width) << bofs & mask()); break; // bfins
	}
	if constexpr (M != 0) {
		st4(adr, data >> 32);
		st4(adr + 4, (u32)data);
	}
	else d[R0] = data >> 32;
}

template <int M, int S, int DM, typename F> void Tiny68020::sftrot(u16 op, F func) {
	if constexpr (S < 3) { // register
		if (u32 v = d[R0], s = op & 0x20 ? d[R9] & 0x3f : ((op >> 9) - 1 & 7) + 1; s) {
			int sop = op >> (S < 3 ? 3 : 9) & 3;
			if constexpr (S == 0) v = sop ? (u8)v : (s8)v;
			if constexpr (S == 1) v = sop ? (u16)v : (s16)v;
			s = sop & 2 ? sop == 2 ? s % ((8 << S) + 1) : s & (8 << S) - 1 : s > bits(S) ? bits(S) : s;
			stD<S>(R0, fset<DM, S>((u32)func(v, s), s, v));
		}
		else fset<NS | ZS | V0 | C0, S>(v);
	}
	else ea<3, M, 1>(op, [&](u32 v) { return fset<DM, 1>((u32)func(v, 1), 1, v); }); // memory
}

template<int W, int M> void Tiny68020::bcd(u16 op) {
	auto add = [&](u8 x, u8 y) {
		s16 l = (y & 0xf) + (x & 0xf) + X(), cl = l >= 0xa, u = (y & 0xf0) + (x & 0xf0) + (cl << 4), cu = u >= 0xa0;
		return fbcd(u - (cu ? 0xa0 : 0) + l - (cl ? 0xa : 0), cu);
	};
	auto sub = [&](u8 x, u8 y) {
		s16 l = (y & 0xf) - (x & 0xf) - X(), cl = l < 0, u = (y & 0xf0) - (x & 0xf0) - (cl << 4), cu = u < 0;
		return fbcd(u + (cu ? 0xa0 : 0) + l + (cl ? 0xa : 0), cu);
	};
	if constexpr (W == 2) ea<3, M, 0>(op, [&](u8 v) { return sub(v, 0); }); // nbcd
	else { // abcd / sbcd
		u8 r, x, y;
		if constexpr (!M) { x = d[R9]; y = d[R0]; }
		else { x = ld1(--a[R9]); y = ld1(--a[R0]); }
		if constexpr (!W) r = add(x, y);
		else r = sub(x, y);
		if constexpr (!M) stD<0>(R9, r);
		else st1(a[R9], r);
	}
}

template <int C, int S> void Tiny68020::bcc(u16 op) {
	if (cond<C>()) {
		u32 t = pc;
		if constexpr (S == 2) t += fetch2();
		else if constexpr (S == 4) t += fetch4();
		else t += (s8)op;
		pc = t;
	}
	else pc += S;
}

template <int S> void Tiny68020::bsr(u16 op) {
	u32 t = pc;
	if constexpr (S == 2) t += fetch2();
	else if constexpr (S == 4) t += fetch4();
	else t += (s8)op;
	push4(pc);
	pc = t;
}

template<int C> void Tiny68020::dbcc(u16 op) {
	if (!cond<C>()) {
		s16 s = d[R0] - 1;
		stD<1>(R0, s);
		pc += s != -1 ? fetch2() - 2 : 2;
	}
	else pc += 2;
}

void Tiny68020::rte(u16) {
	u32 t = pop2();
	pc = pop4();
	SetSR(t);
	if constexpr (MPU_TYPE >= 68020)
		if ((pop2() & 0xf000) == 0x2000) a[7] += 4;
}

template <int M, int S> void Tiny68020::cas(u16 op) {
	u16 t = fetch2();
	ea<1, M, S>(op, [&](u32 v) {
		if (fcmp<S>(v - d[t & 7], d[t & 7], v)) stD<S>(t & 7, v);
		else ea<2, M, S>(op, [&]{ return d[t >> 6 & 7]; });
	});
}

void Tiny68020::SetSR(u16 data, bool perm) {
	if (!perm && !(sr & MS) && data & MS) Trap(8);
	cr[sr & MS ? sr & MM ? CR_MSP : CR_ISP : CR_USP] = a[7];
	a[7] = cr[data & MS ? data & MM ? CR_MSP : CR_ISP : CR_USP];
	sr = data & 0xff1f;
	if (data & MT) { trace_pc = pc; SPCFLAGS_SET(SPCFLAG_TRACE); } // BasiliskII
}

void Tiny68020::Trap(u32 vector, u32 param) {
	if (vector >= 0x100) {
		fprintf(stderr, "ignored trap vector: %d\n", vector);
		return;
	}
	u16 sr0 = sr;
	SetSR((sr0 & ~MT) | MS, true);
	if constexpr (MPU_TYPE >= 68020) {
		if (vector >= 5 && vector <= 9) {
			push4(param);
			push2(0x2000 | (vector << 2 & 0xffc));
		}
		else push2(vector << 2 & 0xffc);
	}
	push4(pc);
	push2(sr0);
	pc = get4(cr[CR_VBR] + (vector << 2));
	if (!pc) {
		fprintf(stderr, "undefined trap: %d\n", vector);
		exit(1);
	}
}

void Tiny68020::Execute() {
	do {
#if TINY68020_TRACE
		tracep->pc = pc;
		tracep->index = 0;
#endif
		Insn::exec1(this, fetch2());
#if TINY68020_TRACE
		tracep->ccr = sr;
#if TINY68020_TRACE > 1
		if (++tracep >= tracebuf + TRACEMAX - 1) StopTrace();
#else
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
#endif
#endif
	} while (!spcflags || !m68k_do_specialties()); // BasiliskII
}

template<int C> int Tiny68020::cond() {
	if constexpr (C == 1) return 0;
	if constexpr (C == 2) return !(sr & (MC | MZ));
	if constexpr (C == 3) return sr & (MC | MZ);
	if constexpr (C == 4) return !(sr & MC);
	if constexpr (C == 5) return sr & MC;
	if constexpr (C == 6) return !(sr & MZ);
	if constexpr (C == 7) return sr & MZ;
	if constexpr (C == 8) return !(sr & MV);
	if constexpr (C == 9) return sr & MV;
	if constexpr (C == 10) return !(sr & MN);
	if constexpr (C == 11) return sr & MN;
	if constexpr (C == 12) return ~(sr ^ sr << (LN - LV)) & MN;
	if constexpr (C == 13) return (sr ^ sr << (LN - LV)) & MN;
	if constexpr (C == 14) return ~(sr << (LN - LZ) | (sr ^ sr << (LN - LV))) & MN;
	if constexpr (C == 15) return (sr << (LN - LZ) | (sr ^ sr << (LN - LV))) & MN;
	return 1;
};

// M68000PRM.pdf page 3-18

#define BITS	(8 << S)
#define MSB_N	(BITS - 1)
#define MXC		(MX | MC)

template<int DM, int S> Tiny68020::u32 Tiny68020::fset(u32 r, u32 s, u32 d) {
	if constexpr ((DM & 0xf) == C0) sr &= ~MC;
	if constexpr ((DM & 0xf) == CSUB) sr = ((s & ~d) | (r & ~d) | (s & r)) >> MSB_N & 1 ? sr | MC : sr & ~MC;
	if constexpr ((DM & 0xf) == CSL) sr = s && (d >> (BITS - s) & 1) ? sr | MC : sr & ~MC;
	if constexpr ((DM & 0xf) == CSR) sr = s && (d >> (s - 1) & 1) ? sr | MC : sr & ~MC;
	if constexpr ((DM & 0xf0) == V0) sr &= ~MV;
	if constexpr ((DM & 0xf0) == V1) sr |= MV;
	if constexpr ((DM & 0xf0) == VADD) sr = ((s & d & ~r) | (~s & ~d & r)) >> MSB_N & 1 ? sr | MV : sr & ~MV;
	if constexpr ((DM & 0xf0) == VSUB) sr = ((~s & d & ~r) | (s & ~d & r)) >> MSB_N & 1 ? sr | MV : sr & ~MV;
	if constexpr ((DM & 0xf0) == VSL) sr = (r >> MSB_N & 1 ? ~r : r) >> (BITS - s) ? sr | MV : sr & ~MV;
	if constexpr ((DM & 0xf00) == Z0) sr &= ~MZ;
	if constexpr ((DM & 0xf00) == ZS) sr = r & u32((1LL << BITS) - 1) ? sr & ~MZ : sr | MZ;
	if constexpr ((DM & 0xf00) == ZSX) sr = r & u32((1LL << BITS) - 1) ? sr & ~MZ : sr;
	if constexpr ((DM & 0xf00) == ZBF) sr = r & (1LL << s) - 1 ? sr & ~MZ : sr | MZ;
	if constexpr ((DM & 0xf000) == N0) sr &= ~MN;
	if constexpr ((DM & 0xf000) == NS) sr = r >> MSB_N & 1 ? sr | MN : sr & ~MN;
	if constexpr ((DM & 0xf000) == NBF) sr = r >> (s - 1) & 1 ? sr | MN : sr & ~MN;
	if constexpr ((DM & 0xf0000) == XADD) sr = ((s & d) | (~r & d) | (s & ~r)) >> MSB_N & 1 ? sr | MXC : sr & ~MXC;
	if constexpr ((DM & 0xf0000) == XSUB) sr = ((s & ~d) | (r & ~d) | (s & r)) >> MSB_N & 1 ? sr | MXC : sr & ~MXC;
	if constexpr ((DM & 0xf0000) == XSL) sr = s ? d >> (BITS - s) & 1 ? sr | MXC : sr & ~MXC : sr & ~MC;
	if constexpr ((DM & 0xf0000) == XXL) sr = s ? d >> (BITS - s) & 1 ? sr | MXC : sr & ~MXC : sr & MX ? sr | MC : sr & ~MC;
	if constexpr ((DM & 0xf0000) == XSR) sr = s ? d >> (s - 1) & 1 ? sr | MXC : sr & ~MXC : sr & ~MC;
	if constexpr ((DM & 0xf0000) == XXR) sr = s ? d >> (s - 1) & 1 ? sr | MXC : sr & ~MXC : sr & MX ? sr | MC : sr & ~MC;
	if constexpr ((DM & 0xf0000) == XBCD) sr = s ? sr | MXC : sr & ~MXC;
	return r;
}

template<int S, typename T> Tiny68020::u32 Tiny68020::fmul(u64 r) {
	sr &= ~MC;
	sr = r ? sr & ~MZ : sr | MZ;
	if constexpr (!std::is_same_v<T, void>) sr = r != (T)r ? sr | MV : sr & ~MV;
	sr = r >> MSB_N & 1 ? sr | MN : sr & ~MN;
	return (u32)r;
}

#if TINY68020_TRACE
#include <string>
void Tiny68020::StopTrace() {
	TraceBuffer *endp = tracep;
	int i = 0;
	FILE *fo;
	if (!(fo = fopen((std::string(getenv("HOME")) + "/Desktop/trace.txt").c_str(), "w"))) exit(1);
	do {
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
		fprintf(fo, "%4d %08x ", i++, tracep->pc);
		fprintf(fo, "%04x ", __builtin_bswap16((u16 &)m[tracep->pc]));
		fprintf(fo, "%c%c%c%c%c ",
				tracep->ccr & 0x10 ? 'X' : '-',
				tracep->ccr & 0x08 ? 'N' : '-',
				tracep->ccr & 0x04 ? 'Z' : '-',
				tracep->ccr & 0x02 ? 'V' : '-',
				tracep->ccr & 0x01 ? 'C' : '-');
		for (Acs *p = tracep->acs; p < tracep->acs + tracep->index; p++) {
			switch (p->type) {
				case acsLoad8:
					fprintf(fo, "L %08x %02x ", p->adr, p->data & 0xff);
					break;
				case acsLoad16:
					fprintf(fo, "L %08x %04x ", p->adr, p->data & 0xffff);
					break;
				case acsLoad32:
					fprintf(fo, "L %08x %08x ", p->adr, p->data);
					break;
				case acsStore8:
					fprintf(fo, "S %08x %02x ", p->adr, p->data & 0xff);
					break;
				case acsStore16:
					fprintf(fo, "S %08x %04x ", p->adr, p->data & 0xffff);
					break;
				case acsStore32:
					fprintf(fo, "S %08x %08x ", p->adr, p->data);
					break;
				case acsStoreD:
					fprintf(fo, "%08x->D%d ", p->data, p->adr);
					break;
				case acsStoreA:
					fprintf(fo, "%08x->A%d ", p->data, p->adr);
					break;
			}
		}
		fprintf(fo, "\n");
	} while (tracep != endp);
	fclose(fo);
	fprintf(stderr, "trace dumped.\n");
	exit(1);
}
#endif

void Tiny68020::undef(u16 op) {
	fprintf(stderr, "undefined instruction: PC=%06x OP=%04x\n", pc - 2, op);
#if TINY68020_TRACE
	StopTrace();
#endif
	exit(1);
}

// ---- for BasiliskII

void Tiny68020::importRegs(M68kRegisters &r) {
	memcpy(d, r.d, sizeof(d));
	memcpy(a, r.a, sizeof(a));
	SetSR(r.sr);
}

void Tiny68020::exportRegs(M68kRegisters &r) {
	memcpy(r.d, d, sizeof(d));
	memcpy(r.a, a, sizeof(a));
	r.sr = sr;
}

void Tiny68020::emulop(u16 op) {
	if (op & 0xff) m68k_emulop(op);
	else m68k_emulop_return();
}

void Tiny68020::execsub(u32 v, M68kRegisters &r, bool isTrap) {
	u32 oldpc = pc;
	memcpy(d, r.d, sizeof(d));
	memcpy(a, r.a, sizeof(a) - sizeof(a[0]));
	push2(0x7100);
	if (isTrap) push2(v);
	else push4(a[7]);
	SetPC(isTrap ? a[7] : v);
	quit_program = 0;
	m68k_execute();
	a[7] += isTrap ? 4 : 2;
	pc = oldpc;
	memcpy(r.d, d, sizeof(d));
	memcpy(r.a, a, sizeof(a) - sizeof(a[0]));
	quit_program = 0;
}

// ---- end
