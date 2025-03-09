#ifndef __ARCH64_TIMER__
#define __ARCH64_TIMER__

typedef long time64_t;
#define TICK_10 10	/* ms */
#define TICK_100 100	/* ms */

#define CNTFRQ_EL0_VALUE 62500000  /*时钟每秒触发62500000*/

#define TICK_TIMER_COUNT (CNTFRQ_EL0_VALUE / (1000/TICK_10))

struct timespec64 {
    time64_t    tv_sec;			/* seconds */
    time64_t    tv_nsec;		/* nanoseconds */
};

extern int  arch64_timer_init(uint64 freq_value);

extern void arch64_timer_start(void);
extern int arch64_timer_stop();
extern uint64 second_to_ticks(int seconds);
extern void xos_timer_init();


#endif
