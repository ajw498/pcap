

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <swis.h>
#include <kernel.h>
#include <time.h>


#include "wiresalmonmod.h"

void semiprint(char *msg);

static _kernel_oserror error_nomem = {0x58C80, "Out of memory"};
static _kernel_oserror error_overflow = {0x58C81, "Buffer overflowed while capturing, some packets may have been dropped"};

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
	struct drivers *next;
	struct dib *dib;
};

struct module {
	int start;
	int init;
	int final;
	int service;
	int title;
	int help;
	int command;
	int swibase;
};

#define MAXCLAIMS 20

static struct {
	char *volatile writeptr;
	char *volatile writeend;
	volatile int overflow;
	pcaprec_hdr_t rechdr;
	struct drivers *drivers;
	void *oldswihandler;
	FILE *capturing;
	int numrxclaims;
	void *rxclaims[2*MAXCLAIMS];
} workspace;

static char *databuffer1 = NULL;
static char *databuffer2;
static int databuffersize;
static int writebuffer;

#define MAX_DRIVERS 10

/* rmreinit all DCI4 driver modules, to ensure that we can insert our hook into the rx routines */
_kernel_oserror *reinit_drivers(void)
{
	int swibases[MAX_DRIVERS];
	int numswis = 0;
	_kernel_oserror *err;
	struct drivers *chain;
	int i;

	/* Create a list of the SWI bases of all driver modules */
	err = _swix(OS_ServiceCall, _INR(0,1) | _OUT(0), 0, 0x9B, &chain);
	if (err) return err;
	while (chain) {
		struct drivers *next = chain->next;
		if (numswis < MAX_DRIVERS) {
			/* Only add if not already in the list */
			for (i = 0; i < numswis; i++) {
				if (chain->dib->dib_swibase == swibases[i]) break;
			}
			if (i == numswis) {
				swibases[numswis++] = chain->dib->dib_swibase;
			}
		}
		free(chain);
		chain = next;
	}

	/* Search through module list to find modules with matching SWI bases */
	for (i = 0; i < numswis; i++) {
		int j = 0;
		struct module *module;
		while (_swix(OS_Module, _INR(0,2) | _OUT(3), 12, j, 0, &module) == NULL) {
			if (module->swibase == swibases[i]) {
				/* Found match, so reinitialise it */
				char *title = (char *)module + module->title;
				err = _swix(OS_Module, _INR(0,1), 3, title);
				if (err) return err;
				break;
			}
			j++;
		}
	}
	return NULL;
}

_kernel_oserror *callevery_handler(_kernel_swi_regs *r, void *pw)
{
	/* Increment timestamp */
	workspace.rechdr.ts_usec += 20000;
	if (workspace.rechdr.ts_usec > 1000000) {
		workspace.rechdr.ts_usec -= 1000000;
		workspace.rechdr.ts_sec += 1;
	}

	if (workspace.writeptr == ((writebuffer == 1) ? databuffer1 : databuffer2)) {
		// No new data
		return NULL;
	}

	_swix(OS_AddCallBack, _INR(0, 1), callback, pw);
	return NULL;
}

_kernel_oserror *callback_handler(_kernel_swi_regs *r, void *pw)
{
	char *end;
	char *start;
	static volatile int sema = 0;

	/* Turn inturrupts off while switching buffers, to ensure the
	   SWI handler sees a consistant view */
	_swix(OS_IntOff,0);
	if (sema) {
		/* Prevent reentrancy */
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
		/* Drop whole packets */
		end = start; 
	}
	/* Reenable interrupts before doing I/O */
	_swix(OS_IntOn,0);

	/* Write data to file */
	if (workspace.capturing) {
		fwrite(start, 1, end - start, workspace.capturing);
	}
	sema = 0;

	return NULL;
}

static _kernel_oserror *stop_capture(void)
{
	FILE *file = workspace.capturing;

	/* Ensure capturing has stopped before closing the file */
	workspace.capturing = NULL;
	if (file) fclose(file);

	if (databuffer1) free(databuffer1);
	databuffer1 = NULL;

	if (workspace.overflow) {
		return &error_overflow;
	}
	return NULL;
}

static _kernel_oserror *start_capture(char *filename, int bufsize)
{
	FILE *file;
	pcap_hdr_t hdr;

	stop_capture();

	if (bufsize == 0) bufsize = 256*1024;
	databuffersize = bufsize;
	databuffer1 = malloc(databuffersize);
	if (databuffer1 == NULL) return &error_nomem;
	databuffer2 = databuffer1 + databuffersize/2;
	workspace.writeptr = databuffer1;
	workspace.writeend = databuffer2;
	writebuffer = 1;

	workspace.overflow = 0;

	file = fopen(filename,"wb");
	if (file == NULL) {
		return _kernel_last_oserror();
	}

	/* Write the pcap file header */
	hdr.magic_number = 0xa1b2c3d4;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.thiszone = 0;
	hdr.sigfigs = 0;
	hdr.snaplen = databuffersize/2;
	hdr.network = 1;
	fwrite(&hdr, sizeof(hdr), 1, file);

	/* Enable capturing to start */
	workspace.capturing = file;

	return NULL;
}

_kernel_oserror *finalise(int fatal, int podule, void *private_word)
{
	stop_capture();

	_swix(OS_RemoveTickerEvent, _INR(0,1), callevery, private_word);
	_swix(OS_RemoveTickerEvent, _INR(0,1), callback, private_word);

	return releaseswi();
}

_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	_kernel_oserror *err;

	(void)cmd_tail;
	(void)podule_base;

	workspace.rechdr.ts_sec = time(NULL);
	workspace.rechdr.ts_usec = 0;
	workspace.drivers = NULL;
	workspace.numrxclaims = 0;
	workspace.capturing = NULL;

	_swix(OS_IntOff,0);
	err = claimswi(&workspace);
	_swix(OS_IntOn,0);

	if (err) return err;

	err = _swix(OS_CallEvery, _INR(0,2), 1, callevery, private_word);
	if (err) {
		releaseswi();
		return err;
	}

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

_kernel_oserror *swi(int swi_no, _kernel_swi_regs *r, void *private_word)
{
	switch (swi_no) {
	case 0: return reinit_drivers();
	case 1: return start_capture((char *)r->r[0], r->r[1]);
	case 2: return stop_capture();
	default: return error_BAD_SWI;
	}
	return NULL;
}

