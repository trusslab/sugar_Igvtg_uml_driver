/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <as-layout.h>
#include <init.h>
#include <kern_util.h>
#include <os.h>
#include <um_malloc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include "../isol_prints.h"

char *gpu_dev_file_name = INTEL_GPU_DEV_FILE_NAME;

int i915_user_init(void);
int vgt_isol_init(void);

#define BUG abort

#define PGD_BOUND (4 * 1024 * 1024)
#define STACKSIZE (8 * 1024 * 1024)
#define THREAD_NAME_LEN (256)

long elf_aux_hwcap;

static void set_stklim(void)
{
	struct rlimit lim;

	if (getrlimit(RLIMIT_STACK, &lim) < 0) {
		perror("getrlimit");
		exit(1);
	}
	if ((lim.rlim_cur == RLIM_INFINITY) || (lim.rlim_cur > STACKSIZE)) {
		lim.rlim_cur = STACKSIZE;
		if (setrlimit(RLIMIT_STACK, &lim) < 0) {
			perror("setrlimit");
			exit(1);
		}
	}
}

static __init void do_uml_initcalls(void)
{
	initcall_t *call;

	call = &__uml_initcall_start;
	while (call < &__uml_initcall_end) {
		(*call)();
		call++;
	}
}

static void last_ditch_exit(int sig)
{
	uml_cleanup();
	exit(1);
}

static void install_fatal_handler(int sig)
{
	struct sigaction action;

	/* All signals are enabled in this handler ... */
	sigemptyset(&action.sa_mask);

	/*
	 * ... including the signal being handled, plus we want the
	 * handler reset to the default behavior, so that if an exit
	 * handler is hanging for some reason, the UML will just die
	 * after this signal is sent a second time.
	 */
	action.sa_flags = SA_RESETHAND | SA_NODEFER;
	action.sa_restorer = NULL;
	action.sa_handler = last_ditch_exit;
}

#define UML_LIB_PATH	":" OS_LIB_PATH "/uml"

static void setup_env_path(void)
{
	char *new_path = NULL;
	char *old_path = NULL;
	int path_len = 0;

	old_path = getenv("PATH");
	/*
	 * if no PATH variable is set or it has an empty value
	 * just use the default + /usr/lib/uml
	 */
	if (!old_path || (path_len = strlen(old_path)) == 0) {
		if (putenv("PATH=:/bin:/usr/bin/" UML_LIB_PATH))
			perror("couldn't putenv");
		return;
	}

	/* append /usr/lib/uml to the existing path */
	path_len += strlen("PATH=" UML_LIB_PATH) + 1;
	new_path = malloc(path_len);
	if (!new_path) {
		perror("couldn't malloc to set a new PATH");
		return;
	}
	snprintf(new_path, path_len, "PATH=%s" UML_LIB_PATH, old_path);
	if (putenv(new_path)) {
		perror("couldn't putenv to set a new PATH");
		free(new_path);
	}
}

extern void scan_elf_aux( char **envp);


void wait_for_interrupt_finish(void)
{
	/* condition variable implementation */
	/* semaphore implementation */
	/* Waits for a signal (our interrupt) to arrive and be handled */
	usleep(10000);

}

double get_time(void)
{
	struct timeval t;
	double time;

	gettimeofday(&t, NULL);

	/* return in usec */
	/* return in sec */
	time = t.tv_sec + t.tv_usec / 1000000.0;

	return time;
}

double last_t_samp = 0;
void isol_print_time(void)
{
	struct timeval t;
	double t_samp;

	gettimeofday(&t, NULL);

	/* return in usec */
	/* return in sec */
	t_samp = t.tv_sec * 1000000.0 + t.tv_usec;
	last_t_samp = t_samp;
}

void isol_print_time2(void)
{
	struct timeval t;
	double t_samp;

	gettimeofday(&t, NULL);

	/* return in usec */
	/* return in sec */
	t_samp = t.tv_sec * 1000000.0 + t.tv_usec;
	last_t_samp = t_samp;
}

int um_printf(const char *fmt)
{
	return PRINTK_STUB(fmt);
}

