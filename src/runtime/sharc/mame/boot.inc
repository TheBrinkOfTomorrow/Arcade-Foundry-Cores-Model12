// license:BSD-3-Clause
// copyright-holders:Ville Linde
// Transplanted from MAME src/devices/cpu/sharc by scripts/transplant_sharc.py:
// regenerate, do not edit (THIRD_PARTY.md).
void adsp21062_device::external_iop_write(offs_t address, uint32_t data)
{
	// host packing mode used to determine width of external host bus width (16/32)
	// if writing to external port DMA buffer, just write the data directly;
	// external_dma_write() handles packed data
	if (m_core->syscon & 0x10 && address != 0x04)
	{
		if ((m_core->iop_write_num++ & 1) == 0)
		{
			m_core->iop_data = data & 0xffff;
			return;
		}
		else
		{
			m_core->iop_data |= (data & 0xffff) << 16;
		}
	}
	else
	{
		m_core->iop_data = data;
	}

	if (address == 0x1c)
	{
		m_core->dma[6].control = m_core->iop_data;
	}
	else
	{
		m_data.write_dword(address, m_core->iop_data);
	}
}

void adsp21062_device::external_dma_write(offs_t address, uint64_t data)
{
	/*
	All addresses in the 17-bit index registers are offset by 0x0002 0000, the
	first internal RAM location, before they are used by the DMA controller.
	*/

	offs_t const index = (m_core->dma[6].int_index & 0x1ffff) | 0x20000;
	unsigned const mswf = BIT(m_core->dma[6].control, 8);
	unsigned const pmode = BIT(m_core->dma[6].control, 6, 2);
	unsigned const dtype = BIT(m_core->dma[6].control, 5);
	switch (pmode)
	{
		case 0:         // no packing
		{
			if (dtype)
				pm_write32(index, data);
			else
				dm_write32(index, data);

			m_core->dma[6].int_index += m_core->dma[6].int_modifier;
			break;
		}
		case 2:         // 16/48 packing
		{
			// FIXME: honour DTYPE
			unsigned const word = address % 3;
			unsigned const shift = (mswf ? (2 - word) : word) * 16;

			uint64_t r = pm_read48(index);
			r &= ~(uint64_t(0xffff) << shift);
			r |= (data & 0xffff) << shift;

			pm_write48(index, r);

			if (word == 2)
			{
				m_core->dma[6].int_index += m_core->dma[6].int_modifier;
			}
			break;
		}
		default:
		{
			throw SharcFatal("sharc_external_dma_write: unimplemented packing mode %d\n", pmode);
		}
	}
}
