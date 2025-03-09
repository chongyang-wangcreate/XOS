#ifndef __VECTOR_H__
#define __VECTOR_H__


/*
    EL1t (EL1-Thread mode)
    EL1h (EL1-Handler mode)
*/
#define BAD_SYNC_EL1t		0 
#define BAD_IRQ_EL1t		1 
#define BAD_FIQ_EL1t		2 
#define BAD_ERROR_EL1t		3 

#define BAD_SYNC_EL1h		4 
#define BAD_IRQ_EL1h		5 
#define BAD_FIQ_EL1h		6
#define BAD_ERROR_EL1h		7 

#define BAD_SYNC_EL0_64	    	8 
#define IRQ_INVALID_EL0_64	    	9 
#define BAD_FIQ_EL0_64		10 
#define BAD_ERROR_EL0_64		11 

#define BAD_SYNC_EL0_32		12 
#define BAD_IRQ_EL0_32		13 
#define BAD_FIQ_EL0_32		14 
#define BAD_ERROR_EL0_32		15 
#define SYNC_ERROR			16 
#define SYSCALL_ERROR			17 
#define DATA_ABORT_ERROR		18





#endif
