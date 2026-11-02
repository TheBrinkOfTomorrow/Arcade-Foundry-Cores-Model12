// license:BSD-3-Clause
// copyright-holders:Ville Linde
// Transplanted from MAME src/devices/cpu/sharc by scripts/transplant_sharc.py:
// regenerate, do not edit (THIRD_PARTY.md).
inline void adsp21062_device::CHANGE_PC(uint32_t newpc)
{
	m_core->pc = newpc;
	m_core->daddr = newpc;
	m_core->faddr = newpc+1;
	m_core->nfaddr = newpc+2;
}

inline void adsp21062_device::CHANGE_PC_DELAYED(uint32_t newpc)
{
	m_core->nfaddr = newpc;

	m_core->delay_slot1 = m_core->pc;
	m_core->delay_slot2 = m_core->daddr;
}

template <unsigned N>
inline uint64_t adsp21062_device::pm_r(offs_t offset)
{
	offs_t slot = offset >> 12;
	offs_t base = (offset & 0xfff) + (slot >> 1) * (3<<12);
	if (slot & 1)
		return (uint64_t(m_blocks[N][base + 0x2000]) << 16) | (m_blocks[N][base + 0x1000] & 0xffff);
	else
		return (uint64_t(m_blocks[N][base         ]) << 16) | (m_blocks[N][base + 0x1000] >> 16);
}

template <unsigned N>
inline void adsp21062_device::pm_w(offs_t offset, uint64_t data, uint64_t mem_mask)
{
	offs_t slot = offset >> 12;
	offs_t base = (offset & 0xfff) + (slot >> 1) * (3<<12);
	if (slot & 1)
	{
		if (ACCESSING_BITS_0_15)
			m_blocks[N][base + 0x1000] = (m_blocks[N][base + 0x1000] & 0xffff0000) | (data & 0xffff);
		m_blocks[N][base + 0x2000] = (m_blocks[N][base + 0x2000] & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16);
	}
	else
	{
		m_blocks[N][base + 0x0000] = (m_blocks[N][base + 0x0000] & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16);
		if (ACCESSING_BITS_0_15)
			m_blocks[N][base + 0x1000] = (m_blocks[N][base + 0x1000] & 0xffff) | ((data & 0xffff) << 16);
	}
}

template <unsigned N>
inline uint32_t adsp21062_device::dm_short_r(offs_t offset)
{
	return util::little_endian_cast<uint16_t const>(&m_blocks[N][0])[offset];
}

template <unsigned N>
inline uint32_t adsp21062_device::dm_short_se_r(offs_t offset)
{
	return uint32_t(int32_t(util::little_endian_cast<int16_t const>(&m_blocks[N][0])[offset]));
}

template <unsigned N>
inline void adsp21062_device::dm_short_w(offs_t offset, uint32_t data)
{
	util::little_endian_cast<uint16_t>(&m_blocks[N][0])[offset] = uint16_t(data);
}

inline uint32_t adsp21062_device::iop_r(offs_t offset)
{
	switch (offset)
	{
		case 0x00: return 0;    // System configuration

		case 0x37:      // DMA status
		{
			return m_core->dma_status;
		}
		default:
		if (!false)
			throw SharcFatal("sharc_iop_r: Unimplemented IOP reg %02X at %08X\n", offset, m_core->pc);

		return 0;
	}
}

inline void adsp21062_device::iop_w(offs_t offset, uint32_t data)
{
	switch (offset)
	{
		case 0x00: m_core->syscon = data; break;
		case 0x02: break;       // External Memory Wait State Configuration
		case 0x04: // External port DMA buffer 0
		{
			external_dma_write(m_core->extdma_shift, data);
			m_core->extdma_shift++;
			if (m_core->extdma_shift == 3)
				m_core->extdma_shift = 0;
			break;
		}

		case 0x08: break;       // Message Register 0
		case 0x09: break;       // Message Register 1
		case 0x0a: break;       // Message Register 2
		case 0x0b: break;       // Message Register 3
		case 0x0c: break;       // Message Register 4
		case 0x0d: break;       // Message Register 5
		case 0x0e: break;       // Message Register 6
		case 0x0f: break;       // Message Register 7

		case 0x14: // reserved??? written by Last Bronx
		case 0x17: break;

		// DMA 6
		case 0x1c:
		{
			m_core->dma[6].control = data;
			if (data & 0x1)
			{
				sharc_iop_delayed_w(0x1c, data, 1);
			}
			break;
		}

		case 0x20: break;

		case 0x40: m_core->dma[6].int_index = data; return;
		case 0x41: m_core->dma[6].int_modifier = data; return;
		case 0x42: m_core->dma[6].int_count = data; return;
		case 0x43: m_core->dma[6].chain_ptr = data; return;
		case 0x44: m_core->dma[6].gen_purpose = data; return;
		case 0x45: m_core->dma[6].ext_index = data; return;
		case 0x46: m_core->dma[6].ext_modifier = data; return;
		case 0x47: m_core->dma[6].ext_count = data; return;

		// DMA 7
		case 0x1d:
		{
			m_core->dma[7].control = data;
			if (data & 0x1)
			{
				sharc_iop_delayed_w(0x1d, data, 30);
			}
			break;
		}

		case 0x48: m_core->dma[7].int_index = data; return;
		case 0x49: m_core->dma[7].int_modifier = data; return;
		case 0x4a: m_core->dma[7].int_count = data; return;
		case 0x4b: m_core->dma[7].chain_ptr = data; return;
		case 0x4c: m_core->dma[7].gen_purpose = data; return;
		case 0x4d: m_core->dma[7].ext_index = data; return;
		case 0x4e: m_core->dma[7].ext_modifier = data; return;
		case 0x4f: m_core->dma[7].ext_count = data; return;

		default:
			throw SharcFatal("sharc_iop_w: Unimplemented IOP reg %02X, %08X at %08X\n", offset, data, m_core->pc);
	}
}

inline void adsp21062_device::check_interrupts()
{
	if ((m_core->imask & m_core->irq_pending) && (m_core->mode1 & MODE1_IRPTEN) && !m_core->interrupt_active &&
		m_core->pc != m_core->delay_slot1 && m_core->pc != m_core->delay_slot2)
	{
		int which = 0;
		for (int i = 0; i < 32; i++)
		{
			if (BIT(m_core->irq_pending, i))
				break;
			which++;
		}

		PUSH_PC();
		if (m_core->idle)
			m_core->pcstk = m_core->pc + 1;
		else
			m_core->pcstk = m_core->daddr;

		m_core->irptl |= 1 << which;

		// TODO: timer and VIRPT interrupts also push the status stack
		if (which >= 6 && which <= 8)
			PUSH_STATUS_STACK();

		CHANGE_PC(0x20000 + (which * 0x4));

		/* TODO: alter IMASKP */

		m_core->active_irq_num = which;
		m_core->irq_pending &= ~(1 << which);

		m_core->interrupt_active = 1;
	}
}
