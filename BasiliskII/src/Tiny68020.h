// Tiny68020
// Copyright 2021-2024 © Yasuo Kuwahara
// MIT License

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#define TINY68020_TRACE		0

#if TINY68020_TRACE
#define TINY68020_TRACE_LOG(adr, data, type) \
	if (tracep->index < ACSMAX) tracep->acs[tracep->index++] = { adr, data, type }
#else
#define TINY68020_TRACE_LOG(adr, data, type)
#endif

struct M68kRegisters;	// BasiliskII

class Tiny68020 {
	friend class Insn;
	using s8 = int8_t;
	using u8 = uint8_t;
	using s16 = int16_t;
	using u16 = uint16_t;
	using s32 = int32_t;
	using u32 = uint32_t;
	using s64 = int64_t;
	using u64 = uint64_t;
	static constexpr int MPU_TYPE = 68020;
	enum {
		LC, LV, LZ, LN, LX, LI = 8, LM = 12, LS, LT = 15
	};
	enum {
		MC = 1 << LC, MV = 1 << LV, MZ = 1 << LZ, MN = 1 << LN, MX = 1 << LX,
		MI = 7 << LI, MM = 1 << LM, MS = 1 << LS, MT = 1 << LT
	};
	enum {
		F0 = 1, F1, FS, FSX, FADD, FSUB, FSUBX, FBCD, FBF, FSL, FSR, FXL, FXR
	};
#define F(flag, type)	flag##type = F##type << (L##flag << 2)
	enum {
		F(C, 0), F(C, SUB), F(C, SL), F(C, SR),
		F(V, 0), F(V, 1), F(V, S), F(V, ADD), F(V, SUB), F(V, SL),
		F(Z, 0), F(Z, S), F(Z, SX), F(Z, BF),
		F(N, 0), F(N, S), F(N, BF),
		F(X, ADD), F(X, SUB), F(X, SL), F(X, SR), F(X, XL), F(X, XR), F(X, BCD)
	};
#undef F
	enum {
		CR_USP = 8, CR_VBR, CR_MSP = 11, CR_ISP
	};
public:
	Tiny68020();
	void Reset();
	void SetMemoryPtr(u8 *p) { m = p; }
	void Execute();
	u32 GetPC() const { return pc; }
	void SetPC(u32 v) { pc = v; }
	void Trace() { Trap(9, trace_pc); }
	void IRQ(int level) {
		if ((level &= 7) < 7 && level <= (sr >> LI & 7)) return;
		Trap(24 + level);
		sr = (sr & ~MI) | level << LI;
	}
#if TINY68020_TRACE
	void StopTrace();
#endif
	void importRegs(M68kRegisters &r);
	void exportRegs(M68kRegisters &r);
	void execsub(u32 v, M68kRegisters &r, bool isTrap);
private:
	template<int S> void stD(u32 n, u32 data) {
		if constexpr (S == 0) d[n] = (d[n] & 0xffffff00) | (data & 0xff);
		else if constexpr (S == 1) d[n] = (d[n] & 0xffff0000) | (data & 0xffff);
		else d[n] = data;
		TINY68020_TRACE_LOG(n, d[n], acsStoreD);
	}
	void stA(u32 n, u32 data) {
		a[n] = data;
		TINY68020_TRACE_LOG(n, a[n], acsStoreA);
	}
	// memory access -- start
	u32 ld1(u32 adr) {
		u32 data = m[adr];
		TINY68020_TRACE_LOG(adr, data, acsLoad8);
		return data;
	}
	u32 ld2(u32 adr) {
		u32 data = __builtin_bswap16((u16 &)m[adr]);
		TINY68020_TRACE_LOG(adr, data, acsLoad16);
		return data;
	}
	u32 ld4(u32 adr) {
		u32 data = __builtin_bswap32((u32 &)m[adr]);
		TINY68020_TRACE_LOG(adr, data, acsLoad32);
		return data;
	}
	void st1(u32 adr, u8 data) {
		m[adr] = data;
		TINY68020_TRACE_LOG(adr, data, acsStore8);
	}
	void st2(u32 adr, u16 data) {
		(u16 &)m[adr] = __builtin_bswap16(data);
		TINY68020_TRACE_LOG(adr, data, acsStore16);
	}
	void st4(u32 adr, u32 data) {
		(u32 &)m[adr] = __builtin_bswap32(data);
		TINY68020_TRACE_LOG(adr, data, acsStore32);
	}
	u32 get4(u32 p) { return ld4(p); }
	s16 fetch2() {
		s16 v = __builtin_bswap16((u16 &)m[pc]);
		pc += 2;
		return v;
	}
	u32 fetch4() {
		u32 v = __builtin_bswap32((u32 &)m[pc]);
		pc += 4;
		return v;
	}
	// memory access -- end
	template<int S> u32 ld(u32 adr) {
		if constexpr (S == 0) return ld1(adr);
		else if constexpr (S == 1) return ld2(adr);
		else return ld4(adr);
	}
	template<int S> void st(u32 adr, u32 data) {
		if constexpr (S == 0) st1(adr, data);
		else if constexpr (S == 1) st2(adr, data);
		else st4(adr, data);
	}
	void push2(u16 d) { st2(a[7] -= 2, d); }
	void push4(u32 d) { st4(a[7] -= 4, d); }
	u32 pop2() { a[7] += 2; return ld2(a[7] - 2); }
	u32 pop4() { a[7] += 4; return ld4(a[7] - 4); }
	template<int S> u32 getImm() {
		if constexpr (S < 2) return fetch2();
		else return fetch4();
	}
	template<int RW, int M, int S, typename F> u32 ea(int reg, F func);
	void SetSR(u16 data, bool perm = false);
	void Trap(u32 vector, u32 param = 0);
	template<int C> int cond();
	int X() const { return (sr & MX) != 0; }
	u8 *m;
	u32 a[8], d[8];
	u16 sr;
	u32 cr[16];
	u32 pc, trace_pc;
#if TINY68020_TRACE
	static constexpr int TRACEMAX = 10000;
	static constexpr int ACSMAX = 32;
	enum {
		acsStoreD = 1, acsStoreA,
		acsStore8, acsStore16, acsStore32, acsLoad8, acsLoad16, acsLoad32
	};
	struct Acs {
		u32 adr, data;
		u8 type;
	};
	struct TraceBuffer {
		u32 pc;
		Acs acs[ACSMAX];
		u8 index, ccr;
	};
	TraceBuffer tracebuf[TRACEMAX];
	TraceBuffer *tracep;
#endif
	template<int DM, int S = 0> u32 fset(u32 r = 0, u32 s = 0, u32 d = 0);
	template<int S> u32 fadd(u32 r, u32 s, u32 d) { return fset<XADD | NS | ZS | VADD, S>(r, s, d); }
	template<int S> u32 faddx(u32 r, u32 s, u32 d) { return fset<XADD | NS | ZSX | VADD, S>(r, s, d); }
	template<int S> u32 fsub(u32 r, u32 s, u32 d) { return fset<XSUB | NS | ZS | VSUB, S>(r, s, d); }
	template<int S> u32 fsubx(u32 r, u32 s, u32 d) { return fset<XSUB | NS | ZSX | VSUB, S>(r, s, d); }
	template<int S> u32 fcmp(u32 r, u32 s, u32 d) { return fset<NS | ZS | VSUB | CSUB, S>(r, s, d); }
	template<int S> u32 flogic(u32 r) { return fset<NS | ZS | V0 | C0, S>(r); }
	template<int S> u32 fbtst(u32 r) { return fset<ZS, S>(r); }
	u32 fbf(u32 r, u8 width) { return fset<NBF | ZBF | V0 | C0>(r, width); }
	u32 fbcd(u32 r, u32 x) { return fset<XBCD | ZSX>(r, x); }
	u32 fchk(u32 r) { return fset<NS>(r); }
	template<int S = 2, typename T = void> u32 fmul(u64 r);
#define R0		(op & 7)
#define R9		(op >> 9 & 7)
	template<int M> void movea_w(u16 op) { ea<1, M, 1>(op, [&](s16 v) { stA(R9, v); }); } // movea.w <ea>,An
	template<int M> void movea_l(u16 op) { ea<1, M, 2>(op, [&](u32 v) { stA(R9, v); }); } // movea.l <ea>,An
	template<int M1, int M2, int S> void move_ea_ea(u16 op) { // move <ea>,<ea>
		ea<1, M1, S>(op, [&](u32 v) { ea<2, M2, S>(R9, [&]{ return flogic<S>(v); }); });
	}
	void moveq(u16 op) { d[R9] = flogic<2>(s8(op)); } // moveq #<data>,Dn
	template<int M, int S> void movem_rm(u16 op);
	template<int M, int S> void movem_mr(u16 op);
	void movep(u16 op);
	void movec(u16 op);
	template<int M> void move_ccr_ea(u16 op) { ea<2, M, 1>(op, [&]{ return sr & 0x1f; }); } // move ccr,<ea>
	template<int M> void move_ea_ccr(u16 op) { ea<1, M, 1>(op, [&](u32 v) { sr = (sr & 0xff00) | (v & 0x1f); }); } // move <ea>,ccr
	template<int M> void move_sr_ea(u16 op) { // move sr,<ea>
		if constexpr (MPU_TYPE >= 68010)
			if (!(sr & MS)) Trap(8);
		ea<2, M, 1>(op, [&]{ return sr; });
	}
	template<int M> void move_ea_sr(u16 op) { ea<1, M, 1>(op, [&](u32 v) { SetSR(v); }); } // move <ea>,sr
	void move_usp_an(u16 op) { // move usp,An
		if (!(sr & MS)) Trap(8);
		a[R0] = cr[CR_USP];
	}
	void move_an_usp(u16 op) { // move An,usp
		if (!(sr & MS)) Trap(8);
		cr[CR_USP] = a[R0];
	}
	template<int M, int S> void clr(u16 op) { // clr <ea>
		//if constexpr (MPU_TYPE >= 68010)
			ea<2, M, S>(op, [&]{ return flogic<0>(0); });
		//else ea<3, M, S>(op, [&](u32) { return flogic<0>(0); });
	}
	void swap(u16 op) { stD<2>(R0, flogic<2>(d[R0] << 16 | d[R0] >> 16)); } // swap Dn
	template<int M, int S> void add_ea_d(u16 op) { // add <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, fadd<S>(d[R9] + v, v, d[R9])); });
	}
	template<int M, int S> void add_d_ea(u16 op) {// add Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return fadd<S>(v + d[R9], d[R9], v); });
	}
	template<int M> void adda_w(u16 op) { ea<1, M, 1>(op, [&](s16 v) { stA(R9, a[R9] + v); }); } // adda.w <ea>,An
	template<int M> void adda_l(u16 op) { ea<1, M, 2>(op, [&](u32 v) { stA(R9, a[R9] + v); }); } // adda.l <ea>,An
	template<int M, int S> void addi(u16 op) { // addi #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return fadd<S>(v + t, t, v); });
	}
	template<int M, int S> void addq(u16 op) { // addq #<data>,<ea>
		ea<3, M, S>(op, [&](u32 v) { u32 t = ((op >> 9) - 1 & 7) + 1; return fadd<S>(v + t, t, v); });
	}
	void addqa(u16 op) { ea<3, 1, 2>(op, [&](u32 v) { return v + ((op >> 9) - 1 & 7) + 1; }); } // addq #<data>,An
	template<int S> void addx_m(u16 op) { // addx -(Ay),-(Ax)
		u32 s = ld<S>(a[R0] -= 1 << S), v = ld<S>(a[R9] -= 1 << S); st<S>(a[R9], faddx<S>(v + s + X(), s, v));
	}
	template<int S> void addx_r(u16 op) { stD<S>(R9, faddx<S>(d[R9] + d[R0] + X(), d[R0], d[R9])); } // addx Dy,Dx
	template<int M, int S> void sub_ea_d(u16 op) { // sub <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, fsub<S>(d[R9] - v, v, d[R9])); });
	}
	template<int M, int S> void sub_d_ea(u16 op) {// sub Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return fsub<S>(v - d[R9], d[R9], v); });
	}
	template<int M> void suba_w(u16 op) { ea<1, M, 1>(op, [&](s16 v) { stA(R9, a[R9] - v); }); } // suba.w <ea>,An
	template<int M> void suba_l(u16 op) { ea<1, M, 2>(op, [&](u32 v) { stA(R9, a[R9] - v); }); } // suba.l <ea>,An
	template<int M, int S> void subi(u16 op) { // sbui #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return fsub<S>(v - t, t, v); });
	}
	template<int M, int S> void subq(u16 op) { // subq #<data>,<ea>
		ea<3, M, S>(op, [&](u32 v) { u32 t = ((op >> 9) - 1 & 7) + 1; return fsub<S>(v - t, t, v); });
	}
	void subq_a(u16 op) { ea<3, 1, 2>(op, [&](u32 v) { return v - ((op >> 9) - 1 & 7) - 1; }); } // subq #<data>,An
	template<int S> void subx_m(u16 op) { // subx -(Ay),-(Ax)
		u32 s = ld<S>(a[R0] -= 1 << S), v = ld<S>(a[R9] -= 1 << S); st<S>(a[R9], fsubx<S>(v - s - X(), s, v));
	}
	template<int S> void subx_r(u16 op) { stD<S>(R9, fsubx<S>(d[R9] - d[R0] - X(), d[R0], d[R9])); } // subx Dx,Dy
	template<int M, int S> void cmp(u16 op) { ea<1, M, S>(op, [&](u32 v) { fcmp<S>(d[R9] - v, v, d[R9]); }); } // cmp <ea>,Dn
	template<int M> void cmpa_w(u16 op) { ea<1, M, 1>(op, [&](s16 v) { fcmp<2>(a[R9] - v, v, a[R9]); }); } // cmpa.w <ea>,An
	template<int M> void cmpa_l(u16 op) { ea<1, M, 2>(op, [&](u32 v) { fcmp<2>(a[R9] - v, v, a[R9]); }); } // cmpa.l <ea>,An
	template<int M, int S> void cmpi(u16 op) { // cmpi #<data>,<ea>
		u32 t = getImm<S>(); ea<1, M, S>(op, [&](u32 v) { fcmp<S>(v - t, t, v); });
	}
	template<int S> void cmpm(u16 op) { // cmpm (Ay)+,(Ax)+
		u32 s = ld<S>(a[R0]), d; a[R0] += 1 << S; d = ld<S>(a[R9]); a[R9] += 1 << S; fcmp<S>(d - s, s, d);
	}
	template<int M, int S> void neg(u16 op) { ea<3, M, S>(op, [&](u32 v) { return fsub<S>(-v, v, 0); }); } // neg <ea>
	template<int M, int S> void negx(u16 op) { ea<3, M, S>(op, [&](u32 v) { return fsubx<S>(-v - X(), v, 0); }); } // negx <ea>
	template<int M> void muls_w(u16 op) { ea<1, M, 1>(op, [&](s16 v) { stD<2>(R9, fmul((s16)d[R9] * v)); }); } // muls.w <ea>,Dn
	template<int M> void mulu_w(u16 op) { ea<1, M, 1>(op, [&](u16 v) { stD<2>(R9, fmul((u16)d[R9] * v)); }); } // mulu.w <ea>,Dn
	template<int M> void mul_l(u16 op) {
		if (u32 op2 = fetch2(); op2 & 0x800) mul_l<M, s32, s64>(op, op2); // muls.l
		else mul_l<M, u32, u64>(op, op2); // mulu.l
	}
	template<int M, typename T32, typename T64> void mul_l(u16 op, u16 op2);
	template<int M, typename T16, typename T32> void div_w(u16 op);
	template<int M> void divs_w(u16 op) { div_w<M, s16, s32>(op); }
	template<int M> void divu_w(u16 op) { div_w<M, u16, u32>(op); }
	template<int M> void div_l(u16 op) {
		if (u16 op2 = fetch2(); op2 & 0x800) div_l<M, s32, s64>(op, op2); // divs.l
		else div_l<M, u32, u64>(op, op2); // divu.l
	}
	template<int M, typename T32, typename T64> void div_l(u16 op, u16 op2);
	template<int M, int S> void and_ea_d(u16 op) { ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, flogic<S>(d[R9] & v)); }); } // and <ea>,Dn
	template<int M, int S> void  or_ea_d(u16 op) { ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, flogic<S>(d[R9] | v)); }); } //  or <ea>,Dn
	template<int M, int S> void and_d_ea(u16 op) { ea<3, M, S>(op, [&](u32 v) { return flogic<S>(d[R9] & v); }); } // and Dn,<ea>
	template<int M, int S> void  or_d_ea(u16 op) { ea<3, M, S>(op, [&](u32 v) { return flogic<S>(d[R9] | v); }); } //  or Dn,<ea>
	template<int M, int S> void eor_d_ea(u16 op) { ea<3, M, S>(op, [&](u32 v) { return flogic<S>(d[R9] ^ v); }); } // eor Dn,<ea>
	template<int M, int S> void andi_ea(u16 op) { // andi #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return flogic<S>(t & v); });
	}
	template<int M, int S> void  ori_ea(u16 op) { //  ori #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return flogic<S>(t | v); });
	}
	template<int M, int S> void eori_ea(u16 op) { // eori #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return flogic<S>(t ^ v); });
	}
	template<int M, int S> void  tst(u16 op) { ea<1, M, S>(op, [&](u32 v) { flogic<S>(v); }); } // tst <ea>
	template<int M, int S> void _not(u16 op) { ea<3, M, S>(op, [&](u32 v) { return flogic<S>(~v); }); } // not <ea>
	template<int M> void tas(u16 op) { ea<3, M, 0>(op, [&](u32 v) { return flogic<0>(v) | 0x80; }); } // tas <ea>
	void andi_ccr(u16) { sr = (sr & 0xff00) | ((sr & fetch2()) & 0x1f); } // andi #<data>,ccr
	void  ori_ccr(u16) { sr = (sr & 0xff00) | ((sr | fetch2()) & 0x1f); } //  ori #<data>,ccr
	void eori_ccr(u16) { sr = (sr & 0xff00) | ((sr ^ fetch2()) & 0x1f); } // eori #<data>,ccr
	void andi_sr(u16) { SetSR(sr & fetch2()); } // andi #<data>,sr
	void  ori_sr(u16) { SetSR(sr | fetch2()); } //  ori #<data>,sr
	void eori_sr(u16) { SetSR(sr ^ fetch2()); } // eori #<data>,sr
	template<int W, int M, typename F> void bitop(u16 op, F func);
	template<int M> void btst(u16 op) { bitop<2, M>(op, []{}); } // btst Dn,<ea>
	template<int M> void bclr(u16 op) { bitop<3, M>(op, [](u32 v, u32 m) { return v & ~m; }); } // bclr Dn,<ea>
	template<int M> void bset(u16 op) { bitop<3, M>(op, [](u32 v, u32 m) { return v | m; }); } // bset Dn,<ea>
	template<int M> void bchg(u16 op) { bitop<3, M>(op, [](u32 v, u32 m) { return v ^ m; }); } // bchg Dn,<ea>
	template<int M> void btst_i(u16 op) { bitop<0, M>(op, []{}); } // btst #<data>,<ea>
	template<int M> void bclr_i(u16 op) { bitop<1, M>(op, [](u32 v, u32 m) { return v & ~m; }); } // bclr #<data>,<ea>
	template<int M> void bset_i(u16 op) { bitop<1, M>(op, [](u32 v, u32 m) { return v | m; }); } // bset #<data>,<ea>
	template<int M> void bchg_i(u16 op) { bitop<1, M>(op, [](u32 v, u32 m) { return v ^ m; }); } // bchg #<data>,<ea>
	template<int M> void bitfield(u16 op);
	template<int M, int S, int DM, typename F> void sftrot(u16 op, F opfunc);
	constexpr int bits(int S) { return S < 3 ? 8 << S : 16; }
	template<int M, int S> void asr(u16 op) {
		sftrot<M, S, XSR | NS | ZS | V0>(op, [&](u64 v, u32 s) {
			return v >> s | (v >> (bits(S) - 1) & 1 ? -1 << (bits(S) - s) : 0);
		});
	}
	template<int M, int S> void lsr(u16 op) {
		sftrot<M, S, XSR | NS | ZS | V0>(op, [&](u64 v, u32 s) { return v >> s; });
	}
	template<int M, int S> void roxr(u16 op) {
		sftrot<M, S, XXR | NS | ZS | V0>(op, [&](u64 v, u32 s) {
			u32 t = bits(S) + 1 - s; return v >> s | (t < 32 ? v << t : 0) | X() << (bits(S) - s);
		});
	}
	template<int M, int S> void ror(u16 op) {
		sftrot<M, S, NS | ZS | V0 | CSR>(op, [&](u64 v, u32 s) { return v >> s | v << (bits(S) - s); });
	}
	template<int M, int S> void asl(u16 op) {
		sftrot<M, S, XSL | NS | ZS | VSL>(op, [&](u64 v, u32 s) { return v << s; });
	}
	template<int M, int S> void lsl(u16 op) {
		sftrot<M, S, XSL | NS | ZS | V0>(op, [&](u64 v, u32 s) { return v << s; });
	}
	template<int M, int S> void roxl(u16 op) {
		sftrot<M, S, XXL | NS | ZS | V0>(op, [&](u64 v, u32 s) {
			u32 t = bits(S) + 1 - s; return v << s | (t < 32 ? v >> t : 0) | X() << (s - 1);
		});
	}
	template<int M, int S> void rol(u16 op) {
		sftrot<M, S, NS | ZS | V0 | CSL>(op, [&](u64 v, u32 s) { return v << s | v >> (bits(S) - s); });
	}
	template<int W, int M> void bcd(u16 op);
	void abcd_r(u16 op) { bcd<0, 0>(op); } // abcd Dy,Dx
	void abcd_m(u16 op) { bcd<0, 1>(op); } // abcd -(Ay), -(Ax)
	void sbcd_r(u16 op) { bcd<1, 0>(op); } // sbcd Dy,Dx
	void sbcd_m(u16 op) { bcd<1, 1>(op); } // sbcd -(Ay), -(Ax)
	template<int M> void nbcd(u16 op) { bcd<2, M>(op); } // nbcd <ea>
	template<int M> void jmp(u16 op) { pc = ea<0, M, 0>(op, []{}); } // jmp <ea>
	template<int M> void jsr(u16 op) { u32 t = ea<0, M, 0>(op, []{}); push4(pc); pc = t; } // jsr <ea>
	template<int M> void pea(u16 op) { push4(ea<0, M, 2>(op, []{})); } // pea <ea>
	void link_w(u16 op) { push4(a[R0]); a[R0] = a[7]; a[7] += fetch2(); } // link.w An,#<displacement>
	void link_l(u16 op) { push4(a[R0]); a[R0] = a[7]; a[7] += fetch4(); } // link.l An,#<displacement>
	void unlk(u16 op) { a[7] = a[R0]; a[R0] = pop4(); } // unlk An
	template <int C, int S> void bcc(u16 op);
	template <int S> void bsr(u16 op);
	template<int M, int C> void scc(u16 op) { ea<2, M, 0>(op, [&]{ return cond<C>() ? 0xff : 0; }); } // scc <ea>
	template<int C> void dbcc(u16 op);
	template<int C> void trapcc(u16 op) { // trapcc #<data>
		if (!(op & 4)) pc += (op & 2) + ((op & 1) << 1);
		if (cond<C>()) Trap(7);
	}
	void trap(u16 op) { Trap(32 + (op & 0xf)); } // trap #<vector>
	void trapv(u16) { if (sr & MV) Trap(7); }
	void rts(u16) { pc = pop4(); }
	void rte(u16);
	void rtd(u16) { u32 t = pop4(); a[7] += fetch2(); pc = t; } // rtd #<displacement>
	void rtr(u16) { SetSR(pop2()); pc = pop4(); }
	void exg_dd(u16 op) { std::swap(d[R9], d[R0]); } // exg Dx,Dy
	void exg_aa(u16 op) { std::swap(a[R9], a[R0]); } // exg Ax,Ay
	void exg_da(u16 op) { std::swap(d[R9], a[R0]); } // exg Dx,Ay
	void ext_w(u16 op) { stD<1>(R0, flogic<0>((s8)d[R0])); } // ext.w
	void ext_l(u16 op) { flogic<2>(d[R0] = (s16)d[R0]); } // ext.l
	void ext_bl(u16 op) { flogic<2>(d[R0] = (s8)d[R0]); } // extb.l
	template<int M> void lea(u16 op) { stA(R9, ea<0, M, 2>(op, []{})); } // lea <ea>,An
	template<int M> void chk_w(u16 op) { // chk.w <ea>,Dn
		if (s16 x = d[R9]; x < 0) { fchk(-1); Trap(6); }
		else ea<1, M, 1>(op, [&](s16 v) { if (x > v) { fchk(0); Trap(6); } });
	}
	template<int M> void chk_l(u16 op) { // chk.l <ea>,Dn
		if (s32 x = d[R9]; x < 0) { fchk(-1); Trap(6); }
		else ea<1, M, 2>(op, [&](s32 v) { if (x > v) { fchk(0); Trap(6); } });
	}
	void undef(u16);
	void reset(u16) { fprintf(stderr, "RESET instruction\n"); }
	void a_line(u16) { pc -= 2; Trap(10); }
	void f_line(u16 op) { pc -= 2; Trap(11); fprintf(stderr, "F-line trap: %04x\n", op); } // CINV,cp*,CPUSH,FPinst,MOVE16
	void nop(u16) {}
	template <int M, int S> void cas(u16 op);
	void bkpt(u16) { fprintf(stderr, "BKPT\n"); exit(1); }
	void x00c0(u16) { fprintf(stderr, "CMP2/CHK2/CALLM/RETM\n"); exit(1); }
	void x08c0(u16) { fprintf(stderr, "CAS2/MOVES\n"); exit(1); }
	void xf280(u16) { pc += 2; } // for booting KT7.5.3
	void emulop(u16); // BasiliskII
};
