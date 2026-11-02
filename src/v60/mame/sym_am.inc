// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Transplanted from MAME src/devices/cpu/v60 by scripts/transplant_v60.py:
// regenerate, do not edit (THIRD_PARTY.md).
// AM1 Functions (for ReadAM)
// **************************

uint32_t SymAm::am1Register()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = sym_cast_uint8_t(m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = sym_cast_uint16_t(m_reg[m_modval & 0x1F]);
		break;
	case 2:
		m_amout = m_reg[m_modval & 0x1F];
		break;
	}

	return 1;
}


uint32_t SymAm::am1RegisterIndirect()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F]);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
		break;
	}

	return 1;
}


uint32_t SymAm::bam1RegisterIndirect()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	return 1;
}


uint32_t SymAm::am1RegisterIndirectIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 2;
}


uint32_t SymAm::bam1RegisterIndirectIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}


uint32_t SymAm::am1Autoincrement()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F]);
		m_reg[m_modval & 0x1F]++;
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F]);
		m_reg[m_modval & 0x1F] +=2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
		m_reg[m_modval & 0x1F] +=4;
		break;
	}

	return 1;
}


uint32_t SymAm::bam1Autoincrement()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F] +=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F] +=4;
		break;
	default:
		v60_fatal("CPU - BAM1 - 7\n");
		break;
	}
	return 1;
}


uint32_t SymAm::am1Autodecrement()
{
	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F]--;
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_reg[m_modval & 0x1F]-=2;
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F]);
		break;
	case 2:
		m_reg[m_modval & 0x1F]-=4;
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
		break;
	}

	return 1;
}


uint32_t SymAm::bam1Autodecrement()
{
	m_bamoffset = 0;
	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F]-=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F]-=4;
		break;
	default:
		v60_fatal("CPU - BAM1 - 7\n");
		break;
	}
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	return 1;
}


uint32_t SymAm::am1Displacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
		break;
	}

	return 2;
}


uint32_t SymAm::bam1Displacement8()
{
	m_bamoffset = m_program->read_byte(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}



uint32_t SymAm::am1Displacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
		break;
	}

	return 3;
}


uint32_t SymAm::bam1Displacement16()
{
	m_bamoffset = OpRead16(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1Displacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1Displacement32()
{
	m_bamoffset = OpRead32(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t SymAm::am1DisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}


uint32_t SymAm::bam1DisplacementIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1DisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}


uint32_t SymAm::bam1DisplacementIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t SymAm::am1DisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}


uint32_t SymAm::bam1DisplacementIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}



uint32_t SymAm::am1PCDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
		break;
	}

	return 2;
}


uint32_t SymAm::bam1PCDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}


uint32_t SymAm::am1PCDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
		break;
	}

	return 3;
}


uint32_t SymAm::bam1PCDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1PCDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + OpRead32(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + OpRead32(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1PCDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t SymAm::am1PCDisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}


uint32_t SymAm::bam1PCDisplacementIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}



uint32_t SymAm::am1PCDisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}


uint32_t SymAm::bam1PCDisplacementIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t SymAm::am1PCDisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}


uint32_t SymAm::bam1PCDisplacementIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t SymAm::am1DisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
		break;
	}

	return 2;
}


uint32_t SymAm::bam1DisplacementIndirect8()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
	return 2;
}


uint32_t SymAm::am1DisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
		break;
	}

	return 3;
}


uint32_t SymAm::bam1DisplacementIndirect16()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
	return 3;
}


uint32_t SymAm::am1DisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1DisplacementIndirect32()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
	return 5;
}


uint32_t SymAm::am1DisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}


uint32_t SymAm::bam1DisplacementIndirectIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1DisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}


uint32_t SymAm::bam1DisplacementIndirectIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t SymAm::am1DisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}


uint32_t SymAm::bam1DisplacementIndirectIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t SymAm::am1PCDisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
		break;
	}

	return 2;
}


uint32_t SymAm::bam1PCDisplacementIndirect8()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
	return 2;
}


uint32_t SymAm::am1PCDisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
		break;
	}

	return 3;
}


uint32_t SymAm::bam1PCDisplacementIndirect16()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
	return 3;
}


uint32_t SymAm::am1PCDisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1PCDisplacementIndirect32()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
	return 5;
}


uint32_t SymAm::am1PCDisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}


uint32_t SymAm::bam1PCDisplacementIndirectIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1PCDisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}


uint32_t SymAm::bam1PCDisplacementIndirectIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t SymAm::am1PCDisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}


uint32_t SymAm::bam1PCDisplacementIndirectIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t SymAm::am1DoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;
	}

	return 3;
}


uint32_t SymAm::bam1DoubleDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 2);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1DoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1DoubleDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 3);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t SymAm::am1DoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;
	}

	return 9;
}


uint32_t SymAm::bam1DoubleDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 5);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 9;
}


uint32_t SymAm::am1PCDoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;
	}

	return 3;
}


uint32_t SymAm::bam1PCDoubleDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 2);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t SymAm::am1PCDoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1PCDoubleDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 3);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}



uint32_t SymAm::am1PCDoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;
	}

	return 9;
}


uint32_t SymAm::bam1PCDoubleDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 5);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 9;
}


uint32_t SymAm::am1DirectAddress()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(OpRead32(m_modadd + 1));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(OpRead32(m_modadd + 1));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1DirectAddress()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
	return 5;
}


uint32_t SymAm::am1DirectAddressIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}


uint32_t SymAm::bam1DirectAddressIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t SymAm::am1DirectAddressDeferred()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
		break;
	}

	return 5;
}


uint32_t SymAm::bam1DirectAddressDeferred()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
	return 5;
}


uint32_t SymAm::am1DirectAddressDeferredIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}


