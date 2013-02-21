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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/hidp.h>

#include "textfile.h"
#include "hidd.h"

static int store_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req)
{
	char filename[PATH_MAX + 1], addr[18], *str;
	int err, size;

	ba2str(src, addr);
	create_name(filename, PATH_MAX, STORAGEDIR, addr, "l2cap");

	size = 15 + 3 + 3 + 5 + 9 + strlen(req->name) + 2;
	str = malloc(size);
	if (!str)
		return -ENOMEM;

	snprintf(str, size - 1, "%04X:%04X:%04X %02X %02X %04X %08X",
			req->vendor, req->product, req->version,
			req->subclass, req->country, req->parser, req->flags);

	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	ba2str(dst, addr);
	err = textfile_put(filename, addr, str);

	free(str);

	return err;
}

int get_stored_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req)
{
	char filename[PATH_MAX + 1], addr[18], *str;
	unsigned int vendor, product, version, subclass, country, parser;

	ba2str(src, addr);
	create_name(filename, PATH_MAX, STORAGEDIR, addr, "l2cap");

	ba2str(dst, addr);
	str = textfile_get(filename, addr);
	if (!str)
		return -EIO;

	sscanf(str, "%04X:%04X:%04X %02X %02X %04X %08X",
			&vendor, &product, &version, &subclass, &country,
			&parser, &req->flags);

	free(str);

	req->vendor   = vendor;
	req->product  = product;
	req->version  = version;
	req->subclass = subclass;
	req->country  = country;
	req->parser   = parser;

	return 0;
}

int get_sdp_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req)
{
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	bdaddr_t bdaddr;
	uint32_t range = 0x0000ffff;
	sdp_session_t *s;
	sdp_list_t *search, *attrid, *pnp_rsp;
	sdp_record_t *rec;
	sdp_data_t *pdlist;
	uuid_t svclass;
	int err;

	s = sdp_connect(src, dst, SDP_RETRY_IF_BUSY | SDP_WAIT_ON_CLOSE);
	if (!s)
		return -1;

	sdp_uuid16_create(&svclass, PNP_INFO_SVCLASS_ID);
	search = sdp_list_append(NULL, &svclass);
	attrid = sdp_list_append(NULL, &range);

	err = sdp_service_search_attr_req(s, search,
					SDP_ATTR_REQ_RANGE, attrid, &pnp_rsp);

	sdp_list_free(search, NULL);
	sdp_list_free(attrid, NULL);

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if (getsockname(s->sock, (struct sockaddr *) &addr, &addrlen) < 0)
		bacpy(&bdaddr, src);
	else
		bacpy(&bdaddr, &addr.l2_bdaddr);

	sdp_close(s);

	if (err)
		return -1;

	if (pnp_rsp) {
		rec = (sdp_record_t *) pnp_rsp->data;

		pdlist = sdp_data_get(rec, 0x0201);
		req->vendor = pdlist ? pdlist->val.uint16 : 0x0000;

		pdlist = sdp_data_get(rec, 0x0202);
		req->product = pdlist ? pdlist->val.uint16 : 0x0000;

		pdlist = sdp_data_get(rec, 0x0203);
		req->version = pdlist ? pdlist->val.uint16 : 0x0000;

		sdp_record_free(rec);
	}

	req->parser = 0x0100;
	req->subclass = 0xc0;
	req->country = 0;

	if (bacmp(&bdaddr, BDADDR_ANY))
		store_device_info(&bdaddr, dst, req);

	return 0;
}
