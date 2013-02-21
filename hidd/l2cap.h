/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2003-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __MY_L2CAP_H /* __L2CAP_H is defined in bluetooth layer */
#define __MY_L2CAP_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/hidp.h>

#define L2CAP_PSM_HIDP_CTRL 0x11
#define L2CAP_PSM_HIDP_INTR 0x13

extern int get_stored_device_info(const bdaddr_t *src, const bdaddr_t *dst,
		struct hidp_connadd_req *req);
extern int get_sdp_device_info(const bdaddr_t *src, const bdaddr_t *dst,
		struct hidp_connadd_req *req);

extern int create_file(const char *filename, const mode_t mode);
extern int create_name(char *buf, size_t size, const char *path,
				const char *address, const char *name);

extern int textfile_put(const char *pathname, const char *key,
		const char *value);
extern char *textfile_get(const char *pathname, const char *key);

#endif
