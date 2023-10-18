// TinyPPC
// Copyright 2023 © Yasuo Kuwahara
// MIT License

#include "TinyPPC.h"
#include "sysdeps.h"		// SheepShaver
#include "spcflags.hpp"		// SheepShaver
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

#define P(x)			(cnv.pmf = &TinyPPC::x, cnv.p)
#define PI(x, i)		(cnv.pmf = &TinyPPC::x<i>, cnv.p)
#define Rc(o, s, x, i) {\
a(o, s, 0, 0x7ff, PI(x, i));\
a(o, s, 1, 0x7ff, PI(x, i | 1));\
}
#define RcR(o, x, i) {\
a(o, 0, 0, 1, PI(x, i));\
a(o, 0, 1, 1, PI(x, i | 1));\
}
#define RcF(o, s, x, i) {\
a(o, s, 0, 0x3f, PI(x, i));\
a(o, s, 1, 0x3f, PI(x, i | 1));\
}
#define OERc(o, s, x, i) {\
a(o, s, 0, 0x7ff, PI(x, i));\
a(o, s, 1, 0x7ff, PI(x, i | 1));\
a(o, s | 0x200, 0, 0x7ff, PI(x, i | 2));\
a(o, s | 0x200, 1, 0x7ff, PI(x, i | 3));\
}

