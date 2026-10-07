#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <kvm/snapshot.h>
#include <kvm/reg.h>

static int snap_get_reg_list(struct kvm_cpu *vcpu,
                             struct kvm_reg_list **reg_list)
{
        struct kvm_reg_list query;
        struct kvm_reg_list *list;
        size_t size;

        memset(&query, 0, sizeof(query));

        /*
         * Prima chiamata:
         *
         * n = 0 significa che non stiamo fornendo spazio
         * per nessun elemento dell'array reg[].
         *
         * KVM deve quindi restituire E2BIG e indicare in
         * query.n quanti registri sono necessari.
         */
        if (ioctl(vcpu->vcpu_fd, KVM_GET_REG_LIST, &query) == 0) {
                /*
                 * Una vCPU ARM64 normale deve avere almeno un
                 * registro. Se il kernel ha accettato n = 0,
                 * controlliamo comunque il risultato.
                 */
                if (query.n == 0) {
                        pr_err("KVM_GET_REG_LIST returned zero registers\n");
                        return -1;
                }

                /*
                 * Caso particolare: il kernel ha accettato la
                 * richiesta direttamente. Possiamo comunque
                 * procedere usando query.n.
                 */
        } else if (errno != E2BIG) {
                pr_err("KVM_GET_REG_LIST failed: %s\n",
                       strerror(errno));
                return -1;
        }

        if (query.n == 0) {
                pr_err("KVM_GET_REG_LIST returned zero registers\n");
                return -1;
        }

        /*
         * struct kvm_reg_list contiene:
         *
         *     __u64 n;
         *     __u64 reg[];
         *
         * quindi allochiamo spazio anche per reg[query.n].
         */
        if (query.n > (SIZE_MAX - sizeof(*list)) /
                      sizeof(list->reg[0])) {
                pr_err("Register list size overflow\n");
                return -1;
        }

        size = sizeof(*list) +
               query.n * sizeof(list->reg[0]);

        list = malloc(size);
        if (!list) {
                pr_err("Unable to allocate register list\n");
                return -1;
        }

        memset(list, 0, size);

        list->n = query.n;

        /*
         * Seconda chiamata:
         * KVM riempie reg[] con gli ID dei registri.
         */
        if (ioctl(vcpu->vcpu_fd, KVM_GET_REG_LIST, list) < 0) {
                pr_err("KVM_GET_REG_LIST failed: %s\n",
                       strerror(errno));
                free(list);
                return -1;
        }

        *reg_list = list;

        return 0;
}         

static int snap_find_writable_regs(struct kvm_cpu *vcpu,
                                   struct kvm_reg_list *reg_list,
                                   uint64_t **writable_regs,
                                   uint32_t *writable_count)
{
        uint64_t *regs;
        uint64_t i;
        uint32_t count = 0;

        regs = calloc(reg_list->n, sizeof(*regs));
        if (!regs) {
                pr_err("Unable to allocate writable register list\n");
                return -ENOMEM;
        }

        for (i = 0; i < reg_list->n; i++) {
                uint64_t id = reg_list->reg[i];
                uint32_t size;
                void *value;
                struct kvm_one_reg reg;
                int ret;

                size = KVM_REG_SIZE(id);

                if (size == 0) {
                        pr_info("Skipping register "
                                "0x%016llx: invalid size\n",
                                (unsigned long long)id);
                        continue;
                }

                value = malloc(size);
                if (!value) {
                        free(regs);
                        return -ENOMEM;
                }

                memset(&reg, 0, sizeof(reg));

                reg.id = id;
                reg.addr = (unsigned long)value;

                /*
                 * Primo controllo sulla possibilità di leggere i registri.
                 */
                ret = ioctl(vcpu->vcpu_fd,
                            KVM_GET_ONE_REG,
                            &reg);

                if (ret < 0) {
                        pr_info("Skipping register "
                                "0x%016llx: GET failed: %s\n",
                                (unsigned long long)id,
                                strerror(errno));
                        free(value);
                        continue;
                }

                /*
                 * Secondo controllo sulla possibilità di scrivere sui registri
                 */
                ret = ioctl(vcpu->vcpu_fd,
                            KVM_SET_ONE_REG,
                            &reg);

                if (ret < 0) {
                        pr_info("Skipping register "
                                "0x%016llx: SET failed: %s\n",
                                (unsigned long long)id,
                                strerror(errno));
                        free(value);
                        continue;
                }

                regs[count++] = id;

                free(value);
        }

        *writable_regs = regs;
        *writable_count = count;

        return 0;
}

