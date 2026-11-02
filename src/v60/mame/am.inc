// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Transplanted from MAME src/devices/cpu/v60 by scripts/transplant_v60.py:
// regenerate, do not edit (THIRD_PARTY.md).
// AM1 Functions (for ReadAM)
// **************************

uint32_t Core::am1Register()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = (uint8_t)m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = (uint16_t)m_reg[m_modval & 0x1F];
		break;
	case 2:
		m_amout = m_reg[m_modval & 0x1F];
		break;
	}

	return 1;
}


uint32_t Core::am1RegisterIndirect()
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


uint32_t Core::bam1RegisterIndirect()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	return 1;
}


uint32_t Core::am1RegisterIndirectIndexed()
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


uint32_t Core::bam1RegisterIndirectIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}


uint32_t Core::am1Autoincrement()
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


uint32_t Core::bam1Autoincrement()
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


uint32_t Core::am1Autodecrement()
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


uint32_t Core::bam1Autodecrement()
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


uint32_t Core::am1Displacement8()
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


uint32_t Core::bam1Displacement8()
{
	m_bamoffset = m_program->read_byte(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}



uint32_t Core::am1Displacement16()
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


uint32_t Core::bam1Displacement16()
{
	m_bamoffset = OpRead16(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1Displacement32()
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


uint32_t Core::bam1Displacement32()
{
	m_bamoffset = OpRead32(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t Core::am1DisplacementIndexed8()
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


uint32_t Core::bam1DisplacementIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1DisplacementIndexed16()
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


uint32_t Core::bam1DisplacementIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t Core::am1DisplacementIndexed32()
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


uint32_t Core::bam1DisplacementIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}



uint32_t Core::am1PCDisplacement8()
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


uint32_t Core::bam1PCDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}


uint32_t Core::am1PCDisplacement16()
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


uint32_t Core::bam1PCDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1PCDisplacement32()
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


uint32_t Core::bam1PCDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t Core::am1PCDisplacementIndexed8()
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


uint32_t Core::bam1PCDisplacementIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}



uint32_t Core::am1PCDisplacementIndexed16()
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


uint32_t Core::bam1PCDisplacementIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t Core::am1PCDisplacementIndexed32()
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


uint32_t Core::bam1PCDisplacementIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t Core::am1DisplacementIndirect8()
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


uint32_t Core::bam1DisplacementIndirect8()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
	return 2;
}


uint32_t Core::am1DisplacementIndirect16()
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


uint32_t Core::bam1DisplacementIndirect16()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
	return 3;
}


uint32_t Core::am1DisplacementIndirect32()
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


uint32_t Core::bam1DisplacementIndirect32()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
	return 5;
}


uint32_t Core::am1DisplacementIndirectIndexed8()
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


uint32_t Core::bam1DisplacementIndirectIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1DisplacementIndirectIndexed16()
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


uint32_t Core::bam1DisplacementIndirectIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t Core::am1DisplacementIndirectIndexed32()
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


uint32_t Core::bam1DisplacementIndirectIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t Core::am1PCDisplacementIndirect8()
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


uint32_t Core::bam1PCDisplacementIndirect8()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
	return 2;
}


uint32_t Core::am1PCDisplacementIndirect16()
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


uint32_t Core::bam1PCDisplacementIndirect16()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
	return 3;
}


uint32_t Core::am1PCDisplacementIndirect32()
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


uint32_t Core::bam1PCDisplacementIndirect32()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
	return 5;
}


uint32_t Core::am1PCDisplacementIndirectIndexed8()
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


uint32_t Core::bam1PCDisplacementIndirectIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1PCDisplacementIndirectIndexed16()
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


uint32_t Core::bam1PCDisplacementIndirectIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}


uint32_t Core::am1PCDisplacementIndirectIndexed32()
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


uint32_t Core::bam1PCDisplacementIndirectIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t Core::am1DoubleDisplacement8()
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


uint32_t Core::bam1DoubleDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 2);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1DoubleDisplacement16()
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


uint32_t Core::bam1DoubleDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 3);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t Core::am1DoubleDisplacement32()
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


uint32_t Core::bam1DoubleDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 5);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 9;
}


uint32_t Core::am1PCDoubleDisplacement8()
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


uint32_t Core::bam1PCDoubleDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 2);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t Core::am1PCDoubleDisplacement16()
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


uint32_t Core::bam1PCDoubleDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 3);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}



uint32_t Core::am1PCDoubleDisplacement32()
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


uint32_t Core::bam1PCDoubleDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 5);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 9;
}


uint32_t Core::am1DirectAddress()
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