void um_clflush_virt_range(void *addr, unsigned long length)
{
	const char *path = "/proc/sys/vm/drop_caches";
	FILE *vgt_file;
	int err = 0;
	
	if ((vgt_file = fopen(path, "w")) == NULL) {
	    err = errno;
	}
	/* The format of the string is:
	 * domid,aperture_size,gm_size,fence_size. This means we want the vgt
	 * driver to create a vgt instanc for Domain domid with the required
	 * parameters. NOTE: aperture_size and gm_size are in MB.
	 */
	if (!err && fprintf(vgt_file, "3") < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;
	
	if (err) {
	    exit(-1);
	}
}

/* The vgt code is adopted and modified from qemu/hw/display/vga-vgt.c */
/* vgt start */

/* These are the default values */
static int vgt_low_gm_sz = 64; /* in MB */
static int vgt_high_gm_sz = 448; /* in MB */
static int vgt_fence_sz = 4;
static int vgt_primary = 1; /* -1 means "not specified */
static int vgt_cap = 0;
static int vgt_is_local = 1; /* 1 means that driver should assign vGPU to OS process not VM. Use 0 for VM */
static const char *vgt_monitor_config_file;

#define VGT_OPREGION_SIZE 0x2000 /* two pages for opregion */
int vgt_domid;
void *vgt_local_opregion_va;
void *vgt_local_mmio;
unsigned long PCI_CONFIG_BASE_ADDR = 0;

/*
 *  Inform vGT driver to create a vGT instance
 */
static void create_vgt_instance(void)
{
	const char *path = "/sys/kernel/vgt/control/create_vgt_instance";
	FILE *vgt_file;
	int err = 0;
	
	/* get a resonable domid under either xen or kvm */
	vgt_domid = (int) getpid();
	
	if (vgt_low_gm_sz <= 0 || vgt_high_gm_sz <=0 ||
		vgt_cap < 0 || vgt_cap > 100 ||
	    	vgt_primary < -1 || vgt_primary > 1 ||
	    vgt_fence_sz <=0) {
	    abort();
	}

	/* FIXME: we're allocating one more page at the end of opregion to use for PCI_CONFIG space.
	 * We should, however, use a separate address for that and we don't really need to allocate
	 * memory for it since we will immediately remove the permissions for that page anyway. */
	if (posix_memalign(&vgt_local_opregion_va, 0x1000, VGT_OPREGION_SIZE + 0x1000)) {
		PRINTF_ERR("Error: posix_memalign failed.\n");
		return;
	}

	PCI_CONFIG_BASE_ADDR = vgt_local_opregion_va + 0x2000;
	
	if ((vgt_file = fopen(path, "w")) == NULL) {
	    err = errno;
	    PRINTF_ERR("vGT: open %s failed\n", path);
	}
	/* The format of the string is:
	 * domid,aperture_size,gm_size,fence_size. This means we want the vgt
	 * driver to create a vgt instanc for Domain domid with the required
	 * parameters. NOTE: aperture_size and gm_size are in MB.
	 */
	if (!err && fprintf(vgt_file, "%d,%u,%u,%u,%d,%d,%d,%lx\n", vgt_domid,
	    vgt_low_gm_sz, vgt_high_gm_sz, vgt_fence_sz, vgt_primary, vgt_cap, vgt_is_local, vgt_local_opregion_va) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;
	
	if (err) {
	    PRINTF_ERR("vGT: failed: errno=%d\n", err);
	    exit(-1);
	}
	
}

/*
 *  Inform vGT driver to close a vGT instance
 */
void destroy_vgt_instance(void)
{
    const char *path = "/sys/kernel/vgt/control/create_vgt_instance";
    FILE *vgt_file, *shell_output;
    int err = 0;
    int tmp, fast_switch = 0;

    if ((vgt_file = fopen(path, "w")) == NULL) {
        PRINTF_ERR("vGT: open %s failed\n", path);
        err = errno;
    }

    /* -domid means we want the vgt driver to free the vgt instance
     * of Domain domid.
     * */
    if (!err && fprintf(vgt_file, "%d\n", -vgt_domid) < 0)
        err = errno;

    if (!err && fclose(vgt_file) != 0)
        err = errno;

    if (fast_switch)
        system("echo 1 > /sys/kernel/vgt/control/display_switch_method");

    if (err) {
        exit(-1);
    }
}

/* vgt end */

int vgt_isol_pci_write_config(int size, int where, int val)
{
	const char *path = "/sys/kernel/vgt/control/pci_config";
	FILE *vgt_file;
	int err = 0;
	
	if ((vgt_file = fopen(path, "w")) == NULL) {
	    err = errno;
	}
	
	if (!err && fprintf(vgt_file, "1,%d,%d,%d\n", size, where, val) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;
	
	if (err) {
	}

	return err;
}

int vgt_isol_pci_read_config(int size, int where, int *val)
{
	const char *path = "/sys/kernel/vgt/control/pci_config";
	FILE *vgt_file;
	int err = 0;
	
	if ((vgt_file = fopen(path, "w")) == NULL) {
	    err = errno;
	}
	
	if (!err && fprintf(vgt_file, "0,%d,%d\n", size, where) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;

	if ((vgt_file = fopen(path, "r")) == NULL) {
	    err = errno;
	}
	
	if (!err && fscanf(vgt_file, "%d\n", val) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;

	if (err) {
	}
	
	return err;
}

int vgt_isol_write_mmio(int size, unsigned long where, unsigned long val)
{
	const char *path = "/sys/kernel/vgt/control/mmio";
	FILE *vgt_file;
	int err = 0;

	if ((vgt_file = fopen(path, "w")) == NULL) {
	    err = errno;
	}
	
	if (!err && fprintf(vgt_file, "1,%d,%d,%lu\n", size, (int) where, val) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;
	
	if (err) {
	}

	return err;
}

int vgt_isol_read_mmio(int size, unsigned long where, unsigned long *val)
{
	const char *path = "/sys/kernel/vgt/control/mmio";
	FILE *vgt_file;
	int err = 0;
	
	if ((vgt_file = fopen(path, "w")) == NULL) {
	    err = errno;
	}
	
	if (!err && fprintf(vgt_file, "0,%d,%d\n", size, (int) where) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;
	
	if ((vgt_file = fopen(path, "r")) == NULL) {
	    err = errno;
	}
	
	if (!err && fscanf(vgt_file, "%lu\n", val) < 0)
	    err = errno;
	
	if (!err && fclose(vgt_file) != 0)
	    err = errno;
	
	if (err) {
	}
	
	return err;
}

int gisol_fd = -1;

void vgt_isol_handle_signal(void);

unsigned long signal_counter = 0;

void vgt_signal_handler(int sig, struct siginfo *unused_si, struct uml_pt_regs *regs)
{
	signal_counter++;

	vgt_isol_handle_signal();
}

void vgt_terminate(int sig, struct siginfo *unused_si, struct uml_pt_regs *regs)
{

	destroy_vgt_instance();
}

int gisol_emulate_reg_stub(void *arg)
{
	int ret;

	if (gisol_fd < 0) {
		PRINTF_ERR("Error: Could not open file\n");
		return -1;
	}

	ret = ioctl(gisol_fd, 0xc00c6701, arg);

	return ret;
}

int gisol_i2c_transfer_stub(void *arg)
{
	int ret;

	if (gisol_fd < 0) {
		PRINTF_ERR("Error: Could not open file\n");
		return -1;
	}

	ret = ioctl(gisol_fd, 0xc0106704, arg);

	return ret;
}

int gisol_set_mode_stub(void *arg)
{
	int ret;

	if (gisol_fd < 0) {
		PRINTF_ERR("Error: Could not open file\n");
		return -1;
	}

	ret = ioctl(gisol_fd, 0x40146705, arg);

	return ret;
}

void test_func(void)
{
}

int lib_initialized = 0;
int isol_open_syscall(const char *filename, int flags, int mode, int fd);
int isol_ioctl_syscall(int fd, unsigned long cmd, void *arg);
void *isol_mmap_syscall(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
int isol_munmap_syscall(void *addr, size_t length);
size_t isol_read_syscall(int fd, void *buf, size_t nbyte);
int isol_poll_syscall(int fd, int timeout);

int g_flip_mode = 0;

void isol_flip_mode(int flip_mode)
{
	g_flip_mode = flip_mode;
}

int main_removed(int argc, char **argv, char **envp);
extern void isol_set_current(void); /* in arch/um/kernel/um_arch.c */

int isol_open(const char *pathname, int flags, mode_t mode)
{
	int ret;
	int size1, size2, compare_size;
	char *fake_argv = ".\linux";
	int fd;

	isol_set_current();	
	
	if (!lib_initialized) {
		
		main_removed(1, &fake_argv, NULL);
		
		lib_initialized = 1;
	}

	size1 = strlen(gpu_dev_file_name);
	size2 = strlen(pathname);
	compare_size = (size1 < size2) ? size1 : size2;
	
	if (strncmp(pathname, gpu_dev_file_name, compare_size)) {
		return -1;
	}

	/*
	 * This is just because we need a legit fd value.
	 * Alternatively, we can open a dummy file.
	 */
	fd = open(gpu_dev_file_name, O_RDWR);

	ret = isol_open_syscall(pathname, flags, (int) mode, fd);

	if (ret) {
		PRINTF_ERR("Error from isol_open_syscall\n", ret);	
		close(fd);
		return -1;
	}

	return fd;
}

int isol_ioctl(int fd, unsigned long cmd, void *arg)
{
	int ret;

	if (!lib_initialized) {
		PRINTF_ERR("Error: library not initialized.\n");
		return -1;
	}
	
	isol_set_current();

	ret = isol_ioctl_syscall(fd, cmd, arg);

	return ret;
}

void *isol_mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset)
{
	void *ret_addr;

	if (!lib_initialized) {
		PRINTF_ERR("Error: library not initialized.\n");
		return;
	}
	
	isol_set_current();

	ret_addr = isol_mmap_syscall(addr, length, prot, flags, fd, offset);

	return;
}

int isol_munmap(void *addr, size_t length)
{
	int ret;
	
	if (!lib_initialized) {
		PRINTF_ERR("Error: library not initialized.\n");
		return -1;
	}
	
	isol_set_current();
		
	ret = isol_munmap_syscall(addr, length);
	
	if (ret) {
		/* This is the correct expected return value */
		errno = ret;
		ret = -1;
	}		

	return ret;
}

size_t isol_read(int fd, void *buf, size_t nbyte)
{
	size_t ret;

	if (!lib_initialized) {
		PRINTF_ERR("Error: library not initialized.\n");
		return 0;
	}
	
	isol_set_current();

	ret = isol_read_syscall(fd, buf, nbyte);

	return ret;
}

int isol_poll(int fd, int timeout)
{
	int ret;

	if (!lib_initialized) {
		PRINTF_ERR("Error: library not initialized.\n");
		return 0;
	}
	
	isol_set_current();

	ret = isol_poll_syscall(fd, timeout);

	if (ret > 0)
		ret = 1;
	
	return ret;
}

int isol_close(int fd)
{
	destroy_vgt_instance();
	close(fd);
}

#include <time.h>
#include <poll.h>
void isol_delay(void)
{
	
	struct timespec t;
	
	t.tv_sec = 5;
	t.tv_nsec = 1000;
	
	nanosleep(&t, NULL);
	
}

void *poll_for_interrupts(void *arg)
{
	struct pollfd pfd;
	
	pfd.fd = gisol_fd;
	pfd.events = POLLIN;
	
	while (1) {
		poll(&pfd, 1, -1);
		BUG();
		
	}
}

unsigned long get_sbrk_val(void)
{
	return sbrk(0);
}

#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8

/*
 * Let any architecture override either of the following before
 * including this file.
 */

#ifndef _IOC_SIZEBITS
# define _IOC_SIZEBITS	14
#endif

#ifndef _IOC_DIRBITS
# define _IOC_DIRBITS	2
#endif

#define _IOC_NRMASK	((1 << _IOC_NRBITS)-1)
#define _IOC_TYPEMASK	((1 << _IOC_TYPEBITS)-1)
#define _IOC_SIZEMASK	((1 << _IOC_SIZEBITS)-1)
#define _IOC_DIRMASK	((1 << _IOC_DIRBITS)-1)

#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

#ifndef _IOC_WRITE
# define _IOC_WRITE	1U
#endif

#ifndef _IOC_READ
# define _IOC_READ	2U
#endif

#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))

#define _IOC_TYPECHECK(t) (sizeof(t))
#define _IOWR(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOW(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define DRM_IOCTL_BASE			'd'
#define DRM_COMMAND_BASE                0x40
#define DRM_I915_GEM_PINPAGES           0x37
#define DRM_I915_GEM_VGT_DMABUF         0x38
#define DRM_IOW(nr,type)		_IOW(DRM_IOCTL_BASE,nr,type)
#define DRM_IOWR(nr,type)		_IOWR(DRM_IOCTL_BASE,nr,type)

struct drm_i915_gem_pinpages {
	__u64 start_page; /* virtual page number */
	__u32 num_pages;
	__u8  pin;
#define I915_PINPAGES_PIN (1<<0)
#define I915_PINPAGES_UNPIN (1<<1)
};

struct drm_i915_gem_vgt_dmabuf {
	__u32 start_page; /* virtual page number */
	__u32 num_pages;
	__u32 flags;
	__u32 fd;
};

#define DRM_IOCTL_I915_GEM_PINPAGES		DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_PINPAGES, struct drm_i915_gem_pinpages)
#define DRM_IOCTL_I915_GEM_VGT_DMABUF		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_VGT_DMABUF, struct drm_i915_gem_vgt_dmabuf)

int ioctl_fd = -1;

int um_pin_memory(unsigned long start_page, unsigned long num_pages)
{
	int ret;
	struct drm_i915_gem_pinpages args;
	int i, *ptr;

	if (ioctl_fd == -1) {
		ioctl_fd = open(gpu_dev_file_name, O_RDWR);
		if (ioctl_fd <= 0)
			return -1;
	}

	/* fault into the addresses */

	args.start_page = (__u64) start_page;
	args.num_pages = (__u32) num_pages;
	args.pin = I915_PINPAGES_PIN;

	ret = ioctl(ioctl_fd, DRM_IOCTL_I915_GEM_PINPAGES, &args);
	return ret;
}

int um_unpin_memory(unsigned long start_page, unsigned long num_pages)
{
	int ret;
	struct drm_i915_gem_pinpages args;

	if (ioctl_fd <= 0)
		return -1;

	args.start_page = (__u64) start_page;
	args.num_pages = (__u32) num_pages;
	args.pin = I915_PINPAGES_UNPIN;

	ret = ioctl(ioctl_fd, DRM_IOCTL_I915_GEM_PINPAGES, &args);
	return ret;
}

int um_dma_buf_export(unsigned long start_page, unsigned long num_pages, int flags)
{
	int ret;
	struct drm_i915_gem_vgt_dmabuf args;

	if (ioctl_fd == -1) {
		ioctl_fd = open(gpu_dev_file_name, O_RDWR);
		if (ioctl_fd <= 0)
			return -1;
	}

	args.start_page = (__u32) start_page;
	args.num_pages = (__u32) num_pages;
	args.flags = (__u32) flags;

	ret = ioctl(ioctl_fd, DRM_IOCTL_I915_GEM_VGT_DMABUF, &args);
	if (ret < 0)
		return ret;

	return args.fd;
}

int main_removed(int argc, char **argv, char **envp)
{
	char **new_argv;
	int ret, i, err;

	set_stklim();

	setup_env_path();

	setsid();

	new_argv = malloc((argc + 1) * sizeof(char *));
	if (new_argv == NULL) {
		perror("Mallocing argv");
		exit(1);
	}
	for (i = 0; i < argc; i++) {
		new_argv[i] = strdup(argv[i]);
		if (new_argv[i] == NULL) {
			perror("Mallocing an arg");
			exit(1);
		}
	}
	new_argv[argc] = NULL;

	/*
	 * Allow these signals to bring down a UML if all other
	 * methods of control fail.
	 */
	install_fatal_handler(SIGINT);
	install_fatal_handler(SIGTERM);

#ifdef CONFIG_ARCH_REUSE_HOST_VSYSCALL_AREA
	scan_elf_aux(envp);
#endif

	do_uml_initcalls();

	create_vgt_instance();

	ret = linux_main(argc, argv);
	unblock_signals();

	i915_user_init();
	vgt_isol_init(); 
	
	/*
	pthread_t tid;
	int err2;
	
	err2 = pthread_create(&tid, NULL, &poll_for_interrupts, NULL);
        if (err2 != 0)
            fprintf(stderr, "thread creation failed:[%s]", strerror(err2));
        else
            fprintf(stderr, "thread created successfully\n");
	*/
	return 0;

	/*
	 * Disable SIGPROF - I have no idea why libc doesn't do this or turn
	 * off the profiling time, but UML dies with a SIGPROF just before
	 * exiting when profiling is active.
	 */
	change_sig(SIGPROF, 0);

	/*
	 * This signal stuff used to be in the reboot case.  However,
	 * sometimes a SIGVTALRM can come in when we're halting (reproducably
	 * when writing out gcov information, presumably because that takes
	 * some time) and cause a segfault.
	 */

	/* stop timers and set SIGVTALRM to be ignored */
	disable_timer();

	/* disable SIGIO for the fds and set SIGIO to be ignored */
	err = deactivate_all_fds();
	if (err)
		printf("deactivate_all_fds failed, errno = %d\n", -err);

	/*
	 * Let any pending signals fire now.  This ensures
	 * that they won't be delivered after the exec, when
	 * they are definitely not expected.
	 */
	unblock_signals();

	/* Reboot */
	if (ret) {
		printf("\n");
		execvp(new_argv[0], new_argv);
		perror("Failed to exec kernel");
		ret = 1;
	}
	printf("\n");
	return uml_exitcode;
}

extern void *__real_malloc(int);

void *__wrap_malloc(int size)
{
	void *ret;

	if (!kmalloc_ok)
		return __real_malloc(size);
	else if (size <= UM_KERN_PAGE_SIZE)
		/* finding contiguous pages can be hard*/
		ret = uml_kmalloc(size, UM_GFP_KERNEL);
	else ret = vmalloc(size);

	/*
	 * glibc people insist that if malloc fails, errno should be
	 * set by malloc as well. So we do.
	 */
	if (ret == NULL)
		errno = ENOMEM;

	return ret;
}

void *__wrap_calloc(int n, int size)
{
	void *ptr = __wrap_malloc(n * size);

	if (ptr == NULL)
		return NULL;
	memset(ptr, 0, n * size);
	return ptr;
}

extern void __real_free(void *);

extern unsigned long high_physmem;

void __wrap_free(void *ptr)
{
	unsigned long addr = (unsigned long) ptr;

	/*
	 * We need to know how the allocation happened, so it can be correctly
	 * freed.  This is done by seeing what region of memory the pointer is
	 * in -
	 * 	physical memory - kmalloc/kfree
	 *	kernel virtual memory - vmalloc/vfree
	 * 	anywhere else - malloc/free
	 * If kmalloc is not yet possible, then either high_physmem and/or
	 * end_vm are still 0 (as at startup), in which case we call free, or
	 * we have set them, but anyway addr has not been allocated from those
	 * areas. So, in both cases __real_free is called.
	 *
	 * CAN_KMALLOC is checked because it would be bad to free a buffer
	 * with kmalloc/vmalloc after they have been turned off during
	 * shutdown.
	 * XXX: However, we sometimes shutdown CAN_KMALLOC temporarily, so
	 * there is a possibility for memory leaks.
	 */

	if ((addr >= uml_physmem) && (addr < high_physmem)) {
		if (kmalloc_ok)
			kfree(ptr);
	}
	else if ((addr >= start_vm) && (addr < end_vm)) {
		if (kmalloc_ok)
			vfree(ptr);
	}
	else __real_free(ptr);
}

/* /dev/mem code modified from http://www.denx.de/wiki/bin/view/PPCEmbedded/DeviceDrivers#Section_Accessin */

void *um_malloc(size_t size)
{
	void *addr;
	int err;

	err = posix_memalign(&addr, 0x1000, size);

	if (err) {
		PRINTF_ERR("Error: could not allocate memory, err = -%d\n", err);
		return NULL;
	}

	return addr;
}

void um_free(const void *addr, size_t size)
{
	free(addr);
	/* from os_unmap_memory() */

}
