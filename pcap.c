

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

_kernel_oserror *finalise(int fatal, int podule, void *private_word)
{
	return releaseswi();
}

static struct {
	char *databuffer;
	int databuffersize;
	int writeoffset;
	int readoffset;
	int overflow;
} workspace;

_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	(void)cmd_tail;
	(void)podule_base;

	workspace.databuffersize = 512*1024;
	workspace.databuffer = malloc(workspace.databuffersize);
	if (workspace.databuffer == NULL) return &ErrNoMem;
	{char buf[256]; sprintf(buf, "databuffer addr %p",&workspace.databuffer);semiprint(buf);}

	_swix(OS_IntOff,0);
	claimswi(&workspace);
	{char buf[256]; sprintf(buf, "new handler %p old %x",swihandler,oldswihandler);semiprint(buf);}
	_swix(OS_IntOn,0);
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