uint32_t Core::bam1DirectAddress()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
	return 5;
}


uint32_t Core::am1DirectAddressIndexed()
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


uint32_t Core::bam1DirectAddressIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t Core::am1DirectAddressDeferred()
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


uint32_t Core::bam1DirectAddressDeferred()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
	return 5;
}


uint32_t Core::am1DirectAddressDeferredIndexed()
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


uint32_t Core::bam1DirectAddressDeferredIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t Core::am1Immediate()
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


uint32_t Core::am1ImmediateQuick()
{
	m_amout = m_modval & 0xF;
	return 1;
}





// AM1 Tables (for ReadAM)
// ***********************

uint32_t Core::am1Error1()
{
	v60_fatal("CPU - AM1 - 1 (PC=%06x)\n", PC);
}


uint32_t Core::bam1Error1()
{
	v60_fatal("CPU - BAM1 - 1 (PC=%06x)\n", PC);
}


uint32_t Core::am1Error2()
{
	v60_fatal("CPU - AM1 - 2 (PC=%06x)\n", PC);
}


uint32_t Core::bam1Error2()
{
	v60_fatal("CPU - BAM1 - 2 (PC=%06x)\n", PC);
}


uint32_t Core::am1Error3()
{
	v60_fatal("CPU - AM1 - 3 (PC=%06x)\n", PC);
}


uint32_t Core::bam1Error3()
{
	v60_fatal("CPU - BAM1 - 3 (PC=%06x)\n", PC);
}


uint32_t Core::am1Error4()
{
	v60_fatal("CPU - AM1 - 4 (PC=%06x)\n", PC);
}


uint32_t Core::bam1Error4()
{
	v60_fatal("CPU - BAM1 - 4 (PC=%06x)\n", PC);
}


uint32_t Core::am1Error5()
{
	v60_fatal("CPU - AM1 - 5 (PC=%06x)\n", PC);
}


uint32_t Core::bam1Error5()
{
	v60_fatal("CPU - BAM1 - 5 (PC=%06x)\n", PC);
}


uint32_t Core::bam1Error6()
{
	v60_fatal("CPU - BAM1 - 6 (PC=%06x)\n", PC);
}


const Core::am_func Core::s_AMTable1_G7a[16] =
{
	&Core::am1PCDisplacementIndexed8,
	&Core::am1PCDisplacementIndexed16,
	&Core::am1PCDisplacementIndexed32,
	&Core::am1DirectAddressIndexed,
	&Core::am1Error5,
	&Core::am1Error5,
	&Core::am1Error5,
	&Core::am1Error5,
	&Core::am1PCDisplacementIndirectIndexed8,
	&Core::am1PCDisplacementIndirectIndexed16,
	&Core::am1PCDisplacementIndirectIndexed32,
	&Core::am1DirectAddressDeferredIndexed,
	&Core::am1Error5,
	&Core::am1Error5,
	&Core::am1Error5,
	&Core::am1Error5
};


const Core::am_func Core::s_BAMTable1_G7a[16] =
{
	&Core::bam1PCDisplacementIndexed8,
	&Core::bam1PCDisplacementIndexed16,
	&Core::bam1PCDisplacementIndexed32,
	&Core::bam1DirectAddressIndexed,
	&Core::bam1Error5,
	&Core::bam1Error5,
	&Core::bam1Error5,
	&Core::bam1Error5,
	&Core::bam1PCDisplacementIndirectIndexed8,
	&Core::bam1PCDisplacementIndirectIndexed16,
	&Core::bam1PCDisplacementIndirectIndexed32,
	&Core::bam1DirectAddressDeferredIndexed,
	&Core::bam1Error5,
	&Core::bam1Error5,
	&Core::bam1Error5,
	&Core::bam1Error5
};



uint32_t Core::am1Group7a()
{
	if (!(m_modval2 & 0x10))
		return am1Error4();

	return (this->*s_AMTable1_G7a[m_modval2 & 0xF])();
}


uint32_t Core::bam1Group7a()
{
	if (!(m_modval2 & 0x10))
		return bam1Error4();

	return (this->*s_BAMTable1_G7a[m_modval2 & 0xF])();
}


const Core::am_func Core::s_AMTable1_G7[32] =
{
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1ImmediateQuick,
	&Core::am1PCDisplacement8,
	&Core::am1PCDisplacement16,
	&Core::am1PCDisplacement32,
	&Core::am1DirectAddress,
	&Core::am1Immediate,
	&Core::am1Error2,
	&Core::am1Error2,
	&Core::am1Error2,
	&Core::am1PCDisplacementIndirect8,
	&Core::am1PCDisplacementIndirect16,
	&Core::am1PCDisplacementIndirect32,
	&Core::am1DirectAddressDeferred,
	&Core::am1PCDoubleDisplacement8,
	&Core::am1PCDoubleDisplacement16,
	&Core::am1PCDoubleDisplacement32,
	&Core::am1Error2
};