static int snap_write_all(int fd, const void *buffer, size_t size)
{
        const unsigned char *ptr = buffer;
        size_t remaining = size;

        while (remaining > 0) {
                ssize_t written;

                written = write(fd, ptr, remaining);

                if (written < 0) {
                        if (errno == EINTR)
                                continue;

                        pr_err("write() failed: %s\n",
                               strerror(errno));
                        return -1;
                }

                if (written == 0) {
                        pr_err("write() returned 0 bytes\n");
                        return -1;
                }

                ptr += written;
                remaining -= written;
        }

        return 0;
}

static int snap_read_all(int fd, void *buffer, size_t size)
{
        unsigned char *ptr = buffer;
        size_t remaining = size;

        while (remaining > 0) {
                ssize_t n;

                n = read(fd, ptr, remaining);

                if (n < 0) {
                        if (errno == EINTR)
                                continue;

                        pr_err("read() failed: %s\n",
                               strerror(errno));
                        return -1;
                }

                if (n == 0) {
                        pr_err("Unexpected end of snapshot\n");
                        return -1;
                }

                ptr += n;
                remaining -= n;
        }

        return 0;
}

static int snap_save_regs(int fd,
                          struct kvm_cpu *vcpu,
                          uint64_t *writable_regs,
                          uint32_t writable_count)
{
        uint32_t i;

        for (i = 0; i < writable_count; i++) {
                uint64_t id = writable_regs[i];
                uint32_t size;
                void *value;
                struct kvm_one_reg reg;
                struct snap_reg_header reg_header;
                int ret;

                size = KVM_REG_SIZE(id);

                if (size == 0) {
                        pr_err("Invalid size for register "
                               "0x%016llx\n",
                               (unsigned long long)id);
                        return -EINVAL;
                }

                value = malloc(size);
                if (!value) {
                        pr_err("Unable to allocate %u bytes for "
                               "register 0x%016llx\n",
                               size,
                               (unsigned long long)id);
                        return -ENOMEM;
                }

                memset(&reg, 0, sizeof(reg));

                reg.id = id;
                reg.addr = (unsigned long)value;

                /*
                 * Legge il valore che viene salvato nello snapshot
                 */
                ret = ioctl(vcpu->vcpu_fd,
                            KVM_GET_ONE_REG,
                            &reg);

                if (ret < 0) {
                        pr_err("KVM_GET_ONE_REG failed for register "
                               "0x%016llx: %s\n",
                               (unsigned long long)id,
                               strerror(errno));
                        free(value);
                        return -1;
                }

                reg_header.id = id;
                reg_header.size = size;
                reg_header.reserved = 0;

                /*
                 * Header dei registri.
                 */
                ret = snap_write_all(fd,
                                     &reg_header,
                                     sizeof(reg_header));

                if (ret < 0) {
                        free(value);
                        return ret;
                }

                /*
                 * Valore dei registri.
                 */
                ret = snap_write_all(fd,
                                     value,
                                     size);

                if (ret < 0) {
                        free(value);
                        return ret;
                }

                free(value);
        }

        return 0;
}               

static int snap_load_regs(struct kvm_cpu *vcpu,
                          int fd,
                          uint32_t nr_regs)
{
        uint32_t i;

        for (i = 0; i < nr_regs; i++) {
                struct snap_reg_header reg_header;
                struct kvm_one_reg reg;
                void *value;
                uint32_t expected_size;
                int ret;

                memset(&reg_header, 0, sizeof(reg_header));

                if (snap_read_all(fd,
                                  &reg_header,
                                  sizeof(reg_header)) < 0) {
                        pr_err("Failed to read register header %u\n",
                               i);
                        return -1;
                }

                if (reg_header.size == 0) {
                        pr_err("Invalid size for register "
                               "0x%016llx\n",
                               (unsigned long long)reg_header.id);
                        return -1;
                }

                expected_size = KVM_REG_SIZE(reg_header.id);

                if (expected_size == 0) {
                        pr_err("Unknown register size for "
                               "0x%016llx\n",
                               (unsigned long long)reg_header.id);
                        return -1;
                }

                if (reg_header.size != expected_size) {
                        pr_err("Register size mismatch for "
                               "0x%016llx: snapshot=%u, "
                               "KVM=%u\n",
                               (unsigned long long)reg_header.id,
                               reg_header.size,
                               expected_size);
                        return -1;
                }

                value = malloc(reg_header.size);

                if (!value) {
                        pr_err("Unable to allocate %u bytes for "
                               "register 0x%016llx\n",
                               reg_header.size,
                               (unsigned long long)reg_header.id);
                        return -ENOMEM;
                }

                if (snap_read_all(fd,
                                  value,
                                  reg_header.size) < 0) {
                        free(value);
                        return -1;
                }

                memset(&reg, 0, sizeof(reg));

                reg.id = reg_header.id;
                reg.addr = (unsigned long)value;

                ret = ioctl(vcpu->vcpu_fd,
                            KVM_SET_ONE_REG,
                            &reg);

                if (ret < 0) {
                        pr_err("KVM_SET_ONE_REG failed for "
                               "register 0x%016llx: %s\n",
                               (unsigned long long)reg_header.id,
                               strerror(errno));

                        free(value);
                        return -1;
                }

                free(value);
        }

        return 0;
}