static struct Insn {
	using pmf_t = void (TinyPPC::*)(uint32_t);
	using pf_t = void (*)(TinyPPC *, uint32_t);
	Insn() {
		union { pmf_t pmf; pf_t p; } cnv; // not portable
		for (int i = 0; i < 0x20000; i++) fn[i] = P(undef);
		OERc(31, 266, addsub, 0x40); // add[o][.]
		OERc(31, 10, addsub, 0x44); // addc[o][.]
		OERc(31, 138, addsub, 0x64); // addc[o][.]
		a(14, 0, 0, 0, PI(addsub, 0x280)); // addi
		a(12, 0, 0, 0, PI(addsub, 0x84)); // addic
		a(13, 0, 0, 0, PI(addsub, 0x85)); // addic.
		a(15, 0, 0, 0, PI(addsub, 0x2c0)); // addis
		OERc(31, 234, addsub, 0x2c); // addme[o][.]
		OERc(31, 202, addsub, 0x24); // addze[o][.]
		OERc(31, 40, addsub, 0x150); // subf[o][.]
		OERc(31, 8, addsub, 0x154); // subfc[o][.]
		OERc(31, 136, addsub, 0x164); // subfe[o][.]
		a(8, 0, 0, 0, PI(addsub, 0x194)); // subfic
		OERc(31, 232, addsub, 0x12c); // subfme[o][.]
		OERc(31, 200, addsub, 0x124); // subfze[o][.]
		OERc(31, 104, addsub, 0x110); // neg[o][.]
		Rc(31, 75, mul, 0); // mulhw
		Rc(31, 11, mul, 4); // mulhwu
		a(7, 0, 0, 0, PI(mul, 8)); // mulli
		OERc(31, 235, mul, 12); // mullw
		OERc(31, 491, divw, 0);
		OERc(31, 459, divwu, 4);
		a(31, 0, 0, 0x7fe, PI(cmp, 0)); // cmp
		a(11, 0, 0, 0, PI(cmp, 1)); // cmpi
		a(31, 32, 0, 0x7fe, PI(cmpl, 0)); // cmpl
		a(10, 0, 0, 0, PI(cmpl, 1)); // cmpli
		Rc(31, 28, logic, 0); // and[.]
		Rc(31, 60, logic, 0x10); // andc[.]
		a(28, 0, 0, 0, PI(logic, 0x21)); // andi.
		a(29, 0, 0, 0, PI(logic, 0x31)); // andis.
		Rc(31, 284, logic, 0xa); // eqv[.]
		Rc(31, 476, logic, 2); // nand[.]
		Rc(31, 124, logic, 6); // nor[.]
		Rc(31, 444, logic, 4); // or[.]
		Rc(31, 412, logic, 0x14); // orc[.]
		a(24, 0, 0, 0, PI(logic, 0x24)); // ori
		a(25, 0, 0, 0, PI(logic, 0x34)); // oris
		Rc(31, 316, logic, 8); // xor[.]
		a(26, 0, 0, 0, PI(logic, 0x28)); // xori
		a(27, 0, 0, 0, PI(logic, 0x38)); // xoris
		Rc(31, 26, cntlzw, 0);
		Rc(31, 954, extsb, 0);
		Rc(31, 922, extsh, 0);
		RcR(20, sftrot, 0xac); // rlwimi[.]
		RcR(21, sftrot, 0xa8); // rlwinm[.]
		RcR(23, sftrot, 0x2a8); // rlwnm[.]
		Rc(31, 24, sftrot, 0x380); // slw[.]
		Rc(31, 792, sftrot, 0x342); // sraw[.]
		Rc(31, 824, sftrot, 0x42); // srawi[.]
		Rc(31, 536, sftrot, 0x300); // srw[.]
		Rc(63, 264, farith, 0xb2); // fabs[.]
		RcF(63, 21, farith, 0x20); // fadd[.]
		RcF(59, 21, farith, 0x24); // fadds[.]
		Rc(63, 14, farith, 0xf0); // fctiw[.]
		Rc(63, 15, farith, 0xe0); // fctiwz[.]
		RcF(63, 18, farith, 0x50); // fdiv[.]
		RcF(59, 18, farith, 0x54); // fdivs[.]
		RcF(63, 29, farith, 0x60); // fmadd[.]
		RcF(59, 29, farith, 0x64); // fmadds[.]
		Rc(63, 72, farith, 0x12); // fmr[.]
		RcF(63, 28, farith, 0x70); // fmsub[.]
		RcF(59, 28, farith, 0x74); // fmsubs[.]
		RcF(63, 25, farith, 0x40); // fmul[.]
		RcF(59, 25, farith, 0x44); // fmuls[.]
		Rc(63, 136, farith, 0xc2); // fnabs[.]
		Rc(63, 40, farith, 0xd2); // fneg[.]
		RcF(63, 31, farith, 0x68); // fnmadd[.]
		RcF(59, 31, farith, 0x6c); // fnmadds[.]
		RcF(63, 30, farith, 0x78); // fnmsub[.]
		RcF(59, 30, farith, 0x7c); // fnmsubs[.]
		RcF(63, 24, farith, 0x80); // fres[.]
		Rc(63, 12, farith, 0x14); // frsp[.]
		RcF(63, 26, farith, 0x90); // frsqrte[.]
		RcF(63, 23, farith, 0xa2); // fsel[.]
		RcF(63, 22, farith, 0x90); // fsqrt[.]
		RcF(59, 22, farith, 0x94); // fsqrts[.]
		RcF(63, 20, farith, 0x30); // fsub[.]
		RcF(59, 20, farith, 0x34); // fsubs[.]
		a(63, 32, 0, 0x7fe, PI(farith, 0)); // fcmpo
		a(63, 0, 0, 0x7fe, PI(farith, 0)); // fcmpu
		a(63, 64, 0, 0x7fe, P(mcrfs));
		Rc(63, 583, mffs, 0); // mffs[.]
		Rc(63, 70, mtfs, 4); // mtfsb0[.]
		Rc(63, 38, mtfs, 2); // mtfsb1[.]
		Rc(63, 711, mtfs, 0x16); // mtfsf[.]
		Rc(63, 134, mtfs, 0x26); // mtfsfi[.]
		a(34, 0, 0, 0, PI(loadstore, 0x80)); // lbz
		a(35, 0, 0, 0, PI(loadstore, 0x20)); // lbzu
		a(31, 119, 0, 0x7fe, PI(loadstore, 0x60)); // lbzux
		a(31, 87, 0, 0x7fe, PI(loadstore, 0xc0)); // lbzx
		a(50, 0, 0, 0, PI(loadstore, 0x8b)); // lfd
		a(51, 0, 0, 0, PI(loadstore, 0x2b)); // lfdu
		a(31, 631, 0, 0x7fe, PI(loadstore, 0x6b)); // lfdux
		a(31, 599, 0, 0x7fe, PI(loadstore, 0xcb)); // lfdx
		a(48, 0, 0, 0, PI(loadstore, 0x8a)); // lfs
		a(49, 0, 0, 0, PI(loadstore, 0x2a)); // lfsu
		a(31, 567, 0, 0x7fe, PI(loadstore, 0x6a)); // lfsux
		a(31, 535, 0, 0x7fe, PI(loadstore, 0xca)); // lfsx
		a(42, 0, 0, 0, PI(loadstore, 0x89)); // lha
		a(43, 0, 0, 0, PI(loadstore, 0x29)); // lhau
		a(31, 375, 0, 0x7fe, PI(loadstore, 0x69)); // lhaux
		a(31, 343, 0, 0x7fe, PI(loadstore, 0xc9)); // lhax
		a(31, 790, 0, 0x7fe, PI(loadstore, 0xc5)); // lhbrx
		a(40, 0, 0, 0, PI(loadstore, 0x81)); // lhz
		a(41, 0, 0, 0, PI(loadstore, 0x21)); // lhzu
		a(31, 311, 0, 0x7fe, PI(loadstore, 0x61)); // lhzux
		a(31, 279, 0, 0x7fe, PI(loadstore, 0xc1)); // lhzx
		a(46, 0, 0, 0, PI(loadstore, 0x8e)); // lmw
		a(31, 597, 0, 0x7fe, PI(loadstore, 0x88)); // lswi
		a(31, 533, 0, 0x7fe, PI(loadstore, 0xcc)); // lswx
		a(31, 20, 0, 0x7fe, PI(loadstore, 0xcf)); // lwarx
		a(31, 534, 0, 0x7fe, PI(loadstore, 0xc6)); // lwbrx
		a(32, 0, 0, 0, PI(loadstore, 0x82)); // lwz
		a(33, 0, 0, 0, PI(loadstore, 0x22)); // lwzu
		a(31, 55, 0, 0x7fe, PI(loadstore, 0x62)); // lwzux
		a(31, 23, 0, 0x7fe, PI(loadstore, 0xc2)); // lwzx
		a(38, 0, 0, 0, PI(loadstore, 0x90)); // stb
		a(39, 0, 0, 0, PI(loadstore, 0x30)); // stbu
		a(31, 247, 0, 0x7fe, PI(loadstore, 0x70)); // stbux
		a(31, 215, 0, 0x7fe, PI(loadstore, 0xd0)); // stbx
		a(54, 0, 0, 0, PI(loadstore, 0x9b)); // stfd
		a(55, 0, 0, 0, PI(loadstore, 0x3b)); // stfdu
		a(31, 759, 0, 0x7fe, PI(loadstore, 0x7b)); // stfdux
		a(31, 727, 0, 0x7fe, PI(loadstore, 0xdb)); // stfdx
		a(31, 983, 0, 0x7fe, PI(loadstore, 0xdd)); // stfiwx
		a(52, 0, 0, 0, PI(loadstore, 0x9a)); // stfs
		a(53, 0, 0, 0, PI(loadstore, 0x3a)); // stfsu
		a(31, 695, 0, 0x7fe, PI(loadstore, 0x7a)); // stfsux
		a(31, 663, 0, 0x7fe, PI(loadstore, 0xda)); // stfsx
		a(44, 0, 0, 0, PI(loadstore, 0x91)); // sth
		a(31, 918, 0, 0x7fe, PI(loadstore, 0xd5)); // sthbrx
		a(45, 0, 0, 0, PI(loadstore, 0x31)); // sthu
		a(31, 439, 0, 0x7fe, PI(loadstore, 0x71)); // sthux
		a(31, 407, 0, 0x7fe, PI(loadstore, 0xd1)); // sthx
		a(47, 0, 0, 0, PI(loadstore, 0x9e)); // stmw
		a(31, 725, 0, 0x7fe, PI(loadstore, 0x98)); // stswi
		a(31, 661, 0, 0x7fe, PI(loadstore, 0xdc)); // stswx
		a(36, 0, 0, 0, PI(loadstore, 0x92)); // stw
		a(31, 662, 0, 0x7fe, PI(loadstore, 0xd6)); // stwbrx
		a(31, 150, 1, 0x7ff, PI(loadstore, 0xdf)); // stwcx.
		a(37, 0, 0, 0, PI(loadstore, 0x32)); // stwu
		a(31, 183, 0, 0x7fe, PI(loadstore, 0x72)); // stwux
		a(31, 151, 0, 0x7fe, PI(loadstore, 0xd2)); // stwx
		a(18, 0, 0, 3, PI(branch, 0)); // b
		a(18, 0, 1, 3, PI(branch, 1)); // bl
		a(18, 1, 0, 3, PI(branch, 2)); // ba
		a(18, 1, 1, 3, PI(branch, 3)); // bla
		a(16, 0, 0, 3, PI(branch, 4)); // bc
		a(16, 0, 1, 3, PI(branch, 5)); // bcl
		a(16, 1, 0, 3, PI(branch, 6)); // bca
		a(16, 1, 1, 3, PI(branch, 7)); // bcla
		a(19, 528, 0, 0x7ff, PI(branch, 8)); // bcctr
		a(19, 528, 1, 0x7ff, PI(branch, 9)); // bcctrl
		a(19, 16, 0, 0x7ff, PI(branch, 12)); // bclr
		a(19, 16, 1, 0x7ff, PI(branch, 13)); // bclrl
		a(19, 257, 0, 0x7fe, PI(cond, 0)); // crand
		a(19, 129, 0, 0x7fe, PI(cond, 8)); // crandc
		a(19, 289, 0, 0x7fe, PI(cond, 5)); // creqv
		a(19, 225, 0, 0x7fe, PI(cond, 1)); // crnand
		a(19, 33, 0, 0x7fe, PI(cond, 3)); // crnor
		a(19, 449, 0, 0x7fe, PI(cond, 2)); // cror
		a(19, 417, 0, 0x7fe, PI(cond, 10)); // crorc
		a(19, 193, 0, 0x7fe, PI(cond, 4)); // crxor
		a(19, 0, 0, 0x7fe, P(mcrf));
		a(31, 512, 0, 0x7fe, P(mcrxr));
		a(31, 19, 0, 0x7fe, P(mfcr));
		a(31, 339, 0, 0x7fe, P(mfspr));
		a(31, 371, 0, 0x7fe, P(mftb));
		a(31, 144, 0, 0x7fe, P(mtcrf));
		a(31, 467, 0, 0x7fe, P(mtspr));
		a(31, 854, 0, 0x7fe, P(nop)); // eieio
		a(19, 150, 0, 0x7fe, P(nop)); // isync
		a(31, 598, 0, 0x7fe, P(nop)); // sync
		a(31, 982, 0, 0x7fe, P(nop)); // icbi
		a(31, 758, 0, 0x7fe, P(nop)); // dcba
		a(31, 86, 0, 0x7fe, P(nop)); // dcbf
		a(31, 470, 0, 0x7fe, P(nop)); // dcbi
		a(31, 54, 0, 0x7fe, P(nop)); // dcbst
		a(31, 278, 0, 0x7fe, P(nop)); // dcbt
		a(31, 246, 0, 0x7fe, P(nop)); // dcbtst
		a(31, 1014, 0, 0x7fe, P(dcbz)); // dcbz
		/*
		a(31, 370, 0, 0x7fe, P(nop)); // tlbia
		a(31, 306, 0, 0x7fe, P(nop)); // tlbie
		a(31, 978, 0, 0x7ff, P(nop)); // tlbld
		a(31, 1010, 0, 0x7ff, P(nop)); // tlbll
		a(31, 566, 0, 0x7fe, P(nop)); // tlbsync
		a(31, 310, 0, 0x7fe, P(nop)); // eciwx
		a(31, 438, 0, 0x7fe, P(nop)); // ecowx
		a(31, 83, 0, 0x7fe, P(nop)); // mfmsr
		a(31, 595, 0, 0x7fe, P(nop)); // mfsr
		a(31, 659, 0, 0x7fe, P(nop)); // mfsrin
		a(31, 146, 0, 0x7fe, P(nop)); // mtmsr
		a(31, 210, 0, 0x7fe, P(nop)); // mtsr
		a(31, 242, 0, 0x7fe, P(nop)); // mtsrin
		a(19, 50, 0, 0x7fe, P(nop)); // rfi
		a(17, 1, 0, 2, P(nop)); // sc
		a(31, 4, 0, 0x7fe, P(nop)); // tw
		a(3, 0, 0, 0, P(nop)); // twi
		*/
		a(6, 0, 0, 0, P(sheep)); // SheepShaver
	}
	void a(int opcd, int sop, int lsb, int mask, pf_t f) {
		int start = opcd << 11 & 0x1f800;
		int op = start | (sop << 1 & 0x7fe) | (lsb & 1);
		int lim = start + 0x800;
		mask |= 0x1f800;
		for (int i = start; i < lim; i++)
			if ((i & mask) == op) fn[i] = f;
	}
	static void exec1(TinyPPC *p, uint32_t op) { fn[op >> 26 << 11 | (op & 0x7ff)](p, op); }
	static inline pf_t fn[0x20000];
} insn;