const Core::am_func Core::s_BAMTable1_G7[32] =
{
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1Error6,
	&Core::bam1PCDisplacement8,
	&Core::bam1PCDisplacement16,
	&Core::bam1PCDisplacement32,
	&Core::bam1DirectAddress,
	&Core::bam1Error6,
	&Core::bam1Error2,
	&Core::bam1Error2,
	&Core::bam1Error2,
	&Core::bam1PCDisplacementIndirect8,
	&Core::bam1PCDisplacementIndirect16,
	&Core::bam1PCDisplacementIndirect32,
	&Core::bam1DirectAddressDeferred,
	&Core::bam1PCDoubleDisplacement8,
	&Core::bam1PCDoubleDisplacement16,
	&Core::bam1PCDoubleDisplacement32,
	&Core::bam1Error2
};




const Core::am_func Core::s_AMTable1_G6[8] =
{
	&Core::am1DisplacementIndexed8,
	&Core::am1DisplacementIndexed16,
	&Core::am1DisplacementIndexed32,
	&Core::am1RegisterIndirectIndexed,
	&Core::am1DisplacementIndirectIndexed8,
	&Core::am1DisplacementIndirectIndexed16,
	&Core::am1DisplacementIndirectIndexed32,
	&Core::am1Group7a
};


const Core::am_func Core::s_BAMTable1_G6[8] =
{
	&Core::bam1DisplacementIndexed8,
	&Core::bam1DisplacementIndexed16,
	&Core::bam1DisplacementIndexed32,
	&Core::bam1RegisterIndirectIndexed,
	&Core::bam1DisplacementIndirectIndexed8,
	&Core::bam1DisplacementIndirectIndexed16,
	&Core::bam1DisplacementIndirectIndexed32,
	&Core::bam1Group7a
};



uint32_t Core::am1Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable1_G6[m_modval2 >> 5])();
}


uint32_t Core::bam1Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_BAMTable1_G6[m_modval2 >> 5])();
}



uint32_t Core::am1Group7()
{
	return (this->*s_AMTable1_G7[m_modval & 0x1F])();
}


uint32_t Core::bam1Group7()
{
	return (this->*s_BAMTable1_G7[m_modval & 0x1F])();
}


const Core::am_func Core::s_AMTable1[2][8] =
{
	{
		&Core::am1Displacement8,
		&Core::am1Displacement16,
		&Core::am1Displacement32,
		&Core::am1RegisterIndirect,
		&Core::am1DisplacementIndirect8,
		&Core::am1DisplacementIndirect16,
		&Core::am1DisplacementIndirect32,
		&Core::am1Group7
	},

	{
		&Core::am1DoubleDisplacement8,
		&Core::am1DoubleDisplacement16,
		&Core::am1DoubleDisplacement32,
		&Core::am1Register,
		&Core::am1Autoincrement,
		&Core::am1Autodecrement,
		&Core::am1Group6,
		&Core::am1Error1
	}
};



const Core::am_func Core::s_BAMTable1[2][8] =
{
	{
		&Core::bam1Displacement8,
		&Core::bam1Displacement16,
		&Core::bam1Displacement32,
		&Core::bam1RegisterIndirect,
		&Core::bam1DisplacementIndirect8,
		&Core::bam1DisplacementIndirect16,
		&Core::bam1DisplacementIndirect32,
		&Core::bam1Group7
	},

	{
		&Core::bam1DoubleDisplacement8,
		&Core::bam1DoubleDisplacement16,
		&Core::bam1DoubleDisplacement32,
		&Core::bam1Error6,
		&Core::bam1Autoincrement,
		&Core::bam1Autodecrement,
		&Core::bam1Group6,
		&Core::bam1Error1
	}
};

// AM2 Functions (for ReadAMAddress)
// *********************************

uint32_t Core::am2Register()
{
	m_amflag = 1;
	m_amout = m_modval & 0x1F;
	return 1;
}


uint32_t Core::am2RegisterIndirect()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}


uint32_t Core::bam2RegisterIndirect()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = 0;
	return 1;
}


uint32_t Core::am2RegisterIndirectIndexed()
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


uint32_t Core::bam2RegisterIndirectIndexed()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F];
	m_bamoffset = m_reg[m_modval & 0x1F];
	return 2;
}


