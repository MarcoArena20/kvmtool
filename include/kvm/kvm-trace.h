#ifndef KVM_TRACE_H
#define KVM_TRACE_H

#include <stdint.h>
#include <stddef.h>
#include "kvm/kvm-cpu.h"
#include "kvm/kvm-arch-state.h"

struct kvm_trace_entry {
    uint64_t step;
    uint64_t pc;
    struct kvm_trace_arch_state *arch_state;
};

struct kvm_trace {
    struct kvm_trace_entry *entries;
    void *arch_states;

    size_t count;
    size_t capacity;
    size_t arch_state_size;

    bool has_cycle;
    size_t cycle_start;
    size_t cycle_length;
    size_t cycle_repetitions;
};

struct kvm_trace *kvm_trace__create(size_t capacity);

int kvm_trace__record(struct kvm_trace *trace, struct kvm_cpu *vcpu);

int kvm_trace__save(const struct kvm_trace *trace, const char *path);

struct kvm_trace *kvm_trace__load(const char *path);

const struct kvm_trace_entry *kvm_trace__get(const struct kvm_trace *trace, size_t index);

void kvm_trace__free(struct kvm_trace *trace);

bool kvm_trace__detect_cycle(struct kvm_trace *trace, size_t max_period, size_t min_repetitions);

int kvm_trace__analyze_dir(const char *dir_path);

#endif
