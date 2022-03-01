/*
 * Chainload from LinuxBoot into another UEFI executable.
 *
 * This uses the kexec_load system call to allow arbitrary files and
 * segments to be passed into the new kernel image.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/kexec.h>
#include <linux/reboot.h>
#include <getopt.h>
#include "chainload.h"
#include "resume.h"

/*
 * The kexec_load() sysmtel call is not in any header, so we must
 * create a fake entry for it.

           struct kexec_segment {
               void   *buf;        // Buffer in user space
               size_t  bufsz;      // Buffer length in user space
               void   *mem;        // Physical address of kernel
               size_t  memsz;      // Physical address length
           };
*/

long kexec_load(
	unsigned long entry,
	unsigned long nr_segments,
	struct kexec_segment *segments,
	unsigned long flags
)
{
	return syscall(__NR_kexec_load, entry, nr_segments, segments, flags);
}


/*
 * Import the pre-compiled purgatory that will hand off control
 * via the UEFI boot services LoadImageProtocol.
 */

#ifndef CHAINLOAD_BIN
#error "CHAINLOAD_BIN is not defined"
#endif

asm(
".globl _purgatory;"
"_purgatory:\n"
".incbin \"" CHAINLOAD_BIN "\";"
".globl _purgatory_end;"
"_purgatory_end:\n"
);
extern const char _purgatory, _purgatory_end;


// TODO: where is this defined?
#define PAGESIZE (4096uL)
#define PAGE_ROUND(x) (((x) + PAGESIZE - 1) & ~(PAGESIZE-1))

static int verbose = 0;

static void * read_file(const char * filename, size_t * size_out)
{
	const int fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto fail_open;
	
	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0)
		goto fail_stat;

	size_t size = statbuf.st_size;

	if (size_out && *size_out != 0)
	{
		// limit the size to the provided size
		// or if the file is a device (such as /dev/mem)
		if (size > *size_out || (statbuf.st_mode & S_IFCHR) != 0)
			size = *size_out;
	}

	if (verbose)
		fprintf(stderr, "%s: %zu bytes\n", filename, size);

	uint8_t * buf = malloc(size);
	if (!buf)
		goto fail_malloc;

	size_t off = 0;
	while(off < size)
	{
		ssize_t rc = read(fd, buf + off, size - off);
		if (rc < 0)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;
			goto fail_read;
		}

		off += rc;
	}
	

	if (size_out)
		*size_out = size;

	return buf;

fail_read:
	free(buf);
fail_malloc:
fail_stat:
	close(fd);
fail_open:
	return NULL;
}


static const char usage[] =
"-h | --help                This help\n"
"-v | --verbose             Print debug info\n"
//"-f | --filesystem N        File system number for image devicepath\n"
"-r | --no-reboot           Do not execute final kexec call\n"
"-p | --purgatory file.bin  Binary file to pass through for chainloading\n"
"-c | --context file.bin    UEFI context (defaults to /dev/mem)\n"
"\n"
"";

static const struct option options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "verbose", no_argument, 0, 'v' },
	{ "no-reboot", no_argument, 0, 'r' },
	{ "filesystem", required_argument, 0, 'f' },
	{ "purgatory", required_argument, 0, 'p' },
	{ "context", required_argument, 0, 'c' },
	{ 0, 0, 0, 0},
};