static int snap_save_ram(struct kvm *kvm, int fd)
{
        unsigned char *ptr;
        uint64_t remaining;
        uint64_t total_written = 0;

        ptr = kvm->ram_start;
        remaining = kvm->ram_size;

        pr_info("Saving %llu bytes of guest RAM...\n",
                (unsigned long long)remaining);

        while (remaining > 0) {
                size_t chunk;

                if (remaining > SNAP_RAM_CHUNK)
                        chunk = SNAP_RAM_CHUNK;
                else
                        chunk = (size_t)remaining;

                if (snap_write_all(fd, ptr, chunk) < 0) {
                        pr_err("Failed to write guest RAM at "
                               "offset 0x%llx\n",
                               (unsigned long long)total_written);
                        return -1;
                }

                ptr += chunk;
                remaining -= chunk;
                total_written += chunk;

                pr_info("RAM snapshot: %llu / %llu bytes\n",
                        (unsigned long long)total_written,
                        (unsigned long long)kvm->ram_size);
        }

        pr_info("Guest RAM saved successfully\n");

        return 0;
}


static int snap_load_ram(struct kvm *kvm, int fd)
{
        unsigned char *ptr;
        uint64_t remaining;
        uint64_t total_read = 0;

        ptr = kvm->ram_start;
        remaining = kvm->ram_size;

        pr_info("Loading %llu bytes of guest RAM...\n",
                (unsigned long long)remaining);

        while (remaining > 0) {
                size_t chunk;

                if (remaining > SNAP_RAM_CHUNK)
                        chunk = SNAP_RAM_CHUNK;
                else
                        chunk = (size_t)remaining;

                if (snap_read_all(fd, ptr, chunk) < 0) {
                        pr_err("Failed to read guest RAM at "
                               "offset 0x%llx\n",
                               (unsigned long long)total_read);
                        return -1;
                }

                ptr += chunk;
                remaining -= chunk;
                total_read += chunk;
        }

        pr_info("Guest RAM loaded successfully\n");

        return 0;
}

static void snap_dump_important_regs(struct kvm_cpu *vcpu)
{
    uint64_t pstate;
    uint64_t pc;
    uint64_t vbar_el2;
    uint64_t spsr_el2;
    uint64_t elr_el2;

    pstate   = get_reg(vcpu->vcpu_fd, PSTATE);
    pc       = get_reg(vcpu->vcpu_fd, PC);
    vbar_el2 = get_reg(vcpu->vcpu_fd, VBAR_EL2);
    spsr_el2 = get_reg(vcpu->vcpu_fd, SPSR_EL2);
    elr_el2  = get_reg(vcpu->vcpu_fd, ELR_EL2);

    pr_info("========== SNAPSHOT CPU STATE ==========\n");

    pr_info("PSTATE   = 0x%016llx  M=0x%llx\n",
            (unsigned long long)pstate,
            (unsigned long long)(pstate & 0xf));

    pr_info("PC       = 0x%016llx\n",
            (unsigned long long)pc);

    pr_info("VBAR_EL2 = 0x%016llx\n",
            (unsigned long long)vbar_el2);

    pr_info("SPSR_EL2 = 0x%016llx\n",
            (unsigned long long)spsr_el2);

    pr_info("ELR_EL2  = 0x%016llx\n",
            (unsigned long long)elr_el2);
	
    pr_info("VBAR+200 = 0x%016llx\n",
            (unsigned long long)((vbar_el2 & ~0x7ffULL) + 0x200));

    pr_info("VBAR+400 = 0x%016llx\n",
            (unsigned long long)((vbar_el2 & ~0x7ffULL) + 0x400));

    pr_info("=========================================\n");
}