uint32_t SymAm::bam1DirectAddressDeferredIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t SymAm::am1Immediate()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = OpRead8(m_modadd + 1);
		return 2;

	case 1:
		m_amout = OpRead16(m_modadd + 1);
		return 3;

	case 2:
		m_amout = OpRead32(m_modadd + 1);
		return 5;
	}

	// It should not be here!  Written to avoid warning
	assert(0);
	return 1;
}


uint32_t SymAm::am1ImmediateQuick()
{
	m_amout = m_modval & 0xF;
	return 1;
}





// AM1 Tables (for ReadAM)
// ***********************

uint32_t SymAm::am1Error1()
{
	v60_fatal("CPU - AM1 - 1 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam1Error1()
{
	v60_fatal("CPU - BAM1 - 1 (PC=%06x)\n", PC);
}


uint32_t SymAm::am1Error2()
{
	v60_fatal("CPU - AM1 - 2 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam1Error2()
{
	v60_fatal("CPU - BAM1 - 2 (PC=%06x)\n", PC);
}


uint32_t SymAm::am1Error3()
{
	v60_fatal("CPU - AM1 - 3 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam1Error3()
{
	v60_fatal("CPU - BAM1 - 3 (PC=%06x)\n", PC);
}


uint32_t SymAm::am1Error4()
{
	v60_fatal("CPU - AM1 - 4 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam1Error4()
{
	v60_fatal("CPU - BAM1 - 4 (PC=%06x)\n", PC);
}


uint32_t SymAm::am1Error5()
{
	v60_fatal("CPU - AM1 - 5 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam1Error5()
{
	v60_fatal("CPU - BAM1 - 5 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam1Error6()
{
	v60_fatal("CPU - BAM1 - 6 (PC=%06x)\n", PC);
}


const SymAm::am_func SymAm::s_AMTable1_G7a[16] =
{
	&SymAm::am1PCDisplacementIndexed8,
	&SymAm::am1PCDisplacementIndexed16,
	&SymAm::am1PCDisplacementIndexed32,
	&SymAm::am1DirectAddressIndexed,
	&SymAm::am1Error5,
	&SymAm::am1Error5,
	&SymAm::am1Error5,
	&SymAm::am1Error5,
	&SymAm::am1PCDisplacementIndirectIndexed8,
	&SymAm::am1PCDisplacementIndirectIndexed16,
	&SymAm::am1PCDisplacementIndirectIndexed32,
	&SymAm::am1DirectAddressDeferredIndexed,
	&SymAm::am1Error5,
	&SymAm::am1Error5,
	&SymAm::am1Error5,
	&SymAm::am1Error5
};


const SymAm::am_func SymAm::s_BAMTable1_G7a[16] =
{
	&SymAm::bam1PCDisplacementIndexed8,
	&SymAm::bam1PCDisplacementIndexed16,
	&SymAm::bam1PCDisplacementIndexed32,
	&SymAm::bam1DirectAddressIndexed,
	&SymAm::bam1Error5,
	&SymAm::bam1Error5,
	&SymAm::bam1Error5,
	&SymAm::bam1Error5,
	&SymAm::bam1PCDisplacementIndirectIndexed8,
	&SymAm::bam1PCDisplacementIndirectIndexed16,
	&SymAm::bam1PCDisplacementIndirectIndexed32,
	&SymAm::bam1DirectAddressDeferredIndexed,
	&SymAm::bam1Error5,
	&SymAm::bam1Error5,
	&SymAm::bam1Error5,
	&SymAm::bam1Error5
};



uint32_t SymAm::am1Group7a()
{
	if (!(m_modval2 & 0x10))
		return am1Error4();

	return (this->*s_AMTable1_G7a[m_modval2 & 0xF])();
}


uint32_t SymAm::bam1Group7a()
{
	if (!(m_modval2 & 0x10))
		return bam1Error4();

	return (this->*s_BAMTable1_G7a[m_modval2 & 0xF])();
}


const SymAm::am_func SymAm::s_AMTable1_G7[32] =
{
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1ImmediateQuick,
	&SymAm::am1PCDisplacement8,
	&SymAm::am1PCDisplacement16,
	&SymAm::am1PCDisplacement32,
	&SymAm::am1DirectAddress,
	&SymAm::am1Immediate,
	&SymAm::am1Error2,
	&SymAm::am1Error2,
	&SymAm::am1Error2,
	&SymAm::am1PCDisplacementIndirect8,
	&SymAm::am1PCDisplacementIndirect16,
	&SymAm::am1PCDisplacementIndirect32,
	&SymAm::am1DirectAddressDeferred,
	&SymAm::am1PCDoubleDisplacement8,
	&SymAm::am1PCDoubleDisplacement16,
	&SymAm::am1PCDoubleDisplacement32,
	&SymAm::am1Error2
};


const SymAm::am_func SymAm::s_BAMTable1_G7[32] =
{
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1Error6,
	&SymAm::bam1PCDisplacement8,
	&SymAm::bam1PCDisplacement16,
	&SymAm::bam1PCDisplacement32,
	&SymAm::bam1DirectAddress,
	&SymAm::bam1Error6,
	&SymAm::bam1Error2,
	&SymAm::bam1Error2,
	&SymAm::bam1Error2,
	&SymAm::bam1PCDisplacementIndirect8,
	&SymAm::bam1PCDisplacementIndirect16,
	&SymAm::bam1PCDisplacementIndirect32,
	&SymAm::bam1DirectAddressDeferred,
	&SymAm::bam1PCDoubleDisplacement8,
	&SymAm::bam1PCDoubleDisplacement16,
	&SymAm::bam1PCDoubleDisplacement32,
	&SymAm::bam1Error2
};




const SymAm::am_func SymAm::s_AMTable1_G6[8] =
{
	&SymAm::am1DisplacementIndexed8,
	&SymAm::am1DisplacementIndexed16,
	&SymAm::am1DisplacementIndexed32,
	&SymAm::am1RegisterIndirectIndexed,
	&SymAm::am1DisplacementIndirectIndexed8,
	&SymAm::am1DisplacementIndirectIndexed16,
	&SymAm::am1DisplacementIndirectIndexed32,
	&SymAm::am1Group7a
};


const SymAm::am_func SymAm::s_BAMTable1_G6[8] =
{
	&SymAm::bam1DisplacementIndexed8,
	&SymAm::bam1DisplacementIndexed16,
	&SymAm::bam1DisplacementIndexed32,
	&SymAm::bam1RegisterIndirectIndexed,
	&SymAm::bam1DisplacementIndirectIndexed8,
	&SymAm::bam1DisplacementIndirectIndexed16,
	&SymAm::bam1DisplacementIndirectIndexed32,
	&SymAm::bam1Group7a
};



uint32_t SymAm::am1Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable1_G6[m_modval2 >> 5])();
}


uint32_t SymAm::bam1Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_BAMTable1_G6[m_modval2 >> 5])();
}



uint32_t SymAm::am1Group7()
{
	return (this->*s_AMTable1_G7[m_modval & 0x1F])();
}


uint32_t SymAm::bam1Group7()
{
	return (this->*s_BAMTable1_G7[m_modval & 0x1F])();
}


const SymAm::am_func SymAm::s_AMTable1[2][8] =
{
	{
		&SymAm::am1Displacement8,
		&SymAm::am1Displacement16,
		&SymAm::am1Displacement32,
		&SymAm::am1RegisterIndirect,
		&SymAm::am1DisplacementIndirect8,
		&SymAm::am1DisplacementIndirect16,
		&SymAm::am1DisplacementIndirect32,
		&SymAm::am1Group7
	},

	{
		&SymAm::am1DoubleDisplacement8,
		&SymAm::am1DoubleDisplacement16,
		&SymAm::am1DoubleDisplacement32,
		&SymAm::am1Register,
		&SymAm::am1Autoincrement,
		&SymAm::am1Autodecrement,
		&SymAm::am1Group6,
		&SymAm::am1Error1
	}
};



const SymAm::am_func SymAm::s_BAMTable1[2][8] =
{
	{
		&SymAm::bam1Displacement8,
		&SymAm::bam1Displacement16,
		&SymAm::bam1Displacement32,
		&SymAm::bam1RegisterIndirect,
		&SymAm::bam1DisplacementIndirect8,
		&SymAm::bam1DisplacementIndirect16,
		&SymAm::bam1DisplacementIndirect32,
		&SymAm::bam1Group7
	},

	{
		&SymAm::bam1DoubleDisplacement8,
		&SymAm::bam1DoubleDisplacement16,
		&SymAm::bam1DoubleDisplacement32,
		&SymAm::bam1Error6,
		&SymAm::bam1Autoincrement,
		&SymAm::bam1Autodecrement,
		&SymAm::bam1Group6,
		&SymAm::bam1Error1
	}
};

// AM2 Functions (for ReadAMAddress)
// *********************************

uint32_t SymAm::am2Register()
{
	m_amflag = 1;
	m_amout = m_modval & 0x1F;
	return 1;
}


uint32_t SymAm::am2RegisterIndirect()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}


uint32_t SymAm::bam2RegisterIndirect()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = 0;
	return 1;
}


uint32_t SymAm::am2RegisterIndirectIndexed()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 2;
}


uint32_t SymAm::bam2RegisterIndirectIndexed()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F];
	m_bamoffset = m_reg[m_modval & 0x1F];
	return 2;
}


uint32_t SymAm::am2Autoincrement()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];

	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F] += 1;
		break;
	case 1:
		m_reg[m_modval & 0x1F] += 2;
		break;
	case 2:
		m_reg[m_modval & 0x1F] += 4;
		break;
	case 3:
		m_reg[m_modval & 0x1F] += 8;
		break;
	}

	return 1;
}


