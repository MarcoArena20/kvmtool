#include "kvm/builtin-fuzz.h"
#include "kvm/registers.h"
#include <kvm/kvm.h>
#include <kvm/parse-options.h>
#include <kvm/kvm-ipc.h>
#include <kvm/util.h>
#include <kvm/kvm-cmd.h>

static const char *instance_name;

static const char * const fuzz_usage[] = {
        "lkvm fuzz [-n name]",
        NULL
};

static const struct option fuzz_options[] = {
        OPT_GROUP("General options:"),
        OPT_STRING('n', "name", &instance_name, "name", "Instance name"),
        OPT_END()
};

static void parse_fuzz_options(int argc, const char **argv)
{
        while (argc != 0) {
                argc = parse_options(argc, argv, fuzz_options, fuzz_usage,
                                PARSE_OPT_STOP_AT_NON_OPTION);
                if (argc != 0)
                        kvm_fuzz_help();
        }
}

void kvm_fuzz_help(void)
{
        usage_with_options(fuzz_usage, fuzz_options);
}

static int do_fuzz(const char *name, int sock)
{
        int r;

        r = kvm_ipc__send(sock, KVM_IPC_FUZZ);
        if (r)
                return r;

        printf("Guest %s fuzzed\n", name);

        return 0;
}


int kvm_cmd_fuzz(int argc, const char **argv, const char *prefix){

	int instance;
        int r;

        parse_fuzz_options(argc, argv);

        if (instance_name == NULL)
                kvm_fuzz_help();

        instance = kvm__get_sock_by_instance(instance_name);

        if (instance <= 0)
                die("Failed locating instance");

        r = do_fuzz(instance_name, instance);

        close(instance);

        return r;


}