static constexpr struct DigitMask {
	constexpr DigitMask() : m() {
		for (int i = 0; i < 256; i++) {
			uint32_t t = 0;
			for (int j = 0x80; j; j >>= 1) {
				t <<= 4;
				if (i & j) t |= 0xf;
			}
			m[i] = t;
		}
	}
	uint32_t m[256];
} digitmask;

TinyPPC::TinyPPC() {
#if TINYPPC_TRACE
	memset(tracebuf, 0, sizeof(tracebuf));
	tracep = tracebuf;
#endif
}

void TinyPPC::Reset() {
	memset(gpr, 0, sizeof(gpr));
	memset(fpr, 0, sizeof(fpr));
	lr = ctr = cr = xer = fpscr = reserve_adr = 0;
	reserve = false;
	// SheepShaver
	extern u32 ROMBase, KernelDataAddr;
	pc = ROMBase + 0x310000;
	gpr[3] = ROMBase + 0x30d000;
	gpr[4] = KernelDataAddr + 0x1000;
	spcflags_init();
}

template<int M> void TinyPPC::addsub(u32 op) {
	u32 a, b;
	if constexpr ((M & 0x200) != 0) a = rAz(op);
	else a = rA(op);
	if constexpr ((M & 0x100) != 0) a = ~a;
	if constexpr ((M & 0xc0) == 0x00) b = 0;
	if constexpr ((M & 0xc0) == 0x40) b = rB(op);
	if constexpr ((M & 0xc0) == 0x80) b = (s16)op;
	if constexpr ((M & 0xc0) == 0xc0) b = op << 16;
	u64 v = (u64)a + b;
	if constexpr ((M & 0x20) != 0) v += xer >> 29 & 1;
	if constexpr ((M & 0x10) != 0) v++;
	if constexpr ((M & 8) != 0) v--;
	if constexpr ((M & 4) != 0) update_ca(v >> 32);
	if constexpr ((M & 2) != 0) update_ov(((a & b & ~(u32)v) | (~a & ~b & (u32)v)) & MSB);
	if constexpr (M & 1) update_cr0((u32)v);
	stD(op, (u32)v);
}