uint32_t SymAm::bam2Autoincrement()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = 0;

	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F] +=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F] +=4;
		break;
	default:
		v60_fatal("CPU - AM2 - 7 (t0 PC=%x)\n", PC);
		break;
	}

	return 1;
}


uint32_t SymAm::am2Autodecrement()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F] -= 1;
		break;
	case 1:
		m_reg[m_modval & 0x1F] -= 2;
		break;
	case 2:
		m_reg[m_modval & 0x1F] -= 4;
		break;
	case 3:
		m_reg[m_modval & 0x1F] -= 8;
		break;
	}

	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}


uint32_t SymAm::bam2Autodecrement()
{
	m_amflag = 0;
	m_bamoffset = 0;

	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F]-=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F]-=4;
		break;
	default:
		v60_fatal("CPU - BAM2 - 7 (PC=%06x)\n", PC);
		break;
	}

	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}



uint32_t SymAm::am2Displacement8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t SymAm::bam2Displacement8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t SymAm::am2Displacement16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t SymAm::bam2Displacement16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t SymAm::am2Displacement32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1);

	return 5;
}


uint32_t SymAm::bam2Displacement32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = OpRead32(m_modadd + 1);

	return 5;
}


uint32_t SymAm::am2DisplacementIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}


uint32_t SymAm::bam2DisplacementIndexed8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t SymAm::am2DisplacementIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}


uint32_t SymAm::bam2DisplacementIndexed16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t SymAm::am2DisplacementIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}


