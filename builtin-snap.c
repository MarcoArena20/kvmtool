#include <errno.h>
#include "kvm/read-write.h"
#include "kvm/builtin-snap.h"
#include "kvm/registers.h"
#include <kvm/kvm.h>
#include <kvm/parse-options.h>
#include <kvm/kvm-ipc.h>
#include <kvm/util.h>
#include <kvm/kvm-cmd.h>

static const char *instance_name;

static const char * const snap_usage[] = {
	"lkvm snap [-n name]",
	NULL
};

static const struct option snap_options[] = {
	OPT_GROUP("General options:"),
	OPT_STRING('n', "name", &instance_name, "name", "Instance name"),
	OPT_END()
};

static void parse_snap_options(int argc, const char **argv)
{
	while (argc != 0) {
		argc = parse_options(argc, argv, snap_options, snap_usage,
				PARSE_OPT_STOP_AT_NON_OPTION);
		if (argc != 0)
			kvm_snap_help();
	}
}

void kvm_snap_help(void)
{
	usage_with_options(snap_usage, snap_options);
}

static int do_snap(const char *name, int sock)
{
	int status;
	int r;
	ssize_t n;

	r = kvm_ipc__send(sock, KVM_IPC_SNAP);
	if (r < 0) {
        	pr_err("Failed to send snapshot request for guest %s\n", name);
        	return r;
    	}

	n = read_in_full(sock, &status, sizeof(status));

    	if (n != sizeof(status)) {
        	
		int err = n < 0 ? errno : EIO;
		pr_err("Failed to receive snapshot status for guest %s\n", name);
        	return -err;
    	}
	
	if (status < 0) {
        	pr_err("Snapshot failed for guest %s (error %d)\n", name, status);
        	return status;
    	}
	
	printf("Guest %s snapshotted\n", name);

	return 0;
}

int kvm_cmd_snap(int argc, const char **argv, const char *prefix)
{	

        int instance;
	int r;

	parse_snap_options(argc, argv);

	if (instance_name == NULL)
		kvm_snap_help();

	instance = kvm__get_sock_by_instance(instance_name);

	if (instance <= 0)
		die("Failed locating instance");

	r = do_snap(instance_name, instance);

	close(instance);

	return r;
}