template<int M> void TinyPPC::mul(u32 op) {
	u32 a = rA(op), b, v;
	if constexpr ((M & 0xc) == 0x0) v = (s64)(s32)a * (s32)rB(op) >> 32; // mulhw
	if constexpr ((M & 0xc) == 0x4) v = (u64)a * rB(op) >> 32; // mulhwu
	if constexpr ((M & 0xc) == 0x8) v = u32((s64)(s32)a * (s16)op); // mulli
	if constexpr ((M & 0xc) == 0xc) v = u32((u64)a * (b = rB(op))); // mullw
	if constexpr ((M & 2) != 0) update_ov((a ^ b ^ v) & MSB); // mullwo[.] only
	if constexpr (M & 1) update_cr0(v);
	stD(op, v);
}

template<int M, typename T> void TinyPPC::div(u32 op) {
	T a = rA(op), b = rB(op), v = b ? a / b : 0;
	if constexpr ((M & 2) != 0) {
		bool f = !b;
		if constexpr ((M & 4) == 0) f |= a == MSB && b == -1;
		update_ov(f);
	}
	if constexpr (M & 1) update_cr0(v);
	stD(op, v);
}

template<int M> void TinyPPC::logic(u32 op) {
	u32 s = rS(op), b, v;
	if constexpr ((M & 0x30) == 0x00) b = rB(op);
	if constexpr ((M & 0x30) == 0x10) b = ~rB(op);
	if constexpr ((M & 0x30) == 0x20) b = u16(op);
	if constexpr ((M & 0x30) == 0x30) b = op << 16;
	if constexpr ((M & 0xc) == 0) v = s & b;
	if constexpr ((M & 0xc) == 4) v = s | b;
	if constexpr ((M & 0xc) == 8) v = s ^ b;
	if constexpr ((M & 2) != 0) v = ~v;
	if constexpr (M & 1) update_cr0(v);
	stA(op, v);
}