uint32_t SymAm::bam2DisplacementIndexed32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t SymAm::am2PCDisplacement8()
{
	m_amflag = 0;
	m_amout = PC + (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t SymAm::bam2PCDisplacement8()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t SymAm::am2PCDisplacement16()
{
	m_amflag = 0;
	m_amout = PC + (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t SymAm::bam2PCDisplacement16()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t SymAm::am2PCDisplacement32()
{
	m_amflag = 0;
	m_amout = PC + OpRead32(m_modadd + 1);

	return 5;
}


uint32_t SymAm::bam2PCDisplacement32()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = OpRead32(m_modadd + 1);

	return 5;
}



uint32_t SymAm::am2PCDisplacementIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}


uint32_t SymAm::bam2PCDisplacementIndexed8()
{
	m_amflag = 0;
	m_amout = PC + (int8_t)OpRead8(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t SymAm::am2PCDisplacementIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}


uint32_t SymAm::bam2PCDisplacementIndexed16()
{
	m_amflag = 0;
	m_amout = PC + (int16_t)OpRead16(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t SymAm::am2PCDisplacementIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}


uint32_t SymAm::bam2PCDisplacementIndexed32()
{
	m_amflag = 0;
	m_amout = PC + OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t SymAm::am2DisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));

	return 2;
}


uint32_t SymAm::bam2DisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = 0;
	return 2;
}


uint32_t SymAm::am2DisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));

	return 3;
}


uint32_t SymAm::bam2DisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = 0;
	return 3;
}


uint32_t SymAm::am2DisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));

	return 5;
}


uint32_t SymAm::bam2DisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}


uint32_t SymAm::am2DisplacementIndirectIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}


uint32_t SymAm::bam2DisplacementIndirectIndexed8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t SymAm::am2DisplacementIndirectIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}


uint32_t SymAm::bam2DisplacementIndirectIndexed16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t SymAm::am2DisplacementIndirectIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}


uint32_t SymAm::bam2DisplacementIndirectIndexed32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t SymAm::am2PCDisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));

	return 2;
}


uint32_t SymAm::bam2PCDisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = 0;

	return 2;
}


uint32_t SymAm::am2PCDisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));

	return 3;
}


uint32_t SymAm::bam2PCDisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = 0;

	return 3;
}


uint32_t SymAm::am2PCDisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));

	return 5;
}


uint32_t SymAm::bam2PCDisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}


uint32_t SymAm::am2PCDisplacementIndirectIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}


uint32_t SymAm::bam2PCDisplacementIndirectIndexed8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t SymAm::am2PCDisplacementIndirectIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}



uint32_t SymAm::bam2PCDisplacementIndirectIndexed16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}



uint32_t SymAm::am2PCDisplacementIndirectIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}


uint32_t SymAm::bam2PCDisplacementIndirectIndexed32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t SymAm::am2DoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t SymAm::bam2DoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t SymAm::am2DoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3);

	return 5;
}


uint32_t SymAm::bam2DoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 3);

	return 5;
}


uint32_t SymAm::am2DoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5);

	return 9;
}


uint32_t SymAm::bam2DoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
	m_bamoffset = OpRead32(m_modadd + 5);

	return 9;
}



uint32_t SymAm::am2PCDoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t SymAm::bam2PCDoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t SymAm::am2PCDoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3);

	return 5;
}


uint32_t SymAm::bam2PCDoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 3);

	return 5;
}


uint32_t SymAm::am2PCDoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5);

	return 9;
}


uint32_t SymAm::bam2PCDoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
	m_bamoffset = OpRead32(m_modadd + 5);

	return 9;
}


uint32_t SymAm::am2DirectAddress()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 1);

	return 5;
}


uint32_t SymAm::bam2DirectAddress()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 1);
	m_bamoffset = 0;

	return 5;
}


uint32_t SymAm::am2DirectAddressIndexed()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}


uint32_t SymAm::bam2DirectAddressIndexed()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t SymAm::am2DirectAddressDeferred()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));

	return 5;
}


uint32_t SymAm::bam2DirectAddressDeferred()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}


uint32_t SymAm::am2DirectAddressDeferredIndexed()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}


uint32_t SymAm::bam2DirectAddressDeferredIndexed()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t SymAm::am2Immediate()
{
	m_amflag = 0;
	return am1Immediate();
}


uint32_t SymAm::am2ImmediateQuick()
{
	m_amflag = 0;
	return am1ImmediateQuick();
}



// AM2 Tables (for ReadAMAddress)
// ******************************

uint32_t SymAm::am2Error1()
{
	// f1lap trips this, why?
	v60_log("CPU - AM2 - 1 (PC=%06x)", PC);
	return 0;
}


uint32_t SymAm::am2Error2()
{
	v60_fatal("CPU - AM2 - 2 (PC=%06x)\n", PC);
}


uint32_t SymAm::am2Error3()
{
	v60_fatal("CPU - AM2 - 3 (PC=%06x)\n", PC);
}


uint32_t SymAm::am2Error4()
{
	v60_fatal("CPU - AM2 - 4 (PC=%06x)\n", PC);
}