int main(int argc, char ** argv)
{
	unsigned long flags = KEXEC_ARCH_DEFAULT;
	struct kexec_segment segments[KEXEC_SEGMENT_MAX];

	int do_not_reboot = 0;
	const char * context_file = "/dev/mem";
	const char * purgatory_file = NULL;
	const char * filesystem_str = NULL;

	opterr = 1;
	optind = 1;

	while (1)
	{
		const int opt = getopt_long(argc, argv, "h?f:p:c:vr", options, 0);
		if (opt < 0)
			break;

		switch(opt) {
		case 'f':
			filesystem_str = optarg;
			break;
		case 'p':
			purgatory_file = optarg;
			break;
		case 'c':
			context_file = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'r':
			do_not_reboot = 1;
			break;
		case '?': case 'h':
			printf("%s", usage);
			return EXIT_FAILURE;
		default:
			fprintf(stderr, "%s", usage);
			return EXIT_FAILURE;
		}
	}

	if (argc - optind < 1)
	{
		fprintf(stderr, "Missing EFI Executable to chainload!\n");
		return EXIT_FAILURE;
	}

	size_t exe_size = 0; // unlimited
	const char * exe_file = argv[optind];
	const void * exe_image = read_file(exe_file, &exe_size);
	if (exe_image == NULL)
	{
		perror(exe_file);
		return EXIT_FAILURE;
	}

	size_t context_size = PAGESIZE;
	const void * context_image = read_file(context_file, &context_size);
	if (context_image == NULL)
	{
		perror(context_file);
		return EXIT_FAILURE;
	}

	const void * purgatory_image = &_purgatory;
	size_t purgatory_size = &_purgatory_end - &_purgatory;

	if (purgatory_file)
	{
		// allow a larger puragtory
		purgatory_size = 65536;
		purgatory_image = read_file(purgatory_file, &purgatory_size);
		if (!purgatory_image)
		{
			perror(purgatory_file);
			return EXIT_FAILURE;
		}
	}

	int num_segments = 0;
	uint64_t entry_point = 0x40000000;
	uint64_t exe_addr = CHAINLOAD_IMAGE_ADDR;

	const uefi_context_t * context = (const void*)(UEFI_CONTEXT_OFFSET + (const uint8_t*) context_image);
	if (context->magic != UEFI_CONTEXT_MAGIC)
		fprintf(stderr, "WARNING: context magic %016lx does not have expected magic %016lx\n", context->magic, UEFI_CONTEXT_MAGIC);
	if (verbose)
		printf("context: CR3=%p gST=%p\n", (void*) context->cr3, (void*) context->system_table);
	
	// should we poke filesystem into the context?
	chainload_args_t chainload_args = {
		.magic		= CHAINLOAD_ARGS_MAGIC,
		.image_addr	= exe_addr,
		.image_size	= exe_size,
		.boot_device	= atoi(filesystem_str),
	};

	segments[num_segments++] = (struct kexec_segment){
		.buf	= context_image,
		.bufsz	= context_size,
		.mem	= (const void*) 0x00000000,
		.memsz	= PAGE_ROUND(context_size),
	};

	segments[num_segments++] = (struct kexec_segment){
		.buf	= purgatory_image,
		.bufsz	= purgatory_size,
		.mem	= (const void*) entry_point,
		.memsz	= PAGE_ROUND(purgatory_size),
	};

	segments[num_segments++] = (struct kexec_segment){
		.buf	= exe_image,
		.bufsz	= exe_size,
		.mem	= (const void*) exe_addr,
		.memsz	= PAGE_ROUND(exe_size),
	};

	segments[num_segments++] = (struct kexec_segment){
		.buf	= &chainload_args,
		.bufsz	= sizeof(chainload_args),
		.mem	= (const void *) CHAINLOAD_ARGS_ADDR,
		.memsz	= PAGE_ROUND(sizeof(chainload_args)),
	};

	if(verbose)
	for(int i = 0 ; i < num_segments ; i++)
	{
		const struct kexec_segment * seg = &segments[i];
		printf("%d: %p + %zu => %p + %zu\n",
			i,
			(const void*) seg->buf,
			(size_t) seg->bufsz,
			(const void*) seg->mem,
			(size_t) seg->memsz
		);
	}


	int rc = kexec_load(entry_point, num_segments, segments, flags);
	if (rc < 0)
	{
		perror("kexec_load");
		return EXIT_FAILURE;
	}

	if (do_not_reboot)
		return EXIT_SUCCESS;

	if (verbose)
		fprintf(stderr, "kexec-load: rebooting\n");

	rc = reboot(LINUX_REBOOT_CMD_KEXEC);
	if (rc < 0)
	{
		perror("reboot");
		return EXIT_FAILURE;
	}

	printf("kexec-load: we shouldn't be here...\n");
	return EXIT_SUCCESS; // ???
}
