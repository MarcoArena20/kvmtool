#ifndef KVM_TRACE_ARCH_H
#define KVM_TRACE_ARCH_H

#include "kvm/kvm-cpu.h"

struct kvm_trace_arch_state;

size_t kvm_trace_arch__state_size(void);

int kvm_trace_arch__capture(struct kvm_cpu *vcpu, uint64_t *pc, struct kvm_trace_arch_state *state);

void kvm_trace_arch__dump(const struct kvm_trace_arch_state *state);

#endif