uint32_t SymAm::am2Error5()
{
	v60_fatal("CPU - AM2 - 5 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam2Error1()
{
	v60_fatal("CPU - BAM2 - 1 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam2Error2()
{
	v60_fatal("CPU - BAM2 - 2 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam2Error3()
{
	v60_fatal("CPU - BAM2 - 3 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam2Error4()
{
	v60_fatal("CPU - BAM2 - 4 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam2Error5()
{
	v60_fatal("CPU - BAM2 - 5 (PC=%06x)\n", PC);
}


uint32_t SymAm::bam2Error6()
{
	v60_fatal("CPU - BAM2 - 6 (PC=%06x)\n", PC);
}



const SymAm::am_func SymAm::s_AMTable2_G7a[16] =
{
	&SymAm::am2PCDisplacementIndexed8,
	&SymAm::am2PCDisplacementIndexed16,
	&SymAm::am2PCDisplacementIndexed32,
	&SymAm::am2DirectAddressIndexed,
	&SymAm::am2Error5,
	&SymAm::am2Error5,
	&SymAm::am2Error5,
	&SymAm::am2Error5,
	&SymAm::am2PCDisplacementIndirectIndexed8,
	&SymAm::am2PCDisplacementIndirectIndexed16,
	&SymAm::am2PCDisplacementIndirectIndexed32,
	&SymAm::am2DirectAddressDeferredIndexed,
	&SymAm::am2Error5,
	&SymAm::am2Error5,
	&SymAm::am2Error5,
	&SymAm::am2Error5
};


const SymAm::am_func SymAm::s_BAMTable2_G7a[16] =
{
	&SymAm::bam2PCDisplacementIndexed8,
	&SymAm::bam2PCDisplacementIndexed16,
	&SymAm::bam2PCDisplacementIndexed32,
	&SymAm::bam2DirectAddressIndexed,
	&SymAm::bam2Error5,
	&SymAm::bam2Error5,
	&SymAm::bam2Error5,
	&SymAm::bam2Error5,
	&SymAm::bam2PCDisplacementIndirectIndexed8,
	&SymAm::bam2PCDisplacementIndirectIndexed16,
	&SymAm::bam2PCDisplacementIndirectIndexed32,
	&SymAm::bam2DirectAddressDeferredIndexed,
	&SymAm::bam2Error5,
	&SymAm::bam2Error5,
	&SymAm::bam2Error5,
	&SymAm::bam2Error5
};


uint32_t SymAm::am2Group7a()
{
	if (!(m_modval2 & 0x10))
		return am2Error4();

	return (this->*s_AMTable2_G7a[m_modval2 & 0xF])();
}


uint32_t SymAm::bam2Group7a()
{
	if (!(m_modval2 & 0x10))
		return bam2Error4();

	return (this->*s_BAMTable2_G7a[m_modval2 & 0xF])();
}


const SymAm::am_func SymAm::s_AMTable2_G7[32] =
{
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2ImmediateQuick,
	&SymAm::am2PCDisplacement8,
	&SymAm::am2PCDisplacement16,
	&SymAm::am2PCDisplacement32,
	&SymAm::am2DirectAddress,
	&SymAm::am2Immediate,
	&SymAm::am2Error2,
	&SymAm::am2Error2,
	&SymAm::am2Error2,
	&SymAm::am2PCDisplacementIndirect8,
	&SymAm::am2PCDisplacementIndirect16,
	&SymAm::am2PCDisplacementIndirect32,
	&SymAm::am2DirectAddressDeferred,
	&SymAm::am2PCDoubleDisplacement8,
	&SymAm::am2PCDoubleDisplacement16,
	&SymAm::am2PCDoubleDisplacement32,
	&SymAm::am2Error2
};


const SymAm::am_func SymAm::s_BAMTable2_G7[32] =
{
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2Error6,
	&SymAm::bam2PCDisplacement8,
	&SymAm::bam2PCDisplacement16,
	&SymAm::bam2PCDisplacement32,
	&SymAm::bam2DirectAddress,
	&SymAm::bam2Error6,
	&SymAm::bam2Error2,
	&SymAm::bam2Error2,
	&SymAm::bam2Error2,
	&SymAm::bam2PCDisplacementIndirect8,
	&SymAm::bam2PCDisplacementIndirect16,
	&SymAm::bam2PCDisplacementIndirect32,
	&SymAm::bam2DirectAddressDeferred,
	&SymAm::bam2PCDoubleDisplacement8,
	&SymAm::bam2PCDoubleDisplacement16,
	&SymAm::bam2PCDoubleDisplacement32,
	&SymAm::bam2Error2
};


const SymAm::am_func SymAm::s_AMTable2_G6[8] =
{
	&SymAm::am2DisplacementIndexed8,
	&SymAm::am2DisplacementIndexed16,
	&SymAm::am2DisplacementIndexed32,
	&SymAm::am2RegisterIndirectIndexed,
	&SymAm::am2DisplacementIndirectIndexed8,
	&SymAm::am2DisplacementIndirectIndexed16,
	&SymAm::am2DisplacementIndirectIndexed32,
	&SymAm::am2Group7a
};


const SymAm::am_func SymAm::s_BAMTable2_G6[8] =
{
	&SymAm::bam2DisplacementIndexed8,
	&SymAm::bam2DisplacementIndexed16,
	&SymAm::bam2DisplacementIndexed32,
	&SymAm::bam2RegisterIndirectIndexed,
	&SymAm::bam2DisplacementIndirectIndexed8,
	&SymAm::bam2DisplacementIndirectIndexed16,
	&SymAm::bam2DisplacementIndirectIndexed32,
	&SymAm::bam2Group7a
};





uint32_t SymAm::am2Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable2_G6[m_modval2 >> 5])();
}

uint32_t SymAm::bam2Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_BAMTable2_G6[m_modval2 >> 5])();
}


uint32_t SymAm::am2Group7()
{
	return (this->*s_AMTable2_G7[m_modval & 0x1F])();
}

uint32_t SymAm::bam2Group7()
{
	return (this->*s_BAMTable2_G7[m_modval & 0x1F])();
}



const SymAm::am_func SymAm::s_AMTable2[2][8] =
{
	{
		&SymAm::am2Displacement8,
		&SymAm::am2Displacement16,
		&SymAm::am2Displacement32,
		&SymAm::am2RegisterIndirect,
		&SymAm::am2DisplacementIndirect8,
		&SymAm::am2DisplacementIndirect16,
		&SymAm::am2DisplacementIndirect32,
		&SymAm::am2Group7
	},

	{
		&SymAm::am2DoubleDisplacement8,
		&SymAm::am2DoubleDisplacement16,
		&SymAm::am2DoubleDisplacement32,
		&SymAm::am2Register,
		&SymAm::am2Autoincrement,
		&SymAm::am2Autodecrement,
		&SymAm::am2Group6,
		&SymAm::am2Error1
	}
};


const SymAm::am_func SymAm::s_BAMTable2[2][8] =
{
	{
		&SymAm::bam2Displacement8,
		&SymAm::bam2Displacement16,
		&SymAm::bam2Displacement32,
		&SymAm::bam2RegisterIndirect,
		&SymAm::bam2DisplacementIndirect8,
		&SymAm::bam2DisplacementIndirect16,
		&SymAm::bam2DisplacementIndirect32,
		&SymAm::bam2Group7
	},

	{
		&SymAm::bam2DoubleDisplacement8,
		&SymAm::bam2DoubleDisplacement16,
		&SymAm::bam2DoubleDisplacement32,
		&SymAm::bam2Error6,
		&SymAm::bam2Autoincrement,
		&SymAm::bam2Autodecrement,
		&SymAm::bam2Group6,
		&SymAm::bam2Error1
	}
};

// AM3 Functions (for ReadAM)
// **************************

uint32_t SymAm::am3Register()
{
	switch (m_moddim)
	{
	case 0:
		SETREG8(m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		SETREG16(m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_reg[m_modval & 0x1F] = m_modwritevalw;
		break;
	}

	return 1;
}


uint32_t SymAm::am3RegisterIndirect()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F], m_modwritevalw);
		break;
	}

	return 1;
}


uint32_t SymAm::am3RegisterIndirectIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 2;
}


uint32_t SymAm::am3Autoincrement()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F], m_modwritevalb);
		m_reg[m_modval & 0x1F] += 1;
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F], m_modwritevalh);
		m_reg[m_modval & 0x1F] += 2;
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F], m_modwritevalw);
		m_reg[m_modval & 0x1F] += 4;
		break;
	}

	return 1;
}


