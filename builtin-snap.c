#include <errno.h>
#include <limits.h>
#include "kvm/read-write.h"
#include "kvm/builtin-snap.h"
#include "kvm/registers.h"
#include <kvm/kvm.h>
#include <kvm/parse-options.h>
#include <kvm/kvm-ipc.h>
#include <kvm/util.h>
#include <kvm/kvm-cmd.h>

static const char *instance_name;
static const char *snapshot_directory = "Snapshot";
static const char *snapshot_name = "snapshot";

static const char * const snap_usage[] = {
	"lkvm snap [-n name] [-d directory] [-s snapshot-name] ",
	NULL
};

static const struct option snap_options[] = {
	OPT_GROUP("General options:"),
	OPT_STRING('n', "name", &instance_name, "name", "Instance name"),
	OPT_STRING('d', "directory", &snapshot_directory,
               "directory", "Snapshot output directory (default: Snapshot)"),
    	OPT_STRING('s', "snapshot-name", &snapshot_name,
               "name", "Snapshot name without .bin (default: snapshot)"),
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

static int build_snapshot_path(char *path, size_t path_size,
                               const char *directory,
                               const char *name)
{
    	size_t name_len;
    	size_t dir_len;
    	int n;

    	if (!path || !path_size ||
        	!directory || !*directory ||
        	!name || !*name)
        	return -EINVAL;

    	/* Il nome deve essere un basename, non un percorso. */
    	if (strchr(name, '/'))
        	return -EINVAL;

    	name_len = strlen(name);
    	if (name_len >= sizeof(".bin") - 1 &&
        	strcmp(name + name_len - (sizeof(".bin") - 1), ".bin") == 0)
        	return -EINVAL;

    	dir_len = strlen(directory);

    	n = snprintf(path, path_size, "%s%s%s.bin",
                 	directory,
                 	directory[dir_len - 1] == '/' ? "" : "/",
                 	name);

    	if (n < 0)
        	return -EIO;

    	if ((size_t)n >= path_size)
        	return -ENAMETOOLONG;

    	return 0;
}

static int do_snap(const char *name, int sock, const char *snapshot_path)
{
	int status;
	int r;
	ssize_t n;

	size_t path_len = strlen(snapshot_path) + 1;

	if (path_len > PATH_MAX)
    		return -ENAMETOOLONG;

	r = kvm_ipc__send_msg(sock, KVM_IPC_SNAP,
                      	(u32)path_len,
                      	(u8 *)snapshot_path);

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
    	char snapshot_path[PATH_MAX];

    	parse_snap_options(argc, argv);

    	if (instance_name == NULL)
        	kvm_snap_help();

    	r = build_snapshot_path(snapshot_path, sizeof(snapshot_path),
                            snapshot_directory, snapshot_name);
    	if (r < 0)
        	die("Invalid snapshot path: %s", strerror(-r));

    	instance = kvm__get_sock_by_instance(instance_name);

    	if (instance <= 0)
        	die("Failed locating instance");

    	r = do_snap(instance_name, instance, snapshot_path);

    	close(instance);

    	return r;
}

