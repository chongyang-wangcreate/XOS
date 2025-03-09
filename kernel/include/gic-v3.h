#ifndef XOS_GICV3_H
#define XOS_GICV3_H

#ifndef __ASSEMBLER__

typedef int bool;

void gicv3_init(void);
void gicv3_init_percpu(void);

void gic_eoi(u32 iar);
u32 gic_iar(void);

bool gic_enabled(void);
extern int gic_init(void);


extern void gic_mask_irq(u32 hwirq);

extern void gic_unmask_irq(u32 hwirq);

extern u64 gic_read_iar(void);

extern void gic_eoi_irq(u32 hwirq);

extern void xos_irq_init();


#endif

#endif
