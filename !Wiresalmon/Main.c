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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "event.h"
#include "menu.h"
#include "quit.h"
#include "toolbox.h"
#include "swis.h"
#include "kernel.h"
#include "proginfo.h"
#include "wimplib.h"

#include "Defs.h"
#include "Error.h"

#ifdef Debug
#define Report_Text0            0x54C80
#define Report_Dump             0x54C86
char logstr[256];

void report_str (char* s)
{
  _swix (Report_Text0, _IN(0), (int) s);
}

void report_blk (char* buf, int len, int width, char* text)
{
  _swix (Report_Dump, _INR(0, 3), (int) buf, len, width, (int) text);
}

#endif

#define Wiresalmon_Start   0x58C80
#define Wiresalmon_Stop    0x58C81

MessagesFD           mfd;

ObjectId             ibaricon_id = 0;
ObjectId             ibarmenu_id = 0;
ObjectId             proginfo_id = 0;
ObjectId             main_id     = 0;
ObjectId             error_id    = 0;
static IdBlock       id_block;
static WimpPollBlock poll_block;
static bool          end = false;
static int           event_code = Wimp_ENull;
static int*          pollword_wimp = 0;
static char          pathname[257] = "";

void shade_main_icon (int ih, bool shaded);

int quit_event (int event_code, ToolboxEvent *event, IdBlock *id_block, void *h)
{
  event_code = event_code;
  event = event;
  id_block = id_block;
  h = h;

  end = true;
  return (1);
}

int help_event (int event_code, ToolboxEvent *event, IdBlock *id_block, void *h)
{
  event_code = event_code;
  event = event;
  id_block = id_block;
  h = h;

  _swix (Wimp_StartTask, _IN(0), "Filer_Run <Wiresalmon$Dir>.!Help");

  return (1);
}

int quit_message(WimpMessage *message, void *h)
{
  message = message;
  h = h;

  end = true;
  return(1);
}

int start_capture (int event_code, ToolboxEvent *event, IdBlock *id_block, void *h)
{
  _kernel_oserror* e;
  int size;
  char* p;
  char msg[257];


  writablefield_get_value (0, main_id, MAIN_FNAME, (char*) pathname, sizeof (pathname), &size);
  p = strpbrk (pathname, ".:<");
  if (p == NULL)
  {
    messages_lookup0 ("Drag", (char*) msg, &size);
    report_error ((char*) msg);
    return 1;
  }

  e = _swix (Wiresalmon_Start, _INR(0, 1), (int) pathname, 0);
  if (e)
  {
    report_error (e->errmess);
  }
  else
  {
    /* Reopen the Main window */
    toolbox_show_object (0, main_id, 0, 0, 0, 0);
    shade_main_icon (MAIN_FNAME, true);
    shade_main_icon (MAIN_FTYPE, true);
    shade_main_icon (MAIN_START, true);
    shade_main_icon (MAIN_STOP, false);
  }

  return 1;
}

int stop_capture (int event_code, ToolboxEvent *event, IdBlock *id_block, void *h)
{
  /* Reopen the Main window */
  toolbox_show_object (0, main_id, 0, 0, 0, 0);
  shade_main_icon (MAIN_FNAME, false);
  shade_main_icon (MAIN_FTYPE, false);
  shade_main_icon (MAIN_START, false);
  shade_main_icon (MAIN_STOP, true);

  _swix (Wiresalmon_Stop, 0);

  return 1;
}

