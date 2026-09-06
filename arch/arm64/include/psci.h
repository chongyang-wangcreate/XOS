#ifndef __PSCI_H__
#define __PSCI_H__

#include "types.h"
//int psci_cpu_on(uint64 target_mpidr,uint64 entry,uint64 context_id);
int xos_boot_secondary_cpus(void);


#endif