uint32_t Core::am2Autoincrement()
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


uint32_t Core::bam2Autoincrement()
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


uint32_t Core::am2Autodecrement()
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


uint32_t Core::bam2Autodecrement()
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



uint32_t Core::am2Displacement8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t Core::bam2Displacement8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t Core::am2Displacement16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t Core::bam2Displacement16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t Core::am2Displacement32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1);

	return 5;
}


uint32_t Core::bam2Displacement32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = OpRead32(m_modadd + 1);

	return 5;
}


uint32_t Core::am2DisplacementIndexed8()
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


uint32_t Core::bam2DisplacementIndexed8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t Core::am2DisplacementIndexed16()
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


uint32_t Core::bam2DisplacementIndexed16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t Core::am2DisplacementIndexed32()
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


uint32_t Core::bam2DisplacementIndexed32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t Core::am2PCDisplacement8()
{
	m_amflag = 0;
	m_amout = PC + (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t Core::bam2PCDisplacement8()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = (int8_t)OpRead8(m_modadd + 1);

	return 2;
}


uint32_t Core::am2PCDisplacement16()
{
	m_amflag = 0;
	m_amout = PC + (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t Core::bam2PCDisplacement16()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = (int16_t)OpRead16(m_modadd + 1);

	return 3;
}


uint32_t Core::am2PCDisplacement32()
{
	m_amflag = 0;
	m_amout = PC + OpRead32(m_modadd + 1);

	return 5;
}


uint32_t Core::bam2PCDisplacement32()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = OpRead32(m_modadd + 1);

	return 5;
}



uint32_t Core::am2PCDisplacementIndexed8()
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


uint32_t Core::bam2PCDisplacementIndexed8()
{
	m_amflag = 0;
	m_amout = PC + (int8_t)OpRead8(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t Core::am2PCDisplacementIndexed16()
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


uint32_t Core::bam2PCDisplacementIndexed16()
{
	m_amflag = 0;
	m_amout = PC + (int16_t)OpRead16(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t Core::am2PCDisplacementIndexed32()
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


uint32_t Core::bam2PCDisplacementIndexed32()
{
	m_amflag = 0;
	m_amout = PC + OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t Core::am2DisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));

	return 2;
}


uint32_t Core::bam2DisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = 0;
	return 2;
}


uint32_t Core::am2DisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));

	return 3;
}


uint32_t Core::bam2DisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = 0;
	return 3;
}


uint32_t Core::am2DisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));

	return 5;
}


uint32_t Core::bam2DisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}


uint32_t Core::am2DisplacementIndirectIndexed8()
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


uint32_t Core::bam2DisplacementIndirectIndexed8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t Core::am2DisplacementIndirectIndexed16()
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


uint32_t Core::bam2DisplacementIndirectIndexed16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t Core::am2DisplacementIndirectIndexed32()
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


uint32_t Core::bam2DisplacementIndirectIndexed32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t Core::am2PCDisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));

	return 2;
}


uint32_t Core::bam2PCDisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = 0;

	return 2;
}


uint32_t Core::am2PCDisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));

	return 3;
}


uint32_t Core::bam2PCDisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = 0;

	return 3;
}


uint32_t Core::am2PCDisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));

	return 5;
}


uint32_t Core::bam2PCDisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}


uint32_t Core::am2PCDisplacementIndirectIndexed8()
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


uint32_t Core::bam2PCDisplacementIndirectIndexed8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}


uint32_t Core::am2PCDisplacementIndirectIndexed16()
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



uint32_t Core::bam2PCDisplacementIndirectIndexed16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}



uint32_t Core::am2PCDisplacementIndirectIndexed32()
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


uint32_t Core::bam2PCDisplacementIndirectIndexed32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t Core::am2DoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t Core::bam2DoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t Core::am2DoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3);

	return 5;
}


uint32_t Core::bam2DoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 3);

	return 5;
}


uint32_t Core::am2DoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5);

	return 9;
}


uint32_t Core::bam2DoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
	m_bamoffset = OpRead32(m_modadd + 5);

	return 9;
}



uint32_t Core::am2PCDoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t Core::bam2PCDoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 2);

	return 3;
}


uint32_t Core::am2PCDoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3);

	return 5;
}


uint32_t Core::bam2PCDoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 3);

	return 5;
}


uint32_t Core::am2PCDoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5);

	return 9;
}


uint32_t Core::bam2PCDoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
	m_bamoffset = OpRead32(m_modadd + 5);

	return 9;
}


uint32_t Core::am2DirectAddress()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 1);

	return 5;
}