template<int M> void TinyPPC::sftrot(u32 op) {
	u64 s = rS(op);
	u32 m, n, v;
	if constexpr ((M & 0x300) == 0x000) n = op >> 11 & 0x1f;
	if constexpr ((M & 0x300) == 0x200) n = rB(op) & 0x1f;
	if constexpr ((M & 0x300) == 0x300) n = rB(op) & 0x3f;
	if constexpr ((M & 0xc0) == 0x00) v = u32(s >> n);
	if constexpr ((M & 0xc0) == 0x40) v = u32((s64)(s32)s >> n);
	if constexpr ((M & 0xc0) == 0x80) v = u32(s << n);
	if constexpr ((M & 0x20) != 0)
		if (n) v |= s >> (32 - n); // rot
	if constexpr ((M & 8) != 0) {
		int mb = op >> 6 & 0x1f, me = op >> 1 & 0x1f, b = ~0U >> mb, e = ~0U << (0x1f - me);
		v &= m = mb <= me ? b & e : b | e;
	}
	if constexpr ((M & 4) != 0) v |= rA(op) & ~m;
	if constexpr ((M & 2) != 0) update_ca(n && s & MSB && s & (1 << n) - 1);
	if constexpr (M & 1) update_cr0(v);
	stA(op, v);
}

