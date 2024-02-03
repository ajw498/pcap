/*
	Frontend for starting and stopping captures


	Copyright (C) 2023 David Higton

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "event.h"
#include "gadgets.h"
#include "kernel.h"
#include "swis.h"
#include "toolbox.h"
#include "wimplib.h"
#include "window.h"
#include "Defs.h"

extern MessagesFD mfd;
static char estr[257];
extern ObjectId error_id;


#ifdef Debug
extern char logstr[256];
extern void report_str (char* s);
extern void report_blk (char* buf, int len, int width, char* text);
#endif

void report_error (char* errstr)
{
  window_set_title (0, error_id, "Error");
  button_set_value (0, error_id, ERROR_TEXTAREA, errstr);
  toolbox_show_object (0, error_id, 0, NULL, 0, -1);
}

void close_error (void)
{
  toolbox_hide_object (0, error_id);
}

void messages_lookup (char* token, char* buffer, int buffer_size,
                      char* par1, char* par2, char* par3, char* par4,
                      char** new_buffer, int* new_buffer_size)
{
  /* Looks up token and passes back appropriate message */
  _kernel_oserror* e;
  _kernel_swi_regs regs;

  regs.r[0] = (int) &mfd;
  regs.r[1] = (int) token;
  regs.r[2] = (int) buffer;
  regs.r[3] =       buffer_size;
  regs.r[4] = (int) par1;
  regs.r[5] = (int) par2;
  regs.r[6] = (int) par3;
  regs.r[7] = (int) par4;

  if ((e = _kernel_swi (MessageTrans_Lookup, &regs, &regs)) != NULL)
    return;

  if (new_buffer)
    *new_buffer = (char*) regs.r[2];

  if (new_buffer_size)
    *new_buffer_size = regs.r[3];
}

/* Do a messages_lookup with no parameters to be substituted */
void messages_lookup0 (char* token, char* buf, int* size)
{
  char *lbuf;
  messages_lookup (token, (char*) estr, 256, NULL, NULL, NULL, NULL, &lbuf, size);
  strcpy (buf, lbuf);
}

/* Do a messages_lookup with 1 parameter to be substituted */
void messages_lookup1 (char* token, char* par1, char* buf, int* size)
{
  char* lbuf;
  messages_lookup (token, (char*) estr, 256, par1, NULL, NULL, NULL, &lbuf, size);
  strcpy (buf, lbuf);
}
