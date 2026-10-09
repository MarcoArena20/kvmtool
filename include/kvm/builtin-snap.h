#ifndef KVM__SNAP_H
#define KVM__SNAP_H

int kvm_cmd_snap(int argc, const char **argv, const char *prefix);
void kvm_snap_help(void);
#endif