template<int M> void TinyPPC::loadstore(u32 op) {
	u32 ea;
	auto string = [&](auto f, int n) {
		for (int r = op >> 21 & 0x1f, s = 24; n > 0; n--) {
			f(r, s);
			if ((s -= 8) < 0) s = 24, r = r + 1 & 0x1f;
		}
	};
	auto string_i = [&](auto f) {
		string(f, ((op >> 11) - 1 & 0x1f) + 1);
	};
	auto string_x = [&](auto f) {
		string(f, xer & 0x7f);
	};
	auto string_load = [&](int r, int s) {
		if (s == 24) gpr[r] = 0;
		gpr[r] |= ld1(ea++) << s;
	};
	auto string_store = [&](int r, int s) {
		st1(ea++, gpr[r] >> s);
	};
	if constexpr ((M & 0x80) != 0) ea = rAz(op);
	else ea = rA(op);
	if constexpr ((M & 0x40) != 0) ea += rB(op);
	else if constexpr ((M & 0xb) != 0x8) ea += (s16)op; // displacement unless string
	union { u32 i; float f; } ts;
	if constexpr (!(M & 0x10)) {
		if constexpr ((M & 0xf) == 0x0) stD(op, ld1(ea));
		if constexpr ((M & 0xf) == 0x1) stD(op, ld2(ea));
		if constexpr ((M & 0xf) == 0x2) stD(op, ld4(ea));
		if constexpr ((M & 0xf) == 0x5) stD(op, ld2r(ea));
		if constexpr ((M & 0xf) == 0x6) stD(op, ld4r(ea));
		if constexpr ((M & 0xf) == 0x8) string_i(string_load); // lswi
		if constexpr ((M & 0xf) == 0x9) stD(op, (s16)ld2(ea));
		if constexpr ((M & 0xf) == 0xa) ts.i = ld4(ea), stD(op, FPR { .f = ts.f });
		if constexpr ((M & 0xf) == 0xb) stD(op, FPR { .i = ld8(ea) });
		if constexpr ((M & 0xf) == 0xc) string_x(string_load); // lswx
		if constexpr ((M & 0xf) == 0xe) // lmw
			for (int r = op >> 21 & 0x1f; r <= 0x1f; r++, ea += 4)
				gpr[r] = ld4(ea);
		if constexpr ((M & 0xf) == 0xf) { // lwarx
			stD(op, ld4(ea));
			reserve_adr = ea;
			reserve = true;
		}
	}
	else {
		if constexpr ((M & 0xf) == 0x0) st1(ea, rS(op));
		if constexpr ((M & 0xf) == 0x1) st2(ea, rS(op));
		if constexpr ((M & 0xf) == 0x2) st4(ea, rS(op));
		if constexpr ((M & 0xf) == 0x5) st2r(ea, rS(op));
		if constexpr ((M & 0xf) == 0x6) st4r(ea, rS(op));
		if constexpr ((M & 0xf) == 0x8) string_i(string_store); // stswi
		if constexpr ((M & 0xf) == 0xa) ts.f = frS(op).f, st4(ea, ts.i);
		if constexpr ((M & 0xf) == 0xb) st8(ea, frS(op).i);
		if constexpr ((M & 0xf) == 0xc) string_x(string_store); // stswx
		if constexpr ((M & 0xf) == 0xd) st4(ea, (u32)frS(op).i); // stfiwx
		if constexpr ((M & 0xf) == 0xe) // stmw
			for (int r = op >> 21 & 0x1f; r <= 0x1f; r++, ea += 4)
				st4(ea, gpr[r]);
		if constexpr ((M & 0xf) == 0xf) { // stwcx.
			cr = (cr & ~MSD) | (xer & MSB) >> 3;
			if (reserve && reserve_adr == ea) {
				st4(ea, rS(op));
				cr |= MSB >> 2;
			}
			reserve = false;
		}
	}
	if constexpr ((M & 0x20) != 0) stA(op, ea); // update
}

