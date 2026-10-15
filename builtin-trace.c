#include "kvm/builtin-trace.h"
#include <kvm/kvm.h>
#include <kvm/parse-options.h>
#include <kvm/kvm-ipc.h>
#include <kvm/util.h>
#include <kvm/kvm-cmd.h>

static const char *directory;

static const char * const trace_usage[] = {
        "lkvm snap [-d directory]",
        NULL
};

static const struct option trace_options[] = {
        OPT_GROUP("General options:"),
        OPT_STRING('d', "directory", &directory, "directory", "Directory path"),
        OPT_END()
};

static void parse_trace_options(int argc, const char **argv)
{
        while (argc != 0) {
                argc = parse_options(argc, argv, trace_options, trace_usage,
                                PARSE_OPT_STOP_AT_NON_OPTION);
                if (argc != 0)
                        kvm_trace_help();
        }
}

void kvm_trace_help(void)
{
        usage_with_options(snap_usage, snap_options);
}

int kvm_cmd_trace(int argc, const char **argv, const char *prefix)
{

        parse_trace_options(argc, argv);
	
	ret = kvm_trace__analyze_dir(directory);

	if (ret < 0)
    		pr_err("Trace analysis failed: %d\n", ret);
}