uint32_t SymAm::am3Autodecrement()
{
	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F] -= 1;
		m_program->write_byte(m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_reg[m_modval & 0x1F] -= 2;
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_reg[m_modval & 0x1F] -= 4;
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F], m_modwritevalw);
		break;
	}

	return 1;
}


uint32_t SymAm::am3Displacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 2;
}


uint32_t SymAm::am3Displacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3Displacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 5;
}



uint32_t SymAm::am3DisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3DisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}


uint32_t SymAm::am3DisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}



uint32_t SymAm::am3PCDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int8_t)OpRead8(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 2;
}


uint32_t SymAm::am3PCDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int16_t)OpRead16(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3PCDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + OpRead32(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + OpRead32(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + OpRead32(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t SymAm::am3PCDisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3PCDisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}


uint32_t SymAm::am3PCDisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}


uint32_t SymAm::am3DisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 2;
}


uint32_t SymAm::am3DisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3DisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 5;
}



uint32_t SymAm::am3DisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3DisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}


uint32_t SymAm::am3DisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}


uint32_t SymAm::am3PCDisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 2;
}


uint32_t SymAm::am3PCDisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3PCDisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 5;
}



uint32_t SymAm::am3PCDisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3PCDisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}


uint32_t SymAm::am3PCDisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}



uint32_t SymAm::am3DoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3DoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t SymAm::am3DoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalw);
		break;
	}

	return 9;
}



uint32_t SymAm::am3PCDoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalw);
		break;
	}

	return 3;
}


uint32_t SymAm::am3PCDoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t SymAm::am3PCDoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalw);
		break;
	}

	return 9;
}


uint32_t SymAm::am3DirectAddress()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(OpRead32(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(OpRead32(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(OpRead32(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t SymAm::am3DirectAddressIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}


uint32_t SymAm::am3DirectAddressDeferred()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t SymAm::am3DirectAddressDeferredIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalw);
		break;
	}

	return 6;
}


uint32_t SymAm::am3Immediate()
{
	v60_fatal("CPU - AM3 - IMM (PC=%06x)\n", PC);
}


uint32_t SymAm::am3ImmediateQuick()
{
	v60_fatal("CPU - AM3 - IMMQ (PC=%06x)\n", PC);
}




// AM3 Tables (for ReadAMAddress)
// ******************************

uint32_t SymAm::am3Error1()
{
	v60_fatal("CPU - AM3 - 1 (PC=%06x)\n", PC);
}


uint32_t SymAm::am3Error2()
{
	v60_fatal("CPU - AM3 - 2 (PC=%06x)\n", PC);
}


uint32_t SymAm::am3Error3()
{
	v60_fatal("CPU - AM3 - 3 (PC=%06x)\n", PC);
}


uint32_t SymAm::am3Error4()
{
	v60_fatal("CPU - AM3 - 4 (PC=%06x)\n", PC);
}


uint32_t SymAm::am3Error5()
{
	v60_fatal("CPU - AM3 - 5 (PC=%06x)\n", PC);
}


const SymAm::am_func SymAm::s_AMTable3_G7a[16] =
{
	&SymAm::am3PCDisplacementIndexed8,
	&SymAm::am3PCDisplacementIndexed16,
	&SymAm::am3PCDisplacementIndexed32,
	&SymAm::am3DirectAddressIndexed,
	&SymAm::am3Error5,
	&SymAm::am3Error5,
	&SymAm::am3Error5,
	&SymAm::am3Error5,
	&SymAm::am3PCDisplacementIndirectIndexed8,
	&SymAm::am3PCDisplacementIndirectIndexed16,
	&SymAm::am3PCDisplacementIndirectIndexed32,
	&SymAm::am3DirectAddressDeferredIndexed,
	&SymAm::am3Error5,
	&SymAm::am3Error5,
	&SymAm::am3Error5,
	&SymAm::am3Error5
};


uint32_t SymAm::am3Group7a()
{
	if (!(m_modval2 & 0x10))
		return am3Error4();

	return (this->*s_AMTable3_G7a[m_modval2 & 0xF])();
}


const SymAm::am_func SymAm::s_AMTable3_G7[32] =
{
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3ImmediateQuick,
	&SymAm::am3PCDisplacement8,
	&SymAm::am3PCDisplacement16,
	&SymAm::am3PCDisplacement32,
	&SymAm::am3DirectAddress,
	&SymAm::am3Immediate,
	&SymAm::am3Error2,
	&SymAm::am3Error2,
	&SymAm::am3Error2,
	&SymAm::am3PCDisplacementIndirect8,
	&SymAm::am3PCDisplacementIndirect16,
	&SymAm::am3PCDisplacementIndirect32,
	&SymAm::am3DirectAddressDeferred,
	&SymAm::am3PCDoubleDisplacement8,
	&SymAm::am3PCDoubleDisplacement16,
	&SymAm::am3PCDoubleDisplacement32,
	&SymAm::am3Error2
};


const SymAm::am_func SymAm::s_AMTable3_G6[8] =
{
	&SymAm::am3DisplacementIndexed8,
	&SymAm::am3DisplacementIndexed16,
	&SymAm::am3DisplacementIndexed32,
	&SymAm::am3RegisterIndirectIndexed,
	&SymAm::am3DisplacementIndirectIndexed8,
	&SymAm::am3DisplacementIndirectIndexed16,
	&SymAm::am3DisplacementIndirectIndexed32,
	&SymAm::am3Group7a
};





uint32_t SymAm::am3Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable3_G6[m_modval2 >> 5])();
}



