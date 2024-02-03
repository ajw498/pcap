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

#define VERSION    "0.02 (2023 November 4)"
#define DEF_FNAME  "capture/pcap"
/* Event IDs */
#define EV_QUIT     1
#define EV_HELP     2
#define EV_START    3
#define EV_STOP     4
#define EV_ERROR_OK 5
/* Gadget IDs */
#define MAIN_FNAME  0
#define MAIN_FTYPE  1
#define MAIN_START  2
#define MAIN_STOP   3
#define ERROR_TEXTAREA 0
#define ERROR_OK    1
