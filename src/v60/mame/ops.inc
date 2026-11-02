// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Transplanted from MAME src/devices/cpu/v60 by scripts/transplant_v60.py:
// regenerate, do not edit (THIRD_PARTY.md).
/*
 * DIVX: the second operand should be treated as dword instead of word
 * GETATE, GETPTE and GETRA should not be used
 * UPDPSW: _CY and _OV must be cleared or unchanged? I suppose
 *   cleared, like TEST being done on the mask operand.
 * MOVT: I cannot understand exactly what happens to the result
 *   when an overflow occurs
 *
 * Unimplemented opcodes:
 * ROTC, UPDATE, UPDPTE
 */


/*
 *  Macro to access data in operands decoded with ReadAMAddress()
 */

#define F12LOADOPBYTE(num)                          \
	if (m_flag##num)                                \
		appb = (uint8_t)m_reg[m_op##num];         \
	else                                                \
		appb = m_program->read_byte(m_op##num);

#define F12LOADOPHALF(num)                          \
	if (m_flag##num)                                \
		apph = (uint16_t)m_reg[m_op##num];        \
	else                                                \
		apph = m_program->read_word_unaligned(m_op##num);

#define F12LOADOPWORD(num)                          \
	if (m_flag##num)                                \
		appw = m_reg[m_op##num];                \
	else                                                \
		appw = m_program->read_dword_unaligned(m_op##num);

#define F12STOREOPBYTE(num)                         \
	if (m_flag##num)                                \
		SETREG8(m_reg[m_op##num], appb);        \
	else                                                \
		m_program->write_byte(m_op##num, appb);

#define F12STOREOPHALF(num)                         \
	if (m_flag##num)                                \
		SETREG16(m_reg[m_op##num], apph);       \
	else                                                \
		m_program->write_word_unaligned(m_op##num, apph);

#define F12STOREOPWORD(num)                         \
	if (m_flag##num)                                \
		m_reg[m_op##num] = appw;                \
	else                                                \
		m_program->write_dword_unaligned(m_op##num, appw);

#define F12LOADOP1BYTE()  F12LOADOPBYTE(1)
#define F12LOADOP1HALF()  F12LOADOPHALF(1)
#define F12LOADOP1WORD()  F12LOADOPWORD(1)

#define F12LOADOP2BYTE()  F12LOADOPBYTE(2)
#define F12LOADOP2HALF()  F12LOADOPHALF(2)
#define F12LOADOP2WORD()  F12LOADOPWORD(2)

#define F12STOREOP1BYTE()  F12STOREOPBYTE(1)
#define F12STOREOP1HALF()  F12STOREOPHALF(1)
#define F12STOREOP1WORD()  F12STOREOPWORD(1)

#define F12STOREOP2BYTE()  F12STOREOPBYTE(2)
#define F12STOREOP2HALF()  F12STOREOPHALF(2)
#define F12STOREOP2WORD()  F12STOREOPWORD(2)

#define F12END()                                  \
	return m_amlength1 + m_amlength2 + 2;


// Decode the first operand of the instruction and prepare
// writing to the second operand.






// Decode both format 1 / 2 operands


template <class H> uint32_t opADDB()
{
	H::enter(*this, "opADDB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	ADDB(appb, (uint8_t)m_op1, 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opADDH()
{
	H::enter(*this, "opADDH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	ADDW(apph, (uint16_t)m_op1, 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opADDW()
{
	H::enter(*this, "opADDW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	ADDL(appw, (uint32_t)m_op1, 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opADDCB()
{
	H::enter(*this, "opADDCB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	ADDB(appb, (uint8_t)m_op1, _CY?1:0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opADDCH()
{
	H::enter(*this, "opADDCH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	ADDW(apph, (uint16_t)m_op1, _CY?1:0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opADDCW()
{
	H::enter(*this, "opADDCW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	ADDL(appw, (uint32_t)m_op1, _CY?1:0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opANDB()
{
	H::enter(*this, "opANDB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	appb &= m_op1;
	_OV = 0;
	_S = ((appb & 0x80) != 0);
	_Z = (appb == 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opANDH()
{
	H::enter(*this, "opANDH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	apph &= m_op1;
	_OV = 0;
	_S = ((apph & 0x8000) != 0);
	_Z = (apph == 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opANDW()
{
	H::enter(*this, "opANDW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	appw &= m_op1;
	_OV = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opCALL()
{
	H::enter(*this, "opCALL");
	H::F12DecodeOperands(*this, Am::Addr, 0,Am::Addr, 2);

	SP -= 4;
	m_program->write_dword_unaligned(SP, AP);
	AP = m_op2;

	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + m_amlength1 + m_amlength2 + 2);
	PC = m_op1;

	return 0;
}


template <class H> uint32_t opCHKAR()
{
	H::enter(*this, "opCHKAR");
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Read, 0);

	// No MMU and memory permissions yet @@@
	_Z = 1;
	_CY = 0;
	_S = 0;

	F12END();
}


template <class H> uint32_t opCHKAW()
{
	H::enter(*this, "opCHKAW");
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Read, 0);

	// No MMU and memory permissions yet @@@
	_Z = 1;
	_CY = 0;
	_S = 0;

	F12END();
}


template <class H> uint32_t opCHKAE()
{
	H::enter(*this, "opCHKAE");
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Read, 0);

	// No MMU and memory permissions yet @@@
	_Z = 1;
	_CY = 0;
	_S = 0;

	F12END();
}


template <class H> uint32_t opCHLVL()
{
	H::enter(*this, "opCHLVL");
	uint32_t oldPSW;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Read, 0);

	if (m_op1 > 3)
	{
		v60_fatal("Illegal data field on opCHLVL, PC=%x\n", PC);
	}

	oldPSW = v60_update_psw_for_exception(0, m_op1);

	SP -= 4;
	m_program->write_dword_unaligned(SP, m_op2);

	SP -= 4;
	m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x1800 + m_op1 * 0x100, 8));

	SP -= 4;
	m_program->write_dword_unaligned(SP, oldPSW);

	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + m_amlength1 + m_amlength2 + 2);

	PC = GETINTVECT(24 + m_op1);

	return 0;
}


template <class H> uint32_t opCLR1()
{
	H::enter(*this, "opCLR1");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_CY = ((appw & (1 << m_op1)) != 0);
	_Z = !(_CY);

	appw &= ~(1 << m_op1);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opCMPB()
{
	H::enter(*this, "opCMPB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Read, 0);

	appb = (uint8_t)m_op2;
	SUBB(appb, (uint8_t)m_op1, 0);

	F12END();
}


template <class H> uint32_t opCMPH()
{
	H::enter(*this, "opCMPH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Read, 1);

	apph = (uint16_t)m_op2;
	SUBW(apph, (uint16_t)m_op1, 0);

	F12END();
}



template <class H> uint32_t opCMPW()
{
	H::enter(*this, "opCMPW");
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Read, 2);

	SUBL(m_op2, (uint32_t)m_op1, 0);

	F12END();
}


template <class H> uint32_t opDIVB()
{
	H::enter(*this, "opDIVB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	_OV = ((appb == 0x80) && (m_op1 == 0xFF));
	if (m_op1 && !_OV)
		appb= (int8_t)appb / (int8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opDIVH()
{
	H::enter(*this, "opDIVH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	_OV = ((apph == 0x8000) && (m_op1 == 0xFFFF));
	if (m_op1 && !_OV)
		apph = (int16_t)apph / (int16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opDIVW()
{
	H::enter(*this, "opDIVW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_OV = ((appw == 0x80000000) && (m_op1 == 0xFFFFFFFF));
	if (m_op1 && !_OV)
		appw = (int32_t)appw / (int32_t)m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opDIVX()
{
	H::enter(*this, "opDIVX");
	int32_t a, b;
	int64_t dv;

	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 3);

	if (m_flag2)
		dv = ((uint64_t)m_reg[(m_op2 & 0x1F) + 1] << 32) | m_reg[m_op2 & 0x1F];
	else
		dv = m_program->read_qword_unaligned(m_op2);

	a = div_64x32_rem(dv, m_op1, b);

	_S = ((a & 0x80000000) != 0);
	_Z = (a == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
	{
		m_program->write_dword_unaligned(m_op2, a);
		m_program->write_dword_unaligned(m_op2 + 4, b);
	}

	F12END();
}


template <class H> uint32_t opDIVUX()
{
	H::enter(*this, "opDIVUX");
	uint32_t a, b;
	uint64_t dv;

	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 3);

	if (m_flag2)
		dv = ((uint64_t)m_reg[(m_op2 & 0x1F) + 1] << 32) | m_reg[m_op2 & 0x1F];
	else
		dv = m_program->read_qword_unaligned(m_op2);

	a = divu_64x32_rem(dv, m_op1, b);

	_S = ((a & 0x80000000) != 0);
	_Z = (a == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
	{
		m_program->write_dword_unaligned(m_op2, a);
		m_program->write_dword_unaligned(m_op2 + 4, b);
	}

	F12END();
}



template <class H> uint32_t opDIVUB()
{
	H::enter(*this, "opDIVUB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	_OV = 0;
	if (m_op1)  appb /= (uint8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opDIVUH()
{
	H::enter(*this, "opDIVUH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	_OV = 0;
	if (m_op1)  apph /= (uint16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opDIVUW()
{
	H::enter(*this, "opDIVUW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_OV = 0;
	if (m_op1)  appw /= m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opINB()
{
	H::enter(*this, "opINB");
	H::F12DecodeFirstOperand(*this, Am::Addr, 0);
	m_modwritevalb = m_io->read_byte(m_op1);

	if ( m_stall_io )
	{
		m_stall_io = 0;
		return 0;
	}

	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opINH()
{
	H::enter(*this, "opINH");
	H::F12DecodeFirstOperand(*this, Am::Addr, 1);
	m_modwritevalh = m_io->read_word_unaligned(m_op1);

	if ( m_stall_io )
	{
		m_stall_io = 0;
		return 0;
	}

	H::F12WriteSecondOperand(*this, 1);
	F12END();
}


template <class H> uint32_t opINW()
{
	H::enter(*this, "opINW");
	H::F12DecodeFirstOperand(*this, Am::Addr, 2);
	m_modwritevalw = m_io->read_dword_unaligned(m_op1);

	if ( m_stall_io )
	{
		m_stall_io = 0;
		return 0;
	}

	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opLDPR()
{
	H::enter(*this, "opLDPR");
	H::F12DecodeOperands(*this, Am::Addr, 2,Am::Read, 2);
	if (m_op2 <= 28)
	{
		if (m_flag1)
			m_reg[m_op2 + 36] = m_reg[m_op1];
		else
			m_reg[m_op2 + 36] = m_op1;
	}
	else
	{
		v60_fatal("Invalid operand on LDPR PC=%x\n", PC);
	}
	F12END();
}


template <class H> uint32_t opLDTASK()
{
	H::enter(*this, "opLDTASK");
	int i;
	H::F12DecodeOperands(*this, Am::Addr, 2,Am::Read, 2);

	v60WritePSW(v60ReadPSW() & 0xefffffff);

	TR = m_op2;

	TKCW = m_program->read_dword_unaligned(m_op2);
	m_op2 += 4;
	if(SYCW & 0x100) {
		L0SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}
	if(SYCW & 0x200) {
		L1SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}
	if(SYCW & 0x400) {
		L2SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}
	if(SYCW & 0x800) {
		L3SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}

	v60ReloadStack();

	// 31 registers supported, _not_ 32
	for(i = 0; i < 31; i++)
		if(m_op1 & (1 << i)) {
			m_reg[i] = m_program->read_dword_unaligned(m_op2);
			m_op2 += 4;
		}

	// #### Ignore the virtual addressing crap.

	F12END();
}


template <class H> uint32_t opMOVD()
{
	H::enter(*this, "opMOVD");
	uint64_t d;

	H::F12DecodeOperands(*this, Am::Addr, 3,Am::Addr, 3);

	if (m_flag1)
		d = ((uint64_t)m_reg[(m_op1 & 0x1F) + 1] << 32) | m_reg[m_op1 & 0x1F];
	else
		d = m_program->read_qword_unaligned(m_op1);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = (uint32_t)d;
		m_reg[(m_op2 & 0x1F) + 1] = (uint32_t)(d >> 32);
	}
	else
		m_program->write_qword_unaligned(m_op2, d);

	F12END();
}


template <class H> uint32_t opMOVB()
{
	H::enter(*this, "opMOVB");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);
	m_modwritevalb = (uint8_t)m_op1;
	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opMOVH()
{
	H::enter(*this, "opMOVH");
	H::F12DecodeFirstOperand(*this, Am::Read, 1);
	m_modwritevalh = (uint16_t)m_op1;
	H::F12WriteSecondOperand(*this, 1);
	F12END();
}


template <class H> uint32_t opMOVW()
{
	H::enter(*this, "opMOVW");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);
	m_modwritevalw = m_op1;
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVEAB()
{
	H::enter(*this, "opMOVEAB");
	H::F12DecodeFirstOperand(*this, Am::Addr, 0);
	m_modwritevalw = m_op1;
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVEAH()
{
	H::enter(*this, "opMOVEAH");
	H::F12DecodeFirstOperand(*this, Am::Addr, 1);
	m_modwritevalw = m_op1;
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVEAW()
{
	H::enter(*this, "opMOVEAW");
	H::F12DecodeFirstOperand(*this, Am::Addr, 2);
	m_modwritevalw = m_op1;
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVSBH()
{
	H::enter(*this, "opMOVSBH");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);
	m_modwritevalh = (int8_t)(m_op1 & 0xFF);
	H::F12WriteSecondOperand(*this, 1);
	F12END();
}


template <class H> uint32_t opMOVSBW()
{
	H::enter(*this, "opMOVSBW");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);
	m_modwritevalw = (int8_t)(m_op1 & 0xFF);
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVSHW()
{
	H::enter(*this, "opMOVSHW");
	H::F12DecodeFirstOperand(*this, Am::Read, 1);
	m_modwritevalw = (int16_t)(m_op1 & 0xFFFF);
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVTHB()
{
	H::enter(*this, "opMOVTHB");
	H::F12DecodeFirstOperand(*this, Am::Read, 1);
	m_modwritevalb = (uint8_t)(m_op1 & 0xFF);

	// Check for overflow: the truncated bits must match the sign
	//  of the result, otherwise overflow
	if (((m_modwritevalb & 0x80) == 0x80 && ((m_op1 & 0xFF00) == 0xFF00)) ||
			((m_modwritevalb & 0x80) == 0 && ((m_op1 & 0xFF00) == 0x0000)))
		_OV = 0;
	else
		_OV = 1;

	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opMOVTWB()
{
	H::enter(*this, "opMOVTWB");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);
	m_modwritevalb = (uint8_t)(m_op1 & 0xFF);

	// Check for overflow: the truncated bits must match the sign
	//  of the result, otherwise overflow
	if (((m_modwritevalb & 0x80) == 0x80 && ((m_op1 & 0xFFFFFF00) == 0xFFFFFF00)) ||
			((m_modwritevalb & 0x80) == 0 && ((m_op1 & 0xFFFFFF00) == 0x00000000)))
		_OV = 0;
	else
		_OV = 1;

	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opMOVTWH()
{
	H::enter(*this, "opMOVTWH");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);
	m_modwritevalh = (uint16_t)(m_op1 & 0xFFFF);

	// Check for overflow: the truncated bits must match the sign
	//  of the result, otherwise overflow
	if (((m_modwritevalh & 0x8000) == 0x8000 && ((m_op1 & 0xFFFF0000) == 0xFFFF0000)) ||
			((m_modwritevalh & 0x8000) == 0 && ((m_op1 & 0xFFFF0000) == 0x00000000)))
		_OV = 0;
	else
		_OV = 1;

	H::F12WriteSecondOperand(*this, 1);
	F12END();
}



template <class H> uint32_t opMOVZBH()
{
	H::enter(*this, "opMOVZBH");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);
	m_modwritevalh = (uint16_t)m_op1;
	H::F12WriteSecondOperand(*this, 1);
	F12END();
}


template <class H> uint32_t opMOVZBW()
{
	H::enter(*this, "opMOVZBW");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);
	m_modwritevalw = m_op1;
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMOVZHW()
{
	H::enter(*this, "opMOVZHW");
	H::F12DecodeFirstOperand(*this, Am::Read, 1);
	m_modwritevalw = m_op1;
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opMULB()
{
	H::enter(*this, "opMULB");
	uint8_t appb;
	uint32_t tmp;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	tmp = (int8_t)appb * (int32_t)(int8_t)m_op1;
	appb = tmp;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);
	_OV = ((tmp >> 8) != 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opMULH()
{
	H::enter(*this, "opMULH");
	uint16_t apph;
	uint32_t tmp;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	tmp = (int16_t)apph * (int32_t)(int16_t)m_op1;
	apph = tmp;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);
	_OV = ((tmp >> 16) != 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opMULW()
{
	H::enter(*this, "opMULW");
	uint32_t appw;
	uint64_t tmp;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	tmp = mul_32x32(appw, m_op1);
	appw = tmp;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);
	_OV = ((tmp >> 32) != 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opMULUB()
{
	H::enter(*this, "opMULUB");
	uint8_t appb;
	uint32_t tmp;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	tmp = appb * (uint8_t)m_op1;
	appb = tmp;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);
	_OV = ((tmp >> 8) != 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opMULUH()
{
	H::enter(*this, "opMULUH");
	uint16_t apph;
	uint32_t tmp;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	tmp = apph * (uint16_t)m_op1;
	apph = tmp;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);
	_OV = ((tmp >> 16) != 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opMULUW()
{
	H::enter(*this, "opMULUW");
	uint32_t appw;
	uint64_t tmp;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	tmp = mulu_32x32(appw, m_op1);
	appw = tmp;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);
	_OV = ((tmp >> 32) != 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opNEGB()
{
	H::enter(*this, "opNEGB");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);

	m_modwritevalb = 0;
	SUBB(m_modwritevalb, (int8_t)m_op1, 0);
	_CY = m_modwritevalb ? 1 : 0;

	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opNEGH()
{
	H::enter(*this, "opNEGH");
	H::F12DecodeFirstOperand(*this, Am::Read, 1);

	m_modwritevalh = 0;
	SUBW(m_modwritevalh, (int16_t)m_op1, 0);
	_CY = m_modwritevalh ? 1 : 0;

	H::F12WriteSecondOperand(*this, 1);
	F12END();
}


template <class H> uint32_t opNEGW()
{
	H::enter(*this, "opNEGW");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);

	m_modwritevalw = 0;
	SUBL(m_modwritevalw, (int32_t)m_op1, 0);
	_CY = m_modwritevalw ? 1 : 0;

	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opNOTB()
{
	H::enter(*this, "opNOTB");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);
	m_modwritevalb=~m_op1;

	_OV = 0;
	_S = ((m_modwritevalb & 0x80) != 0);
	_Z = (m_modwritevalb == 0);

	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opNOTH()
{
	H::enter(*this, "opNOTH");
	H::F12DecodeFirstOperand(*this, Am::Read, 1);
	m_modwritevalh=~m_op1;

	_OV = 0;
	_S = ((m_modwritevalh & 0x8000) != 0);
	_Z = (m_modwritevalh == 0);

	H::F12WriteSecondOperand(*this, 1);
	F12END();
}


template <class H> uint32_t opNOTW()
{
	H::enter(*this, "opNOTW");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);
	m_modwritevalw=~m_op1;

	_OV = 0;
	_S = ((m_modwritevalw & 0x80000000) != 0);
	_Z = (m_modwritevalw == 0);

	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opNOT1()
{
	H::enter(*this, "opNOT1");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_CY = ((appw & (1 << m_op1)) != 0);
	_Z = !(_CY);

	if (_CY)
		appw &= ~(1 << m_op1);
	else
		appw |= (1 << m_op1);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opORB()
{
	H::enter(*this, "opORB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	ORB(appb, (uint8_t)m_op1);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opORH()
{
	H::enter(*this, "opORH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	ORW(apph, (uint16_t)m_op1);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opORW()
{
	H::enter(*this, "opORW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	ORL(appw, (uint32_t)m_op1);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opOUTB()
{
	H::enter(*this, "opOUTB");
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 2);
	m_io->write_byte(m_op2,(uint8_t)m_op1);
	F12END();
}


template <class H> uint32_t opOUTH()
{
	H::enter(*this, "opOUTH");
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 2);
	m_io->write_word_unaligned(m_op2,(uint16_t)m_op1);
	F12END();
}


template <class H> uint32_t opOUTW()
{
	H::enter(*this, "opOUTW");
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);
	m_io->write_dword_unaligned(m_op2, m_op1);
	F12END();
}


template <class H> uint32_t opREMB()
{
	H::enter(*this, "opREMB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	_OV = 0;
	if (m_op1)
		appb= (int8_t)appb % (int8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opREMH()
{
	H::enter(*this, "opREMH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	_OV = 0;
	if (m_op1)
		apph = (int16_t)apph % (int16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opREMW()
{
	H::enter(*this, "opREMW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_OV = 0;
	if (m_op1)
		appw = (int32_t)appw % (int32_t)m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opREMUB()
{
	H::enter(*this, "opREMUB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	_OV = 0;
	if (m_op1)
		appb %= (uint8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opREMUH()
{
	H::enter(*this, "opREMUH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	_OV = 0;
	if (m_op1)
		apph %= (uint16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opREMUW()
{
	H::enter(*this, "opREMUW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_OV = 0;
	if (m_op1)
		appw %= m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opROTB()
{
	H::enter(*this, "opROTB");
	uint8_t appb;
	int8_t i, count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
			appb = (appb << 1) | ((appb & 0x80) >> 7);

		_CY = (appb & 0x1) != 0;
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
			appb = (appb >> 1) | ((appb & 0x1) << 7);

		_CY = (appb & 0x80) != 0;
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appb & 0x80) != 0;
	_Z = (appb == 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opROTH()
{
	H::enter(*this, "opROTH");
	uint16_t apph;
	int8_t i, count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 1);

	F12LOADOP2HALF();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
			apph = (apph << 1) | ((apph & 0x8000) >> 15);

		_CY = (apph & 0x1) != 0;
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
			apph = (apph >> 1) | ((apph & 0x1) << 15);

		_CY = (apph & 0x8000) != 0;
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (apph & 0x8000) != 0;
	_Z = (apph == 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opROTW()
{
	H::enter(*this, "opROTW");
	uint32_t appw;
	int8_t i, count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 2);

	F12LOADOP2WORD();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
			appw = (appw << 1) | ((appw & 0x80000000) >> 31);

		_CY = (appw & 0x1) != 0;
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
			appw = (appw >> 1) | ((appw & 0x1) << 31);

		_CY = (appw & 0x80000000) != 0;
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appw & 0x80000000) != 0;
	_Z = (appw == 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opROTCB()
{
	H::enter(*this, "opROTCB");
	uint8_t appb;
	int8_t i, cy, count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();
	NORMALIZEFLAGS();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)((appb & 0x80) >> 7);
			appb = (appb << 1) | cy;
		}
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (appb & 1);
			appb = (appb >> 1) | (cy << 7);
		}
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appb & 0x80) != 0;
	_Z = (appb == 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opROTCH()
{
	H::enter(*this, "opROTCH");
	uint16_t apph;
	int8_t i, cy, count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 1);

	F12LOADOP2HALF();
	NORMALIZEFLAGS();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)((apph & 0x8000) >> 15);
			apph = (apph << 1) | cy;
		}
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)(apph & 1);
			apph = (apph >> 1) | ((uint16_t)cy << 15);
		}
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (apph & 0x8000) != 0;
	_Z = (apph == 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opROTCW()
{
	H::enter(*this, "opROTCW");
	uint32_t appw;
	int8_t i, cy, count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 2);

	F12LOADOP2WORD();
	NORMALIZEFLAGS();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)((appw & 0x80000000) >> 31);
			appw = (appw << 1) | cy;
		}
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)(appw & 1);
			appw = (appw >> 1) | ((uint32_t)cy << 31);
		}
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appw & 0x80000000) != 0;
	_Z = (appw == 0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opRVBIT()
{
	H::enter(*this, "opRVBIT");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);

	m_modwritevalb = bitswap<8>(m_op1, 0, 1, 2, 3, 4, 5, 6, 7);

	H::F12WriteSecondOperand(*this, 0);
	F12END();
}


template <class H> uint32_t opRVBYT()
{
	H::enter(*this, "opRVBYT");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);

	m_modwritevalw = swapendian_int32(m_op1);

	H::F12WriteSecondOperand(*this, 2);
	F12END();
}


template <class H> uint32_t opSET1()
{
	H::enter(*this, "opSET1");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	_CY = ((appw & (1 << m_op1)) != 0);
	_Z = !(_CY);

	appw |= (1 << m_op1);

	F12STOREOP2WORD();
	F12END();
}



template <class H> uint32_t opSETF()
{
	H::enter(*this, "opSETF");
	H::F12DecodeFirstOperand(*this, Am::Read, 0);

	// Normalize the flags
	NORMALIZEFLAGS();

	switch (m_op1 & 0xF)
	{
	case 0:
		if (!_OV) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 1:
		if (_OV) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 2:
		if (!_CY) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 3:
		if (_CY) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 4:
		if (!_Z) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 5:
		if (_Z) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 6:
		if (!(_CY | _Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 7:
		if ((_CY | _Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 8:
		if (!_S) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 9:
		if (_S) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 10:
		m_modwritevalb = 1;
		break;
	case 11:
		m_modwritevalb = 0;
		break;
	case 12:
		if (!(_S^_OV)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 13:
		if ((_S^_OV)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 14:
		if (!((_S^_OV)|_Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 15:
		if (((_S^_OV)|_Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	}

	H::F12WriteSecondOperand(*this, 0);

	F12END();
}


/*
#define SHIFTLEFT_OY(val, count, bitsize) \
{\
    uint32_t tmp = ((val) >> (bitsize - 1)) & 1; \
    tmp <<= count; \
    tmp -= 1; \
    tmp <<= (bitsize - (count)); \
    _OV = (((val) & tmp) != tmp); \
    _CY = (((val) & (1 << (count - 1))) != 0); \
}
*/

// During the shift, the overflow is set if the sign bit changes at any point during the shift
#define SHIFTLEFT_OV(val, count, bitsize) \
{\
	uint32_t tmp; \
	if (count == 32) \
		tmp = 0xFFFFFFFF; \
	else \
		tmp = ((1 << (count)) - 1); \
	tmp <<= (bitsize - (count)); \
	if (((val) >> (bitsize - 1)) & 1) \
		_OV = (((val) & tmp) != tmp); \
	else \
		_OV = (((val) & tmp) != 0); \
}

#define SHIFTLEFT_CY(val, count, bitsize) \
	_CY = (uint8_t)(((val) >> (bitsize - count)) & 1);



#define SHIFTARITHMETICRIGHT_OV(val, count, bitsize) \
	_OV = 0;

#define SHIFTARITHMETICRIGHT_CY(val, count, bitsize) \
	_CY = (uint8_t)(((val) >> (count - 1)) & 1);



template <class H> uint32_t opSHAB()
{
	H::enter(*this, "opSHAB");
	uint8_t appb;
	int8_t count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	count = (int8_t)(m_op1 & 0xFF);

	// Special case: destination unchanged, flags set
	if (count == 0)
	{
		_CY = _OV = 0;
		SetSZPF_Byte(appb);
	}
	else if (count > 0)
	{
		SHIFTLEFT_OV(appb, count, 8);

		// @@@ Undefined what happens to CY when count >= bitsize
		SHIFTLEFT_CY(appb, count, 8);

		// do the actual shift...
		if (count >= 8)
			appb = 0;
		else
			appb <<= count;

		// and set zero and sign
		SetSZPF_Byte(appb);
	}
	else
	{
		count = -count;

		SHIFTARITHMETICRIGHT_OV(appb, count, 8);
		SHIFTARITHMETICRIGHT_CY(appb, count, 8);

		if (count >= 8)
			appb = (appb & 0x80) ? 0xFF : 0;
		else
			appb = ((int8_t)appb) >> count;

		SetSZPF_Byte(appb);
	}

//  v60_log("SHAB: %x _CY: %d _Z: %d _OV: %d _S: %d\n", appb, _CY, _Z, _OV, _S);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opSHAH()
{
	H::enter(*this, "opSHAH");
	uint16_t apph;
	int8_t count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 1);

	F12LOADOP2HALF();

	count = (int8_t)(m_op1 & 0xFF);

	// Special case: destination unchanged, flags set
	if (count == 0)
	{
		_CY = _OV = 0;
		SetSZPF_Word(apph);
	}
	else if (count > 0)
	{
		SHIFTLEFT_OV(apph, count, 16);

		// @@@ Undefined what happens to CY when count >= bitsize
		SHIFTLEFT_CY(apph, count, 16);

		// do the actual shift...
		if (count >= 16)
			apph = 0;
		else
			apph <<= count;

		// and set zero and sign
		SetSZPF_Word(apph);
	}
	else
	{
		count = -count;

		SHIFTARITHMETICRIGHT_OV(apph, count, 16);
		SHIFTARITHMETICRIGHT_CY(apph, count, 16);

		if (count >= 16)
			apph = (apph & 0x8000) ? 0xFFFF : 0;
		else
			apph = ((int16_t)apph) >> count;

		SetSZPF_Word(apph);
	}

//  v60_log("SHAH: %x >> %d = %x _CY: %d _Z: %d _OV: %d _S: %d\n", oldval, count, apph, _CY, _Z, _OV, _S);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opSHAW()
{
	H::enter(*this, "opSHAW");
	uint32_t appw;
	int8_t count;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 2);

	F12LOADOP2WORD();

	count = (int8_t)(m_op1 & 0xFF);

	// Special case: destination unchanged, flags set
	if (count == 0)
	{
		_CY = _OV = 0;
		SetSZPF_Long(appw);
	}
	else if (count > 0)
	{
		SHIFTLEFT_OV(appw, count, 32);

		// @@@ Undefined what happens to CY when count >= bitsize
		SHIFTLEFT_CY(appw, count, 32);

		// do the actual shift...
		if (count >= 32)
			appw = 0;
		else
			appw <<= count;

		// and set zero and sign
		SetSZPF_Long(appw);
	}
	else
	{
		count = -count;

		SHIFTARITHMETICRIGHT_OV(appw, count, 32);
		SHIFTARITHMETICRIGHT_CY(appw, count, 32);

		if (count >= 32)
			appw = (appw & 0x80000000) ? 0xFFFFFFFF : 0;
		else
			appw = ((int32_t)appw) >> count;

		SetSZPF_Long(appw);
	}

//  v60_log("SHAW: %x >> %d = %x _CY: %d _Z: %d _OV: %d _S: %d\n", oldval, count, appw, _CY, _Z, _OV, _S);

	F12STOREOP2WORD();
	F12END();
}



template <class H> uint32_t opSHLB()
{
	H::enter(*this, "opSHLB");
	uint8_t appb;
	int8_t count;
	uint32_t tmp;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		// left shift flags:
		// carry gets the last bit shifted out,
		// overflow is always CLEARed

		_OV = 0;  // default to no overflow

		// now handle carry
		tmp = appb & 0xff;
		tmp <<= count;
		SetCFB(tmp);    // set carry properly

		// do the actual shift...
		appb <<= count;

		// and set zero and sign
		SetSZPF_Byte(appb);
	}
	else
	{
		if (count == 0)
		{
			// special case: clear carry and overflow, do nothing else
			_CY = _OV = 0;
			SetSZPF_Byte(appb); // doc. is unclear if this is true...
		}
		else
		{
			// right shift flags:
			// carry = last bit shifted out
			// overflow always cleared
			tmp = appb & 0xff;
			tmp >>= ((-count) - 1);
			_CY = (uint8_t)(tmp & 0x1);
			_OV = 0;

			appb >>= -count;
			SetSZPF_Byte(appb);
		}
	}

//  v60_log("SHLB: %x _CY: %d _Z: %d _OV: %d _S: %d\n", appb, _CY, _Z, _OV, _S);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opSHLH()
{
	H::enter(*this, "opSHLH");
	uint16_t apph;
	int8_t count;
	uint32_t tmp;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 1);

	F12LOADOP2HALF();

	count = (int8_t)(m_op1 & 0xFF);
//  v60_log("apph: %x count: %d  ", apph, count);
	if (count > 0)
	{
		// left shift flags:
		// carry gets the last bit shifted out,
		// overflow is always CLEARed

		_OV = 0;

		// now handle carry
		tmp = apph & 0xffff;
		tmp <<= count;
		SetCFW(tmp);    // set carry properly

		// do the actual shift...
		apph <<= count;

		// and set zero and sign
		SetSZPF_Word(apph);
	}
	else
	{
		if (count == 0)
		{
			// special case: clear carry and overflow, do nothing else
			_CY = _OV = 0;
			SetSZPF_Word(apph); // doc. is unclear if this is true...
		}
		else
		{
			// right shift flags:
			// carry = last bit shifted out
			// overflow always cleared
			tmp = apph & 0xffff;
			tmp >>= ((-count) - 1);
			_CY = (uint8_t)(tmp & 0x1);
			_OV = 0;

			apph >>= -count;
			SetSZPF_Word(apph);
		}
	}

//  v60_log("SHLH: %x _CY: %d _Z: %d _OV: %d _S: %d\n", apph, _CY, _Z, _OV, _S);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opSHLW()
{
	H::enter(*this, "opSHLW");
	uint32_t appw;
	int8_t count;
	uint64_t tmp;

	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 2);

	F12LOADOP2WORD();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		// left shift flags:
		// carry gets the last bit shifted out,
		// overflow is always CLEARed

		_OV = 0;

		// now handle carry
		tmp = appw & 0xffffffff;
		tmp <<= count;
		SetCFL(tmp);    // set carry properly

		// do the actual shift...
		appw <<= count;

		// and set zero and sign
		SetSZPF_Long(appw);
	}
	else
	{
		if (count == 0)
		{
			// special case: clear carry and overflow, do nothing else
			_CY = _OV = 0;
			SetSZPF_Long(appw); // doc. is unclear if this is true...
		}
		else
		{
			// right shift flags:
			// carry = last bit shifted out
			// overflow always cleared
			tmp = (uint64_t)(appw & 0xffffffff);
			tmp >>= ((-count) - 1);
			_CY = (uint8_t)(tmp & 0x1);
			_OV = 0;

			appw >>= -count;
			SetSZPF_Long(appw);
		}
	}

	//v60_log("SHLW: %x _CY: %d _Z: %d _OV: %d _S: %d\n", appw, _CY, _Z, _OV, _S);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opSTPR()
{
	H::enter(*this, "opSTPR");
	H::F12DecodeFirstOperand(*this, Am::Read, 2);
	if (m_op1 <= 28)
		m_modwritevalw = m_reg[m_op1 + 36];
	else
	{
		v60_fatal("Invalid operand on STPR PC=%x\n", PC);
	}
	H::F12WriteSecondOperand(*this, 2);
	F12END();
}



template <class H> uint32_t opSUBB()
{
	H::enter(*this, "opSUBB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	SUBB(appb, (uint8_t)m_op1, 0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opSUBH()
{
	H::enter(*this, "opSUBH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	SUBW(apph, (uint16_t)m_op1, 0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opSUBW()
{
	H::enter(*this, "opSUBW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	SUBL(appw, (uint32_t)m_op1, 0);

	F12STOREOP2WORD();
	F12END();
}



template <class H> uint32_t opSUBCB()
{
	H::enter(*this, "opSUBCB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	SUBB(appb, (uint8_t)m_op1, _CY?1:0);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opSUBCH()
{
	H::enter(*this, "opSUBCH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	SUBW(apph, (uint16_t)m_op1, _CY?1:0);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opSUBCW()
{
	H::enter(*this, "opSUBCW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	SUBL(appw, (uint32_t)m_op1, _CY?1:0);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opTEST1()
{
	H::enter(*this, "opTEST1");
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Read, 2);

	_CY = ((m_op2 & (1 << m_op1)) != 0);
	_Z = !(_CY);

	F12END();
}


template <class H> uint32_t opUPDPSWW()
{
	H::enter(*this, "opUPDPSWW");
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Read, 2);

	/* can only modify condition code and control fields */
	m_op2 &= 0xFFFFFF;
	m_op1 &= 0xFFFFFF;
	v60WritePSW((v60ReadPSW() & (~m_op2)) | (m_op1 & m_op2));

	F12END();
}


template <class H> uint32_t opUPDPSWH()
{
	H::enter(*this, "opUPDPSWH");
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Read, 2);

	/* can only modify condition code fields */
	m_op2 &= 0xFFFF;
	m_op1 &= 0xFFFF;
	v60WritePSW((v60ReadPSW() & (~m_op2)) | (m_op1 & m_op2));

	F12END();
}


template <class H> uint32_t opXCHB()
{
	H::enter(*this, "opXCHB");
	uint8_t appb, temp;

	H::F12DecodeOperands(*this, Am::Addr, 0,Am::Addr, 0);

	F12LOADOP1BYTE();
	temp = appb;
	F12LOADOP2BYTE();
	F12STOREOP1BYTE();
	appb = temp;
	F12STOREOP2BYTE();

	F12END()
}


template <class H> uint32_t opXCHH()
{
	H::enter(*this, "opXCHH");
	uint16_t apph, temp;

	H::F12DecodeOperands(*this, Am::Addr, 1,Am::Addr, 1);

	F12LOADOP1HALF();
	temp = apph;
	F12LOADOP2HALF();
	F12STOREOP1HALF();
	apph = temp;
	F12STOREOP2HALF();

	F12END()
}


template <class H> uint32_t opXCHW()
{
	H::enter(*this, "opXCHW");
	uint32_t appw, temp;

	H::F12DecodeOperands(*this, Am::Addr, 2,Am::Addr, 2);

	F12LOADOP1WORD();
	temp = appw;
	F12LOADOP2WORD();
	F12STOREOP1WORD();
	appw = temp;
	F12STOREOP2WORD();

	F12END()
}


template <class H> uint32_t opXORB()
{
	H::enter(*this, "opXORB");
	uint8_t appb;
	H::F12DecodeOperands(*this, Am::Read, 0,Am::Addr, 0);

	F12LOADOP2BYTE();

	XORB(appb, (uint8_t)m_op1);

	F12STOREOP2BYTE();
	F12END();
}


template <class H> uint32_t opXORH()
{
	H::enter(*this, "opXORH");
	uint16_t apph;
	H::F12DecodeOperands(*this, Am::Read, 1,Am::Addr, 1);

	F12LOADOP2HALF();

	XORW(apph, (uint16_t)m_op1);

	F12STOREOP2HALF();
	F12END();
}


template <class H> uint32_t opXORW()
{
	H::enter(*this, "opXORW");
	uint32_t appw;
	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 2);

	F12LOADOP2WORD();

	XORL(appw, (uint32_t)m_op1);

	F12STOREOP2WORD();
	F12END();
}


template <class H> uint32_t opMULX()
{
	H::enter(*this, "opMULX");
	int32_t a, b;
	int64_t res;

	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 3);

	if (m_flag2)
	{
		a = m_reg[m_op2 & 0x1F];
	}
	else
	{
		a = m_program->read_dword_unaligned(m_op2);
	}

	res = mul_32x32(a, m_op1);

	b = (int32_t)((res >> 32)&0xffffffff);
	a = (int32_t)(res & 0xffffffff);

	_S = ((b & 0x80000000) != 0);
	_Z = (a == 0 && b == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
		m_program->write_qword_unaligned(m_op2, res);

	F12END();
}


template <class H> uint32_t opMULUX()
{
	H::enter(*this, "opMULUX");
	int32_t a, b;
	uint64_t res;

	H::F12DecodeOperands(*this, Am::Read, 2,Am::Addr, 3);

	if (m_flag2)
	{
		a = m_reg[m_op2 & 0x1F];
	}
	else
	{
		a = m_program->read_dword_unaligned(m_op2);
	}

	res = mulu_32x32(a, m_op1);
	b = (int32_t)((res >> 32)&0xffffffff);
	a = (int32_t)(res & 0xffffffff);

	_S = ((b & 0x80000000) != 0);
	_Z = (a == 0 && b == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
		m_program->write_qword_unaligned(m_op2, res);

	F12END();
}


#define F2END() \
	return 2 + m_amlength1 + m_amlength2;

#define F2LOADOPFLOAT(num)                              \
	if (m_flag##num)                                    \
		appf = u2f(m_reg[m_op##num]);               \
	else                                                    \
		appf = u2f(m_program->read_dword_unaligned(m_op##num));

#define F2STOREOPFLOAT(num)                              \
	if (m_flag##num)                                    \
		m_reg[m_op##num] = f2u(appf);               \
	else                                                    \
		m_program->write_dword_unaligned(m_op##num, f2u(appf));







template <class H> uint32_t opCVTWS()
{
	H::enter(*this, "opCVTWS");
	float val;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);

	// Convert to float
	val = (float)(int32_t)m_op1;
	m_modwritevalw = f2u(val);

	_OV = 0;
	_CY = (val < 0.0f);
	_S = ((m_modwritevalw & 0x80000000) != 0);
	_Z = (val == 0.0f);

	H::F2WriteSecondOperand(*this, 2);
	F2END();
}


template <class H> uint32_t opCVTSW()
{
	H::enter(*this, "opCVTSW");
	float val;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);

	// Apply RDI rounding control
	val = u2f(m_op1);
	switch (TKCW & 7)
	{
	case 0: val = roundf(val); break;
	case 1: val = floorf(val); break;
	case 2: val = ceilf(val); break;
	default: val = truncf(val); break;
	}

	// Convert to uint32_t
	m_modwritevalw = (uint32_t)(int64_t)val;

	_S = ((m_modwritevalw & 0x80000000) != 0);
	_OV = (_S && val >= 0.0f) || (!_S && val <= -1.0f);
	_Z = (m_modwritevalw == 0);

	H::F2WriteSecondOperand(*this, 2);
	F2END();
}


template <class H> uint32_t opMOVFS()
{
	H::enter(*this, "opMOVFS");
	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	m_modwritevalw = m_op1;
	H::F2WriteSecondOperand(*this, 2);
	F2END();
}


template <class H> uint32_t opNEGFS()
{
	H::enter(*this, "opNEGFS");
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	appf = -u2f(m_op1);

	_OV = 0;
	_CY = (appf < 0.0f);
	_S = ((f2u(appf) & 0x80000000) != 0);
	_Z = (appf == 0.0f);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opABSFS()
{
	H::enter(*this, "opABSFS");
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	appf = u2f(m_op1);

	if (appf < 0)
		appf = -appf;

	_OV = 0;
	_CY = 0;
	_S = ((f2u(appf) & 0x80000000) != 0);
	_Z = (appf == 0.0f);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opADDFS()
{
	H::enter(*this, "opADDFS");
	uint32_t appw;
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	F2LOADOPFLOAT(2);

	appf += u2f(m_op1);

	appw = f2u(appf);
	_OV = _CY = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opSUBFS()
{
	H::enter(*this, "opSUBFS");
	uint32_t appw;
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	F2LOADOPFLOAT(2);

	appf -= u2f(m_op1);

	appw = f2u(appf);
	_OV = _CY = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opMULFS()
{
	H::enter(*this, "opMULFS");
	uint32_t appw;
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	F2LOADOPFLOAT(2);

	appf *= u2f(m_op1);

	appw = f2u(appf);
	_OV = _CY = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opDIVFS()
{
	H::enter(*this, "opDIVFS");
	uint32_t appw;
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	F2LOADOPFLOAT(2);

	appf /= u2f(m_op1);

	appw = f2u(appf);
	_OV = _CY = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opSCLFS()
{
	H::enter(*this, "opSCLFS");
	uint32_t appw;
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 1);
	H::F2DecodeSecondOperand(*this, Am::Addr, 2);

	F2LOADOPFLOAT(2);

	if ((int16_t)m_op1 < 0)
		appf /= 1 << -(int16_t)m_op1;
	else
		appf *= 1 << m_op1;

	appw = f2u(appf);
	_OV = _CY = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F2STOREOPFLOAT(2);
	F2END()
}


template <class H> uint32_t opCMPF()
{
	H::enter(*this, "opCMPF");
	float appf;

	H::F2DecodeFirstOperand(*this, Am::Read, 2);
	H::F2DecodeSecondOperand(*this, Am::Read, 2);

	appf = u2f(m_op2) - u2f(m_op1);

	_Z = (appf == 0);
	_S = (appf < 0);
	_OV = 0;
	_CY = 0;

	F2END();
}


template <class H> uint32_t op5FUNHANDLED()
{
	H::enter(*this, "op5FUNHANDLED");
	v60_fatal("Unhandled 5F opcode at %08x\n", PC);
}


template <class H> uint32_t op5CUNHANDLED()
{
	H::enter(*this, "op5CUNHANDLED");
	v60_fatal("Unhandled 5C opcode at %08x\n", PC);
}







template <class H> uint32_t op5F()
{
	H::enter(*this, "op5F");
	m_instflags = OpRead8(PC + 1);
	return (this->*Tables<H>::s_Op5FTable[m_instflags & 0x1F])();
}



template <class H> uint32_t op5C()
{
	H::enter(*this, "op5C");
	m_instflags = OpRead8(PC + 1);
	return (this->*Tables<H>::s_Op5CTable[m_instflags & 0x1F])();
}

template <class H> uint32_t opINCB()
{
	H::enter(*this, "opINCB");
	uint8_t appb;
	m_modadd = PC + 1;
	m_moddim = 0;

	m_amlength1 = H::ReadAMAddress(*this);

	if (m_amflag)
		appb = (uint8_t)m_reg[m_amout];
	else
		appb = m_program->read_byte(m_amout);

	ADDB(appb, 1, 0);

	if (m_amflag)
		SETREG8(m_reg[m_amout], appb);
	else
		m_program->write_byte(m_amout, appb);

	return m_amlength1 + 1;
}


template <class H> uint32_t opINCH()
{
	H::enter(*this, "opINCH");
	uint16_t apph;
	m_modadd = PC + 1;
	m_moddim = 1;

	m_amlength1 = H::ReadAMAddress(*this);

	if (m_amflag)
		apph = (uint16_t)m_reg[m_amout];
	else
		apph = m_program->read_word_unaligned(m_amout);

	ADDW(apph, 1, 0);

	if (m_amflag)
		SETREG16(m_reg[m_amout], apph);
	else
		m_program->write_word_unaligned(m_amout, apph);

	return m_amlength1 + 1;
}


template <class H> uint32_t opINCW()
{
	H::enter(*this, "opINCW");
	uint32_t appw;
	m_modadd = PC + 1;
	m_moddim = 2;

	m_amlength1 = H::ReadAMAddress(*this);

	if (m_amflag)
		appw = m_reg[m_amout];
	else
		appw = m_program->read_dword_unaligned(m_amout);

	ADDL(appw, 1, 0);

	if (m_amflag)
		m_reg[m_amout] = appw;
	else
		m_program->write_dword_unaligned(m_amout, appw);

	return m_amlength1 + 1;
}


template <class H> uint32_t opDECB()
{
	H::enter(*this, "opDECB");
	uint8_t appb;
	m_modadd = PC + 1;
	m_moddim = 0;

	m_amlength1 = H::ReadAMAddress(*this);

	if (m_amflag)
		appb = (uint8_t)m_reg[m_amout];
	else
		appb = m_program->read_byte(m_amout);

	SUBB(appb, 1, 0);

	if (m_amflag)
		SETREG8(m_reg[m_amout], appb);
	else
		m_program->write_byte(m_amout, appb);

	return m_amlength1 + 1;
}


template <class H> uint32_t opDECH()
{
	H::enter(*this, "opDECH");
	uint16_t apph;
	m_modadd = PC + 1;
	m_moddim = 1;

	m_amlength1 = H::ReadAMAddress(*this);

	if (m_amflag)
		apph = (uint16_t)m_reg[m_amout];
	else
		apph = m_program->read_word_unaligned(m_amout);

	SUBW(apph, 1, 0);

	if (m_amflag)
		SETREG16(m_reg[m_amout], apph);
	else
		m_program->write_word_unaligned(m_amout, apph);

	return m_amlength1 + 1;
}


template <class H> uint32_t opDECW()
{
	H::enter(*this, "opDECW");
	uint32_t appw;
	m_modadd = PC + 1;
	m_moddim = 2;

	m_amlength1 = H::ReadAMAddress(*this);

	if (m_amflag)
		appw = m_reg[m_amout];
	else
		appw = m_program->read_dword_unaligned(m_amout);

	SUBL(appw, 1, 0);

	if (m_amflag)
		m_reg[m_amout] = appw;
	else
		m_program->write_dword_unaligned(m_amout, appw);

	return m_amlength1 + 1;
}


template <class H> uint32_t opJMP()
{
	H::enter(*this, "opJMP");
	m_modadd = PC + 1;
	m_moddim = 0;

	// Read the address of the operand
	H::ReadAMAddress(*this);

	// It cannot be a register!!
	assert(m_amflag == 0);

	// Jump there
	PC = m_amout;

	return 0;
}


template <class H> uint32_t opJSR()
{
	H::enter(*this, "opJSR");
	m_modadd = PC + 1;
	m_moddim = 0;

	// Read the address of the operand
	m_amlength1 = H::ReadAMAddress(*this);

	// It cannot be a register!!
	assert(m_amflag == 0);

	// Save NextPC into the stack
	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + m_amlength1 + 1);

	// Jump there
	PC = m_amout;

	return 0;
}


template <class H> uint32_t opPREPARE()
{
	H::enter(*this, "opPREPARE");
	m_modadd = PC + 1;
	m_moddim = 2;

	// Read the operand
	m_amlength1 = H::ReadAM(*this);

	// step 1: save frame pointer on the stack
	SP -= 4;
	m_program->write_dword_unaligned(SP, FP);

	// step 2: FP = new SP
	FP = SP;

	// step 3: SP -= operand
	SP -= m_amout;

	return m_amlength1 + 1;
}


template <class H> uint32_t opRET()
{
	H::enter(*this, "opRET");
	m_modadd = PC + 1;
	m_moddim = 2;

	// Read the operand
	H::ReadAM(*this);

	// Read return address from stack
	PC = m_program->read_dword_unaligned(SP);
	SP +=4;

	// Restore AP from stack
	AP = m_program->read_dword_unaligned(SP);
	SP +=4;

	// Skip stack frame
	SP += m_amout;

	return 0;
}


template <class H> uint32_t opTRAP()
{
	H::enter(*this, "opTRAP");
	uint32_t oldPSW;

	m_modadd = PC + 1;
	m_moddim = 0;

	// Read the operand
	m_amlength1 = H::ReadAM(*this);

	// Normalize the flags
	NORMALIZEFLAGS();

	switch ((m_amout >> 4) & 0xF)
	{
	case 0:
		if (!_OV) return m_amlength1 + 1;
		else break;
	case 1:
		if (_OV) return m_amlength1 + 1;
		else break;
	case 2:
		if (!_CY) return m_amlength1 + 1;
		else break;
	case 3:
		if (_CY) return m_amlength1 + 1;
		else break;
	case 4:
		if (!_Z) return m_amlength1 + 1;
		else break;
	case 5:
		if (_Z) return m_amlength1 + 1;
		else break;
	case 6:
		if (!(_CY | _Z)) return m_amlength1 + 1;
		else break;
	case 7:
		if ((_CY | _Z)) return m_amlength1 + 1;
		else break;
	case 8:
		if (!_S) return m_amlength1 + 1;
		else break;
	case 9:
		if (_S) return m_amlength1 + 1;
		else break;
	case 10:
		break;
	case 11:
		return m_amlength1 + 1;
	case 12:
		if (!(_S^_OV)) return m_amlength1 + 1;
		else break;
	case 13:
		if ((_S^_OV)) return m_amlength1 + 1;
		else break;
	case 14:
		if (!((_S^_OV)|_Z)) return m_amlength1 + 1;
		else break;
	case 15:
		if (((_S^_OV)|_Z)) return m_amlength1 + 1;
		else break;
	}

	oldPSW = v60_update_psw_for_exception(0, 0);

	// Issue the software trap with interrupts
	SP -= 4;
	m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x3000 + 0x100 * (m_amout & 0xF), 4));

	SP -= 4;
	m_program->write_dword_unaligned(SP, oldPSW);

	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + m_amlength1 + 1);

	PC = GETINTVECT(48 + (m_amout & 0xF));

	return 0;
}


template <class H> uint32_t opRETIU()
{
	H::enter(*this, "opRETIU");
	uint32_t newPSW;
	m_modadd = PC + 1;
	m_moddim = 1;

	// Read the operand
	H::ReadAM(*this);

	// Restore PC and PSW from stack
	PC = m_program->read_dword_unaligned(SP);
	SP += 4;

	newPSW = m_program->read_dword_unaligned(SP);
	SP += 4;

	// Destroy stack frame
	SP += m_amout;

	v60WritePSW(newPSW);

	return 0;
}


template <class H> uint32_t opRETIS()
{
	H::enter(*this, "opRETIS");
	uint32_t newPSW;

	m_modadd = PC + 1;
	m_moddim = 1;

	// Read the operand
	H::ReadAM(*this);

	// Restore PC and PSW from stack
	PC = m_program->read_dword_unaligned(SP);
	SP += 4;

	newPSW = m_program->read_dword_unaligned(SP);
	SP += 4;

	// Destroy stack frame
	SP += m_amout;

	v60WritePSW(newPSW);

	return 0;
}


template <class H> uint32_t opSTTASK()
{
	H::enter(*this, "opSTTASK");
	int i;
	uint32_t adr;

	m_modadd = PC + 1;
	m_moddim = 2;

	m_amlength1 = H::ReadAM(*this);

	adr = TR;

	v60WritePSW(v60ReadPSW() | 0x10000000);
	v60SaveStack();

	m_program->write_dword_unaligned(adr, TKCW);
	adr += 4;
	if(SYCW & 0x100) {
		m_program->write_dword_unaligned(adr, L0SP);
		adr += 4;
	}
	if(SYCW & 0x200) {
		m_program->write_dword_unaligned(adr, L1SP);
		adr += 4;
	}
	if(SYCW & 0x400) {
		m_program->write_dword_unaligned(adr, L2SP);
		adr += 4;
	}
	if(SYCW & 0x800) {
		m_program->write_dword_unaligned(adr, L3SP);
		adr += 4;
	}

	// 31 registers supported, _not_ 32
	for(i = 0; i < 31; i++)
		if(m_amout & (1 << i)) {
			m_program->write_dword_unaligned(adr, m_reg[i]);
			adr += 4;
		}

	// #### Ignore the virtual addressing crap.

	return m_amlength1 + 1;
}


template <class H> uint32_t opGETPSW()
{
	H::enter(*this, "opGETPSW");
	m_modadd = PC + 1;
	m_moddim = 2;
	m_modwritevalw = v60ReadPSW();

	// Write PSW to the operand
	m_amlength1 = H::WriteAM(*this);

	return m_amlength1 + 1;
}


template <class H> uint32_t opTASI()
{
	H::enter(*this, "opTASI");
	uint8_t appb;
	m_modadd = PC + 1;
	m_moddim = 0;

	// Load the address of the operand
	m_amlength1 = H::ReadAMAddress(*this);

	// Load uint8_t from the address
	if (m_amflag)
		appb = (uint8_t)m_reg[m_amout & 0x1f];
	else
		appb = m_program->read_byte(m_amout);

	// Set the flags for SUB appb, FF
	SUBB(appb, 0xff, 0);

	// Write FF in the operand
	if (m_amflag)
		SETREG8(m_reg[m_amout & 0x1f], 0xff);
	else
		m_program->write_byte(m_amout, 0xff);

	return m_amlength1 + 1;
}


template <class H> uint32_t opCLRTLB()
{
	H::enter(*this, "opCLRTLB");
	m_modadd = PC + 1;
	m_moddim = 2;

	// Read the operand
	m_amlength1 = H::ReadAM(*this);

	// @@@ TLB not yet emulated

	return m_amlength1 + 1;
}


template <class H> uint32_t opPOPM()
{
	H::enter(*this, "opPOPM");
	int i;

	m_modadd = PC + 1;
	m_moddim = 2;

	// Read the bit register list
	m_amlength1 = H::ReadAM(*this);

	for (i = 0;i < 31;i++)
		if (m_amout & (1 << i))
		{
			m_reg[i] = m_program->read_dword_unaligned(SP);
			SP += 4;
		}

	if (m_amout & (1 << 31))
	{
		v60WritePSW((v60ReadPSW() & 0xffff0000) | m_program->read_word_unaligned(SP));
		SP += 4;
	}

	return m_amlength1 + 1;
}


template <class H> uint32_t opPUSHM()
{
	H::enter(*this, "opPUSHM");
	int i;

	m_modadd = PC + 1;
	m_moddim = 2;

	// Read the bit register list
	m_amlength1 = H::ReadAM(*this);

	if (m_amout & (1 << 31))
	{
		SP -= 4;
		m_program->write_dword_unaligned(SP, v60ReadPSW());
	}

	for (i = 0;i < 31;i++)
		if (m_amout & (1 << (30 - i)))
		{
			SP -= 4;
			m_program->write_dword_unaligned(SP, m_reg[(30 - i)]);
		}


	return m_amlength1 + 1;
}


template <class H> uint32_t opTESTB()
{
	H::enter(*this, "opTESTB");
	m_modadd = PC + 1;
	m_moddim = 0;

	// Read the operand
	m_amlength1 = H::ReadAM(*this);

	_Z = (m_amout == 0);
	_S = ((m_amout & 0x80) != 0);
	_CY = 0;
	_OV = 0;

	return m_amlength1 + 1;
}


template <class H> uint32_t opTESTH()
{
	H::enter(*this, "opTESTH");
	m_modadd = PC + 1;
	m_moddim = 1;

	// Read the operand
	m_amlength1 = H::ReadAM(*this);

	_Z = (m_amout == 0);
	_S = ((m_amout & 0x8000) != 0);
	_CY = 0;
	_OV = 0;

	return m_amlength1 + 1;
}


template <class H> uint32_t opTESTW()
{
	H::enter(*this, "opTESTW");
	m_modadd = PC + 1;
	m_moddim = 2;

	// Read the operand
	m_amlength1 = H::ReadAM(*this);

	_Z = (m_amout == 0);
	_S = ((m_amout & 0x80000000) != 0);
	_CY = 0;
	_OV = 0;

	return m_amlength1 + 1;
}


template <class H> uint32_t opPUSH()
{
	H::enter(*this, "opPUSH");
	m_modadd = PC + 1;
	m_moddim = 2;

	m_amlength1 = H::ReadAM(*this);

	SP-=4;
	m_program->write_dword_unaligned(SP, m_amout);

	return m_amlength1 + 1;
}


template <class H> uint32_t opPOP()
{
	H::enter(*this, "opPOP");
	m_modadd = PC + 1;
	m_moddim = 2;
	m_modwritevalw = m_program->read_dword_unaligned(SP);
	SP +=4;
	m_amlength1 = H::WriteAM(*this);

	return m_amlength1 + 1;
}



template <class H> uint32_t opINCB_0()
{
	H::enter(*this, "opINCB_0"); m_modm = 0; return opINCB<H>(); }

template <class H> uint32_t opINCB_1()
{
	H::enter(*this, "opINCB_1"); m_modm = 1; return opINCB<H>(); }

template <class H> uint32_t opINCH_0()
{
	H::enter(*this, "opINCH_0"); m_modm = 0; return opINCH<H>(); }

template <class H> uint32_t opINCH_1()
{
	H::enter(*this, "opINCH_1"); m_modm = 1; return opINCH<H>(); }

template <class H> uint32_t opINCW_0()
{
	H::enter(*this, "opINCW_0"); m_modm = 0; return opINCW<H>(); }

template <class H> uint32_t opINCW_1()
{
	H::enter(*this, "opINCW_1"); m_modm = 1; return opINCW<H>(); }


template <class H> uint32_t opDECB_0()
{
	H::enter(*this, "opDECB_0"); m_modm = 0; return opDECB<H>(); }

template <class H> uint32_t opDECB_1()
{
	H::enter(*this, "opDECB_1"); m_modm = 1; return opDECB<H>(); }

template <class H> uint32_t opDECH_0()
{
	H::enter(*this, "opDECH_0"); m_modm = 0; return opDECH<H>(); }

template <class H> uint32_t opDECH_1()
{
	H::enter(*this, "opDECH_1"); m_modm = 1; return opDECH<H>(); }

template <class H> uint32_t opDECW_0()
{
	H::enter(*this, "opDECW_0"); m_modm = 0; return opDECW<H>(); }

template <class H> uint32_t opDECW_1()
{
	H::enter(*this, "opDECW_1"); m_modm = 1; return opDECW<H>(); }


template <class H> uint32_t opJMP_0()
{
	H::enter(*this, "opJMP_0"); m_modm = 0; return opJMP<H>(); }

template <class H> uint32_t opJMP_1()
{
	H::enter(*this, "opJMP_1"); m_modm = 1; return opJMP<H>(); }


template <class H> uint32_t opJSR_0()
{
	H::enter(*this, "opJSR_0"); m_modm = 0; return opJSR<H>(); }

template <class H> uint32_t opJSR_1()
{
	H::enter(*this, "opJSR_1"); m_modm = 1; return opJSR<H>(); }


template <class H> uint32_t opPREPARE_0()
{
	H::enter(*this, "opPREPARE_0"); m_modm = 0; return opPREPARE<H>(); }

template <class H> uint32_t opPREPARE_1()
{
	H::enter(*this, "opPREPARE_1"); m_modm = 1; return opPREPARE<H>(); }


template <class H> uint32_t opRET_0()
{
	H::enter(*this, "opRET_0"); m_modm = 0; return opRET<H>(); }

template <class H> uint32_t opRET_1()
{
	H::enter(*this, "opRET_1"); m_modm = 1; return opRET<H>(); }


template <class H> uint32_t opTRAP_0()
{
	H::enter(*this, "opTRAP_0"); m_modm = 0; return opTRAP<H>(); }

template <class H> uint32_t opTRAP_1()
{
	H::enter(*this, "opTRAP_1"); m_modm = 1; return opTRAP<H>(); }


template <class H> uint32_t opRETIU_0()
{
	H::enter(*this, "opRETIU_0"); m_modm = 0; return opRETIU<H>(); }

template <class H> uint32_t opRETIU_1()
{
	H::enter(*this, "opRETIU_1"); m_modm = 1; return opRETIU<H>(); }


template <class H> uint32_t opRETIS_0()
{
	H::enter(*this, "opRETIS_0"); m_modm = 0; return opRETIS<H>(); }

template <class H> uint32_t opRETIS_1()
{
	H::enter(*this, "opRETIS_1"); m_modm = 1; return opRETIS<H>(); }


template <class H> uint32_t opGETPSW_0()
{
	H::enter(*this, "opGETPSW_0"); m_modm = 0; return opGETPSW<H>(); }

template <class H> uint32_t opGETPSW_1()
{
	H::enter(*this, "opGETPSW_1"); m_modm = 1; return opGETPSW<H>(); }


template <class H> uint32_t opTASI_0()
{
	H::enter(*this, "opTASI_0"); m_modm = 0; return opTASI<H>(); }

template <class H> uint32_t opTASI_1()
{
	H::enter(*this, "opTASI_1"); m_modm = 1; return opTASI<H>(); }


template <class H> uint32_t opCLRTLB_0()
{
	H::enter(*this, "opCLRTLB_0"); m_modm = 0; return opCLRTLB<H>(); }

template <class H> uint32_t opCLRTLB_1()
{
	H::enter(*this, "opCLRTLB_1"); m_modm = 1; return opCLRTLB<H>(); }


template <class H> uint32_t opPOPM_0()
{
	H::enter(*this, "opPOPM_0"); m_modm = 0; return opPOPM<H>(); }

template <class H> uint32_t opPOPM_1()
{
	H::enter(*this, "opPOPM_1"); m_modm = 1; return opPOPM<H>(); }


template <class H> uint32_t opPUSHM_0()
{
	H::enter(*this, "opPUSHM_0"); m_modm = 0; return opPUSHM<H>(); }

template <class H> uint32_t opPUSHM_1()
{
	H::enter(*this, "opPUSHM_1"); m_modm = 1; return opPUSHM<H>(); }


template <class H> uint32_t opTESTB_0()
{
	H::enter(*this, "opTESTB_0"); m_modm = 0; return opTESTB<H>(); }

template <class H> uint32_t opTESTB_1()
{
	H::enter(*this, "opTESTB_1"); m_modm = 1; return opTESTB<H>(); }


template <class H> uint32_t opTESTH_0()
{
	H::enter(*this, "opTESTH_0"); m_modm = 0; return opTESTH<H>(); }

template <class H> uint32_t opTESTH_1()
{
	H::enter(*this, "opTESTH_1"); m_modm = 1; return opTESTH<H>(); }


template <class H> uint32_t opTESTW_0()
{
	H::enter(*this, "opTESTW_0"); m_modm = 0; return opTESTW<H>(); }

template <class H> uint32_t opTESTW_1()
{
	H::enter(*this, "opTESTW_1"); m_modm = 1; return opTESTW<H>(); }


template <class H> uint32_t opPUSH_0()
{
	H::enter(*this, "opPUSH_0"); m_modm = 0; return opPUSH<H>(); }

template <class H> uint32_t opPUSH_1()
{
	H::enter(*this, "opPUSH_1"); m_modm = 1; return opPUSH<H>(); }


template <class H> uint32_t opPOP_0()
{
	H::enter(*this, "opPOP_0"); m_modm = 0; return opPOP<H>(); }

template <class H> uint32_t opPOP_1()
{
	H::enter(*this, "opPOP_1"); m_modm = 1; return opPOP<H>(); }


template <class H> uint32_t opSTTASK_0()
{
	H::enter(*this, "opSTTASK_0"); m_modm = 0; return opSTTASK<H>(); }

template <class H> uint32_t opSTTASK_1()
{
	H::enter(*this, "opSTTASK_1"); m_modm = 1; return opSTTASK<H>(); }

/*
    FULLY TRUSTED
*/

template <class H> uint32_t opBGT8()
{
	H::enter(*this, "opBGT8");
	NORMALIZEFLAGS();

	if (!((_S ^ _OV) | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBGT16()
{
	H::enter(*this, "opBGT16");
	NORMALIZEFLAGS();

	if (!((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}



template <class H> uint32_t opBGE8()
{
	H::enter(*this, "opBGE8");
	NORMALIZEFLAGS();

	if (!(_S ^ _OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBGE16()
{
	H::enter(*this, "opBGE16");
	NORMALIZEFLAGS();

	if (!(_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBLT8()
{
	H::enter(*this, "opBLT8");
	NORMALIZEFLAGS();

	if ((_S ^ _OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBLT16()
{
	H::enter(*this, "opBLT16");
	NORMALIZEFLAGS();

	if ((_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}



template <class H> uint32_t opBLE8()
{
	H::enter(*this, "opBLE8");
	NORMALIZEFLAGS();

	if (((_S ^ _OV) | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBLE16()
{
	H::enter(*this, "opBLE16");
	NORMALIZEFLAGS();

	if (((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBH8()
{
	H::enter(*this, "opBH8");
	if (!(_CY | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBH16()
{
	H::enter(*this, "opBH16");
	if (!(_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBNH8()
{
	H::enter(*this, "opBNH8");
	if ((_CY | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBNH16()
{
	H::enter(*this, "opBNH16");
	if ((_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBNL8()
{
	H::enter(*this, "opBNL8");
	if (!(_CY))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBNL16()
{
	H::enter(*this, "opBNL16");
	if (!(_CY))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBL8()
{
	H::enter(*this, "opBL8");
	if ((_CY))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBL16()
{
	H::enter(*this, "opBL16");
	if ((_CY))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBNE8()
{
	H::enter(*this, "opBNE8");
	if (!(_Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBNE16()
{
	H::enter(*this, "opBNE16");
	if (!(_Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBE8()
{
	H::enter(*this, "opBE8");
	if ((_Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBE16()
{
	H::enter(*this, "opBE16");
	if ((_Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBNV8()
{
	H::enter(*this, "opBNV8");
	if (!(_OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBNV16()
{
	H::enter(*this, "opBNV16");
	if (!(_OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBV8()
{
	H::enter(*this, "opBV8");
	if ((_OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBV16()
{
	H::enter(*this, "opBV16");
	if ((_OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBP8()
{
	H::enter(*this, "opBP8");
	if (!(_S))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBP16()
{
	H::enter(*this, "opBP16");
	if (!(_S))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBN8()
{
	H::enter(*this, "opBN8");
	if ((_S))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}


template <class H> uint32_t opBN16()
{
	H::enter(*this, "opBN16");
	if ((_S))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


template <class H> uint32_t opBR8()
{
	H::enter(*this, "opBR8");
	PC += (int8_t)OpRead8(PC + 1);
	return 0;
}


template <class H> uint32_t opBR16()
{
	H::enter(*this, "opBR16");
	PC += (int16_t)OpRead16(PC + 1);
	return 0;
}


template <class H> uint32_t opBSR()
{
	H::enter(*this, "opBSR");
	// Save Next PC onto the stack
	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + 3);

	// Jump to subroutine
	PC += (int16_t)OpRead16(PC + 1);
	return 0;
}

/*
 *  HALT: must add log
 */

template <class H> uint32_t opBRK()
{
	H::enter(*this, "opBRK");
/*
    uint32_t oldPSW = v60_update_psw_for_exception(0, 0);

    SP -=4;
    m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x0d00, 4));
    SP -=4;
    m_program->write_dword_unaligned(SP, oldPSW);
    SP -=4;
    m_program->write_dword_unaligned(SP, PC + 1);
    PC = GETINTVECT(13);
*/
	v60_log("Skipping BRK opcode! PC=%x", PC);

	return 1;
}


template <class H> uint32_t opBRKV()
{
	H::enter(*this, "opBRKV");
	uint32_t oldPSW = v60_update_psw_for_exception(0, 0);

	SP -=4;
	m_program->write_dword_unaligned(SP, PC);
	SP -=4;
	m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x1501, 4));
	SP -=4;
	m_program->write_dword_unaligned(SP, oldPSW);
	SP -=4;
	m_program->write_dword_unaligned(SP, PC + 1);
	PC = GETINTVECT(21);

	return 0;
}


template <class H> uint32_t opCLRTLBA()
{
	H::enter(*this, "opCLRTLBA");
	// @@@ TLB not yet supported
	v60_log("Skipping CLRTLBA opcode! PC=%x\n", PC);
	return 1;
}


template <class H> uint32_t opDISPOSE()
{
	H::enter(*this, "opDISPOSE");
	SP = FP;
	FP = m_program->read_dword_unaligned(SP);
	SP +=4;

	return 1;
}


template <class H> uint32_t opHALT()
{
	H::enter(*this, "opHALT");
	// @@@ It should wait for an interrupt to occur
	//v60_log("HALT found: skipping");
	return 1;
}


template <class H> uint32_t opNOP()
{
	H::enter(*this, "opNOP");
	return 1;
}


template <class H> uint32_t opRSR()
{
	H::enter(*this, "opRSR");
	PC = m_program->read_dword_unaligned(SP);
	SP +=4;

	return 0;
}


template <class H> uint32_t opTRAPFL()
{
	H::enter(*this, "opTRAPFL");
	if ((TKCW & 0x1F0) & ((v60ReadPSW() & 0x1F00) >> 4))
	{
		// @@@ FPU exception
		v60_fatal("Hit TRAPFL! PC=%x\n", PC);
	}

	return 1;
}

/*
    FULLY TRUSTED
*/

template <class H> uint32_t opTB(int reg)
{
	H::enter(*this, "opTB");
	if (m_reg[reg] == 0)
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBGT(int reg)
{
	H::enter(*this, "opDBGT");
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && !((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBLE(int reg)
{
	H::enter(*this, "opDBLE");
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && ((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}



template <class H> uint32_t opDBGE(int reg)
{
	H::enter(*this, "opDBGE");
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && !(_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBLT(int reg)
{
	H::enter(*this, "opDBLT");
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && (_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBH(int reg)
{
	H::enter(*this, "opDBH");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBNH(int reg)
{
	H::enter(*this, "opDBNH");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}



template <class H> uint32_t opDBL(int reg)
{
	H::enter(*this, "opDBL");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_CY))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBNL(int reg)
{
	H::enter(*this, "opDBNL");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_CY))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBE(int reg)
{
	H::enter(*this, "opDBE");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBNE(int reg)
{
	H::enter(*this, "opDBNE");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBV(int reg)
{
	H::enter(*this, "opDBV");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBNV(int reg)
{
	H::enter(*this, "opDBNV");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBN(int reg)
{
	H::enter(*this, "opDBN");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_S))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBP(int reg)
{
	H::enter(*this, "opDBP");
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_S))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


template <class H> uint32_t opDBR(int reg)
{
	H::enter(*this, "opDBR");
	m_reg[reg]--;

	if (m_reg[reg] != 0)
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}







template <class H> uint32_t opC6()
{
	H::enter(*this, "opC6");
	uint8_t appb = OpRead8(PC + 1);
	return (this->*Tables<H>::s_OpC6Table[appb >> 5])(appb & 0x1f);
}


template <class H> uint32_t opC7()
{
	H::enter(*this, "opC7");
	uint8_t appb = OpRead8(PC + 1);
	return (this->*Tables<H>::s_OpC7Table[appb >> 5])(appb & 0x1f);
}

/*
 * CMPC: What happens to _S flag if the strings are identical?
 *   I suppose that it will be cleared. And is it set or cleared
 *   when the first one is a substring of the second? I suppose
 *   cleared (since _S should be (src > dst))
 * MOVC: Why MOVCS does not exist in downward version?
 * SHCHDB / SHCHDH: R27 is filled with the offset from the start or from the end?
 *
 * Strange stuff:
 *   SCHC opcodes does *not* modify _Z flag as stated in V60 manual:
 *   they do the opposite (set if not found, reset if found)
 */

#define F7AEND()  \
	return m_amlength1 + m_amlength2 + 4;

#define F7BEND()  \
	return m_amlength1 + m_amlength2 + 3;

#define F7CEND()  \
	return m_amlength1 + m_amlength2 + 3;

#define F7BCREATEBITMASK(x) \
	x = ((1 << (x)) - 1)

#define F7CCREATEBITMASK(x) \
	x = ((1 << (x)) - 1)













#define F7CLOADOP1BYTE(appb) \
	if (m_flag1) \
		appb = (uint8_t)(m_reg[m_op1]&0xFF); \
	else \
		appb = m_program->read_byte(m_op1);

#define F7CLOADOP2BYTE(appb) \
	if (m_flag2) \
		appb = (uint8_t)(m_reg[m_op2]&0xFF); \
	else \
		appb = m_program->read_byte(m_op2);


#define F7CSTOREOP2BYTE() \
	if (m_flag2) \
		SETREG8(m_reg[m_op2], appb); \
	else \
		m_program->write_byte(m_op2, appb);

#define F7CSTOREOP2HALF() \
	if (m_flag2) \
		SETREG16(m_reg[m_op2], apph); \
	else \
		m_program->write_word_unaligned(m_op2, apph);

template <class H> uint32_t opCMPSTRB(uint8_t bFill, uint8_t bStop)
{
	H::enter(*this, "opCMPSTRB");
	uint32_t i, dest;
	uint8_t c1, c2;

	H::F7aDecodeOperands(*this, Am::Addr, 0,Am::Addr, 0);

	// Filling
	if (bFill)
	{
		if (m_lenop1 < m_lenop2)
		{
			for (i = m_lenop1; i < m_lenop2; i++)
				m_program->write_byte(m_op1 + i,(uint8_t)R26);
		}
		else if (m_lenop2 < m_lenop1)
		{
			for (i = m_lenop2; i < m_lenop1; i++)
				m_program->write_byte(m_op2 + i,(uint8_t)R26);
		}
	}

	dest = (m_lenop1 < m_lenop2 ? m_lenop1 : m_lenop2);

	_Z = 0;
	_S = 0;
	if (bStop) _CY = 1;

	for (i = 0; i < dest; i++)
	{
		c1 = m_program->read_byte(m_op1 + i);
		c2 = m_program->read_byte(m_op2 + i);

		if (c1 > c2)
		{
			_S = 1;   break;
		}
		else if (c2 > c1)
		{
			_S = 0;   break;
		}

		if (bStop)
			if (c1 == (uint8_t)R26 || c2 == (uint8_t)R26)
			{
				_CY = 0;
				break;
			}
	}

	R28 = m_lenop1 + i;
	R27 = m_lenop2 + i;

	if (i == dest)
	{
		if (m_lenop1 > m_lenop2)
			_S = 1;
		else if (m_lenop2 > m_lenop1)
			_S = 0;
		else
			_Z = 1;
	}

	F7AEND();
}


template <class H> uint32_t opCMPSTRH(uint8_t bFill, uint8_t bStop)
{
	H::enter(*this, "opCMPSTRH");
	uint32_t i, dest;
	uint16_t c1, c2;

	H::F7aDecodeOperands(*this, Am::Addr, 0,Am::Addr, 0);

	// Filling
	if (bFill)
	{
		if (m_lenop1 < m_lenop2)
		{
			for (i = m_lenop1; i < m_lenop2; i++)
				m_program->write_word_unaligned(m_op1 + i * 2,(uint16_t)R26);
		}
		else if (m_lenop2 < m_lenop1)
		{
			for (i = m_lenop2; i < m_lenop1; i++)
				m_program->write_word_unaligned(m_op2 + i * 2,(uint16_t)R26);
		}
	}

	dest = (m_lenop1 < m_lenop2 ? m_lenop1 : m_lenop2);

	_Z = 0;
	_S = 0;
	if (bStop) _CY = 1;

	for (i = 0; i < dest; i++)
	{
		c1 = m_program->read_word_unaligned(m_op1 + i * 2);
		c2 = m_program->read_word_unaligned(m_op2 + i * 2);

		if (c1 > c2)
		{
			_S = 1;   break;
		}
		else if (c2 > c1)
		{
			_S = 0;   break;
		}

		if (bStop)
			if (c1 == (uint16_t)R26 || c2 == (uint16_t)R26)
			{
				_CY = 0;
				break;
			}
	}

	R28 = m_lenop1 + i * 2;
	R27 = m_lenop2 + i * 2;

	if (i == dest)
	{
		if (m_lenop1 > m_lenop2)
			_S = 1;
		else if (m_lenop2 > m_lenop1)
			_S = 0;
		else
			_Z = 1;
	}

	F7AEND();
}




template <class H> uint32_t opMOVSTRUB(uint8_t bFill, uint8_t bStop)
{
	H::enter(*this, "opMOVSTRUB");
	uint32_t i, dest;
	uint8_t c1;

//  if (bStop)
//  {
//      int a = 1;
//  }

	H::F7aDecodeOperands(*this, Am::Addr, 0,Am::Addr, 0);

	dest = (m_lenop1 < m_lenop2 ? m_lenop1 : m_lenop2);

	for (i = 0; i < dest; i++)
	{
		m_program->write_byte(m_op2 + i,(c1 = m_program->read_byte(m_op1 + i)));

		if (bStop && c1 == (uint8_t)R26)
			break;
	}

	R28 = m_op1 + i;
	R27 = m_op2 + i;

	if (bFill && m_lenop1 < m_lenop2)
	{
		for (;i < m_lenop2; i++)
			m_program->write_byte(m_op2 + i,(uint8_t)R26);

		R27 = m_op2 + i;
	}


	F7AEND();
}


template <class H> uint32_t opMOVSTRDB(uint8_t bFill, uint8_t bStop)
{
	H::enter(*this, "opMOVSTRDB");
	uint32_t i, dest;
	uint8_t c1;

	H::F7aDecodeOperands(*this, Am::Addr, 0,Am::Addr, 0);

	dest = (m_lenop1 < m_lenop2 ? m_lenop1 : m_lenop2);

	for (i = 0; i < dest; i++)
	{
		m_program->write_byte(m_op2 + (dest - i - 1),(c1 = m_program->read_byte(m_op1 + (dest - i - 1))));

		if (bStop && c1 == (uint8_t)R26)
			break;
	}

	R28 = m_op1 + (m_lenop1 - i - 1);
	R27 = m_op2 + (m_lenop2 - i - 1);

	if (bFill && m_lenop1 < m_lenop2)
	{
		for (;i < m_lenop2; i++)
			m_program->write_byte(m_op2 + dest + (m_lenop2 - i - 1),(uint8_t)R26);

		R27 = m_op2 + (m_lenop2 - i - 1);
	}


	F7AEND();
}



template <class H> uint32_t opMOVSTRUH(uint8_t bFill, uint8_t bStop)
{
	H::enter(*this, "opMOVSTRUH");
	uint32_t i, dest;
	uint16_t c1;

//  if (bStop)
//  {   int a = 1; }

	H::F7aDecodeOperands(*this, Am::Addr, 1,Am::Addr, 1);

	dest = (m_lenop1 < m_lenop2 ? m_lenop1 : m_lenop2);

	for (i = 0; i < dest; i++)
	{
		m_program->write_word_unaligned(m_op2 + i * 2,(c1 = m_program->read_word_unaligned(m_op1 + i * 2)));

		if (bStop && c1 == (uint16_t)R26)
			break;
	}

	R28 = m_op1 + i * 2;
	R27 = m_op2 + i * 2;

	if (bFill && m_lenop1 < m_lenop2)
	{
		for (;i < m_lenop2; i++)
			m_program->write_word_unaligned(m_op2 + i * 2,(uint16_t)R26);

		R27 = m_op2 + i * 2;
	}

	F7AEND();
}


template <class H> uint32_t opMOVSTRDH(uint8_t bFill, uint8_t bStop)
{
	H::enter(*this, "opMOVSTRDH");
	uint32_t i, dest;
	uint16_t c1;

//  if (bFill | bStop)
//  { int a = 1; }

	H::F7aDecodeOperands(*this, Am::Addr, 1,Am::Addr, 1);

//  if (m_lenop1 != m_lenop2)
//  { int a = 1; }

	dest = (m_lenop1 < m_lenop2 ? m_lenop1 : m_lenop2);

	for (i = 0; i < dest; i++)
	{
		m_program->write_word_unaligned(m_op2 + (dest - i - 1) * 2,(c1 = m_program->read_word_unaligned(m_op1 + (dest - i - 1) * 2)));

		if (bStop && c1 == (uint16_t)R26)
			break;
	}

	R28 = m_op1 + (m_lenop1 - i - 1) * 2;
	R27 = m_op2 + (m_lenop2 - i - 1) * 2;

	if (bFill && m_lenop1 < m_lenop2)
	{
		for (;i < m_lenop2; i++)
			m_program->write_word_unaligned(m_op2 + (m_lenop2 - i - 1) * 2,(uint16_t)R26);

		R27 = m_op2 + (m_lenop2 - i - 1) * 2;
	}

	F7AEND();
}


template <class H> uint32_t opSEARCHUB(uint8_t bSearch)
{
	H::enter(*this, "opSEARCHUB");
	uint8_t appb;
	uint32_t i;

	H::F7bDecodeOperands(*this, Am::Addr, 0,Am::Read, 0);

	for (i = 0; i < m_lenop1; i++)
	{
		appb = (m_program->read_byte(m_op1 + i) == (uint8_t)m_op2);
		if ((bSearch && appb) || (!bSearch && !appb))
			break;
	}

	R28 = m_op1 + i;
	R27 = i;

	// This is the opposite as stated in V60 manual...
	if (i != m_lenop1)
		_Z = 0;
	else
		_Z = 1;

	F7BEND();
}


template <class H> uint32_t opSEARCHUH(uint8_t bSearch)
{
	H::enter(*this, "opSEARCHUH");
	uint8_t appb;
	uint32_t i;

	H::F7bDecodeOperands(*this, Am::Addr, 1,Am::Read, 1);

	for (i = 0; i < m_lenop1; i++)
	{
		appb = (m_program->read_word_unaligned(m_op1 + i * 2) == (uint16_t)m_op2);
		if ((bSearch && appb) || (!bSearch && !appb))
			break;
	}

	R28 = m_op1 + i * 2;
	R27 = i;

	if (i != m_lenop1)
		_Z = 0;
	else
		_Z = 1;

	F7BEND();
}


template <class H> uint32_t opSEARCHDB(uint8_t bSearch)
{
	H::enter(*this, "opSEARCHDB");
	uint8_t appb;
	int32_t i;

	H::F7bDecodeOperands(*this, Am::Addr, 0,Am::Read, 0);

	for (i = m_lenop1; i >= 0; i--)
	{
		appb = (m_program->read_byte(m_op1 + i) == (uint8_t)m_op2);
		if ((bSearch && appb) || (!bSearch && !appb))
			break;
	}

	R28 = m_op1 + i;
	R27 = i;

	// This is the opposite as stated in V60 manual...
	if ((uint32_t)i != m_lenop1)
		_Z = 0;
	else
		_Z = 1;

	F7BEND();
}


template <class H> uint32_t opSEARCHDH(uint8_t bSearch)
{
	H::enter(*this, "opSEARCHDH");
	uint8_t appb;
	int32_t i;

	H::F7bDecodeOperands(*this, Am::Addr, 1,Am::Read, 1);

	for (i = m_lenop1 - 1; i >= 0; i--)
	{
		appb = (m_program->read_word_unaligned(m_op1 + i * 2) == (uint16_t)m_op2);
		if ((bSearch && appb) || (!bSearch && !appb))
			break;
	}

	R28 = m_op1 + i * 2;
	R27 = i;

	if ((uint32_t)i != m_lenop1)
		_Z = 0;
	else
		_Z = 1;

	F7BEND();
}



template <class H> uint32_t opSCHCUB()
{
	H::enter(*this, "opSCHCUB"); return opSEARCHUB<H>(1); }

template <class H> uint32_t opSCHCUH()
{
	H::enter(*this, "opSCHCUH"); return opSEARCHUH<H>(1); }

template <class H> uint32_t opSCHCDB()
{
	H::enter(*this, "opSCHCDB"); return opSEARCHDB<H>(1); }

template <class H> uint32_t opSCHCDH()
{
	H::enter(*this, "opSCHCDH"); return opSEARCHDH<H>(1); }

template <class H> uint32_t opSKPCUB()
{
	H::enter(*this, "opSKPCUB"); return opSEARCHUB<H>(0); }

template <class H> uint32_t opSKPCUH()
{
	H::enter(*this, "opSKPCUH"); return opSEARCHUH<H>(0); }

template <class H> uint32_t opSKPCDB()
{
	H::enter(*this, "opSKPCDB"); return opSEARCHDB<H>(0); }

template <class H> uint32_t opSKPCDH()
{
	H::enter(*this, "opSKPCDH"); return opSEARCHDH<H>(0); }


template <class H> uint32_t opCMPCB()
{
	H::enter(*this, "opCMPCB"); return opCMPSTRB<H>(0, 0); }

template <class H> uint32_t opCMPCH()
{
	H::enter(*this, "opCMPCH"); return opCMPSTRH<H>(0, 0); }

template <class H> uint32_t opCMPCFB()
{
	H::enter(*this, "opCMPCFB"); return opCMPSTRB<H>(1, 0); }

template <class H> uint32_t opCMPCFH()
{
	H::enter(*this, "opCMPCFH"); return opCMPSTRH<H>(1, 0); }

template <class H> uint32_t opCMPCSB()
{
	H::enter(*this, "opCMPCSB"); return opCMPSTRB<H>(0, 1); }

template <class H> uint32_t opCMPCSH()
{
	H::enter(*this, "opCMPCSH"); return opCMPSTRH<H>(0, 1); }


template <class H> uint32_t opMOVCUB()
{
	H::enter(*this, "opMOVCUB"); return opMOVSTRUB<H>(0, 0); }

template <class H> uint32_t opMOVCUH()
{
	H::enter(*this, "opMOVCUH"); return opMOVSTRUH<H>(0, 0); }

template <class H> uint32_t opMOVCFUB()
{
	H::enter(*this, "opMOVCFUB"); return opMOVSTRUB<H>(1, 0); }

template <class H> uint32_t opMOVCFUH()
{
	H::enter(*this, "opMOVCFUH"); return opMOVSTRUH<H>(1, 0); }

template <class H> uint32_t opMOVCSUB()
{
	H::enter(*this, "opMOVCSUB"); return opMOVSTRUB<H>(0, 1); }

template <class H> uint32_t opMOVCSUH()
{
	H::enter(*this, "opMOVCSUH"); return opMOVSTRUH<H>(0, 1); }


template <class H> uint32_t opMOVCDB()
{
	H::enter(*this, "opMOVCDB"); return opMOVSTRDB<H>(0, 0); }

template <class H> uint32_t opMOVCDH()
{
	H::enter(*this, "opMOVCDH"); return opMOVSTRDH<H>(0, 0); }

template <class H> uint32_t opMOVCFDB()
{
	H::enter(*this, "opMOVCFDB"); return opMOVSTRDB<H>(1, 0); }

template <class H> uint32_t opMOVCFDH()
{
	H::enter(*this, "opMOVCFDH"); return opMOVSTRDH<H>(1, 0); }


template <class H> uint32_t opEXTBFZ()
{
	H::enter(*this, "opEXTBFZ");
	H::F7bDecodeFirstOperand(*this, Am::BitRead, 11);

	F7BCREATEBITMASK(m_lenop1);

	m_modwritevalw = (m_op1 >> m_bamoffset) & m_lenop1;

	H::F7bWriteSecondOperand(*this, 2);

	F7BEND();
}


template <class H> uint32_t opEXTBFS()
{
	H::enter(*this, "opEXTBFS");
	H::F7bDecodeFirstOperand(*this, Am::BitRead, 11);

	F7BCREATEBITMASK(m_lenop1);

	m_modwritevalw = (m_op1 >> m_bamoffset) & m_lenop1;
	if (m_modwritevalw & ((m_lenop1 + 1) >> 1))
		m_modwritevalw |= ~m_lenop1;

	H::F7bWriteSecondOperand(*this, 2);

	F7BEND();
}


template <class H> uint32_t opEXTBFL()
{
	H::enter(*this, "opEXTBFL");
	uint32_t appw;

	H::F7bDecodeFirstOperand(*this, Am::BitRead, 11);

	appw = m_lenop1;
	F7BCREATEBITMASK(m_lenop1);

	m_modwritevalw = (m_op1 >> m_bamoffset) & m_lenop1;
	m_modwritevalw <<= 32 - appw;

	H::F7bWriteSecondOperand(*this, 2);

	F7BEND();
}


template <class H> uint32_t opSCHBS(uint32_t bSearch1)
{
	H::enter(*this, "opSCHBS");
	uint32_t i, data;
	uint32_t offset;

	H::F7bDecodeFirstOperand(*this, Am::BitAddr, 10);

	// Read first uint8_t
	m_op1 += m_bamoffset / 8;
	data = m_program->read_byte(m_op1);
	offset = m_bamoffset & 7;

	// Scan bitstring
	for (i = 0; i < m_lenop1; i++)
	{
		// Update the work register
		R28 = m_op1;

		// There is a 0 / 1 at current offset?
		if ((bSearch1 && (data&(1 << offset))) ||
			(!bSearch1 && !(data&(1 << offset))))
			break;

		// Next bit please
		offset++;
		if (offset == 8)
		{
			// Next uint8_t please
			offset = 0;
			m_op1++;
			data = m_program->read_byte(m_op1);
		}
	}

	// Set zero if bit not found
	_Z = (i == m_lenop1);

	// Write to destination the final offset
	m_modwritevalw = i;
	H::F7bWriteSecondOperand(*this, 2);

	F7BEND();
}


template <class H> uint32_t opSCH0BSU()
{
	H::enter(*this, "opSCH0BSU"); return opSCHBS<H>(0); }

template <class H> uint32_t opSCH1BSU()
{
	H::enter(*this, "opSCH1BSU"); return opSCHBS<H>(1); }


template <class H> uint32_t opINSBFR()
{
	H::enter(*this, "opINSBFR");
	uint32_t appw;
	H::F7cDecodeOperands(*this, Am::Read, 2,Am::BitAddr, 11);

	F7CCREATEBITMASK(m_lenop1);

	m_op2 += m_bamoffset / 8;
	appw = m_program->read_dword_unaligned(m_op2);
	m_bamoffset &= 7;

	appw &= ~(m_lenop1 << m_bamoffset);
	appw |=  (m_lenop1 & m_op1) << m_bamoffset;

	m_program->write_dword_unaligned(m_op2, appw);

	F7CEND();
}


template <class H> uint32_t opINSBFL()
{
	H::enter(*this, "opINSBFL");
	uint32_t appw;
	H::F7cDecodeOperands(*this, Am::Read, 2,Am::BitAddr, 11);

	m_op1 >>= (32 - m_lenop1);

	F7CCREATEBITMASK(m_lenop1);

	m_op2 += m_bamoffset / 8;
	appw = m_program->read_dword_unaligned(m_op2);
	m_bamoffset &= 7;

	appw &= ~(m_lenop1 << m_bamoffset);
	appw |=  (m_lenop1 & m_op1) << m_bamoffset;

	m_program->write_dword_unaligned(m_op2, appw);

	F7CEND();
}


template <class H> uint32_t opMOVBSD()
{
	H::enter(*this, "opMOVBSD");
	uint32_t i;
	uint8_t srcdata, dstdata;

	H::F7bDecodeOperands(*this, Am::BitAddr, 10, Am::BitAddr, 10);

//  if (m_lenop1 != 1)
//  { int a = 1; }

	m_bamoffset1 += m_lenop1 - 1;
	m_bamoffset2 += m_lenop1 - 1;

	m_op1 += m_bamoffset1 / 8;
	m_op2 += m_bamoffset2 / 8;

	m_bamoffset1 &= 7;
	m_bamoffset2 &= 7;

	srcdata = m_program->read_byte(m_op1);
	dstdata = m_program->read_byte(m_op2);

	for (i = 0; i < m_lenop1; i++)
	{
		// Update work registers
		R28 = m_op1;
		R27 = m_op2;

		dstdata &= ~(1 << m_bamoffset2);
		dstdata |= ((srcdata >> m_bamoffset1) & 1) << m_bamoffset2;

		if (m_bamoffset1 == 0)
		{
			m_bamoffset1 = 8;
			m_op1--;
			srcdata = m_program->read_byte(m_op1);
		}
		if (m_bamoffset2 == 0)
		{
			m_program->write_byte(m_op2, dstdata);
			m_bamoffset2 = 8;
			m_op2--;
			dstdata = m_program->read_byte(m_op2);
		}

		m_bamoffset1--;
		m_bamoffset2--;
	}

	// Flush of the final data
	if (m_bamoffset2 != 7)
		m_program->write_byte(m_op2, dstdata);

	F7BEND();
}


template <class H> uint32_t opMOVBSU()
{
	H::enter(*this, "opMOVBSU");
	uint32_t i;
	uint8_t srcdata, dstdata;

	H::F7bDecodeOperands(*this, Am::BitAddr, 10, Am::BitAddr, 10);

	m_op1 += m_bamoffset1 / 8;
	m_op2 += m_bamoffset2 / 8;

	m_bamoffset1 &= 7;
	m_bamoffset2 &= 7;

	srcdata = m_program->read_byte(m_op1);
	dstdata = m_program->read_byte(m_op2);

	for (i = 0; i < m_lenop1; i++)
	{
		// Update work registers
		R28 = m_op1;
		R27 = m_op2;

		dstdata &= ~(1 << m_bamoffset2);
		dstdata |= ((srcdata >> m_bamoffset1) & 1) << m_bamoffset2;

		m_bamoffset1++;
		m_bamoffset2++;
		if (m_bamoffset1 == 8)
		{
			m_bamoffset1 = 0;
			m_op1++;
			srcdata = m_program->read_byte(m_op1);
		}
		if (m_bamoffset2 == 8)
		{
			m_program->write_byte(m_op2, dstdata);
			m_bamoffset2 = 0;
			m_op2++;
			dstdata = m_program->read_byte(m_op2);
		}
	}

	// Flush of the final data
	if (m_bamoffset2 != 0)
		m_program->write_byte(m_op2, dstdata);

	F7BEND();
}


// RADM 0x20f4b8 holds the time left

template <class H> uint32_t opADDDC()
{
	H::enter(*this, "opADDDC");
	uint8_t appb;
	uint8_t src, dst;

	H::F7cDecodeOperands(*this, Am::Read, 0, Am::Addr, 0);

	if (m_lenop1 != 0)
	{
		v60_log("ADDDC %x (pat: %x)\n", m_op1, m_lenop1);
	}

	F7CLOADOP2BYTE(appb);

	src = (uint8_t)(m_op1 >> 4) * 10 + (uint8_t)(m_op1 & 0xF);
	dst = (appb >> 4) * 10 + (appb & 0xF);

	appb = src + dst + (_CY?1:0);

	if (appb >= 100)
	{
		appb -= 100;
		_CY = 1;
	}
	else
		_CY = 0;

	// compute z flag:
	// cleared if result non-zero or carry generated
	// unchanged otherwise
	if (appb != 0 || _CY)
		_Z = 0;

	appb = ((appb / 10) << 4) | (appb % 10);

	F7CSTOREOP2BYTE();
	F7CEND();
}


template <class H> uint32_t opSUBDC()
{
	H::enter(*this, "opSUBDC");
	int8_t appb;
	uint32_t src, dst;

	H::F7cDecodeOperands(*this, Am::Read, 0, Am::Addr, 0);

	if (m_lenop1 != 0)
	{
		v60_log("SUBDC %x (pat: %x)\n", m_op1, m_lenop1);
	}

	F7CLOADOP2BYTE(appb);

	src = (uint32_t)(m_op1 >> 4) * 10 + (uint32_t)(m_op1 & 0xF);
	dst = ((appb & 0xF0) >> 4) * 10 + (appb & 0xF);

	// Note that this APPB must be SIGNED!
	appb = (int32_t)dst - (int32_t)src - (_CY?1:0);

	if (appb < 0)
	{
		appb += 100;
		_CY = 1;
	}
	else
		_CY = 0;

	// compute z flag:
	// cleared if result non-zero or carry generated
	// unchanged otherwise
	if (appb != 0 || _CY)
		_Z = 0;

	appb = ((appb / 10) << 4) | (appb % 10);

	F7CSTOREOP2BYTE();
	F7CEND();
}


template <class H> uint32_t opSUBRDC()
{
	H::enter(*this, "opSUBRDC");
	int8_t appb;
	uint32_t src, dst;

	H::F7cDecodeOperands(*this, Am::Read, 0, Am::Addr, 0);

	if (m_lenop1 != 0)
	{
		v60_log("SUBRDC %x (pat: %x)\n", m_op1, m_lenop1);
	}

	F7CLOADOP2BYTE(appb);

	src = (uint32_t)(m_op1 >> 4) * 10 + (uint32_t)(m_op1 & 0xF);
	dst = ((appb & 0xF0) >> 4) * 10 + (appb & 0xF);

	// Note that this APPB must be SIGNED!
	appb = (int32_t)src - (int32_t)dst - (_CY?1:0);

	if (appb < 0)
	{
		appb += 100;
		_CY = 1;
	}
	else
		_CY = 0;

	// compute z flag:
	// cleared if result non-zero or carry generated
	// unchanged otherwise
	if (appb != 0 || _CY)
		_Z = 0;

	appb = ((appb / 10) << 4) | (appb % 10);

	F7CSTOREOP2BYTE();
	F7CEND();
}


template <class H> uint32_t opCVTDPZ()
{
	H::enter(*this, "opCVTDPZ");
	uint16_t apph;

	H::F7cDecodeOperands(*this, Am::Read, 0, Am::Addr, 1);

	apph = (uint16_t)(((m_op1 >> 4) & 0xF) | ((m_op1 & 0xF) << 8));
	apph |= (m_lenop1);
	apph |= (m_lenop1 << 8);

	// Z flag is unchanged if src is zero, cleared otherwise
	if (m_op1 != 0) _Z = 0;

	F7CSTOREOP2HALF();
	F7CEND();
}


template <class H> uint32_t opCVTDZP()
{
	H::enter(*this, "opCVTDZP");
	uint8_t appb;
	H::F7cDecodeOperands(*this, Am::Read, 1, Am::Addr, 0);

	if ((m_op1 & 0xF0) != (m_lenop1 & 0xF0) || ((m_op1 >> 8) & 0xF0) != (m_lenop1 & 0xF0))
	{
		// Decimal exception
		v60_log("CVTD.ZP Decimal exception #1!\n");
	}

	if ((m_op1 & 0xF) > 9 || ((m_op1 >> 8) & 0xF) > 9)
	{
		// Decimal exception
		v60_log("CVTD.ZP Decimal exception #2!\n");
	}

	appb = (uint8_t)(((m_op1 >> 8) & 0xF) | ((m_op1 & 0xF) << 4));
	if (appb != 0) _Z = 0;

	F7CSTOREOP2BYTE();
	F7CEND();
}


template <class H> uint32_t op58UNHANDLED()
{
	H::enter(*this, "op58UNHANDLED");
	v60_fatal("Unhandled 58 opcode at PC: /%06x\n", PC);
}


template <class H> uint32_t op5AUNHANDLED()
{
	H::enter(*this, "op5AUNHANDLED");
	v60_fatal("Unhandled 5A opcode at PC: /%06x\n", PC);
}


template <class H> uint32_t op5BUNHANDLED()
{
	H::enter(*this, "op5BUNHANDLED");
	v60_fatal("Unhandled 5B opcode at PC: /%06x\n", PC);
}


template <class H> uint32_t op5DUNHANDLED()
{
	H::enter(*this, "op5DUNHANDLED");
	v60_fatal("Unhandled 5D opcode at PC: /%06x\n", PC);
}


template <class H> uint32_t op59UNHANDLED()
{
	H::enter(*this, "op59UNHANDLED");
	v60_fatal("Unhandled 59 opcode at PC: /%06x\n", PC);
}














template <class H> uint32_t op58()
{
	H::enter(*this, "op58");
	m_subop = OpRead8(PC + 1);

	return (this->*Tables<H>::s_Op58Table[m_subop & 0x1F])();
}


template <class H> uint32_t op5A()
{
	H::enter(*this, "op5A");
	m_subop = OpRead8(PC + 1);

	return (this->*Tables<H>::s_Op5ATable[m_subop & 0x1F])();
}


template <class H> uint32_t op5B()
{
	H::enter(*this, "op5B");
	m_subop = OpRead8(PC + 1);

	return (this->*Tables<H>::s_Op5BTable[m_subop & 0x1F])();
}


template <class H> uint32_t op5D()
{
	H::enter(*this, "op5D");
	m_subop = OpRead8(PC + 1);

	return (this->*Tables<H>::s_Op5DTable[m_subop & 0x1F])();
}


template <class H> uint32_t op59()
{
	H::enter(*this, "op59");
	m_subop = OpRead8(PC + 1);

	return (this->*Tables<H>::s_Op59Table[m_subop & 0x1F])();
}


template <class H> uint32_t opUNHANDLED()
{
	H::enter(*this, "opUNHANDLED");
	v60_fatal("Unhandled OpCode found : %02x at %08x\n", OpRead16(PC), PC);
}
