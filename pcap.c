

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

void claimswi(void);

_kernel_oserror *finalise(int fatal, int podule, void *private_word)
{
	return _swix(OS_ClaimProcessorVector, _INR(0,2), 0x002, oldswihandler, swihandler);
}

char *databuffer;
int databuffersize = 512*1024;
int writeoffset = 0;
int readoffset = 0;
int overflow = 0;

_kernel_oserror *initialise(const char *cmd_tail, int podule_base, void *private_word)
{
	(void)cmd_tail;
	(void)podule_base;

	databuffer = malloc(databuffersize);
	if (databuffer == NULL) return &ErrNoMem;

	_swix(OS_IntOff,0);
	claimswi();
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


