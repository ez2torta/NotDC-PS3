#include "dc/aw/aw_board.h"

// ps3_input.cpp (players 1 and 2): active-low DC pad state. L1/L2 drive DC D (bit 11), which Flycast uses as the coin.
extern u16 kcode[4];

static u32 s_maple_devs;
static u8  s_digital_out;
static u8  s_coins = 0xFF;
static bool s_coin_latch;

void aw_board_reset()
{
	s_maple_devs = 0;
	s_digital_out = 0;
	s_coins = 0xFF;
	s_coin_latch = false;
}

static void coin_update()
{
	const bool pressed = ((kcode[0] & kcode[1]) & (1u << 11)) == 0;   // either player: shared credit
	if (pressed && !s_coin_latch)
	{
		s_coin_latch = true;
		s_coins = 0xFE;   // 1P coin active low
	}
	else if (!pressed)
	{
		s_coin_latch = false;
		s_coins = 0xFF;
	}
}

u32 aw_board_read(u32 addr, u32 size)
{
	coin_update();
	const u32 off = addr & 0x7FFu;
	u32 v = 0xFFu;
	switch (off)
	{
	case 0x280: v = s_coins; break;          // 1P/2P coins, active low
	case 0x284: v = (u32)s_maple_devs << 16; break;
	case 0x288: v = 0; break;
	case 0x28C: v = s_digital_out; break;
	default: break;
	}
	if (size == 1)
		return v & 0xFFu;
	if (size == 2)
		return v & 0xFFFFu;
	return v;
}

void aw_board_write(u32 addr, u32 data, u32 size)
{
	const u32 off = addr & 0x7FFu;
	(void)size;
	switch (off)
	{
	case 0x284: s_maple_devs = (data >> 16) & 0xFFu; break;
	case 0x28C: s_digital_out = (u8)data; break;
	default: break;
	}
}