uint32_t Core::bam2DirectAddress()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 1);
	m_bamoffset = 0;

	return 5;
}


uint32_t Core::am2DirectAddressIndexed()
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


uint32_t Core::bam2DirectAddressIndexed()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t Core::am2DirectAddressDeferred()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));

	return 5;
}


uint32_t Core::bam2DirectAddressDeferred()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}


uint32_t Core::am2DirectAddressDeferredIndexed()
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


uint32_t Core::bam2DirectAddressDeferredIndexed()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}


uint32_t Core::am2Immediate()
{
	m_amflag = 0;
	return am1Immediate();
}


uint32_t Core::am2ImmediateQuick()
{
	m_amflag = 0;
	return am1ImmediateQuick();
}



// AM2 Tables (for ReadAMAddress)
// ******************************

uint32_t Core::am2Error1()
{
	// f1lap trips this, why?
	v60_log("CPU - AM2 - 1 (PC=%06x)", PC);
	return 0;
}


uint32_t Core::am2Error2()
{
	v60_fatal("CPU - AM2 - 2 (PC=%06x)\n", PC);
}


uint32_t Core::am2Error3()
{
	v60_fatal("CPU - AM2 - 3 (PC=%06x)\n", PC);
}


uint32_t Core::am2Error4()
{
	v60_fatal("CPU - AM2 - 4 (PC=%06x)\n", PC);
}


uint32_t Core::am2Error5()
{
	v60_fatal("CPU - AM2 - 5 (PC=%06x)\n", PC);
}


uint32_t Core::bam2Error1()
{
	v60_fatal("CPU - BAM2 - 1 (PC=%06x)\n", PC);
}


uint32_t Core::bam2Error2()
{
	v60_fatal("CPU - BAM2 - 2 (PC=%06x)\n", PC);
}


uint32_t Core::bam2Error3()
{
	v60_fatal("CPU - BAM2 - 3 (PC=%06x)\n", PC);
}


uint32_t Core::bam2Error4()
{
	v60_fatal("CPU - BAM2 - 4 (PC=%06x)\n", PC);
}


uint32_t Core::bam2Error5()
{
	v60_fatal("CPU - BAM2 - 5 (PC=%06x)\n", PC);
}


uint32_t Core::bam2Error6()
{
	v60_fatal("CPU - BAM2 - 6 (PC=%06x)\n", PC);
}



const Core::am_func Core::s_AMTable2_G7a[16] =
{
	&Core::am2PCDisplacementIndexed8,
	&Core::am2PCDisplacementIndexed16,
	&Core::am2PCDisplacementIndexed32,
	&Core::am2DirectAddressIndexed,
	&Core::am2Error5,
	&Core::am2Error5,
	&Core::am2Error5,
	&Core::am2Error5,
	&Core::am2PCDisplacementIndirectIndexed8,
	&Core::am2PCDisplacementIndirectIndexed16,
	&Core::am2PCDisplacementIndirectIndexed32,
	&Core::am2DirectAddressDeferredIndexed,
	&Core::am2Error5,
	&Core::am2Error5,
	&Core::am2Error5,
	&Core::am2Error5
};


const Core::am_func Core::s_BAMTable2_G7a[16] =
{
	&Core::bam2PCDisplacementIndexed8,
	&Core::bam2PCDisplacementIndexed16,
	&Core::bam2PCDisplacementIndexed32,
	&Core::bam2DirectAddressIndexed,
	&Core::bam2Error5,
	&Core::bam2Error5,
	&Core::bam2Error5,
	&Core::bam2Error5,
	&Core::bam2PCDisplacementIndirectIndexed8,
	&Core::bam2PCDisplacementIndirectIndexed16,
	&Core::bam2PCDisplacementIndirectIndexed32,
	&Core::bam2DirectAddressDeferredIndexed,
	&Core::bam2Error5,
	&Core::bam2Error5,
	&Core::bam2Error5,
	&Core::bam2Error5
};


uint32_t Core::am2Group7a()
{
	if (!(m_modval2 & 0x10))
		return am2Error4();

	return (this->*s_AMTable2_G7a[m_modval2 & 0xF])();
}


uint32_t Core::bam2Group7a()
{
	if (!(m_modval2 & 0x10))
		return bam2Error4();

	return (this->*s_BAMTable2_G7a[m_modval2 & 0xF])();
}


