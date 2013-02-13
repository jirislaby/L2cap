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

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/hidp.h>

#include "hidd.h"

#ifdef NEED_PPOLL
#include "ppoll.h"
#endif

static volatile sig_atomic_t __io_canceled = 0;

static void sig_hup(int sig)
{
}

static void sig_term(int sig)
{
	__io_canceled = 1;
}

static int l2cap_listen(const bdaddr_t *bdaddr, unsigned short psm, int lm, int backlog)
{
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	int sk;

	if ((sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, bdaddr);
	addr.l2_psm = htobs(psm);

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(sk);
		return -1;
	}

	setsockopt(sk, SOL_L2CAP, L2CAP_LM, &lm, sizeof(lm));

	memset(&opts, 0, sizeof(opts));
	opts.imtu = HIDP_DEFAULT_MTU;
	opts.omtu = HIDP_DEFAULT_MTU;
	opts.flush_to = 0xffff;

	setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts));

	if (listen(sk, backlog) < 0) {
		close(sk);
		return -1;
	}

	return sk;
}

static int l2cap_accept(int sk, bdaddr_t *bdaddr)
{
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	int nsk;

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if ((nsk = accept(sk, (struct sockaddr *) &addr, &addrlen)) < 0)
		return -1;

	if (bdaddr)
		bacpy(bdaddr, &addr.l2_bdaddr);

	return nsk;
}

static int request_authentication(bdaddr_t *src, bdaddr_t *dst)
{
	struct hci_conn_info_req *cr;
	char addr[18];
	int err, dd, dev_id;

	ba2str(src, addr);
	dev_id = hci_devid(addr);
	if (dev_id < 0)
		return dev_id;

	dd = hci_open_dev(dev_id);
	if (dd < 0)
		return dd;

	cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
	if (!cr)
		return -ENOMEM;

	bacpy(&cr->bdaddr, dst);
	cr->type = ACL_LINK;
	err = ioctl(dd, HCIGETCONNINFO, (unsigned long) cr);
	if (err < 0) {
		free(cr);
		hci_close_dev(dd);
		return err;
	}

	err = hci_authenticate_link(dd, htobs(cr->conn_info->handle), 25000);

	free(cr);
	hci_close_dev(dd);

	return err;
}

static int request_encryption(bdaddr_t *src, bdaddr_t *dst)
{
	struct hci_conn_info_req *cr;
	char addr[18];
	int err, dd, dev_id;

	ba2str(src, addr);
	dev_id = hci_devid(addr);
	if (dev_id < 0)
		return dev_id;

	dd = hci_open_dev(dev_id);
	if (dd < 0)
		return dd;

	cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
	if (!cr)
		return -ENOMEM;

	bacpy(&cr->bdaddr, dst);
	cr->type = ACL_LINK;
	err = ioctl(dd, HCIGETCONNINFO, (unsigned long) cr);
	if (err < 0) {
		free(cr);
		hci_close_dev(dd);
		return err;
	}

	err = hci_encrypt_link(dd, htobs(cr->conn_info->handle), 1, 25000);

	free(cr);
	hci_close_dev(dd);

	return err;
}

static void enable_sixaxis(int csk)
{
	const unsigned char buf[] = {
		0x53 /*HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_FEATURE*/,
		0xf4,  0x42, 0x03, 0x00, 0x00 };
	int err;

	err = write(csk, buf, sizeof(buf));
	if (err < 0)
		fprintf(stderr, "%s: write failed with %d\n", __func__, err);
}

