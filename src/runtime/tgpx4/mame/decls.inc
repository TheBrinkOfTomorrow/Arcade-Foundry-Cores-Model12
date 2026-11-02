// license:BSD-3-Clause
// copyright-holders:Angelo Salese, ElSemi, Matthew Daniels
// Transplanted from MAME src/devices/cpu/mb86235 by scripts/transplant_tgpx4.py:
// regenerate, do not edit (THIRD_PARTY.md).
struct mb86235_flags
	{
		uint32_t az;
		uint32_t an;
		uint32_t av;
		uint32_t au;
		uint32_t ad;
		uint32_t zc;
		uint32_t il;
		uint32_t nr;
		uint32_t zd;
		uint32_t mn;
		uint32_t mz;
		uint32_t mv;
		uint32_t mu;
		uint32_t md;
	};

	struct fifo_state
	{
		uint32_t pc;
		bool has_stalled;
	};

	struct mb86235_internal_state
	{
		uint32_t pc;
		uint32_t delay_pc;
		uint32_t ppc;
		uint32_t aa[8];
		uint32_t ab[8];
		uint32_t ma[8];
		uint32_t mb[8];
		uint32_t ar[8];

		uint32_t sp;
		uint32_t eb;
		uint32_t eo;
		uint32_t rpc;
		uint32_t lpc;

		uint32_t prp;
		uint32_t pwp;
		uint32_t pr[24];

		uint32_t mod;
		// TODO: remove this, use ST instead
		mb86235_flags flags;
		uint32_t st;

		int icount;

		uint32_t arg0;
		uint32_t arg1;
		uint32_t arg2;
		uint32_t arg3;
		uint64_t arg64;

		uint32_t pcp;       /**< PC stack pointer */
		uint32_t pcs[4];    /**< PC stack contents */

		uint32_t jmpdest;
		uint32_t alutemp;
		uint32_t multemp;
		uint32_t condtemp;

		uint32_t pdr;
		uint32_t ddr;

		float fp0;
		bool delay_slot;

		fifo_state cur_fifo_state;
	};

	mb86235_internal_state  *m_core;

// interpreter
	void execute_op(uint64_t op);
	void do_alu1(uint64_t op);
	void do_alu2(uint64_t op);
	void do_alu2_trans2_1(uint64_t op);
	void do_alu2_trans1_1(uint64_t op);
	void do_alu1_trans2_2(uint64_t op);
	void do_alu1_trans1_2(uint64_t op);
	void do_trans1_3(uint64_t op);
	void do_alu_control(uint64_t op);
	inline uint32_t get_prx(uint8_t which);
	inline uint32_t get_constfloat(uint8_t which);
	inline uint32_t get_constint(uint8_t which);
	inline uint32_t get_alureg(uint8_t which, bool isfloatop);
	inline uint32_t get_mulreg(uint8_t which, bool isfloatop);
	inline void set_alureg(uint8_t which, uint32_t value);
	inline void decode_aluop(uint8_t opcode, uint32_t src1, uint32_t src2, uint8_t imm, uint8_t dst_which);
	inline void decode_mulop(bool isfmul, uint32_t src1, uint32_t src2, uint8_t dst_which);
	inline bool decode_branch_jump(uint8_t which);
	inline uint32_t do_control_dst(uint64_t op);
	inline void push_pc(uint32_t pcval);
	inline uint32_t pop_pc();
	inline void set_mod(uint16_t mod1, uint16_t mod2);
	inline uint32_t get_transfer_reg(uint8_t which);
	inline void set_transfer_reg(uint8_t which, uint32_t value);
	inline uint32_t decode_ea(uint8_t mode, uint8_t rx, uint8_t ry, uint16_t disp, bool isbbus);
	inline uint32_t read_abus(uint32_t addr);
	inline uint32_t read_bbus(uint32_t addr);
	inline void write_abus(uint32_t addr, uint32_t data);
	inline void write_bbus(uint32_t addr, uint32_t data);
	inline void increment_pwp();
	inline void increment_prp();
	inline void decrement_prp();
	inline void zero_prp();
	inline void set_alu_flagsd(uint32_t val);
	inline void set_alu_flagsf(float val);
	inline void set_alu_flagsi(int val);
	inline bool get_alu_second_src(uint8_t which);
	void handle_single_step_execution();

bool check_previous_op_stall();