const Core::am_func Core::s_AMTable2_G7[32] =
{
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2ImmediateQuick,
	&Core::am2PCDisplacement8,
	&Core::am2PCDisplacement16,
	&Core::am2PCDisplacement32,
	&Core::am2DirectAddress,
	&Core::am2Immediate,
	&Core::am2Error2,
	&Core::am2Error2,
	&Core::am2Error2,
	&Core::am2PCDisplacementIndirect8,
	&Core::am2PCDisplacementIndirect16,
	&Core::am2PCDisplacementIndirect32,
	&Core::am2DirectAddressDeferred,
	&Core::am2PCDoubleDisplacement8,
	&Core::am2PCDoubleDisplacement16,
	&Core::am2PCDoubleDisplacement32,
	&Core::am2Error2
};


const Core::am_func Core::s_BAMTable2_G7[32] =
{
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2Error6,
	&Core::bam2PCDisplacement8,
	&Core::bam2PCDisplacement16,
	&Core::bam2PCDisplacement32,
	&Core::bam2DirectAddress,
	&Core::bam2Error6,
	&Core::bam2Error2,
	&Core::bam2Error2,
	&Core::bam2Error2,
	&Core::bam2PCDisplacementIndirect8,
	&Core::bam2PCDisplacementIndirect16,
	&Core::bam2PCDisplacementIndirect32,
	&Core::bam2DirectAddressDeferred,
	&Core::bam2PCDoubleDisplacement8,
	&Core::bam2PCDoubleDisplacement16,
	&Core::bam2PCDoubleDisplacement32,
	&Core::bam2Error2
};


const Core::am_func Core::s_AMTable2_G6[8] =
{
	&Core::am2DisplacementIndexed8,
	&Core::am2DisplacementIndexed16,
	&Core::am2DisplacementIndexed32,
	&Core::am2RegisterIndirectIndexed,
	&Core::am2DisplacementIndirectIndexed8,
	&Core::am2DisplacementIndirectIndexed16,
	&Core::am2DisplacementIndirectIndexed32,
	&Core::am2Group7a
};


const Core::am_func Core::s_BAMTable2_G6[8] =
{
	&Core::bam2DisplacementIndexed8,
	&Core::bam2DisplacementIndexed16,
	&Core::bam2DisplacementIndexed32,
	&Core::bam2RegisterIndirectIndexed,
	&Core::bam2DisplacementIndirectIndexed8,
	&Core::bam2DisplacementIndirectIndexed16,
	&Core::bam2DisplacementIndirectIndexed32,
	&Core::bam2Group7a
};





uint32_t Core::am2Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable2_G6[m_modval2 >> 5])();
}

uint32_t Core::bam2Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_BAMTable2_G6[m_modval2 >> 5])();
}


uint32_t Core::am2Group7()
{
	return (this->*s_AMTable2_G7[m_modval & 0x1F])();
}

uint32_t Core::bam2Group7()
{
	return (this->*s_BAMTable2_G7[m_modval & 0x1F])();
}



const Core::am_func Core::s_AMTable2[2][8] =
{
	{
		&Core::am2Displacement8,
		&Core::am2Displacement16,
		&Core::am2Displacement32,
		&Core::am2RegisterIndirect,
		&Core::am2DisplacementIndirect8,
		&Core::am2DisplacementIndirect16,
		&Core::am2DisplacementIndirect32,
		&Core::am2Group7
	},

	{
		&Core::am2DoubleDisplacement8,
		&Core::am2DoubleDisplacement16,
		&Core::am2DoubleDisplacement32,
		&Core::am2Register,
		&Core::am2Autoincrement,
		&Core::am2Autodecrement,
		&Core::am2Group6,
		&Core::am2Error1
	}
};


const Core::am_func Core::s_BAMTable2[2][8] =
{
	{
		&Core::bam2Displacement8,
		&Core::bam2Displacement16,
		&Core::bam2Displacement32,
		&Core::bam2RegisterIndirect,
		&Core::bam2DisplacementIndirect8,
		&Core::bam2DisplacementIndirect16,
		&Core::bam2DisplacementIndirect32,
		&Core::bam2Group7
	},

	{
		&Core::bam2DoubleDisplacement8,
		&Core::bam2DoubleDisplacement16,
		&Core::bam2DoubleDisplacement32,
		&Core::bam2Error6,
		&Core::bam2Autoincrement,
		&Core::bam2Autodecrement,
		&Core::bam2Group6,
		&Core::bam2Error1
	}
};

// AM3 Functions (for ReadAM)
// **************************

uint32_t Core::am3Register()
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


uint32_t Core::am3RegisterIndirect()
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


uint32_t Core::am3RegisterIndirectIndexed()
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


uint32_t Core::am3Autoincrement()
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


uint32_t Core::am3Autodecrement()
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