uint32_t SymAm::am3Group7()
{
	return (this->*s_AMTable3_G7[m_modval & 0x1F])();
}




const SymAm::am_func SymAm::s_AMTable3[2][8] =
{
	{
		&SymAm::am3Displacement8,
		&SymAm::am3Displacement16,
		&SymAm::am3Displacement32,
		&SymAm::am3RegisterIndirect,
		&SymAm::am3DisplacementIndirect8,
		&SymAm::am3DisplacementIndirect16,
		&SymAm::am3DisplacementIndirect32,
		&SymAm::am3Group7
	},

	{
		&SymAm::am3DoubleDisplacement8,
		&SymAm::am3DoubleDisplacement16,
		&SymAm::am3DoubleDisplacement32,
		&SymAm::am3Register,
		&SymAm::am3Autoincrement,
		&SymAm::am3Autodecrement,
		&SymAm::am3Group6,
		&SymAm::am3Error1
	}
};

// NOTE for bit string / field addressing
// ************************************
// m_moddim must be passed as 10 for bit string instructions,
// and as 11 for bit field instructions




// Addressing mode functions and tables

/*
  Input:
  m_modadd
    m_moddim

  Output:
    m_amout
    amLength
*/

uint32_t SymAm::ReadAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable1[m_modm][m_modval >> 5])();
}


uint32_t SymAm::BitReadAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_BAMTable1[m_modm][m_modval >> 5])();
}




/*
  Input:
  m_modadd
    m_moddim

  Output:
    m_amout
    m_amflag
    amLength
*/

uint32_t SymAm::ReadAMAddress()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable2[m_modm][m_modval >> 5])();
}


uint32_t SymAm::BitReadAMAddress()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_BAMTable2[m_modm][m_modval >> 5])();
}


/*
  Input:
  m_modadd
    m_moddim
    m_modwritevalb / H/W

  Output:
    m_amout
    amLength
*/

uint32_t SymAm::WriteAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable3[m_modm][m_modval >> 5])();
}