int snap_save(struct kvm *kvm,
              struct kvm_cpu *vcpu,
              const char *filename)
{
        struct kvm_reg_list *reg_list = NULL;
        uint64_t *writable_regs = NULL;
        uint32_t writable_count = 0;
        struct snap_header header;
        int fd;
        int ret;

        /*
         * Otteniamo la lista di tutti i registri esposti da KVM.
         */
        ret = snap_get_reg_list(vcpu, &reg_list);
        if (ret < 0)
                return ret;

        pr_info("Found %llu KVM registers\n",
                (unsigned long long)reg_list->n);

        /*
         * Determiniamo quali registri possono essere scritti con KVM_SET_ONE_REG.
         */
        pr_info("Testing register writability...\n");

        ret = snap_find_writable_regs(vcpu,
                                      reg_list,
                                      &writable_regs,
                                      &writable_count);

        if (ret < 0) {
                pr_err("Failed to determine writable registers\n");
                goto out;
        }

        pr_info("Found %u writable registers out of %llu\n",
                writable_count,
                (unsigned long long)reg_list->n);

        if (writable_count == 0) {
                pr_err("No writable registers found\n");
                ret = -EINVAL;
                goto out;
        }

        /*
         * Apriamo lo snapshot solo dopo aver trovato il numero di registri salvabili.
         */
        fd = open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

        if (fd < 0) {
                pr_err("Unable to open snapshot '%s': %s\n",
                       filename,
                       strerror(errno));
                ret = -1;
                goto out;
        }

        memset(&header, 0, sizeof(header));

        header.magic = SNAP_MAGIC;
        header.version = SNAP_VERSION;
        header.nr_regs = writable_count;
        header.ram_size = kvm->ram_size;

        /*
         * Header
         */
        if (snap_write_all(fd,
                           &header,
                           sizeof(header)) < 0) {
                pr_err("Failed to write snapshot header\n");
                ret = -1;
                goto close;
        }

        /*
         * Registri della CPU
         */
        pr_info("Saving %u writable CPU registers...\n",
                writable_count);

        ret = snap_save_regs(fd,
                             vcpu,
                             writable_regs,
                             writable_count);

        if (ret < 0) {
                pr_err("Failed to save CPU registers\n");
                goto close;
        }
	

	 /*
         * Stampiamo a video il DUMP dei registri importanti della CPU.
         */
        snap_dump_important_regs(vcpu);

        /*
         * RAM del guest
         */
        ret = snap_save_ram(kvm, fd);

        if (ret < 0) {
                pr_err("Failed to save guest RAM\n");
                goto close;
        }

        
        if (fsync(fd) < 0) {
                pr_err("fsync() failed: %s\n",
                       strerror(errno));
                ret = -1;
                goto close;
        }

        ret = 0;

        pr_info("Snapshot successfully saved to '%s'\n",
                filename);
	
close:
        if (close(fd) < 0) {
                pr_err("close() failed: %s\n",
                       strerror(errno));
                ret = -1;
        }

out:
        free(writable_regs);
        free(reg_list);

        return ret;
}
 

int snap_load(struct kvm *kvm,
                     struct kvm_cpu *vcpu,
                     const char *filename)
{
        struct snap_header header;
        int fd;
        int ret;

        fd = open(filename, O_RDONLY);

        if (fd < 0) {
                pr_err("Unable to open snapshot '%s': %s\n",
                       filename,
                       strerror(errno));
                return -1;
        }

        memset(&header, 0, sizeof(header));

        if (snap_read_all(fd,
                          &header,
                          sizeof(header)) < 0) {
                pr_err("Failed to read snapshot header\n");
                ret = -1;
                goto out;
        }

        if (header.magic != SNAP_MAGIC) {
                pr_err("Invalid snapshot magic: 0x%llx\n",
                       (unsigned long long)header.magic);
                ret = -1;
                goto out;
        }

        if (header.version != SNAP_VERSION) {
                pr_err("Unsupported snapshot version: %u\n",
                       header.version);
                ret = -1;
                goto out;
        }

        if (header.nr_regs == 0) {
                pr_err("Snapshot contains no registers\n");
                ret = -1;
                goto out;
        }

        if (header.ram_size != kvm->ram_size) {
                pr_err("Snapshot RAM size mismatch: "
                       "snapshot=%llu, VM=%llu\n",
                       (unsigned long long)header.ram_size,
                       (unsigned long long)kvm->ram_size);
                ret = -1;
                goto out;
        }

        pr_info("Loading snapshot '%s'\n", filename);
        pr_info("Snapshot contains %u registers\n",
                header.nr_regs);
        pr_info("Snapshot RAM size: %llu bytes\n",
                (unsigned long long)header.ram_size);

        pr_info("Loading CPU registers...\n");

        ret = snap_load_regs(vcpu, fd, header.nr_regs);
        if (ret < 0) {
                pr_err("Failed to load CPU registers\n");
                goto out;
        }

        pr_info("CPU registers loaded successfully\n");

        ret = snap_load_ram(kvm, fd);
        if (ret < 0) {
                pr_err("Failed to load guest RAM\n");
                goto out;
        }

        ret = 0;

        pr_info("Snapshot successfully loaded\n");

out:
        if (close(fd) < 0) {
                pr_err("close() failed: %s\n",
                       strerror(errno));
                ret = -1;
        }

        return ret;
}
