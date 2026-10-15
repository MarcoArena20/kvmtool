#include "kvm/kvm-arch-state.h"

#include <stdint.h>
#include <kvm/registers.h>
#include "kvm/registers-arch.h"

struct kvm_trace_arch_state {
    uint64_t pstate;
    uint64_t elr_el2;
    uint64_t esr_el2;
    uint64_t far_el2;
    uint64_t vbar_el2;
};

size_t kvm_trace_arch__state_size(void)
{
    return sizeof(struct kvm_trace_arch_state);
}

int kvm_trace_arch__capture(struct kvm_cpu *vcpu,
                            uint64_t *pc,
                            struct kvm_trace_arch_state *state)
{
    int ret;

    if (!vcpu || !pc || !state)
        return -EINVAL;

    *pc              = get_reg(vcpu->vcpu_fd, PC);
    state->pstate    = get_reg(vcpu->vcpu_fd, PSTATE);
    state->elr_el2   = get_reg(vcpu->vcpu_fd, ELR_EL2);
    state->esr_el2   = get_reg(vcpu->vcpu_fd, ESR_EL2);
    state->far_el2   = get_reg(vcpu->vcpu_fd, FAR_EL2);
    state->vbar_el2  = get_reg(vcpu->vcpu_fd, VBAR_EL2);

    return 0;
}

void kvm_trace_arch__dump(
    const struct kvm_trace_arch_state *state)
{
    if (!state)
        return;

    printf("PSTATE:   0x%016" PRIx64 "\n", state->pstate);
    printf("ELR_EL2:  0x%016" PRIx64 "\n", state->elr_el2);
    printf("ESR_EL2:  0x%016" PRIx64 "\n", state->esr_el2);
    printf("FAR_EL2:  0x%016" PRIx64 "\n", state->far_el2);
    printf("VBAR_EL2: 0x%016" PRIx64 "\n", state->vbar_el2);
}