int drag_ended (int event_code, ToolboxEvent *event, IdBlock *id_block, void *h)
{
  _kernel_oserror* e;
  WimpUserMessageEvent w;
  int wh; /* Window handle, used twice */
  int size;
  char* dot;

  DraggableDragEndedEvent *ddee = (DraggableDragEndedEvent*) event;
  wh = ddee->window_handle;
  w.data.data_save.destination_window = wh;
  w.data.data_save.destination_icon = ddee->icon_handle;
  w.data.data_save.destination_x = ddee->x;
  w.data.data_save.destination_y = ddee->y;
  w.data.data_save.estimated_size = -1;
  w.data.data_save.file_type = 0xffd; /* Data */
  /* Process the writable icon's contents to just a non-null leafname */
  writablefield_get_value (0, main_id, MAIN_FNAME, (char*) pathname, sizeof (pathname), &size);
  dot = strrchr ((char*) pathname, '.'); /* Pointer to last dot, or NULL */
  if (dot != NULL)
  {
    size = strlen (dot);
    /* Is there anything following? */
    if (strlen (dot + 1) != 0)
    {
      /* Yes, so copy just the leafname */
      strcpy (w.data.data_save.leaf_name, dot + 1);
    }
    else
    {
      /* No, so substitute the default filename */
      strcpy (w.data.data_save.leaf_name, DEF_FNAME);
    }
  }
  else
  {
    /* No dot, so either it's a leafname or it's empty */
    if (strlen (pathname) != 0)
    {
      /* It's a leafname */
      strcpy (w.data.data_save.leaf_name, (char*) pathname);
    }
    else
    {
      /* It's empty, so substitute the default filename */
      strcpy (w.data.data_save.leaf_name, DEF_FNAME);
    }
  }
  w.hdr.my_ref = 42;
  w.hdr.action_code = Wimp_MDataSave;
  w.hdr.size = (44 + strlen (w.data.data_save.leaf_name) + 1 + 3) & 0xFC;
  e = _swix (Wimp_SendMessage, _INR(0, 2), 17, (int) &w, wh);

  return 1;
}

void data_save_ack (WimpUserMessageEvent* w)
{
  /* Remember w is the Wimp poll block, so get all info out
     before anything else can cause a poll */

  strcpy (pathname, w->data.data_save_ack.leaf_name);
  writablefield_set_value (0, main_id, MAIN_FNAME, (char*) pathname);
}

void shade_main_icon (int ih, bool shaded)
{
  unsigned int flags;
  _kernel_oserror* e;

  e = gadget_get_flags (0, main_id, ih, &flags);
  if (shaded)
    gadget_set_flags (0, main_id, ih, flags | 0x80000000);
  else
    gadget_set_flags (0, main_id, ih, flags & 0x7FFFFFFF);

}

void app_init (void)
{
  _kernel_oserror *e;
  char msg[257];
  int size = 256;
  int toolbox_events = 0;
  int wimp_messages[] =
  {
    Wimp_MPreQuit,
    Wimp_MDataSaveAck,
    Wimp_MDataLoadAck,
    0
  };

  e = toolbox_initialise (0, 350, wimp_messages, &toolbox_events, "<Wiresalmon$Dir>", &mfd, &id_block, 0, 0, 0);
  event_initialise (&id_block);

  e = event_register_toolbox_handler (-1, EV_QUIT, quit_event, 0);
  e = event_register_toolbox_handler (-1, EV_HELP, help_event, 0);
  e = toolbox_create_object (0, "ProgInfo", &proginfo_id);
  e = proginfo_set_version (0, proginfo_id, VERSION);
  e = toolbox_create_object (0, "IbarMenu", &ibarmenu_id);
  e = toolbox_create_object (0, "IbarIcon", &ibaricon_id);
  e = toolbox_create_object (0, "Main", &main_id);
  e = toolbox_create_object (0, "Error", &error_id);
  e = event_register_toolbox_handler (-1, EV_START, start_capture, 0);
  e = event_register_toolbox_handler (-1, EV_STOP, stop_capture, 0);
  size = 256;
  messages_lookup0 ("Start", (char*) msg, &size);
  actionbutton_set_text (0, main_id, MAIN_START, msg);
  size = 256;
  messages_lookup0 ("Stop", (char*) msg, &size);
  writablefield_set_value (0, main_id, MAIN_FNAME, DEF_FNAME);
  actionbutton_set_text (0, main_id, MAIN_STOP, msg);
  event_register_message_handler (Wimp_MQuit, quit_message, 0);
  event_register_message_handler (Wimp_MPreQuit, quit_message, 0);
  event_register_toolbox_handler (-1, Draggable_DragEnded, drag_ended, 0);
  shade_main_icon (MAIN_FNAME, false);
  shade_main_icon (MAIN_FTYPE, false);
  shade_main_icon (MAIN_START, false);
  shade_main_icon (MAIN_STOP, true);
}

int main (int argc, char* argv[])
{
  int mt;

  app_init ();
  while (!end)
  {
    event_poll (&event_code, &poll_block, pollword_wimp);
    switch (event_code)
    {
      case Wimp_EUserMessage:
      case Wimp_EUserMessageRecorded:
        mt = poll_block.user_message.hdr.action_code;
        switch (mt)
        {
          case Wimp_MDataSaveAck:
            data_save_ack ((WimpUserMessageEvent*) &poll_block);
            break;
        }
        break;

        break;
    }
  }

  return 0;
}
