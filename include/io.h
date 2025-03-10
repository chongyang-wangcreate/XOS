#ifndef __ASM_ARM_IO_H
#define __ASM_ARM_IO_H

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

#include <barriers.h>
#include <types.h>
#include <compiler_types.h>

/*
 * Generic IO read/write.  These perform native-endian accesses.
 */
#define __raw_writeb __raw_writeb
static inline void __raw_writeb(u8 val, volatile void __iomem *addr)
{
	asm volatile("strb %w0, [%1]" : : "rZ" (val), "r" (addr));
}

#define __raw_writew __raw_writew
static inline void __raw_writew(u16 val, volatile void __iomem *addr)
{
	asm volatile("strh %w0, [%1]" : : "rZ" (val), "r" (addr));
}

#define __raw_writel __raw_writel
static __always_inline void __raw_writel(u32 val, volatile void __iomem *addr)
{
	asm volatile("str %w0, [%1]" : : "rZ" (val), "r" (addr));
}

#define __raw_writeq __raw_writeq
static inline void __raw_writeq(u64 val, volatile void __iomem *addr)
{
	asm volatile("str %x0, [%1]" : : "rZ" (val), "r" (addr));
}

#define __raw_readb __raw_readb
static inline u8 __raw_readb(const volatile void __iomem *addr)
{
	u8 val;
	asm volatile("ldrb %w0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readw __raw_readw
static inline u16 __raw_readw(const volatile void __iomem *addr)
{
	u16 val;

	asm volatile("ldrh %w0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readl __raw_readl
static __always_inline u32 __raw_readl(const volatile void __iomem *addr)
{
	u32 val;
	asm volatile("ldr %w0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readq __raw_readq
static inline u64 __raw_readq(const volatile void __iomem *addr)
{
	u64 val;
	asm volatile("ldr %0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

/* IO barriers */
/*								                                \
* Create a dummy control dependency from the IO read to any	    \
* later instructions. This ensures that a subsequent call to	\
* udelay() will be ordered due to the ISB in get_cycles().	    \
*/

#define __iormb(v)							    \
({									            \
	unsigned long tmp;						    \
									            \
	dma_rmb();					    			\
                    							\
	asm volatile("eor	%0, %1, %1\n"			\
		     "cbnz	%0, ."					    \
		     : "=r" (tmp) : "r" ((unsigned long)(v))	   \
		     : "memory");					               \
})

#define __io_par(v)		__iormb(v)
#define __iowmb()		dma_wmb()
#define __iomb()		dma_mb()

/*
 * Relaxed I/O memory access primitives. These follow the Device memory
 * ordering rules but do not guarantee any ordering relative to Normal memory
 * accesses.
 */
#define readb_relaxed(c)	({ u8  __r = __raw_readb(c); __r; })
#define readw_relaxed(c)	({ u16 __r = __raw_readw(c); __r; })
#define readl_relaxed(c)	({ u32 __r = __raw_readl(c); __r; })
#define readq_relaxed(c)	({ u64 __r = __raw_readq(c); __r; })

#define writeb_relaxed(v,c)	((void)__raw_writeb((v),(c)))
#define writew_relaxed(v,c)	((void)__raw_writew((v),(c)))
#define writel_relaxed(v,c)	((void)__raw_writel((v),(c)))
#define writeq_relaxed(v,c)	((void)__raw_writeq((v),(c)))

/*
 * I/O memory access primitives. Reads are ordered relative to any
 * following Normal memory access. Writes are ordered relative to any prior
 * Normal memory access.
 */
#define readb(c)		({ u8  __v = readb_relaxed(c); __iormb(__v); __v; })
#define readw(c)		({ u16 __v = readw_relaxed(c); __iormb(__v); __v; })
#define readl(c)		({ u32 __v = readl_relaxed(c); __iormb(__v); __v; })
#define readq(c)		({ u64 __v = readq_relaxed(c); __iormb(__v); __v; })

#define writeb(v,c)		({ __iowmb(); writeb_relaxed((v),(c)); })
#define writew(v,c)		({ __iowmb(); writew_relaxed((v),(c)); })
#define writel(v,c)		({ __iowmb(); writel_relaxed((v),(c)); })
#define writeq(v,c)		({ __iowmb(); writeq_relaxed((v),(c)); })

#endif	/* __ASM_ARM_IO_H */