uint32_t Core::am3Displacement8()
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


uint32_t Core::am3Displacement16()
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


uint32_t Core::am3Displacement32()
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



uint32_t Core::am3DisplacementIndexed8()
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


uint32_t Core::am3DisplacementIndexed16()
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


uint32_t Core::am3DisplacementIndexed32()
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



uint32_t Core::am3PCDisplacement8()
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


uint32_t Core::am3PCDisplacement16()
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


uint32_t Core::am3PCDisplacement32()
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


uint32_t Core::am3PCDisplacementIndexed8()
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


uint32_t Core::am3PCDisplacementIndexed16()
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


uint32_t Core::am3PCDisplacementIndexed32()
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


uint32_t Core::am3DisplacementIndirect8()
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


uint32_t Core::am3DisplacementIndirect16()
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


uint32_t Core::am3DisplacementIndirect32()
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



uint32_t Core::am3DisplacementIndirectIndexed8()
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


uint32_t Core::am3DisplacementIndirectIndexed16()
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


uint32_t Core::am3DisplacementIndirectIndexed32()
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


uint32_t Core::am3PCDisplacementIndirect8()
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


uint32_t Core::am3PCDisplacementIndirect16()
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


uint32_t Core::am3PCDisplacementIndirect32()
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



uint32_t Core::am3PCDisplacementIndirectIndexed8()
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


uint32_t Core::am3PCDisplacementIndirectIndexed16()
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


uint32_t Core::am3PCDisplacementIndirectIndexed32()
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



uint32_t Core::am3DoubleDisplacement8()
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


uint32_t Core::am3DoubleDisplacement16()
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


uint32_t Core::am3DoubleDisplacement32()
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



uint32_t Core::am3PCDoubleDisplacement8()
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


uint32_t Core::am3PCDoubleDisplacement16()
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


uint32_t Core::am3PCDoubleDisplacement32()
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


uint32_t Core::am3DirectAddress()
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


uint32_t Core::am3DirectAddressIndexed()
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


uint32_t Core::am3DirectAddressDeferred()
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


uint32_t Core::am3DirectAddressDeferredIndexed()
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


uint32_t Core::am3Immediate()
{
	v60_fatal("CPU - AM3 - IMM (PC=%06x)\n", PC);
}


uint32_t Core::am3ImmediateQuick()
{
	v60_fatal("CPU - AM3 - IMMQ (PC=%06x)\n", PC);
}




// AM3 Tables (for ReadAMAddress)
// ******************************

uint32_t Core::am3Error1()
{
	v60_fatal("CPU - AM3 - 1 (PC=%06x)\n", PC);
}


uint32_t Core::am3Error2()
{
	v60_fatal("CPU - AM3 - 2 (PC=%06x)\n", PC);
}


uint32_t Core::am3Error3()
{
	v60_fatal("CPU - AM3 - 3 (PC=%06x)\n", PC);
}


uint32_t Core::am3Error4()
{
	v60_fatal("CPU - AM3 - 4 (PC=%06x)\n", PC);
}


uint32_t Core::am3Error5()
{
	v60_fatal("CPU - AM3 - 5 (PC=%06x)\n", PC);
}


const Core::am_func Core::s_AMTable3_G7a[16] =
{
	&Core::am3PCDisplacementIndexed8,
	&Core::am3PCDisplacementIndexed16,
	&Core::am3PCDisplacementIndexed32,
	&Core::am3DirectAddressIndexed,
	&Core::am3Error5,
	&Core::am3Error5,
	&Core::am3Error5,
	&Core::am3Error5,
	&Core::am3PCDisplacementIndirectIndexed8,
	&Core::am3PCDisplacementIndirectIndexed16,
	&Core::am3PCDisplacementIndirectIndexed32,
	&Core::am3DirectAddressDeferredIndexed,
	&Core::am3Error5,
	&Core::am3Error5,
	&Core::am3Error5,
	&Core::am3Error5
};


uint32_t Core::am3Group7a()
{
	if (!(m_modval2 & 0x10))
		return am3Error4();

	return (this->*s_AMTable3_G7a[m_modval2 & 0xF])();
}


const Core::am_func Core::s_AMTable3_G7[32] =
{
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3ImmediateQuick,
	&Core::am3PCDisplacement8,
	&Core::am3PCDisplacement16,
	&Core::am3PCDisplacement32,
	&Core::am3DirectAddress,
	&Core::am3Immediate,
	&Core::am3Error2,
	&Core::am3Error2,
	&Core::am3Error2,
	&Core::am3PCDisplacementIndirect8,
	&Core::am3PCDisplacementIndirect16,
	&Core::am3PCDisplacementIndirect32,
	&Core::am3DirectAddressDeferred,
	&Core::am3PCDoubleDisplacement8,
	&Core::am3PCDoubleDisplacement16,
	&Core::am3PCDoubleDisplacement32,
	&Core::am3Error2
};


