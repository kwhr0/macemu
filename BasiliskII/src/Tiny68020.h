// Tiny68020
// Copyright 2021-2023 © Yasuo Kuwahara
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
	using IntrVecFunc = int (*)(int);
	static constexpr int MPU_TYPE = 68020;
	static constexpr int FBUFMAX = 128;
	enum {
		LC, LV, LZ, LN, LX, LI = 8, LM = 12, LS, LT = 15
	};
	enum {
		MC = 1 << LC, MV = 1 << LV, MZ = 1 << LZ, MN = 1 << LN, MX = 1 << LX,
		MI = 7 << LI, MM = 1 << LM, MS = 1 << LS, MT = 1 << LT
	};
	enum {
		FB = 1, F0, FS, FSX, FADD, FSUB, FSUBX, FMUL, FBCD, FBF,
		FSFTROT, FSL = FSFTROT, FSR, FXL, FXR
	};
#define F(flag, type)	flag##type = F##type << (L##flag << 2)
	enum {
		F(C, B), F(C, 0), F(C, ADD), F(C, SUB), F(C, SL), F(C, SR), F(C, XL), F(C, XR), F(C, BCD),
		F(V, B), F(V, 0), F(V, S), F(V, ADD), F(V, SUB), F(V, MUL), F(V, SL),
		F(Z, B), F(Z, 0), F(Z, S), F(Z, SX), F(Z, BCD), F(Z, MUL), F(Z, BF),
		F(N, B), F(N, 0), F(N, S), F(N, MUL), F(N, BF),
		F(X, B), F(X, ADD), F(X, SUB), F(X, SFTROT), F(X, SL), F(X, SR), F(X, BCD)
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
	void IRQ(int level) { intreq = level & 7; }
	void SetIntrVecFunc(IntrVecFunc func) { intrVecFunc = func; }
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
	int ResolvC();
	int ResolvV();
	int ResolvZ();
	int ResolvN();
	int ResolvX();
	int ResolvFlags();
	void SetupFlags(int x);
	int X() { return ResolvX() != 0; }
	void fset0(u32 dm) {
		fp->dm = dm;
		if (++fp >= fbuf + FBUFMAX) ResolvFlags();
	}
	u32 fset2(u32 dm, u32 r, u8 size) {
		fp->dm = dm; fp->r = r; fp->size = size;
		if (++fp >= fbuf + FBUFMAX) ResolvFlags();
		return r;
	}
	u32 fset(u32 dm, u32 s, u32 d, u32 r, u8 size) {
		fp->dm = dm; fp->s = s; fp->d = d; fp->r = r; fp->size = size;
		if (++fp >= fbuf + FBUFMAX) ResolvFlags();
		return r;
	}
	struct FlagDecision {
		u32 dm;
		u32 s, d, r;
		u8 size;
	};
	FlagDecision fbuf[FBUFMAX];
	FlagDecision *fp;
	u8 *m;
	u32 a[8], d[8];
	u16 sr;
	u32 cr[16];
	u32 pc, trace_pc;
	int intreq, trace_flag;
	IntrVecFunc intrVecFunc;
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
	// M68000PRM.pdf page 3-18
	u32 fadd(u32 s, u32 d, u32 r, u8 sz) { return fset(XADD | NS | ZS | VADD | CADD, s, d, r, sz); }
	u32 faddx(u32 s, u32 d, u32 r, u8 sz) { return fset(XADD | NS | ZSX | VADD | CADD, s, d, r, sz); }
	u32 fsub(u32 s, u32 d, u32 r, u8 sz) { return fset(XSUB | NS | ZS | VSUB | CSUB, s, d, r, sz); }
	u32 fsubx(u32 s, u32 d, u32 r, u8 sz) { return fset(XSUB | NS | ZSX | VSUB | CSUB, s, d, r, sz); }
	u32 fcmp(u32 s, u32 d, u32 r, u8 sz) { return fset(NS | ZS | VSUB | CSUB, s, d, r, sz); }
	u32 fmul(u32 s, u32 d, u32 r, u8 type) { return fset(NMUL | ZMUL | VMUL | C0, s, d, r, type); }
	u32 flogic(u32 r, u8 sz) { return fset2(NS | ZS | V0 | C0, r, sz); }
	u32 fbtst(u32 r, u8 sz) { return fset2(ZS, r, sz); }
	u32 fbf(u32 r, u8 width) { return fset2(NBF | ZBF | V0 | C0, r, width); }
	u32 fbcd(u32 x, u32 r) { return fset(XBCD | N0 | ZSX | V0 | CBCD, x, 0, r, 0); }
	u32 fchk(u32 r) { return fset2(NS | Z0 | V0 | C0, r, 0); }
#define R0		(op & 7)
#define R9		(op >> 9 & 7)
	template<int M> void movea_w(u16 op) { // movea.w <ea>,An
		ea<1, M, 1>(op, [&](s16 v) { stA(R9, v); });
	}
	template<int M> void movea_l(u16 op) { // movea.l <ea>,An
		ea<1, M, 2>(op, [&](u32 v) { stA(R9, v); });
	}
	template<int M1, int M2, int S> void move_ea_ea(u16 op) { // move <ea>,<ea>
		ea<1, M1, S>(op, [&](u32 v) { ea<2, M2, S>(R9, [&]{ return flogic(v, S); }); });
	}
	void moveq(u16 op) { d[R9] = flogic(s8(op & 0xff), 2); } // moveq #<data>,Dn
	template<int M, int S> void movem_rm(u16 op);
	template<int M, int S> void movem_mr(u16 op);
	void movep(u16 op);
	void movec(u16 op);
	template<int M> void move_ccr_ea(u16 op) { // move ccr,<ea>
		ea<2, M, 1>(op, [&]{ return ResolvFlags(); });
	}
	template<int M> void move_ea_ccr(u16 op) { // move <ea>,ccr
		ea<1, M, 1>(op, [&](u32 v) { sr = (sr & 0xff00) | (v & 0x1f); SetupFlags(v); });
	}
	template<int M> void move_sr_ea(u16 op) { // move sr,<ea>
		if constexpr (MPU_TYPE >= 68010)
			if (!(sr & MS)) Trap(8);
		ea<2, M, 1>(op, [&]{ return (sr & 0xff00) | ResolvFlags(); });
	}
	template<int M> void move_ea_sr(u16 op) { // move <ea>,sr
		ea<1, M, 1>(op, [&](u32 v) { SetSR(v); });
	}
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
			ea<2, M, S>(op, [&]{ return flogic(0, 0); });
		//else ea<3, M, S>(op, [&](u32) { return flogic(0, 0); });
	}
	void swap(u16 op) { stD<2>(R0, flogic(d[R0] << 16 | d[R0] >> 16, 2)); } // swap Dn
	template<int M, int S> void add_ea_d(u16 op) { // add <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, fadd(v, d[R9], d[R9] + v, S)); });
	}
	template<int M, int S> void add_d_ea(u16 op) { // add Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return fadd(d[R9], v, v + d[R9], S); });
	}
	template<int M> void adda_w(u16 op) { // adda.w <ea>,An
		ea<1, M, 1>(op, [&](s16 v) { stA(R9, a[R9] + v); });
	}
	template<int M> void adda_l(u16 op) { // adda.l <ea>,An
		ea<1, M, 2>(op, [&](u32 v) { stA(R9, a[R9] + v); });
	}
	template<int M, int S> void addi(u16 op) { // addi #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return fadd(t, v, v + t, S); });
	}
	template<int M, int S> void addq(u16 op) { // addq #<data>,<ea>
		ea<3, M, S>(op, [&](u32 v) { u32 t = ((op >> 9) - 1 & 7) + 1; return fadd(t, v, v + t, S); });
	}
	void addqa(u16 op) { // addq #<data>,An
		ea<3, 1, 2>(op, [&](u32 v) { return v + ((op >> 9) - 1 & 7) + 1; });
	}
	template<int S> void addx_m(u16 op) { // addx -(Ay),-(Ax)
		u32 s = ld<S>(a[R0] -= 1 << S), v = ld<S>(a[R9] -= 1 << S);
		st<S>(a[R9], faddx(s, v, v + s + X(), S));
	}
	template<int S> void addx_r(u16 op) { // addx Dy,Dx
		stD<S>(R9, faddx(d[R0], d[R9], d[R9] + d[R0] + X(), S));
	}
	template<int M, int S> void sub_ea_d(u16 op) { // sub <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, fsub(v, d[R9], d[R9] - v, S)); });
	}
	template<int M, int S> void sub_d_ea(u16 op) { // sub Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return fsub(d[R9], v, v - d[R9], S); });
	}
	template<int M> void suba_w(u16 op) { // suba.w <ea>,An
		ea<1, M, 1>(op, [&](s16 v) { stA(R9, a[R9] - v); });
	}
	template<int M> void suba_l(u16 op) { // suba.l <ea>,An
		ea<1, M, 2>(op, [&](u32 v) { stA(R9, a[R9] - v); });
	}
	template<int M, int S> void subi(u16 op) { // sbui #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return fsub(t, v, v - t, S); });
	}
	template<int M, int S> void subq(u16 op) { // subq #<data>,<ea>
		ea<3, M, S>(op, [&](u32 v) { u32 t = ((op >> 9) - 1 & 7) + 1; return fsub(t, v, v - t, S); });
	}
	void subq_a(u16 op) { // subq #<data>,An
		ea<3, 1, 2>(op, [&](u32 v) { return v - ((op >> 9) - 1 & 7) - 1; });
	}
	template<int S> void subx_m(u16 op) { // subx -(Ay),-(Ax)
		u32 s = ld<S>(a[R0] -= 1 << S), v = ld<S>(a[R9] -= 1 << S);
		st<S>(a[R9], fsubx(s, v, v - s - X(), S));
	}
	template<int S> void subx_r(u16 op) { // subx Dx,Dy
		stD<S>(R9, fsubx(d[R0], d[R9], d[R9] - d[R0] - X(), S));
	}
	template<int M, int S> void cmp(u16 op) { // cmp <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { fcmp(v, d[R9], d[R9] - v, S); });
	}
	template<int M> void cmpa_w(u16 op) { // cmpa.w <ea>,An
		ea<1, M, 1>(op, [&](s16 v) { fcmp(v, a[R9], a[R9] - v, 2); });
	}
	template<int M> void cmpa_l(u16 op) { // cmpa.l <ea>,An
		ea<1, M, 2>(op, [&](u32 v) { fcmp(v, a[R9], a[R9] - v, 2); });
	}
	template<int M, int S> void cmpi(u16 op) { // cmpi #<data>,<ea>
		u32 t = getImm<S>(); ea<1, M, S>(op, [&](u32 v) { fcmp(t, v, v - t, S); });
	}
	template<int S> void cmpm(u16 op) { // cmpm (Ay)+,(Ax)+
		u32 s = ld<S>(a[R0]); a[R0] += 1 << S;
		u32 d = ld<S>(a[R9]); a[R9] += 1 << S;
		fcmp(s, d, d - s, S);
	}
	template<int M, int S> void neg(u16 op) { // neg <ea>
		ea<3, M, S>(op, [&](u32 v) { return fsub(v, 0, -v, S); });
	}
	template<int M, int S> void negx(u16 op) { // negx <ea>
		ea<3, M, S>(op, [&](u32 v) { return fsubx(v, 0, -v - X(), S); });
	}
	template<int M> void muls_w(u16 op) { // muls.w <ea>,Dn
		ea<1, M, 1>(op, [&](s16 v) { stD<2>(R9, fmul(v, (s16)d[R9], (s16)d[R9] * v, 0)); });
	}
	template<int M> void mulu_w(u16 op) { // muls.w <ea>,Dn
		ea<1, M, 1>(op, [&](u16 v) { stD<2>(R9, fmul(v, (u16)d[R9], (u16)d[R9] * v, 0)); });
	}
	template<int M> void mul_l(u16 op);
	template<int M, typename T16, typename T32> void div_w(u16 op);
	template<int M> void divs_w(u16 op) { div_w<M, s16, s32>(op); }
	template<int M> void divu_w(u16 op) { div_w<M, u16, u32>(op); }
	template<int M> void div_l(u16 op);
	template<int M, int S> void and_ea_d(u16 op) { // and <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, flogic(d[R9] & v, S)); });
	}
	template<int M, int S> void  or_ea_d(u16 op) { //  or <ea>,Dn
		ea<1, M, S>(op, [&](u32 v) { stD<S>(R9, flogic(d[R9] | v, S)); });
	}
	template<int M, int S> void and_d_ea(u16 op) { // and Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return flogic(d[R9] & v, S); });
	}
	template<int M, int S> void  or_d_ea(u16 op) { //  or Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return flogic(d[R9] | v, S); });
	}
	template<int M, int S> void eor_d_ea(u16 op) { // eor Dn,<ea>
		ea<3, M, S>(op, [&](u32 v) { return flogic(d[R9] ^ v, S); });
	}
	template<int M, int S> void andi_ea(u16 op) { // andi #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return flogic(t & v, S); });
	}
	template<int M, int S> void  ori_ea(u16 op) { //  ori #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return flogic(t | v, S); });
	}
	template<int M, int S> void eori_ea(u16 op) { // eori #<data>,<ea>
		u32 t = getImm<S>(); ea<3, M, S>(op, [&](u32 v) { return flogic(t ^ v, S); });
	}
	template<int M, int S> void  tst(u16 op) { // tst <ea>
		ea<1, M, S>(op, [&](u32 v) { flogic(v, S); });
	}
	template<int M, int S> void _not(u16 op) { // not <ea>
		ea<3, M, S>(op, [&](u32 v) { return flogic(~v, S); });
	}
	template<int M> void tas(u16 op) { // tas <ea>
		ea<3, M, 0>(op, [&](u32 v) { return flogic(v, 0) | 0x80; });
	}
	void andi_ccr(u16) { // andi #<data>,ccr
		SetupFlags(sr = (sr & 0xff00) | ((ResolvFlags() & fetch2()) & 0xff));
	}
	void  ori_ccr(u16) { //  ori #<data>,ccr
		SetupFlags(sr = (sr & 0xff00) | ((ResolvFlags() | fetch2()) & 0xff));
	}
	void eori_ccr(u16) { // eori #<data>,ccr
		SetupFlags(sr = (sr & 0xff00) | ((ResolvFlags() ^ fetch2()) & 0xff));
	}
	void andi_sr(u16) { SetSR(((sr & 0xff00) | ResolvFlags()) & fetch2()); } // andi #<data>,sr
	void  ori_sr(u16) { SetSR(((sr & 0xff00) | ResolvFlags()) | fetch2()); } //  ori #<data>,sr
	void eori_sr(u16) { SetSR(((sr & 0xff00) | ResolvFlags()) ^ fetch2()); } // eori #<data>,sr
	template<int W, int M, typename F> void bitop(u16 op, F func);
	template<int M> void btst(u16 op) { // btst Dn,<ea>
		bitop<2, M>(op, []{});
	}
	template<int M> void bclr(u16 op) { // bclr Dn,<ea>
		bitop<3, M>(op, [](u32 v, u32 m) { return v & ~m; });
	}
	template<int M> void bset(u16 op) { // bset Dn,<ea>
		bitop<3, M>(op, [](u32 v, u32 m) { return v | m; });
	}
	template<int M> void bchg(u16 op) { // bchg Dn,<ea>
		bitop<3, M>(op, [](u32 v, u32 m) { return v ^ m; });
	}
	template<int M> void btst_i(u16 op) { // btst #<data>,<ea>
		bitop<0, M>(op, []{});
	}
	template<int M> void bclr_i(u16 op) { // bclr #<data>,<ea>
		bitop<1, M>(op, [](u32 v, u32 m) { return v & ~m; });
	}
	template<int M> void bset_i(u16 op) { // bset #<data>,<ea>
		bitop<1, M>(op, [](u32 v, u32 m) { return v | m; });
	}
	template<int M> void bchg_i(u16 op) { // bchg #<data>,<ea>
		bitop<1, M>(op, [](u32 v, u32 m) { return v ^ m; });
	}
	template<int M> void bitfield(u16 op);
	template <int M, int S> void sftrot(u16 op);
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
	template<int M, int C> void scc(u16 op) { // scc <ea>
		ea<2, M, 0>(op, [&]{ return cond<C>() ? 0xff : 0; });
	}
	template<int C> void dbcc(u16 op);
	template<int C> void trapcc(u16 op) { // trapcc #<data>
		if (!(op & 4)) pc += (op & 2) + ((op & 1) << 1);
		if (cond<C>()) Trap(7);
	}
	void trap(u16 op) { Trap(32 + (op & 0xf)); } // trap #<vector>
	void trapv(u16) { if (ResolvV()) Trap(7); }
	void rts(u16) { pc = pop4(); }
	void rte(u16);
	void rtd(u16) { u32 t = pop4(); a[7] += fetch2(); pc = t; } // rtd #<displacement>
	void rtr(u16) { SetupFlags(pop2()); pc = pop4(); }
	void exg_dd(u16 op) { std::swap(d[R9], d[R0]); } // exg Dx,Dy
	void exg_aa(u16 op) { std::swap(a[R9], a[R0]); } // exg Ax,Ay
	void exg_da(u16 op) { std::swap(d[R9], a[R0]); } // exg Dx,Ay
	void ext_w(u16 op) { stD<1>(R0, flogic((s8)d[R0], 0)); } // ext.w
	void ext_l(u16 op) { flogic(d[R0] = (s16)d[R0], 2); } // ext.l
	void ext_bl(u16 op) { flogic(d[R0] = (s8)d[R0], 2); } // extb.l
	template<int M> void lea(u16 op) { stA(R9, ea<0, M, 2>(op, []{})); } // lea <ea>,An
	template<int M> void chk_w(u16 op) { // chk.w <ea>,Dn
		s16 x = d[R9];
		if (x < 0) { fchk(0); Trap(6); }
		else ea<1, M, 1>(op, [&](s16 v) { if (x > v) { fchk(-1); Trap(6); } });
	}
	template<int M> void chk_l(u16 op) { // chk.l <ea>,Dn
		s32 x = d[R9];
		if (x < 0) { fchk(0); Trap(6); }
		else ea<1, M, 1>(op, [&](s32 v) { if (x > v) { fchk(-1); Trap(6); } });
	}
	void undef(u16);
	void reset(u16) { fprintf(stderr, "RESET instruction\n"); }
	void a_line(u16) { pc -= 2; Trap(10); }
	void f_line(u16 op) {  // CINV,cp*,CPUSH,FPinst,MOVE16
		pc -= 2; Trap(11); fprintf(stderr, "F-line trap: %04x\n", op);
	}
	void nop(u16) {}
	template <int M, int S> void cas(u16 op);
	void bkpt(u16) { fprintf(stderr, "BKPT\n"); exit(1); }
	void x00c0(u16) { fprintf(stderr, "CMP2/CHK2/CALLM/RETM\n"); exit(1); }
	void x08c0(u16) { fprintf(stderr, "CAS2/MOVES\n"); exit(1); }
	void xf280(u16) { pc += 2; } // for booting KT7.5.3
	void emulop(u16); // BasiliskII
};
