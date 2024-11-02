// TinyPPC
// Copyright 2023-2024 © Yasuo Kuwahara
// MIT License

#include <cstdint>
#include <cstring>

#define TINYPPC_TRACE		0

#if TINYPPC_TRACE
#define TINYPPC_TRACE_LOG(adr, data, type) \
	if (tracep->index < ACSMAX) tracep->acs[tracep->index++] = { adr, data, type }
#else
#define TINYPPC_TRACE_LOG(adr, data, type)
#endif

class TinyPPC {
	friend class Insn;
	friend class powerpc_cpu; // SheepShaver
	friend class sheepshaver_cpu; // SheepShaver
	using s8 = int8_t;
	using u8 = uint8_t;
	using s16 = int16_t;
	using u16 = uint16_t;
	using s32 = int32_t;
	using u32 = uint32_t;
	using s64 = int64_t;
	using u64 = uint64_t;
	static constexpr u32 MSB = 0x80000000U, MSD = 0xf0000000U;
	union FPR { u64 i; double f; };
public:
	TinyPPC();
	void SetMemoryPtr(u8 *p) { m = p; }
	void Reset();
	u32 GetPC() const { return pc; }
	void Execute();
	void Interrupt();
	void StopTrace();
private:
	u32 rA(u32 op) const { return gpr[op >> 16 & 0x1f]; }
	u32 rAz(u32 op) const { int n = op >> 16 & 0x1f; return n ? gpr[n] : 0; }
	u32 rB(u32 op) const { return gpr[op >> 11 & 0x1f]; }
	u32 rD(u32 op) const { return gpr[op >> 21 & 0x1f]; }
	u32 rS(u32 op) const { return gpr[op >> 21 & 0x1f]; } // alias of rD
	void stA(u32 op, u32 data) {
		u32 n = op >> 16 & 0x1f;
		gpr[n] = data;
		TINYPPC_TRACE_LOG(n, gpr[n], acsStoreG);
	}
	void stD(u32 op, u32 data) {
		u32 n = op >> 21 & 0x1f;
		gpr[n] = data;
		TINYPPC_TRACE_LOG(n, gpr[n], acsStoreG);
	}
	FPR frA(u32 op) const { return fpr[op >> 16 & 0x1f]; }
	FPR frB(u32 op) const { return fpr[op >> 11 & 0x1f]; }
	FPR frC(u32 op) const { return fpr[op >> 6 & 0x1f]; }
	FPR frS(u32 op) const { return fpr[op >> 21 & 0x1f]; }
	void stD(u32 op, FPR v) {
		u32 n = op >> 21 & 0x1f;
		fpr[n] = v;
		TINYPPC_TRACE_LOG(n, fpr[n].i, acsStoreF);
	}
	template<typename T> u32 cmp3(T a, T b) { return a < b ? MSB : a > b ? MSB >> 1 : MSB >> 2; }
	void update_cr0(s32 v) {
		cr = (cr & ~MSD) | cmp3(v, 0) | (xer & MSB) >> 3;
	}
	void update_cr1() {
		cr = (cr & ~(MSD >> 4)) | (fpscr & MSD) >> 4;
	}
	void update_cr(u32 op, u32 v) {
		int s = (op >> 23 & 7) << 2;
		cr = (cr & ~(MSD >> s)) | (v & MSD) >> s;
	}
	void update_ca(u32 v) {
		if (v) xer |= MSB >> 2;
		else xer &= ~(MSB >> 2);
	}
	void update_ov(u32 v) {
		if (v) xer |= 3 << 30;
		else xer &= ~(1 << 30);
	}
	// memory access -- start
	u32 fetch4() {
		u32 v = __builtin_bswap32((u32 &)m[pc]);
		pc += 4;
		return v;
	}
	u32 ld1(u32 adr) {
		u32 v = m[adr];
		TINYPPC_TRACE_LOG(adr, v, acsLoad8);
		return v;
	}
	u32 ld2(u32 adr) {
		u32 v = __builtin_bswap16((u16 &)m[adr]);
		TINYPPC_TRACE_LOG(adr, v, acsLoad16);
		return v;
	}
	u32 ld4(u32 adr) {
		u32 v = __builtin_bswap32((u32 &)m[adr]);
		TINYPPC_TRACE_LOG(adr, v, acsLoad32);
		return v;
	}
	u64 ld8(u32 adr) {
		u64 v = __builtin_bswap64((u64 &)m[adr]);
		TINYPPC_TRACE_LOG(adr, v, acsLoad64);
		return v;
	}
	u32 ld2r(u32 adr) {
		u32 v = (u16 &)m[adr];
		TINYPPC_TRACE_LOG(adr, v, acsLoad16);
		return v;
	}
	u32 ld4r(u32 adr) {
		u32 v = (u32 &)m[adr];
		TINYPPC_TRACE_LOG(adr, v, acsLoad32);
		return v;
	}
	void st1(u32 adr, u8 data) {
		m[adr] = data;
		TINYPPC_TRACE_LOG(adr, data, acsStore8);
	}
	void st2(u32 adr, u16 data) {
		(u16 &)m[adr] = __builtin_bswap16(data);
		TINYPPC_TRACE_LOG(adr, data, acsStore16);
	}
	void st4(u32 adr, u32 data) {
		(u32 &)m[adr] = __builtin_bswap32(data);
		TINYPPC_TRACE_LOG(adr, data, acsStore32);
	}
	void st8(u32 adr, u64 data) {
		(u64 &)m[adr] = __builtin_bswap64(data);
		TINYPPC_TRACE_LOG(adr, data, acsStore64);
	}
	void st2r(u32 adr, u16 data) {
		(u16 &)m[adr] = data;
		TINYPPC_TRACE_LOG(adr, data, acsStore16);
	}
	void st4r(u32 adr, u32 data) {
		(u32 &)m[adr] = data;
		TINYPPC_TRACE_LOG(adr, data, acsStore32);
	}
	void stN(u32 adr, u8 data, u32 n) {
		memset(&m[adr], data, n);
	}
	// memory access -- end
#if TINYPPC_TRACE
	static constexpr int TRACEMAX = 10000;
	static constexpr int ACSMAX = 32;
	enum {
		acsStoreG = 1, acsStoreF,
		acsStore8, acsStore16, acsStore32, acsStore64, acsLoad8, acsLoad16, acsLoad32, acsLoad64
	};
	struct Acs {
		u32 adr;
		u64 data;
		u8 type;
	};
	struct TraceBuffer {
		u32 pc, cr;
		Acs acs[ACSMAX];
		u8 index;
	};
	TraceBuffer tracebuf[TRACEMAX];
	TraceBuffer *tracep;
#endif
	template<int M> void addsub(u32 op);
	template<int M, typename T16, typename T32> void cmp(u32 op) {
		T32 a = rA(op), b;
		if constexpr (M) b = (T16)op;
		else b = rB(op);
		update_cr(op, cmp3(a, b) | (xer & MSB) >> 3);
	}
	template<int M> void cmp(u32 op) { cmp<M, s16, s32>(op); }
	template<int M> void cmpl(u32 op) { cmp<M, u16, u32>(op); }
	template<int M> void mul(u32 op);
	template<int M, typename T> void div(u32 op);
	template<int M> void divw(u32 op) { div<M, s32>(op); }
	template<int M> void divwu(u32 op) { div<M, u32>(op); }
	template<int M> void logic(u32 op);
	template<int M> void cntlzw(u32 op) {
		u32 v = 0;
		for (u32 s = rS(op), m = MSB; !(s & m) && v < 32; m >>= 1) v++;
		if constexpr (M) update_cr0(v);
		stA(op, v);
	}
	template<int M, typename T> void ext(u32 op) {
		u32 v = T(rS(op));
		if constexpr (M) update_cr0(v);
		stA(op, v);
	}
	template<int M> void extsb(u32 op) { ext<M, s8>(op); }
	template<int M> void extsh(u32 op) { ext<M, s16>(op); }
	template<int M> void sftrot(u32 op);
	template<int M> void farith(u32 op);
	void mcrf(u32 op) { update_cr(op, cr << ((op >> 18 & 7) << 2)); }
	void mcrfs(u32 op) {
		u32 s = (op >> 18 & 7) << 2;
		update_cr(op, fpscr << s);
		fpscr &= ~(MSD >> s) | 0x6007f0ff;
	}
	template<int M> void mffs(u32 op) {
		stD(op, FPR { .i = fpscr });
		if constexpr (M) update_cr1();
	}
	template<int M> void mtfs(u32 op);
	template<int M> void loadstore(u32 op);
	template<int M> void branch(u32 op);
	template<int M> void cond(u32 op);
	void mcrxr(u32 op) { update_cr(op, xer); xer &= ~MSD; }
	void mfcr(u32 op) { stD(op, cr); }
	void mfspr(u32 op);
	void mftb(u32 op);
	void mtcrf(u32 op);
	void mtspr(u32 op);
	void dcbz(u32 op) { stN(rAz(op) + rB(op) & ~0x1f, 0, 0x20); }
	void nop(u32) {}
	void undef(u32 op);
	void sheep(u32 op);
	void sheepi();
//
	u8 *m;
	u32 gpr[32];
	FPR fpr[32];
	u32 pc, lr, ctr, cr, xer, fpscr, reserve_adr;
	bool reserve;
};
