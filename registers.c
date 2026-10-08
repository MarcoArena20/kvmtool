#include <kvm/registers.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>
#include <kvm/util.h>

uint64_t get_reg(int vcpu_fd, uint64_t reg_id) {
                                                       
        uint64_t value = 0;                            
                                                       
        struct kvm_one_reg reg = {                     
                .id = reg_id,                          
                .addr = (uint64_t)&value,              
        };                                             
                                                       
        if (ioctl(vcpu_fd, KVM_GET_ONE_REG, &reg) < 0) 
                die("KVM_GET_ONE_REG");                
                                                      
        return value;                                 
}                                                     
                                                      
void set_reg(int vcpu_fd, uint64_t reg_id, uint64_t value) {
                                                                   
        struct kvm_one_reg reg = {                                 
                .id = reg_id,                                      
                .addr = (uint64_t)&value,                          
        };                                                         
                                                                   
        if (ioctl(vcpu_fd, KVM_SET_ONE_REG, &reg) < 0)             
                die("KVM_SET_ONE_REG");                            
                                                                   
}

