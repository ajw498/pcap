

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

void swihandler(void);

extern int oldswihandler;

_kernel_oserror *claimswi(void *wkspc);
_kernel_oserror *releaseswi(void);

static struct {
	char *volatile writeptr;
	char *volatile writeend;
	volatile int overflow;
} workspace;

static char *databuffer1;
static char *databuffer2;
static int databuffersize;


_kernel_oserror *callevery_handler(_kernel_swi_regs *r, void *pw)
{
	if ((workspace.writeptr == databuffer1) || (workspace.writeptr == databuffer2)) {
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
	FILE *file;
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
	file = fopen("@.data/pcap","ab");
	if (file) {
		fwrite(start, 1, end - start, file);
		fclose(file); 
	} else {
		semiprint("File open failed");
	}
	sema = 0;
	return NULL;
}

_kernel_oserror *finalise(int fatal, int podule, void *private_word)
{
	_swix(OS_RemoveTickerEvent, _INR(0,1), callevery, private_word);
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

	_swix(OS_IntOff,0);
	claimswi(&workspace);
	{char buf[256]; sprintf(buf, "new handler %p old %x",swihandler,oldswihandler);semiprint(buf);}
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