void SymAm::F12DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
{
	m_instflags = OpRead8(PC + 1);

	// Check if F1 or F2
	if (m_instflags & 0x80)
	{
		m_moddim = dim1;
		m_modm = m_instflags & 0x40;
		m_modadd = PC + 2;
		m_amlength1 = (this->*DecodeOp1)();
		m_op1 = m_amout;
		m_flag1 = m_amflag;
	}
	else
	{
		// Check D flag
		if (m_instflags & 0x20)
		{
			m_moddim = dim1;
			m_modm = m_instflags & 0x40;
			m_modadd = PC + 2;
			m_amlength1 = (this->*DecodeOp1)();
			m_op1 = m_amout;
			m_flag1 = m_amflag;
		}
		else
		{
			if (DecodeOp1 == &SymAm::ReadAM)
			{
				switch (dim1)
				{
				case 0:
					m_op1 = sym_cast_uint8_t(m_reg[m_instflags & 0x1F]);
					break;
				case 1:
					m_op1 = sym_cast_uint16_t(m_reg[m_instflags & 0x1F]);
					break;
				case 2:
					m_op1 = m_reg[m_instflags & 0x1F];
					break;
				}

				m_flag1 = 0;
			}
			else
			{
				m_flag1 = 1;
				m_op1 = m_instflags & 0x1F;
			}

			m_amlength1 = 0;
		}
	}
}
void SymAm::F12WriteSecondOperand(uint8_t dim2)
{
	m_moddim = dim2;

	// Check if F1 or F2
	if (m_instflags & 0x80)
	{
		m_modm = m_instflags & 0x20;
		m_modadd = PC + 2 + m_amlength1;
		m_moddim = dim2;
		m_amlength2 = WriteAM();
	}
	else
	{
		// Check D flag
		if (m_instflags & 0x20)
		{
			switch (dim2)
			{
			case 0:
				SETREG8(m_reg[m_instflags & 0x1F], m_modwritevalb);
				break;
			case 1:
				SETREG16(m_reg[m_instflags & 0x1F], m_modwritevalh);
				break;
			case 2:
				m_reg[m_instflags & 0x1F] = m_modwritevalw;
				break;
			}

			m_amlength2 = 0;
		}
		else
		{
			m_modm = m_instflags & 0x40;
			m_modadd = PC + 2;
			m_moddim = dim2;
			m_amlength2 = WriteAM();
		}
	}
}
void SymAm::F12DecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
{
	uint8_t _if12 = OpRead8(PC + 1);

	// Check if F1 or F2
	if (_if12 & 0x80)
	{
		m_moddim = dim1;
		m_modm = _if12 & 0x40;
		m_modadd = PC + 2;
		m_amlength1 = (this->*DecodeOp1)();
		m_op1 = m_amout;
		m_flag1 = m_amflag;

		m_moddim = dim2;
		m_modm = _if12 & 0x20;
		m_modadd = PC + 2 + m_amlength1;
		m_amlength2 = (this->*DecodeOp2)();
		m_op2 = m_amout;
		m_flag2 = m_amflag;
	}
	else
	{
		// Check D flag
		if (_if12 & 0x20)
		{
			if (DecodeOp2 == &SymAm::ReadAMAddress)
			{
				m_op2 = _if12 & 0x1F;
				m_flag2 = 1;
			}
			else
			{
				switch (dim2)
				{
				case 0:
					m_op2 = sym_cast_uint8_t(m_reg[_if12 & 0x1F]);
					break;
				case 1:
					m_op2 = sym_cast_uint16_t(m_reg[_if12 & 0x1F]);
					break;
				case 2:
					m_op2 = m_reg[_if12 & 0x1F];
					break;
				}
			}

			m_amlength2 = 0;

			m_moddim = dim1;
			m_modm = _if12 & 0x40;
			m_modadd = PC + 2;
			m_amlength1 = (this->*DecodeOp1)();
			m_op1 = m_amout;
			m_flag1 = m_amflag;
		}
		else
		{
			if (DecodeOp1 == &SymAm::ReadAMAddress)
			{
				m_op1 = _if12 & 0x1F;
				m_flag1 = 1;
			}
			else
			{
				switch (dim1)
				{
				case 0:
					m_op1 = sym_cast_uint8_t(m_reg[_if12 & 0x1F]);
					break;
				case 1:
					m_op1 = sym_cast_uint16_t(m_reg[_if12 & 0x1F]);
					break;
				case 2:
					m_op1 = m_reg[_if12 & 0x1F];
					break;
				}
			}
			m_amlength1 = 0;

			m_moddim = dim2;
			m_modm = _if12 & 0x40;
			m_modadd = PC + 2 + m_amlength1;
			m_amlength2 = (this->*DecodeOp2)();
			m_op2 = m_amout;
			m_flag2 = m_amflag;
		}
	}
}
void SymAm::F2DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
{
	m_moddim = dim1;
	m_modm = m_instflags & 0x40;
	m_modadd = PC + 2;
	m_amlength1 = (this->*DecodeOp1)();
	m_op1 = m_amout;
	m_flag1 = m_amflag;
}
void SymAm::F2DecodeSecondOperand(am_func DecodeOp2, uint8_t dim2)
{
	m_moddim = dim2;
	m_modm = m_instflags & 0x20;
	m_modadd = PC + 2 + m_amlength1;
	m_amlength2 = (this->*DecodeOp2)();
	m_op2 = m_amout;
	m_flag2 = m_amflag;
}
void SymAm::F2WriteSecondOperand(uint8_t dim2)
{
	m_moddim = dim2;
	m_modm = m_instflags & 0x20;
	m_modadd = PC + 2 + m_amlength1;
	m_amlength2 = WriteAM();
}
void SymAm::F7aDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
{
	uint8_t appb;
	// Decode first operand
	m_moddim = dim1;
	m_modm = m_subop & 0x40;
	m_modadd = PC + 2;
	m_amlength1 = (this->*DecodeOp1)();
	m_flag1 = m_amflag;
	m_op1 = m_amout;

	// Decode length
	appb = OpRead8(PC + 2 + m_amlength1);
	if (appb & 0x80)
		m_lenop1 = m_reg[appb & 0x1F];
	else
		m_lenop1 = appb;

	// Decode second operand
	m_moddim = dim2;
	m_modm = m_subop & 0x20;
	m_modadd = PC + 3 + m_amlength1;
	m_amlength2 = (this->*DecodeOp2)();
	m_flag2 = m_amflag;
	m_op2 = m_amout;

	// Decode length
	appb = OpRead8(PC + 3 + m_amlength1 + m_amlength2);
	if (appb & 0x80)
		m_lenop2 = m_reg[appb & 0x1F];
	else
		m_lenop2 = appb;
}
void SymAm::F7bDecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
{
	uint8_t appb;
	// Decode first operand
	m_moddim = dim1;
	m_modm = m_subop & 0x40;
	m_modadd = PC + 2;
	m_amlength1 = (this->*DecodeOp1)();
	m_flag1 = m_amflag;
	m_op1 = m_amout;

	// Decode ext
	appb = OpRead8(PC + 2 + m_amlength1);
	if (appb & 0x80)
		m_lenop1 = m_reg[appb & 0x1F];
	else
		m_lenop1 = appb;
}
void SymAm::F7bWriteSecondOperand(uint8_t dim2)
{
	m_moddim = dim2;
	m_modm = m_subop & 0x20;
	m_modadd = PC + 3 + m_amlength1;
	m_amlength2 = WriteAM();
}
void SymAm::F7bDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
{
	// Decode first operand
	F7bDecodeFirstOperand(DecodeOp1, dim1);
	m_bamoffset1 = m_bamoffset;

	// Decode second operand
	m_moddim = dim2;
	m_modm = m_subop & 0x20;
	m_modadd = PC + 3 + m_amlength1;
	m_amlength2 = (this->*DecodeOp2)();
	m_flag2 = m_amflag;
	m_op2 = m_amout;
	m_bamoffset2 = m_bamoffset;
}
void SymAm::F7cDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
{
	uint8_t appb;
	// Decode first operand
	m_moddim = dim1;
	m_modm = m_subop & 0x40;
	m_modadd = PC + 2;
	m_amlength1 = (this->*DecodeOp1)();
	m_flag1 = m_amflag;
	m_op1 = m_amout;

	// Decode second operand
	m_moddim = dim2;
	m_modm = m_subop & 0x20;
	m_modadd = PC + 2 + m_amlength1;
	m_amlength2 = (this->*DecodeOp2)();
	m_flag2 = m_amflag;
	m_op2 = m_amout;

	// Decode ext
	appb = OpRead8(PC + 2 + m_amlength1 + m_amlength2);
	if (appb & 0x80)
		m_lenop1 = m_reg[appb & 0x1F];
	else
		m_lenop1 = appb;
}
