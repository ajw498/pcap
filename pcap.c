

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <swis.h>
#include <kernel.h>

#include "pcapmod.h"

void semiprint(char *msg);

static _kernel_oserror ErrNoMem = {0x0, "Out of memory"};

_kernel_oserror *claimswi(void *wkspc);
_kernel_oserror *releaseswi(void);


typedef struct pcap_hsr_s {
	unsigned magic_number;
	unsigned version_major:16;
	unsigned version_minor:16;
	int thiszone;
	unsigned sigfigs;
	unsigned snaplen;
	unsigned network;
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
	unsigned ts_sec;
	unsigned ts_usec;
	unsigned incl_len;
	unsigned orig_len;
} pcaprec_hdr_t;

struct dib {
	unsigned dib_swibase;
	char *dib_name;
	unsigned dib_unit;
	unsigned char *dib_address;
	char *dib_module;
	char *dib_location;
	unsigned dib_slot;
	unsigned dib_inquire;
};

struct drivers {
	struct dib *dib;
	struct drivers *next;
};

#define MAXCLAIMS 20

static struct {
	char *volatile writeptr;
	char *volatile writeend;
	volatile int overflow;
	pcaprec_hdr_t rechdr;
	struct drivers *drivers;
	void *oldswihandler;
	int capturing;
	int numrxclaims;
	void *rxclaims[2*MAXCLAIMS];
} workspace;

static char *databuffer1;
static char *databuffer2;
static int databuffersize;
static int writebuffer;
static FILE *file;


_kernel_oserror *callevery_handler(_kernel_swi_regs *r, void *pw)
{
	workspace.rechdr.ts_sec += 1;
	if (workspace.writeptr == ((writebuffer == 1) ? databuffer1 : databuffer2)) {
		// No new data
		return NULL;
	}
//	semiprint("New data");
	_swix(OS_AddCallBack, _INR(0, 1), callback, pw);
	return NULL;
}

_kernel_oserror *callback_handler(_kernel_swi_regs *r, void *pw)
{
	char *end;
	char *start;
	static volatile int sema = 0;
	_swix(OS_IntOff,0);
	if (sema) {
		_swix(OS_IntOn,0);
		return NULL;
	}
	sema = 1;

	end = workspace.writeptr;
	if (writebuffer == 1) {
		start = databuffer1;
		workspace.writeptr = databuffer2;
		workspace.writeend = databuffer2 + databuffersize/2;
		writebuffer = 2;
	} else {
		start = databuffer2;
		workspace.writeptr = databuffer1;
		workspace.writeend = databuffer1 + databuffersize/2;
		writebuffer = 1;
	}
	if (workspace.overflow) {
		semiprint("Overflow detected");
		end = start; /* Drop whole packets */
	}
	_swix(OS_IntOn,0);
	if (file) {
		if (end < start) {
			semiprint("end < start!");
		} else if (end - start > 4096) {
			char buf[256];
			sprintf(buf, "outputing large data %d bytes",end - start);
			semiprint(buf);
		}
		fwrite(start, 1, end - start, file);
	} else {
		semiprint("File not open");
	}
	sema = 0;
	return NULL;
}

_kernel_oserror *finalise(int fatal, int podule, void *private_word)
{
	_swix(OS_RemoveTickerEvent, _INR(0,1), callevery, private_word);
	if (file) fclose(file);
	return releaseswi();
}

_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	(void)cmd_tail;
	(void)podule_base;

	databuffersize = 512*1024;
	databuffer1 = malloc(databuffersize);
	if (databuffer1 == NULL) return &ErrNoMem;
//	{char buf[256]; sprintf(buf, "databuffer addr %p",&workspace.databuffer);semiprint(buf);}
	databuffer2 = databuffer1 + databuffersize/2;
	workspace.writeptr = databuffer1;
	workspace.writeend = databuffer2;
	writebuffer = 1;

	remove("@.data/pcap");
	file = fopen("@.data/pcap","wb");
	if (file == NULL) {
		semiprint("File open failed");
	} else {
		pcap_hdr_t hdr;
		hdr.magic_number = 0xa1b2c3d4;
		hdr.version_major = 2;
		hdr.version_minor = 4;
		hdr.thiszone = 0;
		hdr.sigfigs = 0;
		hdr.snaplen = databuffersize/2;
		hdr.network = 1;
		fwrite(&hdr, sizeof(hdr), 1, file);
	}

	workspace.rechdr.ts_sec = 0;
	workspace.rechdr.ts_usec = 0;
	workspace.drivers = NULL;
	workspace.numrxclaims = 0;
	workspace.capturing = 1;

	_swix(OS_IntOff,0);
	claimswi(&workspace);
	_swix(OS_IntOn,0);

	_swix(OS_CallEvery, _INR(0,2), 1, callevery, private_word);

	return 0;
}

/* Called only for Service_DCIDriverStatus */
void service(int service_number,_kernel_swi_regs * r,void * pw)
{
	if (r->r[2]) {
		/* Driver terminating, remove from list */
		struct drivers *driver = workspace.drivers;
		struct drivers **prev = &(workspace.drivers);
		while (driver) {
			if (driver->dib == (struct dib *)r->r[0]) {
				*prev = driver->next;
				free(driver);
				break;
			}
			prev = &(driver->next);
			driver = driver->next;
		}
	} else {
		struct drivers *driver = malloc(sizeof(struct drivers));
		if (driver == NULL) return; /* Not much else we can do */

		driver->dib = (struct dib *)r->r[0];
		driver->next = workspace.drivers;
		workspace.drivers = driver;
	}
}


