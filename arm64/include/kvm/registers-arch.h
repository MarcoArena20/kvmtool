#ifndef __REG_ARCH_H__
#define __REG_ARCH_H__

#include <stdint.h>

// GPRs
#define X(i)      0x6030000000100000ULL + ((uint64_t)i * 2)
#define PC        0x6030000000100040U
L
// Defines the state of the vCPU
#define PSTATE    0x6030000000100042ULL

#define PSTATE_IL_BIT       (1ULL << 20)
#define PSTATE_SS_BIT       (1ULL << 21)
#define PSTATE_PAN_BIT      (1ULL << 22)
#define PSTATE_UAO_BIT      (1ULL << 23)
#define PSTATE_D_BIT        (1ULL << 9)
#define PSTATE_A_BIT        (1ULL << 8)
#define PSTATE_I_BIT        (1ULL << 7)
#define PSTATE_F_BIT        (1ULL << 6)

// Defines where the base address of the exception handler is located:
#define VBAR_EL2  ARM64_SYS_REG(3, 4, 12, 0, 0)

// Defines the exception offsets
#define VECTOR_LOWER_A64_SYNC   0x400ULL 

// Defines the state of the vCPU after the exception has been handled
#define SPSR_EL2  ARM64_SYS_REG(3, 4, 4, 0, 0)

// Defines the exception syndrome 
#define ESR_EL2   ARM64_SYS_REG(3, 4, 5, 2, 0)

#define ESR_EC_SHIFT        26                  
#define ESR_EC_HVC64        (0x16ULL << ESR_EC_SHIFT)
#define ESR_ISS_MASK        0xFFFFULL 

// Defines the address that caused a memory exception
#define FAR_EL2   ARM64_SYS_REG(3, 4, 6, 0, 0)

// Defines the preferred return address for the triggered exception
#define ELR_EL2   ARM64_SYS_REG(3, 4, 4, 0, 1)

#define SCTLR_EL2  ARM64_SYS_REG(3, 4, 1, 0, 0)

#endif
