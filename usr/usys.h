#ifndef __USYS_H__
#define __USYS_H__


enum{

    NR_FORK,
    NR_OPEN,
    NR_READ,
    NR_WRITE,
    NR_SLEEP,
    NR_PWD,
    NR_CD,
    NR_DUP,
    NR_MKDIR,
    NR_STAT,
    NR_READDIR,
    NR_LSEEK,
    NR_CHMOD,
    NR_GETPID,
    NR_MAX

};


#define SYS_CALL_ASM0 \
    "mov x9, %1\n\t"\
    "svc #0\n\t"\
    "mov %0, X0\n\t"

#define SYS_CALL_ASM1 \
    "mov x9, %1\n\t" \
    "mov X0, %2\n\t" \
    "svc #0\n\t" \
    "mov %0, X0\n\t"

#define SYS_CALL_ASM2 \
    "mov x9, %1\n\t" \
    "mov X0, %2\n\t" \
    "mov X1, %3\n\t"\
    "svc #0\n\t" \
    "mov %0, X0\n\t"

#define SYS_CALL_ASM3 \
    "mov x9, %1\n\t" \
    "mov X0, %2\n\t" \
    "mov X1, %3\n\t"\
    "mov X2, %4\n\t"\
    "svc #0\n\t" \
    "mov %0, X0\n\t"

#define SYS_CALL_ASM4 \
    "mov x9, %1\n\t" \
    "mov X0, %2\n\t" \
    "mov X1, %3\n\t"\
    "mov X2, %4\n\t"\
    "mov X3, %5\n\t"\
    "svc #0\n\t" \
    "mov %0, X0\n\t"

#define SYS_CALL_ASM5 \
    "mov x9, %1\n\t" \
    "mov X0, %2\n\t" \
    "mov X1, %3\n\t"\
    "mov X2, %4\n\t"\
    "mov X3, %5\n\t"\
    "mov X4, %6\n\t"\
    "svc #0\n\t" \
    "mov %0, X0\n\t"



static inline long SYS_CALL_DEF0(int call_num)
{
        long __res;
        asm volatile (
        SYS_CALL_ASM0
        : "=r" (__res)
        :"r"(call_num));
        return __res;
}

static inline long SYS_CALL_DEF1(int call_num, uint64_t argv0) {
    long __res;
    asm volatile (
        SYS_CALL_ASM1
        : "=r" (__res)
        : "r" (call_num), "r" (argv0)
    );
    return __res;
}


static inline long SYS_CALL_DEF2(int call_num,uint64_t argv0,uint64_t argv1)
{
        long __res;
        asm volatile (
        SYS_CALL_ASM2
        : "=r" (__res)
        :"r"(call_num),"r"(argv0),"r"(argv1));
        return __res;
}

static inline long SYS_CALL_DEF3(int call_num,uint64_t argv0,uint64_t argv1,uint64_t argv2)
{
        long __res;
        asm volatile (
        SYS_CALL_ASM3
        : "=r" (__res)
        :"r"(call_num),"r"(argv0),"r"(argv1),"r"(argv2));
        return __res;
}


static inline long SYS_CALL_DEF4(int call_num,uint64_t argv0,uint64_t argv1,uint64_t argv2,uint64_t argv3)
{
        long __res;
        asm volatile (
        SYS_CALL_ASM4
        : "=r" (__res)
        :"r"(call_num),"r"(argv0),"r"(argv1),"r"(argv2),"r"(argv3));
        return __res;
}

static inline long SYS_CALL_DEF5(int call_num,uint64_t argv0,uint64_t argv1,uint64_t argv2,uint64_t argv3,uint64_t argv4)
{
        long __res;
        asm volatile (
        SYS_CALL_ASM5
        : "=r" (__res)
        :"r"(call_num),"r"(argv0),"r"(argv1),"r"(argv2),"r"(argv3));
        return __res;
}

#endif