template<int M> void TinyPPC::farith(u32 op) {
	double v;
	if constexpr ((M & 0xf0) == 0x10) v = frB(op).f; // fmr/(frsp)
	if constexpr ((M & 0xf0) == 0x20) v = frA(op).f + frB(op).f; // fadd/fadds
	if constexpr ((M & 0xf0) == 0x30) v = frA(op).f - frB(op).f; // fsub/fsubs
	if constexpr ((M & 0xf0) == 0x40) v = frA(op).f * frC(op).f; // fmul/fmuls
	if constexpr ((M & 0xf0) == 0x50) v = frA(op).f / frB(op).f; // fdiv/fdivs
	if constexpr ((M & 0xf0) == 0x60) v = frA(op).f * frC(op).f + frB(op).f; // f[n]madd/f[n]madds
	if constexpr ((M & 0xf0) == 0x70) v = frA(op).f * frC(op).f - frB(op).f; // f[n]msub/f[n]msubs
	if constexpr ((M & 0xf0) == 0x80) v = 1. / frB(op).f; // fres
	if constexpr ((M & 0xf0) == 0x90) v = std::sqrt(frB(op).f); // fsqrt/fsqrts/frsqrte
	if constexpr ((M & 0xf0) == 0xa0) v = frA(op).f >= 0. ? frC(op).f : frB(op).f; // fsel
	if constexpr ((M & 0xf0) == 0xb0) (u64 &)v = frB(op).i & ~(1LL << 63); // fabs
	if constexpr ((M & 0xf0) == 0xc0) (u64 &)v = frB(op).i | 1LL << 63; // fnabs
	if constexpr ((M & 0xf0) == 0xd0) (u64 &)v = frB(op).i ^ 1LL << 63; // fneg
	if constexpr ((M & 0xe0) == 0xe0) { // fctiw / fctiwz
		int i;
		v = frB(op).f;
		if (v >= (double)MSB) i = MSB - 1;
		else if (v < -(double)MSB) i = MSB;
		else {
			if constexpr ((M & 0x10) != 0)
				switch (fpscr & 3) {
					case 0: i = std::round(v); break;
					case 1: i = std::trunc(v); break;
					case 2: i = std::ceil(v); break;
					case 3: i = std::floor(v); break;
				}
			else i = std::trunc(v);
		}
		stD(op, FPR { .i = (u64)i });
		return;
	}
	if constexpr ((M & 0xf0) == 0x00) { // fcmpo/fcmpu
		double a = frA(op).f, b = frB(op).f;
		u32 c = std::fpclassify(a) == FP_NAN || std::fpclassify(b) == FP_NAN ? MSB >> 3 : cmp3(a, b);
		fpscr = (fpscr & ~0xf000) | c >> 16;
		update_cr(op, c);
		return;
	}
	if constexpr ((M & 8) != 0) v = -v; // fn*
	if constexpr ((M & 4) != 0) v = (float)v; // *s
	if constexpr (!(M & 2)) {
		fpscr &= ~0x1f000;
		switch (std::fpclassify(v)) {
			case FP_ZERO:		fpscr |= std::signbit(v) ? 0x12000 : 0x2000; break;
			case FP_SUBNORMAL:	fpscr |= 0x10000 | (v < 0 ? 0x8000 : 0x4000); break;
			case FP_NAN:		fpscr |= 0x11000; break;
			case FP_INFINITE:	fpscr |= 0x1000 | (v < 0 ? 0x8000 : 0x4000); break;
			case FP_NORMAL:		fpscr |= v < 0 ? 0x8000 : 0x4000; break;
		}
	}
	if constexpr (M & 1) update_cr1();
	stD(op, FPR { .f = v });
}

template<int M> void TinyPPC::branch(u32 op) {
	u32 pc0;
	bool cond_ok;
	if constexpr (M & 1) pc0 = pc;
	if constexpr ((M & 0xc) == 0x0) // b[l][a]
		if constexpr ((M & 2) != 0) pc = (s32)op << 6 >> 6 & ~3;
		else pc += ((s32)op << 6 >> 6 & ~3) - 4;
	else cond_ok = op & 1 << 25 || (op >> 24 & 1) ^ !(cr & MSB >> (op >> 16 & 0x1f));
	if constexpr ((M & 0xc) == 0x4) // bc[l][a]
		if ((op & 1 << 23 || (op >> 22 & 1) ^ (--ctr != 0)) && cond_ok) {
			if constexpr ((M & 2) != 0) pc = (s16)op & ~3;
			else pc += ((s16)op & ~3) - 4;
		}
	if constexpr ((M & 0xc) == 0x8) // bcctr[l]
		if (cond_ok) pc = ctr & ~3;
	if constexpr ((M & 0xc) == 0xc) // bclr[l]
		if (cond_ok) pc = lr & ~3;
	if constexpr (M & 1) lr = pc0;
}

template<int M> void TinyPPC::cond(u32 op) {
	u32 s = op >> 21 & 0x1f, a = cr << (op >> 16 & 0x1f), b = cr << (op >> 11 & 0x1f);
	if constexpr ((M & 8) != 0) b = ~b;
	if constexpr ((M & 6) == 0) a &= b;
	if constexpr ((M & 6) == 2) a |= b;
	if constexpr ((M & 6) == 4) a ^= b;
	if constexpr (M & 1) a = ~a;
	cr = (cr & ~(MSB >> s)) | (a & MSB) >> s;
}

template<int M> void TinyPPC::mtfs(u32 op) {
	u32 m, x, s;
	if constexpr ((M & 0x30) == 0x00) m = MSB >> (op >> 21 & 0x1f), x = ~0U;
	if constexpr ((M & 0x30) == 0x10) m = digitmask.m[op >> 17 & 0xff], x = (u32)frB(op).i;
	if constexpr ((M & 0x30) == 0x20) s = (op >> 23 & 7) << 2, m = MSD >> s, x = op << 16 >> s;
	m &= ~(3 << 29);
	if constexpr ((M & 4) != 0) fpscr &= ~m;
	if constexpr ((M & 2) != 0) fpscr |= x & m;
	if constexpr (M & 1) update_cr1();
}

void TinyPPC::mtcrf(u32 op) {
	u32 m = digitmask.m[op >> 12 & 0xff];
	cr = (cr & ~m) | (rS(op) & m);
}

static constexpr int pr(int x) { return ((x << 5 & 0x3e0) | (x >> 5 & 0x1f)) << 11; }