static int create_device(int ctl, int csk, int isk, uint8_t subclass, int nosdp, int nocheck, int bootonly, int encrypt, int timeout)
{
	struct hidp_connadd_req req;
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	bdaddr_t src, dst;
	char bda[18];
	int err;

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if (getsockname(csk, (struct sockaddr *) &addr, &addrlen) < 0)
		return -1;

	bacpy(&src, &addr.l2_bdaddr);

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if (getpeername(csk, (struct sockaddr *) &addr, &addrlen) < 0)
		return -1;

	bacpy(&dst, &addr.l2_bdaddr);

	memset(&req, 0, sizeof(req));
	req.ctrl_sock = csk;
	req.intr_sock = isk;
	req.flags     = 0;
	req.idle_to   = timeout * 60;

	err = get_stored_device_info(&src, &dst, &req);
	if (!err)
		goto create;

	if (!nocheck) {
		ba2str(&dst, bda);
		syslog(LOG_ERR, "Rejected connection from unknown device %s", bda);
		/* Return no error to avoid run_server() complaining too */
		return 0;
	}

	if (!nosdp) {
		err = get_sdp_device_info(&src, &dst, &req);
		if (err < 0)
			goto error;
	} else {
		struct l2cap_conninfo conn;
		socklen_t size;
		uint8_t class[3];

		memset(&conn, 0, sizeof(conn));
		size = sizeof(conn);
		if (getsockopt(csk, SOL_L2CAP, L2CAP_CONNINFO, &conn, &size) < 0)
			memset(class, 0, 3);
		else
			memcpy(class, conn.dev_class, 3);

		if (class[1] == 0x25 && (class[2] == 0x00 || class[2] == 0x01))
			req.subclass = class[0];
		else
			req.subclass = 0xc0;
	}

create:
	if (subclass != 0x00)
		req.subclass = subclass;

	ba2str(&dst, bda);
	syslog(LOG_INFO, "New HID device %s (%s)", bda, req.name);

	if (encrypt && (req.subclass & 0x40)) {
		err = request_authentication(&src, &dst);
		if (err < 0) {
			syslog(LOG_ERR, "Authentication for %s failed", bda);
			goto error;
		}

		err = request_encryption(&src, &dst);
		if (err < 0)
			syslog(LOG_ERR, "Encryption for %s failed", bda);
	}

	if (bootonly) {
		req.rd_size = 0;
		req.flags |= (1 << HIDP_BOOT_PROTOCOL_MODE);
	}

	if (req.vendor == 0x054c && req.product == 0x0268)
		enable_sixaxis(csk);

	err = ioctl(ctl, HIDPCONNADD, &req);

error:
	if (req.rd_data)
		free(req.rd_data);

	return err;
}

static void run_server(int ctl, int csk, int isk, uint8_t subclass, int nosdp, int nocheck, int bootonly, int encrypt, int timeout)
{
	struct pollfd p[2];
	sigset_t sigs;
	short events;
	int err, ncsk, nisk;

	sigfillset(&sigs);
	sigdelset(&sigs, SIGCHLD);
	sigdelset(&sigs, SIGPIPE);
	sigdelset(&sigs, SIGTERM);
	sigdelset(&sigs, SIGINT);
	sigdelset(&sigs, SIGHUP);

	p[0].fd = csk;
	p[0].events = POLLIN | POLLERR | POLLHUP;

	p[1].fd = isk;
	p[1].events = POLLIN | POLLERR | POLLHUP;

	while (!__io_canceled) {
		p[0].revents = 0;
		p[1].revents = 0;

		if (ppoll(p, 2, NULL, &sigs) < 1)
			continue;

		events = p[0].revents | p[1].revents;

		if (events & POLLIN) {
			ncsk = l2cap_accept(csk, NULL);
			nisk = l2cap_accept(isk, NULL);

			err = create_device(ctl, ncsk, nisk, subclass, nosdp, nocheck, bootonly, encrypt, timeout);
			if (err < 0)
				syslog(LOG_ERR, "HID create error %d (%s)",
						errno, strerror(errno));

			close(nisk);
			sleep(1);
			close(ncsk);
		}
	}
}

static void usage(void)
{
	printf("hidd - Bluetooth HID daemon version %s\n\n", VERSION);

	printf("Usage:\n"
		"\thidd [options]\n"
		"\n");

	printf("Options:\n"
		"\t-i <hciX|bdaddr>     Local HCI device or BD Address\n"
		"\t-t <timeout>         Set idle timeout (in minutes)\n"
		"\t-b <subclass>        Overwrite the boot mode subclass\n"
		"\t-n, --nodaemon       Don't fork daemon to background\n"
		"\t-h, --help           Display help\n"
		"\n");
}