const Core::am_func Core::s_AMTable3_G6[8] =
{
	&Core::am3DisplacementIndexed8,
	&Core::am3DisplacementIndexed16,
	&Core::am3DisplacementIndexed32,
	&Core::am3RegisterIndirectIndexed,
	&Core::am3DisplacementIndirectIndexed8,
	&Core::am3DisplacementIndirectIndexed16,
	&Core::am3DisplacementIndirectIndexed32,
	&Core::am3Group7a
};





uint32_t Core::am3Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable3_G6[m_modval2 >> 5])();
}



uint32_t Core::am3Group7()
{
	return (this->*s_AMTable3_G7[m_modval & 0x1F])();
}




const Core::am_func Core::s_AMTable3[2][8] =
{
	{
		&Core::am3Displacement8,
		&Core::am3Displacement16,
		&Core::am3Displacement32,
		&Core::am3RegisterIndirect,
		&Core::am3DisplacementIndirect8,
		&Core::am3DisplacementIndirect16,
		&Core::am3DisplacementIndirect32,
		&Core::am3Group7
	},

	{
		&Core::am3DoubleDisplacement8,
		&Core::am3DoubleDisplacement16,
		&Core::am3DoubleDisplacement32,
		&Core::am3Register,
		&Core::am3Autoincrement,
		&Core::am3Autodecrement,
		&Core::am3Group6,
		&Core::am3Error1
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

uint32_t Core::ReadAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable1[m_modm][m_modval >> 5])();
}


uint32_t Core::BitReadAM()
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

uint32_t Core::ReadAMAddress()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable2[m_modm][m_modval >> 5])();
}


uint32_t Core::BitReadAMAddress()
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

uint32_t Core::WriteAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable3[m_modm][m_modval >> 5])();
}

void Core::F12DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
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
			if (DecodeOp1 == &Core::ReadAM)
			{
				switch (dim1)
				{
				case 0:
					m_op1 = (uint8_t)m_reg[m_instflags & 0x1F];
					break;
				case 1:
					m_op1 = (uint16_t)m_reg[m_instflags & 0x1F];
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
void Core::F12WriteSecondOperand(uint8_t dim2)
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
void Core::F12DecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
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
			if (DecodeOp2 == &Core::ReadAMAddress)
			{
				m_op2 = _if12 & 0x1F;
				m_flag2 = 1;
			}
			else
			{
				switch (dim2)
				{
				case 0:
					m_op2 = (uint8_t)m_reg[_if12 & 0x1F];
					break;
				case 1:
					m_op2 = (uint16_t)m_reg[_if12 & 0x1F];
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
			if (DecodeOp1 == &Core::ReadAMAddress)
			{
				m_op1 = _if12 & 0x1F;
				m_flag1 = 1;
			}
			else
			{
				switch (dim1)
				{
				case 0:
					m_op1 = (uint8_t)m_reg[_if12 & 0x1F];
					break;
				case 1:
					m_op1 = (uint16_t)m_reg[_if12 & 0x1F];
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
void Core::F2DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
{
	m_moddim = dim1;
	m_modm = m_instflags & 0x40;
	m_modadd = PC + 2;
	m_amlength1 = (this->*DecodeOp1)();
	m_op1 = m_amout;
	m_flag1 = m_amflag;
}
void Core::F2DecodeSecondOperand(am_func DecodeOp2, uint8_t dim2)
{
	m_moddim = dim2;
	m_modm = m_instflags & 0x20;
	m_modadd = PC + 2 + m_amlength1;
	m_amlength2 = (this->*DecodeOp2)();
	m_op2 = m_amout;
	m_flag2 = m_amflag;
}
void Core::F2WriteSecondOperand(uint8_t dim2)
{
	m_moddim = dim2;
	m_modm = m_instflags & 0x20;
	m_modadd = PC + 2 + m_amlength1;
	m_amlength2 = WriteAM();
}
void Core::F7aDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
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
void Core::F7bDecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
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
void Core::F7bWriteSecondOperand(uint8_t dim2)
{
	m_moddim = dim2;
	m_modm = m_subop & 0x20;
	m_modadd = PC + 3 + m_amlength1;
	m_amlength2 = WriteAM();
}
void Core::F7bDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
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
void Core::F7cDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
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
