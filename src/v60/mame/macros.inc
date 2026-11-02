// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Transplanted from MAME src/devices/cpu/v60 by scripts/transplant_v60.py:
// regenerate, do not edit (THIRD_PARTY.md).
#undef _S
// memory accessors
#define OpRead8(a)   m_program->op_read8(a)
#define OpRead16(a)  m_program->op_read16(a)
#define OpRead32(a)  m_program->op_read32(a)


// macros stolen from MAME for flags calc
// note that these types are in x86 naming:
// byte = 8 bit, word = 16 bit, long = 32 bit

// parameter x = result, y = source 1, z = source 2

#define SetOFL_Add(x, y,z)  (_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x80000000) ? 1: 0)
#define SetOFW_Add(x, y,z)  (_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x8000) ? 1 : 0)
#define SetOFB_Add(x, y,z)  (_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x80) ? 1 : 0)

#define SetOFL_Sub(x, y,z)  (_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x80000000) ? 1 : 0)
#define SetOFW_Sub(x, y,z)  (_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x8000) ? 1 : 0)
#define SetOFB_Sub(x, y,z)  (_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x80) ? 1 : 0)

#define SetCFB(x)           {_CY = ((x) & 0x100) ? 1 : 0; }
#define SetCFW(x)           {_CY = ((x) & 0x10000) ? 1 : 0; }
#define SetCFL(x)           {_CY = ((x) & (((uint64_t)1) << 32)) ? 1 : 0; }

#define SetSF(x)            (_S = (x))
#define SetZF(x)            (_Z = (x))

#define SetSZPF_Byte(x)     {_Z = ((uint8_t)(x) == 0);  _S = ((x)&0x80) ? 1 : 0; }
#define SetSZPF_Word(x)     {_Z = ((uint16_t)(x) == 0);  _S = ((x)&0x8000) ? 1 : 0; }
#define SetSZPF_Long(x)     {_Z = ((uint32_t)(x) == 0);  _S = ((x)&0x80000000) ? 1 : 0; }

#define ORB(dst, src)       { (dst) |= (src); _OV = 0; SetSZPF_Byte(dst); }
#define ORW(dst, src)       { (dst) |= (src); _OV = 0; SetSZPF_Word(dst); }
#define ORL(dst, src)       { (dst) |= (src); _OV = 0; SetSZPF_Long(dst); }

#define ANDB(dst, src)      { (dst) &= (src); _OV = 0; SetSZPF_Byte(dst); }
#define ANDW(dst, src)      { (dst) &= (src); _OV = 0; SetSZPF_Word(dst); }
#define ANDL(dst, src)      { (dst) &= (src); _OV = 0; SetSZPF_Long(dst); }

#define XORB(dst, src)      { (dst) ^= (src); _OV = 0; SetSZPF_Byte(dst); }
#define XORW(dst, src)      { (dst) ^= (src); _OV = 0; SetSZPF_Word(dst); }
#define XORL(dst, src)      { (dst) ^= (src); _OV = 0; SetSZPF_Long(dst); }

#define SUBB(dst, src, c)   { unsigned res = (dst) - (src) - (c); SetCFB(res); SetOFB_Sub(res, src, dst); SetSZPF_Byte(res); dst = (uint8_t)res; }
#define SUBW(dst, src, c)   { unsigned res = (dst) - (src) - (c); SetCFW(res); SetOFW_Sub(res, src, dst); SetSZPF_Word(res); dst = (uint16_t)res; }
#define SUBL(dst, src, c)   { uint64_t res = (uint64_t)(dst) - (int64_t)(src) - (c); SetCFL(res); SetOFL_Sub(res, src, dst); SetSZPF_Long(res); dst = (uint32_t)res; }

#define ADDB(dst, src, c)   { unsigned res = (dst) + (src) + (c); SetCFB(res); SetOFB_Add(res, src, dst); SetSZPF_Byte(res); dst = (uint8_t)res; }
#define ADDW(dst, src, c)   { unsigned res = (dst) + (src) + (c); SetCFW(res); SetOFW_Add(res, src, dst); SetSZPF_Word(res); dst = (uint16_t)res; }
#define ADDL(dst, src, c)   { uint64_t res = (uint64_t)(dst) + (uint64_t)(src) + (c); SetCFL(res); SetOFL_Add(res, src, dst); SetSZPF_Long(res); dst = (uint32_t)res; }

#define SETREG8(a, b)       (a) = ((a) & ~0xff) | ((b) & 0xff)
#define SETREG16(a, b)      (a) = ((a) & ~0xffff) | ((b) & 0xffff)


/*
 * Prevent warnings on NetBSD.  All identifiers beginning with an underscore
 * followed by an uppercase letter are reserved by the C standard (ISO / IEC
 * 9899:1999, 7.1.3) to be used by the implementation.  It'd be best to rename
 * all such instances, but this is less intrusive and error-prone.
 */
#undef _S

#define _CY     m_flags.CY
#define _OV     m_flags.OV
#define _S      m_flags.S
#define _Z      m_flags.Z


// Defines of all v60 register...
#define R0      m_reg[0]
#define R1      m_reg[1]
#define R2      m_reg[2]
#define R3      m_reg[3]
#define R4      m_reg[4]
#define R5      m_reg[5]
#define R6      m_reg[6]
#define R7      m_reg[7]
#define R8      m_reg[8]
#define R9      m_reg[9]
#define R10     m_reg[10]
#define R11     m_reg[11]
#define R12     m_reg[12]
#define R13     m_reg[13]
#define R14     m_reg[14]
#define R15     m_reg[15]
#define R16     m_reg[16]
#define R17     m_reg[17]
#define R18     m_reg[18]
#define R19     m_reg[19]
#define R20     m_reg[20]
#define R21     m_reg[21]
#define R22     m_reg[22]
#define R23     m_reg[23]
#define R24     m_reg[24]
#define R25     m_reg[25]
#define R26     m_reg[26]
#define R27     m_reg[27]
#define R28     m_reg[28]
#define AP      m_reg[29]
#define FP      m_reg[30]
#define SP      m_reg[31]

#define PC      m_reg[32]
#define PSW     m_reg[33]

// Privileged registers
#define ISP     m_reg[36]
#define L0SP    m_reg[37]
#define L1SP    m_reg[38]
#define L2SP    m_reg[39]
#define L3SP    m_reg[40]
#define SBR     m_reg[41]
#define TR      m_reg[42]
#define SYCW    m_reg[43]
#define TKCW    m_reg[44]
#define PIR     m_reg[45]
//10-14 reserved
#define PSW2    m_reg[51]
#define ATBR0   m_reg[52]
#define ATLR0   m_reg[53]
#define ATBR1   m_reg[54]
#define ATLR1   m_reg[55]
#define ATBR2   m_reg[56]
#define ATLR2   m_reg[57]
#define ATBR3   m_reg[58]
#define ATLR3   m_reg[59]
#define TRMODE  m_reg[60]
#define ADTR0   m_reg[61]
#define ADTR1   m_reg[62]
#define ADTMR0  m_reg[63]
#define ADTMR1  m_reg[64]
//29-31 reserved

// Defines...
#define NORMALIZEFLAGS() \
{ \
	_S    = _S  ? 1 : 0; \
	_OV   = _OV ? 1 : 0; \
	_Z    = _Z  ? 1 : 0; \
	_CY   = _CY ? 1 : 0; \
}


#define GETINTVECT(nint)                    m_program->read_dword((SBR & ~0xfff) + (nint) * 4)
#define EXCEPTION_CODE_AND_SIZE(code, size) (((code) << 16) | (size))