static struct option main_options[] = {
	{ "help",	0, 0, 'h' },
	{ "nodaemon",	0, 0, 'n' },
	{ "subclass",	1, 0, 'b' },
	{ "timeout",	1, 0, 't' },
	{ "device",	1, 0, 'i' },
	{ "master",	0, 0, 'M' },
	{ "encrypt",	0, 0, 'E' },
	{ "nosdp",	0, 0, 'D' },
	{ "nocheck",	0, 0, 'Z' },
	{ "bootonly",	0, 0, 'B' },
	{ "psmctrl",	1, 0, 0x11 },
	{ "psmintr",	1, 0, 0x13 },
	{ 0, 0, 0, 0 }
};

static inline long int get_number(const char *optarg)
{
	return strtol(optarg, NULL, strncasecmp(optarg, "0x", 2) ? 10 : 16);
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	bdaddr_t bdaddr;
	uint8_t subclass = 0x00;
	char addr[18];
	unsigned short psm_ctrl = L2CAP_PSM_HIDP_CTRL;
	unsigned short psm_intr = L2CAP_PSM_HIDP_INTR;
	int log_option = LOG_NDELAY | LOG_PID;
	int opt, ctl, csk, isk;
	int detach = 1, nosdp = 0, nocheck = 0, bootonly = 0;
	int encrypt = 0, timeout = 30, lm = 0;

	bacpy(&bdaddr, BDADDR_ANY);

	while ((opt = getopt_long(argc, argv, "+i:nt:b:MEDZBh", main_options, NULL)) != -1) {
		switch(opt) {
		case 'i':
			if (!strncasecmp(optarg, "hci", 3))
				hci_devba(atoi(optarg + 3), &bdaddr);
			else
				str2ba(optarg, &bdaddr);
			break;
		case 'n':
			detach = 0;
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		case 'b':
			subclass = get_number(optarg);
			break;
		case 'M':
			lm |= L2CAP_LM_MASTER;
			break;
		case 'E':
			encrypt = 1;
			break;
		case 'D':
			nosdp = 1;
			break;
		case 'Z':
			nocheck = 1;
			break;
		case 'B':
			bootonly = 1;
			break;
		case 'h':
			usage();
			exit(0);
		case 0x11:
			psm_ctrl = get_number(optarg);
			break;
		case 0x13:
			psm_intr = get_number(optarg);
			break;
		default:
			exit(0);
		}
	}

	ba2str(&bdaddr, addr);

	ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HIDP);
	if (ctl < 0) {
		perror("Can't open HIDP control socket");
		exit(1);
	}

	csk = l2cap_listen(&bdaddr, psm_ctrl, lm, 10);
	if (csk < 0) {
		perror("Can't listen on HID control channel");
		close(ctl);
		exit(1);
	}

	isk = l2cap_listen(&bdaddr, psm_intr, lm, 10);
	if (isk < 0) {
		perror("Can't listen on HID interrupt channel");
		close(ctl);
		close(csk);
		exit(1);
	}

        if (detach) {
		if (daemon(0, 0)) {
			perror("Can't start daemon");
        	        exit(1);
		}
	} else
		log_option |= LOG_PERROR;

	openlog("hidd", log_option, LOG_DAEMON);

	if (bacmp(&bdaddr, BDADDR_ANY))
		syslog(LOG_INFO, "Bluetooth HID daemon (%s)", addr);
	else
		syslog(LOG_INFO, "Bluetooth HID daemon");

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;

	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);
	sa.sa_handler = sig_hup;
	sigaction(SIGHUP, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);

	run_server(ctl, csk, isk, subclass, nosdp, nocheck, bootonly, encrypt, timeout);

	syslog(LOG_INFO, "Exit");

	close(csk);
	close(isk);
	close(ctl);

	return 0;
}
