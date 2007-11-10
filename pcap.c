

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

static struct {
	char *volatile writeptr;
	char *volatile writeend;
	volatile int overflow;
	pcaprec_hdr_t rechdr;
} workspace;

static char *databuffer1;
static char *databuffer2;
static int databuffersize;
static FILE *file;


_kernel_oserror *callevery_handler(_kernel_swi_regs *r, void *pw)
{
	if ((workspace.writeptr == databuffer1) || (workspace.writeptr == databuffer2)) {
		// No new data
		return NULL;
	}
//	semiprint("New data");
	_swix(OS_AddCallBack, _INR(0, 1), callback, pw);
	workspace.rechdr.ts_sec += 1;
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
	if (end > databuffer2) {
		start = databuffer2;
		workspace.writeptr = databuffer1;
		workspace.writeend = databuffer2;
	} else {
		start = databuffer1;
		workspace.writeptr = databuffer2;
		workspace.writeend = databuffer2 + databuffersize/2;
	}
	_swix(OS_IntOn,0);
	if (file) {
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

	_swix(OS_IntOff,0);
	claimswi(&workspace);
	_swix(OS_IntOn,0);

	_swix(OS_CallEvery, _INR(0,2), 1, callevery, private_word);

	return 0;
}


int swilist[20];
int numswis = 0;

void service(int service_number,_kernel_swi_regs * r,void * pw)
{
	switch(service_number) {
	case 0x9D: // Service_DCIDriverStatus
		if (r->r[2]) {
			// remove from list;
		} else {
			if (numswis >= 20) break;
			{char buf[256]; sprintf(buf, "service call %x",*((int *)(r->r[0])));semiprint(buf);}
			swilist[numswis++] = *((int *)(r->r[0])); //first word of dib
		}
		break;
	}
}