void TinyPPC::mfspr(u32 op) {
	switch (op & 0x1ff800) {
		case pr(1): stD(op, xer); return;
		case pr(8): stD(op, lr); return;
		case pr(9): stD(op, ctr); return;
		default: fprintf(stderr, "mfspr: unimplemented SPR\n"); break;
	}
	StopTrace();
}

void TinyPPC::mftb(u32 op) {
	switch (op & 0x1ff800) {
		case pr(268): stD(op, u32(25 * GetTicks_usec())); return;	// SheepShaver
		case pr(269): stD(op, 25 * GetTicks_usec() >> 32); return;	// SheepShaver
	}
}

void TinyPPC::mtspr(u32 op) {
	switch (op & 0x1ff800) {
		case pr(1): xer = rS(op) & 0xe000007f; return;
		case pr(8): lr = rS(op); return;
		case pr(9): ctr = rS(op); return;
		default: fprintf(stderr, "mtspr: unimplemented SPR\n"); break;
	}
	StopTrace();
}

void TinyPPC::Execute() {
	extern bool check_spcflags(TinyPPC *);
	do {
#if TINYPPC_TRACE
		tracep->pc = pc;
		tracep->index = 0;
#endif
		Insn::exec1(this, fetch4());
#if TINYPPC_TRACE
		tracep->cr = cr;
#if TINYPPC_TRACE > 1
		if (++tracep >= tracebuf + TRACEMAX - 1) StopTrace();
#else
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
#endif
#endif
	} while (spcflags_empty() || check_spcflags(this)); // SheepShaver
}

void TinyPPC::StopTrace() {
#if TINYPPC_TRACE
	TraceBuffer *endp = tracep;
	int i = 0;
	FILE *fo;
	if (!(fo = fopen((std::string(getenv("HOME")) + "/Desktop/trace.txt").c_str(), "w"))) exit(1);
	do {
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
		fprintf(fo, "%4d %08x ", i++, tracep->pc);
		fprintf(fo, "%08x %08x ", vm_read_memory_4_nolog(tracep->pc), tracep->cr);
		for (Acs *p = tracep->acs; p < tracep->acs + tracep->index; p++) {
			switch (p->type) {
				case acsLoad8:
					fprintf(fo, "L %08x %02llx ", p->adr, p->data & 0xff);
					break;
				case acsLoad16:
					fprintf(fo, "L %08x %04llx ", p->adr, p->data & 0xffff);
					break;
				case acsLoad32:
					fprintf(fo, "L %08x %08llx ", p->adr, p->data & 0xffffffff);
					break;
				case acsLoad64:
					fprintf(fo, "L %08x %016llx ", p->adr, p->data);
					break;
				case acsStore8:
					fprintf(fo, "S %08x %02llx ", p->adr, p->data & 0xff);
					break;
				case acsStore16:
					fprintf(fo, "S %08x %04llx ", p->adr, p->data & 0xffff);
					break;
				case acsStore32:
					fprintf(fo, "S %08x %08llx ", p->adr, p->data & 0xffffffff);
					break;
				case acsStore64:
					fprintf(fo, "S %08x %016llx ", p->adr, p->data);
					break;
				case acsStoreG:
					fprintf(fo, "%08llx->G%d ", p->data & 0xffffffff, p->adr);
					break;
				case acsStoreF:
					fprintf(fo, "%016llx->F%d ", p->data, p->adr);
					break;
			}
		}
		fprintf(fo, "\n");
	} while (tracep != endp);
	fclose(fo);
	fprintf(stderr, "trace dumped.\n");
#endif
	exit(1);
}

void TinyPPC::undef(u32 op) {
	fprintf(stderr, "undefined instruction: %08x\n", op);
	StopTrace();
}

// ---- for SheepShaver

void TinyPPC::Interrupt() {
	RegTmp r;
	memcpy(r.gpr, gpr, sizeof(gpr));
	memcpy(r.fpr, fpr, sizeof(fpr));
	r.pc = pc;
	r.lr = lr;
	r.ctr = ctr;
	r.cr = cr;
	r.xer = xer;
	r.fpscr = fpscr;
	extern void HandleInterrupt(RegTmp *);
	HandleInterrupt(&r);
	memcpy(gpr, r.gpr, sizeof(gpr));
	memcpy(fpr, r.fpr, sizeof(fpr));
	pc = r.pc;
	lr = r.lr;
	ctr = r.ctr;
	cr = r.cr;
	xer = r.xer;
	fpscr = r.fpscr;
}

void TinyPPC::sheep(u32 op) {
	pc -= 4;
	extern void execute_sheep(u32);
	execute_sheep(op);
}

void TinyPPC::sheepi() {
	Insn::exec1(this, 0x50e74001); // rlwimi. r7,r7,8,0,0
}
