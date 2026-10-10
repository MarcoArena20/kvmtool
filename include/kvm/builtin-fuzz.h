#ifndef KVM__FUZZ_H
#define KVM__FUZZ_H

int kvm_cmd_fuzz(int argc, const char **argv, const char *prefix);
void kvm_fuzz_help(void);

#endif

