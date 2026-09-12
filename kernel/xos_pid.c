#include "types.h"
#include "string.h"
#include "spinlock.h"
#include "xos_pid.h"

#define MAX_PID 65535
#define PID_SEG_SIZE 64
#define PID_MAP_SIZE ((MAX_PID + 1) / PID_SEG_SIZE)

typedef struct {
    int next_pid;
    xos_spinlock_t lock;
    uint64 used_pids[PID_MAP_SIZE];
} pid_desc_t;

static pid_desc_t pid_state;

static int pid_is_used(int pid)
{
    return (pid_state.used_pids[pid / PID_SEG_SIZE] &
            (1ULL << (pid % PID_SEG_SIZE))) != 0;
}

static void pid_mark_used(int pid)
{
    pid_state.used_pids[pid / PID_SEG_SIZE] |=
        1ULL << (pid % PID_SEG_SIZE);
}

static void pid_mark_free(int pid)
{
    pid_state.used_pids[pid / PID_SEG_SIZE] &=
        ~(1ULL << (pid % PID_SEG_SIZE));
}

void pid_init(void)
{
    memset(&pid_state, 0, sizeof(pid_state));
    xos_spinlock_init(&pid_state.lock);
    pid_state.next_pid = 1;
    pid_mark_used(0);
}

int alloc_pid(void)
{
    unsigned long flags;
    int scanned;
    int pid;

    flags = xos_spin_lock_irqsave(&pid_state.lock);
    pid = pid_state.next_pid;
    for(scanned = 0; scanned < MAX_PID; scanned++){
        if(pid <= 0 || pid > MAX_PID){
            pid = 1;
        }
        if(!pid_is_used(pid)){
            pid_mark_used(pid);
            pid_state.next_pid = pid + 1;
            xos_spin_unlock_irqrestore(&pid_state.lock, flags);
            return pid;
        }
        pid++;
    }
    xos_spin_unlock_irqrestore(&pid_state.lock, flags);
    return -1;
}

void free_pid(int pid)
{
    unsigned long flags;

    if(pid <= 0 || pid > MAX_PID){
        return;
    }

    flags = xos_spin_lock_irqsave(&pid_state.lock);
    pid_mark_free(pid);
    if(pid < pid_state.next_pid){
        pid_state.next_pid = pid;
    }
    xos_spin_unlock_irqrestore(&pid_state.lock, flags);
}
