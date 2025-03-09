#ifndef __PT_FRAME_H__
#define __PT_FRAME_H__


struct pt_regs{
    unsigned long regs[31];
    unsigned long sp;
    unsigned long pc;
//    uint64 pc;
    unsigned long pstate;
};

#define PSR_MODE_EL0t   0x00000000
#define PSR_MODE_MASK   0x0000000f


static inline int user_mode(const struct pt_regs *regs)
{
	return (((regs)->pstate & PSR_MODE_MASK) == PSR_MODE_EL0t);
}


#endif
