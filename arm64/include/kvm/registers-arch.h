#ifndef __REG_ARCH_H__
#define __REG_ARCH_H__

#include <stdint.h>

// GPRs
#define X(i)      0x6030000000100000ULL + ((uint64_t)i * 2)
#define PC        0x6030000000100040U
L
// Defines the state of the vCPU
#define PSTATE    0x6030000000100042ULL

// Condition flags: NZCV [31:28]
#define PSR_N_BIT           (1ULL << 31)
#define PSR_Z_BIT           (1ULL << 30)
#define PSR_C_BIT           (1ULL << 29)
#define PSR_V_BIT           (1ULL << 28)

// Extension state
#define PSR_DIT_BIT         (1ULL << 24)
#define PSR_TCO_BIT         (1ULL << 25)
#define PSR_SSBS_BIT        (1ULL << 12)

// PAN
#define PSR_PAN_BIT         (1ULL << 22)

// PSTATE mode field
#define PSR_MODE_MASK       0x0FULL
#define PSR_MODE32_BIT      (1ULL << 4)

#define PSR_MODE_EL0t       0x0ULL
#define PSR_MODE_EL1t       0x4ULL
#define PSR_MODE_EL1h       0x5ULL
#define PSR_MODE_EL2t       0x8ULL
#define PSR_MODE_EL2h       0x9ULL
#define PSR_MODE_EL3t       0xCULL
#define PSR_MODE_EL3h       0xDULL

// SPAN: SCTLR_EL2[23]
#define SCTLR_EL2_SPAN      (1ULL << 23)

// DSSBS: SCTLR_EL2[44]
#define SCTLR_ELx_DSSBS     (1ULL << 44)
#define SCTLR_EL2_DSSBS     SCTLR_ELx_DSSBS

#define COMPAT_PSR_F_BIT        0x00000040
#define COMPAT_PSR_I_BIT        0x00000080
#define COMPAT_PSR_E_BIT        0x00000200
#define COMPAT_PSR_MODE_SVC     0x00000013

#define SCTLR_EL1_E0E_MASK      (1 << 24)
#define SCTLR_EL1_EE_MASK       (1 << 25)
#define HCR_EL2_TGE             (1UL << 27)
#define HCR_EL2_E2H             (1UL << 34)


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
