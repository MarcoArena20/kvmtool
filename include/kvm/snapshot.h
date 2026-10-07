#ifndef KVM_SNAPSHOT_H
#define KVM_SNAPSHOT_H

#include <kvm/kvm-cpu-arch.h>
#include <kvm/kvm.h>

#define SNAP_MAGIC   0x534E41504B564D31ULL
#define SNAP_VERSION 1

#define SNAP_RAM_CHUNK (16UL * 1024UL * 1024UL)  

struct snap_header {
        uint64_t magic;
        uint32_t version;
        uint32_t nr_regs;
        uint64_t ram_size;
};

struct snap_reg_header {
        uint64_t id;
        uint32_t size;
        uint32_t reserved;
};


int snap_save(struct kvm *, struct kvm_cpu *, const char *);  
int snap_load(struct kvm *, struct kvm_cpu *, const char *); 

#endif
