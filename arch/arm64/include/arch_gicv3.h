/*
 * arch/arm/include/asm/arch_gicv3.h
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_ARCH_GICV3_H
#define __ASM_ARCH_GICV3_H

#include "stringify.h"
#include "sysreg.h"
#include "barriers.h"
#include "io.h"



#define read_gicreg(r)			read_sysreg_s(SYS_ ## r)
#define write_gicreg(v, r)		write_sysreg_s(v, SYS_ ## r)

//#define read_gicreg(r)			read_sysreg(SYS_ ## r)
//#define write_gicreg(v, r)		write_sysreg(v, SYS_ ## r)


/*
 * Low-level accessors
 *
 * These system registers are 32 bits, but we make sure that the compiler
 * sets the GP register's most significant bits to 0 with an explicit cast.
 */

static inline void gic_write_eoir(u32 irq)
{
	write_sysreg_s(irq, SYS_ICC_EOIR1_EL1);
	isb();
}

static inline void gic_write_dir(u32 irq)
{
	write_sysreg_s(irq, SYS_ICC_DIR_EL1);
	isb();
}

static inline u64 gic_read_iar_common(void)
{
	u64 irqstat;

	irqstat = read_sysreg_s(SYS_ICC_IAR1_EL1);
//    irqstat = read_sysreg(SYS_ICC_IAR1_EL1);
	dsb(sy);
	return irqstat;
}

static inline void gic_write_ctlr(u32 val)
{
	write_sysreg_s(val, SYS_ICC_CTLR_EL1);
	isb();
}

static inline u32 gic_read_ctlr(void)
{
	return read_sysreg_s(SYS_ICC_CTLR_EL1);
}

static inline void gic_write_grpen1(u32 val)
{
	write_sysreg_s(val, SYS_ICC_IGRPEN1_EL1);
	isb();
}

static inline void gic_write_sgi1r(u64 val)
{
	write_sysreg_s(val, SYS_ICC_SGI1R_EL1);
}

static inline u32 gic_read_sre(void)
{
	return read_sysreg_s(SYS_ICC_SRE_EL1);
}

static inline void gic_write_sre(u32 val)
{
	write_sysreg_s(val, SYS_ICC_SRE_EL1);
	isb();
}

static inline void gic_write_bpr1(u32 val)
{
	write_sysreg_s(val, SYS_ICC_BPR1_EL1);
//    write_sysreg(val, SYS_ICC_BPR1_EL1);
}

#define gic_read_typer(c)		readq_relaxed(c)
#define gic_write_irouter(v, c)		writeq_relaxed(v, c)
#define gic_read_lpir(c)		readq_relaxed(c)
#define gic_write_lpir(v, c)		writeq_relaxed(v, c)

#define gits_read_baser(c)		readq_relaxed(c)
#define gits_write_baser(v, c)		writeq_relaxed(v, c)

#define gits_read_cbaser(c)		readq_relaxed(c)
#define gits_write_cbaser(v, c)		writeq_relaxed(v, c)

#define gits_write_cwriter(v, c)	writeq_relaxed(v, c)

#define gicr_read_propbaser(c)		readq_relaxed(c)
#define gicr_write_propbaser(v, c)	writeq_relaxed(v, c)

#define gicr_write_pendbaser(v, c)	writeq_relaxed(v, c)
#define gicr_read_pendbaser(c)		readq_relaxed(c)

#endif /* !__ASM_ARCH_GICV3_H */
