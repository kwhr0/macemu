/*
 *  ppc-bitfields.hpp - Instruction fields
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

#ifndef PPC_BITFIELDS_H
#define PPC_BITFIELDS_H

///
///		Bitfield management
///

template< int FB, int FE >
struct static_mask {
	enum { value = (0xffffffff >> FB) ^ (0xffffffff >> (FE + 1)) };
};

template< int FB >
struct static_mask<FB, 31> {
	enum { value  = 0xffffffff >> FB };
};

template< int FB, int FE >
struct bit_field {
	static inline uint32 mask() {
		return static_mask<FB, FE>::value;
	}
	static inline bool test(uint32 value) {
		return value & mask();
	}
	static inline uint32 extract(uint32 value) {
		const uint32 m = mask() >> (31 - FE);
		return (value >> (31 - FE)) & m;
	}
	static inline void insert(uint32 & data, uint32 value) {
		const uint32 m = mask();
		data = (data & ~m) | ((value << (31 - FE)) & m);
	}
};

template< int CRn > struct CR_SO_field : bit_field< 4*CRn+3, 4*CRn+3 > { };

#endif /* PPC_BITFIELDS_H */
