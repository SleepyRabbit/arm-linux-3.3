/*
 * Copyright (C) 2016 Faraday Corp. (http://www.faraday-tech.com)
 *
 * Modified by Justin
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <mach/platform/board.h>
#include <mach/platform/platform_io.h>
#include <mach/hardware.h>

/* Using named register defines makes the code easier to follow */
#define SCU_REG			r0
#define DDR_REG			r1
#define GPIO_REG		r2
#define FT_GPIO_PIN		r3
#define WORK1_REG		r4
#define WORK2_REG		r5
#define SAVED_DRAM_CLKCTRL_REG	r6

#define MEMCTRL_STATE_CTRL_REG				0x04
#define MEMCTRL_STATUS_SET_SELF_REFRESH		0x04
#define MEMCTRL_STATUS_SELF_REFRESH_READY	0x400

#define SCU_GATING_CLOCK_REG				0xB0
#define SCU_DDR_GATING_CLOCK				0x80

#define GPIO_DATA_INPUT_REG					0x04

#define INTC_VECTOR		0xFFFF000

	.text

ENTRY(gm_sys_suspend)

	@ Save a copy of the used registers in SRAM, r0 is corrupted
	adr	r0, tmp_stack_end
	stmfd	r0!, {r3 - r7, sp, lr}

	@ Load a few common register addresses
	ldr	SCU_REG, [r1]
	ldr	GPIO_REG, [r1, #0x8]
	ldr	FT_GPIO_PIN, [r1, #0xC]
	ldr	DDR_REG, [r1, #0x4] @ r1 save arg address

	@ TOUCH IP into TLB
	ldr	WORK1_REG, [SCU_REG]
	ldr	WORK1_REG, [DDR_REG]
	ldr	WORK2_REG, [GPIO_REG, #GPIO_DATA_INPUT_REG]

	@ Setup DRAM self-refresh mode
	ldr	SAVED_DRAM_CLKCTRL_REG, [DDR_REG, #MEMCTRL_STATE_CTRL_REG]
	orr	WORK2_REG, SAVED_DRAM_CLKCTRL_REG, #MEMCTRL_STATUS_SET_SELF_REFRESH
	str	WORK2_REG, [DDR_REG, #MEMCTRL_STATE_CTRL_REG]

	@ Wait for self-refresh acknowledge
1:
	ldr	WORK2_REG, [DDR_REG, #MEMCTRL_STATE_CTRL_REG]
	and	WORK2_REG, WORK2_REG, #MEMCTRL_STATUS_SELF_REFRESH_READY
	cmp	WORK2_REG, #MEMCTRL_STATUS_SELF_REFRESH_READY
	bne	1b @ Branch until self-refresh mode starts

	@ Gating clock to DRAM
	ldr	WORK1_REG, [SCU_REG, #SCU_GATING_CLOCK_REG]
	ORR	WORK1_REG, WORK1_REG, #SCU_DDR_GATING_CLOCK
	str	WORK1_REG, [SCU_REG, #SCU_GATING_CLOCK_REG]

	@ Wait to push GPIO and exit loop
2:
	ldr	WORK2_REG, [GPIO_REG, #GPIO_DATA_INPUT_REG]
	and	WORK2_REG, WORK2_REG, FT_GPIO_PIN
	cmp	WORK2_REG, FT_GPIO_PIN
	beq	2b @ Branch until push GPIO
@	bne	2b @ Branch until push GPIO

	@ Enable clock to DRAM
	ldr	WORK1_REG, [SCU_REG, #SCU_GATING_CLOCK_REG]
	bic	WORK1_REG, WORK1_REG, #SCU_DDR_GATING_CLOCK
	str	WORK1_REG, [SCU_REG, #SCU_GATING_CLOCK_REG]

	@ Exit self-refresh mode
	@mov WORK1_REG, #INTC_VECTOR
	@ldr	WORK1_REG, [WORK1_REG]

	@ restore regs and return
	adr	r0, tmp_stack
	ldmfd	r0!, {r3 - r7, sp, pc}

tmp_stack:
	.long	0, 0, 0, 0, 0, 0, 0
tmp_stack_end:

ENTRY(gm_sys_suspend_sz)
	.word	. - gm_sys_suspend
