/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2009  Marcel Holtmann <marcel@holtmann.org>
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
#include <stdlib.h>
#include <getopt.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "csr.h"
//johnny_V24_2_s
#define ANDROID_LOG
#ifdef ANDROID_LOG
  #define LOG_TAG "BCCMD"
  #include <utils/Log.h> //all LOGD, LOGE, LOGW macros are defined here
#endif /*MUX_ANDROID*/
//johnny_V24_2_e
#define CSR_TRANSPORT_UNKNOWN	0
#define CSR_TRANSPORT_HCI	1
#define CSR_TRANSPORT_USB	2
#define CSR_TRANSPORT_BCSP	3
#define CSR_TRANSPORT_H4	4
#define CSR_TRANSPORT_3WIRE	5

#define CSR_STORES_PSI		(0x0001)
#define CSR_STORES_PSF		(0x0002)
#define CSR_STORES_PSROM	(0x0004)
#define CSR_STORES_PSRAM	(0x0008)
#define CSR_STORES_DEFAULT	(CSR_STORES_PSI | CSR_STORES_PSF)

#define CSR_TYPE_NULL		0
#define CSR_TYPE_COMPLEX	1
#define CSR_TYPE_UINT8		2
#define CSR_TYPE_UINT16		3
#define CSR_TYPE_UINT32		4

#define CSR_TYPE_ARRAY		CSR_TYPE_COMPLEX
#define CSR_TYPE_BDADDR		CSR_TYPE_COMPLEX

static inline int transport_open(int transport, char *device)
{
	switch (transport) {
	case CSR_TRANSPORT_HCI:
		return csr_open_hci(device);
#ifdef HAVE_LIBUSB
	case CSR_TRANSPORT_USB:
		return csr_open_usb(device);
#endif
	case CSR_TRANSPORT_BCSP:
		return csr_open_bcsp(device);
	case CSR_TRANSPORT_H4:
		return csr_open_h4(device);
	case CSR_TRANSPORT_3WIRE:
		return csr_open_3wire(device);
	default:
		fprintf(stderr, "Unsupported transport\n");
		return -1;
	}
}

static inline int transport_read(int transport, uint16_t varid, uint8_t *value, uint16_t length)
{
	switch (transport) {
	case CSR_TRANSPORT_HCI:
		return csr_read_hci(varid, value, length);
#ifdef HAVE_LIBUSB
	case CSR_TRANSPORT_USB:
		return csr_read_usb(varid, value, length);
#endif
	case CSR_TRANSPORT_BCSP:
		return csr_read_bcsp(varid, value, length);
	case CSR_TRANSPORT_H4:
		return csr_read_h4(varid, value, length);
	case CSR_TRANSPORT_3WIRE:
		return csr_read_3wire(varid, value, length);
	default:
		errno = EOPNOTSUPP;
		return -1;
	}
}

static inline int transport_write(int transport, uint16_t varid, uint8_t *value, uint16_t length)
{
	switch (transport) {
	case CSR_TRANSPORT_HCI:
		return csr_write_hci(varid, value, length);
#ifdef HAVE_LIBUSB
	case CSR_TRANSPORT_USB:
		return csr_write_usb(varid, value, length);
#endif
	case CSR_TRANSPORT_BCSP:
		return csr_write_bcsp(varid, value, length);
	case CSR_TRANSPORT_H4:
		return csr_write_h4(varid, value, length);
	case CSR_TRANSPORT_3WIRE:
		return csr_write_3wire(varid, value, length);
	default:
		errno = EOPNOTSUPP;
		return -1;
	}
}

static inline void transport_close(int transport)
{
	switch (transport) {
	case CSR_TRANSPORT_HCI:
		csr_close_hci();
		break;
#ifdef HAVE_LIBUSB
	case CSR_TRANSPORT_USB:
		csr_close_usb();
		break;
#endif
	case CSR_TRANSPORT_BCSP:
		csr_close_bcsp();
		break;
	case CSR_TRANSPORT_H4:
		csr_close_h4();
		break;
	case CSR_TRANSPORT_3WIRE:
		csr_close_3wire();
		break;
	}
}

static struct {
	uint16_t pskey;
	int type;
	int size;
	char *str;
} storage[] = {
	{ CSR_PSKEY_BDADDR,                   CSR_TYPE_BDADDR,  8,  "bdaddr"   },
	{ CSR_PSKEY_COUNTRYCODE,              CSR_TYPE_UINT16,  0,  "country"  },
	{ CSR_PSKEY_CLASSOFDEVICE,            CSR_TYPE_UINT32,  0,  "devclass" },
	{ CSR_PSKEY_ENC_KEY_LMIN,             CSR_TYPE_UINT16,  0,  "keymin"   },
	{ CSR_PSKEY_ENC_KEY_LMAX,             CSR_TYPE_UINT16,  0,  "keymax"   },
	{ CSR_PSKEY_LOCAL_SUPPORTED_FEATURES, CSR_TYPE_ARRAY,   8,  "features" },
	{ CSR_PSKEY_LOCAL_SUPPORTED_COMMANDS, CSR_TYPE_ARRAY,   18, "commands" },
	{ CSR_PSKEY_HCI_LMP_LOCAL_VERSION,    CSR_TYPE_UINT16,  0,  "version"  },
	{ CSR_PSKEY_LMP_REMOTE_VERSION,       CSR_TYPE_UINT8,   0,  "remver"   },
	{ CSR_PSKEY_HOSTIO_USE_HCI_EXTN,      CSR_TYPE_UINT16,  0,  "hciextn"  },
	{ CSR_PSKEY_HOSTIO_MAP_SCO_PCM,       CSR_TYPE_UINT16,  0,  "mapsco"   },
	{ CSR_PSKEY_UART_BAUDRATE,            CSR_TYPE_UINT16,  0,  "baudrate" },
	{ CSR_PSKEY_HOST_INTERFACE,           CSR_TYPE_UINT16,  0,  "hostintf" },
	{ CSR_PSKEY_ANA_FREQ,                 CSR_TYPE_UINT16,  0,  "anafreq"  },
	{ CSR_PSKEY_ANA_FTRIM,                CSR_TYPE_UINT16,  0,  "anaftrim" },
	{ CSR_PSKEY_USB_VENDOR_ID,            CSR_TYPE_UINT16,  0,  "usbvid"   },
	{ CSR_PSKEY_USB_PRODUCT_ID,           CSR_TYPE_UINT16,  0,  "usbpid"   },
	{ CSR_PSKEY_USB_DFU_PRODUCT_ID,       CSR_TYPE_UINT16,  0,  "dfupid"   },
	{ CSR_PSKEY_INITIAL_BOOTMODE,         CSR_TYPE_UINT16,  0,  "bootmode" },
	{ 0x0000 },
};

static char *storestostr(uint16_t stores)
{
	switch (stores) {
	case 0x0000:
		return "Default";
	case 0x0001:
		return "psi";
	case 0x0002:
		return "psf";
	case 0x0004:
		return "psrom";
	case 0x0008:
		return "psram";
	default:
		return "Unknown";
	}
}

static char *memorytostr(uint16_t type)
{
	switch (type) {
	case 0x0000:
		return "Flash memory";
	case 0x0001:
		return "EEPROM";
	case 0x0002:
		return "RAM (transient)";
	case 0x0003:
		return "ROM (or \"read-only\" flash memory)";
	default:
		return "Unknown";
	}
}

#define OPT_RANGE(min, max) \
		if (argc < (min)) { errno = EINVAL; return -1; } \
		if (argc > (max)) { errno = E2BIG; return -1; }

static struct option help_options[] = {
	{ "help",	0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

static int opt_help(int argc, char *argv[], int *help)
{
	int opt;

	while ((opt=getopt_long(argc, argv, "+h", help_options, NULL)) != EOF) {
		switch (opt) {
		case 'h':
			if (help)
				*help = 1;
			break;
		}
	}

	return optind;
}

#define OPT_HELP(range, help) \
		opt_help(argc, argv, (help)); \
		argc -= optind; argv += optind; optind = 0; \
		OPT_RANGE((range), (range))

static int cmd_builddef(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t def = 0x0000, nextdef = 0x0000;
	int err = 0;

	OPT_HELP(0, NULL);

	printf("Build definitions:\n");

	while (1) {
		memset(array, 0, sizeof(array));
		array[0] = def & 0xff;
		array[1] = def >> 8;

		err = transport_read(transport, CSR_VARID_GET_NEXT_BUILDDEF, array, 8);
		if (err < 0) {
			errno = -err;
			break;
		}

		nextdef = array[2] | (array[3] << 8);

		if (nextdef == 0x0000)
			break;

		def = nextdef;

		printf("0x%04x - %s\n", def, csr_builddeftostr(def));
	}

	return err;
}

static int cmd_keylen(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t handle, keylen;
	int err;

	OPT_HELP(1, NULL);

	handle = atoi(argv[0]);

	memset(array, 0, sizeof(array));
	array[0] = handle & 0xff;
	array[1] = handle >> 8;

	err = transport_read(transport, CSR_VARID_CRYPT_KEY_LENGTH, array, 8);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	handle = array[0] | (array[1] << 8);
	keylen = array[2] | (array[3] << 8);

	printf("Crypt key length: %d bit\n", keylen * 8);

	return 0;
}

static int cmd_clock(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint32_t clock;
	int err;

	OPT_HELP(0, NULL);

	memset(array, 0, sizeof(array));

	err = transport_read(transport, CSR_VARID_BT_CLOCK, array, 8);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	clock = array[2] | (array[3] << 8) | (array[0] << 16) | (array[1] << 24);

	printf("Bluetooth clock: 0x%04x (%d)\n", clock, clock);

	return 0;
}

static int cmd_rand(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t rand;
	int err;

	OPT_HELP(0, NULL);

	memset(array, 0, sizeof(array));

	err = transport_read(transport, CSR_VARID_RAND, array, 8);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	rand = array[0] | (array[1] << 8);

	printf("Random number: 0x%02x (%d)\n", rand, rand);

	return 0;
}

static int cmd_chiprev(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t rev;
	char *str;
	int err;

	OPT_HELP(0, NULL);

	memset(array, 0, sizeof(array));

	err = transport_read(transport, CSR_VARID_CHIPREV, array, 8);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	rev = array[0] | (array[1] << 8);

	switch (rev) {
	case 0x64:
		str = "BC1 ES";
		break;
	case 0x65:
		str = "BC1";
		break;
	case 0x89:
		str = "BC2-External A";
		break;
	case 0x8a:
		str = "BC2-External B";
		break;
	case 0x28:
		str = "BC2-ROM";
		break;
	case 0x43:
		str = "BC3-Multimedia";
		break;
	case 0x15:
		str = "BC3-ROM";
		break;
	case 0xe2:
		str = "BC3-Flash";
		break;
	case 0x26:
		str = "BC4-External";
		break;
	case 0x30:
		str = "BC4-ROM";
		break;
	default:
		str = "NA";
		break;
	}

	printf("Chip revision: 0x%04x (%s)\n", rev, str);

	return 0;
}

static int cmd_buildname(int transport, int argc, char *argv[])
{
	uint8_t array[130];
	char name[64];
	unsigned int i;
	int err;

	OPT_HELP(0, NULL);

	memset(array, 0, sizeof(array));

	err = transport_read(transport, CSR_VARID_READ_BUILD_NAME, array, 128);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	for (i = 0; i < sizeof(name); i++)
		name[i] = array[(i * 2) + 4];

	printf("Build name: %s\n", name);

	return 0;
}

static int cmd_panicarg(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t error;
	int err;

	OPT_HELP(0, NULL);

	memset(array, 0, sizeof(array));

	err = transport_read(transport, CSR_VARID_PANIC_ARG, array, 8);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	error = array[0] | (array[1] << 8);

	printf("Panic code: 0x%02x (%s)\n", error,
					error < 0x100 ? "valid" : "invalid");

	return 0;
}

static int cmd_faultarg(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t error;
	int err;

	OPT_HELP(0, NULL);

	memset(array, 0, sizeof(array));

	err = transport_read(transport, CSR_VARID_FAULT_ARG, array, 8);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	error = array[0] | (array[1] << 8);

	printf("Fault code: 0x%02x (%s)\n", error,
					error < 0x100 ? "valid" : "invalid");

	return 0;
}

static int cmd_coldreset(int transport, int argc, char *argv[])
{
	return transport_write(transport, CSR_VARID_COLD_RESET, NULL, 0);
}

static int cmd_warmreset(int transport, int argc, char *argv[])
{
	return transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);
}

static int cmd_disabletx(int transport, int argc, char *argv[])
{
	return transport_write(transport, CSR_VARID_DISABLE_TX, NULL, 0);
}

static int cmd_enabletx(int transport, int argc, char *argv[])
{
	return transport_write(transport, CSR_VARID_ENABLE_TX, NULL, 0);
}

static int cmd_singlechan(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t channel;

	OPT_HELP(1, NULL);

	channel = atoi(argv[0]);

	if (channel > 2401 && channel < 2481)
		channel -= 2402;

	if (channel > 78) {
		errno = EINVAL;
		return -1;
	}

	memset(array, 0, sizeof(array));
	array[0] = channel & 0xff;
	array[1] = channel >> 8;

	return transport_write(transport, CSR_VARID_SINGLE_CHAN, array, 8);
}

static int cmd_hoppingon(int transport, int argc, char *argv[])
{
	return transport_write(transport, CSR_VARID_HOPPING_ON, NULL, 0);
}

static int cmd_rttxdata1(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t freq, level;

	OPT_HELP(2, NULL);

	freq = atoi(argv[0]);

	if (!strncasecmp(argv[1], "0x", 2))
		level = strtol(argv[1], NULL, 16);
	else
		level = atoi(argv[1]);

	memset(array, 0, sizeof(array));
	array[0] = 0x04;
	array[1] = 0x00;
	array[2] = freq & 0xff;
	array[3] = freq >> 8;
	array[4] = level & 0xff;
	array[5] = level >> 8;

	return transport_write(transport, CSR_VARID_RADIOTEST, array, 8);
}

static int cmd_radiotest(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t freq, level, test;

	OPT_HELP(3, NULL);

	freq = atoi(argv[0]);

	if (!strncasecmp(argv[1], "0x", 2))
		level = strtol(argv[1], NULL, 16);
	else
		level = atoi(argv[1]);

	test = atoi(argv[2]);

	memset(array, 0, sizeof(array));
	array[0] = test & 0xff;
	array[1] = test >> 8;
	array[2] = freq & 0xff;
	array[3] = freq >> 8;
	array[4] = level & 0xff;
	array[5] = level >> 8;

	return transport_write(transport, CSR_VARID_RADIOTEST, array, 8);
}

static int cmd_memtypes(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t type, stores[4] = { 0x0001, 0x0002, 0x0004, 0x0008 };
	int i, err;

	OPT_HELP(0, NULL);

	for (i = 0; i < 4; i++) {
		memset(array, 0, sizeof(array));
		array[0] = stores[i] & 0xff;
		array[1] = stores[i] >> 8;

		err = transport_read(transport, CSR_VARID_PS_MEMORY_TYPE, array, 8);
		if (err < 0)
			continue;

		type = array[2] + (array[3] << 8);

		printf("%s (0x%04x) = %s (%d)\n", storestostr(stores[i]),
					stores[i], memorytostr(type), type);
	}

	return 0;
}

static struct option pskey_options[] = {
	{ "stores",	1, 0, 's' },
	{ "reset",	0, 0, 'r' },
	{ "help",	0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

static int opt_pskey(int argc, char *argv[], uint16_t *stores, int *reset, int *help)
{
	int opt;

	while ((opt=getopt_long(argc, argv, "+s:rh", pskey_options, NULL)) != EOF) {
		switch (opt) {
		case 's':
			if (!stores)
				break;
			if (!strcasecmp(optarg, "default"))
				*stores = 0x0000;
			else if (!strcasecmp(optarg, "implementation"))
				*stores = 0x0001;
			else if (!strcasecmp(optarg, "factory"))
				*stores = 0x0002;
			else if (!strcasecmp(optarg, "rom"))
				*stores = 0x0004;
			else if (!strcasecmp(optarg, "ram"))
				*stores = 0x0008;
			else if (!strcasecmp(optarg, "psi"))
				*stores = 0x0001;
			else if (!strcasecmp(optarg, "psf"))
				*stores = 0x0002;
			else if (!strcasecmp(optarg, "psrom"))
				*stores = 0x0004;
			else if (!strcasecmp(optarg, "psram"))
				*stores = 0x0008;
			else if (!strncasecmp(optarg, "0x", 2))
				*stores = strtol(optarg, NULL, 16);
			else
				*stores = atoi(optarg);
			break;

		case 'r':
			if (reset)
				*reset = 1;
			break;

		case 'h':
			if (help)
				*help = 1;
			break;
		}
	}

	return optind;
}

#define OPT_PSKEY(min, max, stores, reset, help) \
		opt_pskey(argc, argv, (stores), (reset), (help)); \
		argc -= optind; argv += optind; optind = 0; \
		OPT_RANGE((min), (max))

static int cmd_psget(int transport, int argc, char *argv[])
{
	uint8_t array[128];
	uint16_t pskey, length, value, stores = CSR_STORES_DEFAULT;
	uint32_t val32;
	int i, err, reset = 0;

	memset(array, 0, sizeof(array));

	OPT_PSKEY(1, 1, &stores, &reset, NULL);

	if (strncasecmp(argv[0], "0x", 2)) {
		pskey = atoi(argv[0]);

		for (i = 0; storage[i].pskey; i++) {
			if (strcasecmp(storage[i].str, argv[0]))
				continue;

			pskey = storage[i].pskey;
			break;
		}
	} else
		pskey = strtol(argv[0] + 2, NULL, 16);

	memset(array, 0, sizeof(array));
	array[0] = pskey & 0xff;
	array[1] = pskey >> 8;
	array[2] = stores & 0xff;
	array[3] = stores >> 8;

	err = transport_read(transport, CSR_VARID_PS_SIZE, array, 8);
	if (err < 0)
		return err;

	length = array[2] + (array[3] << 8);
	if (length + 6 > (int) sizeof(array) / 2)
		return -EIO;

	memset(array, 0, sizeof(array));
	array[0] = pskey & 0xff;
	array[1] = pskey >> 8;
	array[2] = length & 0xff;
	array[3] = length >> 8;
	array[4] = stores & 0xff;
	array[5] = stores >> 8;

	err = transport_read(transport, CSR_VARID_PS, array, (length + 3) * 2);
	if (err < 0)
		return err;

	switch (length) {
	case 1:
		value = array[6] | (array[7] << 8);
		printf("%s: 0x%04x (%d)\n", csr_pskeytostr(pskey), value, value);
		break;

	case 2:
		val32 = array[8] | (array[9] << 8) | (array[6] << 16) | (array[7] << 24);
		printf("%s: 0x%08x (%d)\n", csr_pskeytostr(pskey), val32, val32);
		break;

	default:
		printf("%s:", csr_pskeytostr(pskey));
		for (i = 0; i < length; i++)
			printf(" 0x%02x%02x", array[(i * 2) + 6], array[(i * 2) + 7]);
		printf("\n");
		break;
	}

	if (reset)
		transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);

	return err;
}

static int cmd_psset(int transport, int argc, char *argv[])
{
	uint8_t array[128];
	uint16_t pskey, length, value, stores = CSR_STORES_PSRAM;
	uint32_t val32;
	int i, err, reset = 0;

	memset(array, 0, sizeof(array));

	OPT_PSKEY(2, 81, &stores, &reset, NULL);

	if (strncasecmp(argv[0], "0x", 2)) {
		pskey = atoi(argv[0]);

		for (i = 0; storage[i].pskey; i++) {
			if (strcasecmp(storage[i].str, argv[0]))
				continue;

			pskey = storage[i].pskey;
			break;
		}
	} else
		pskey = strtol(argv[0] + 2, NULL, 16);

	memset(array, 0, sizeof(array));
	array[0] = pskey & 0xff;
	array[1] = pskey >> 8;
	array[2] = stores & 0xff;
	array[3] = stores >> 8;

	err = transport_read(transport, CSR_VARID_PS_SIZE, array, 8);
	if (err < 0)
		return err;

	length = array[2] + (array[3] << 8);
	if (length + 6 > (int) sizeof(array) / 2)
		return -EIO;

	memset(array, 0, sizeof(array));
	array[0] = pskey & 0xff;
	array[1] = pskey >> 8;
	array[2] = length & 0xff;
	array[3] = length >> 8;
	array[4] = stores & 0xff;
	array[5] = stores >> 8;

	argc--;
	argv++;

	switch (length) {
	case 1:
		if (argc != 1) {
			errno = E2BIG;
			return -1;
		}

		if (!strncasecmp(argv[0], "0x", 2))
			value = strtol(argv[0] + 2, NULL, 16);
		else
			value = atoi(argv[0]);

		array[6] = value & 0xff;
		array[7] = value >> 8;
		break;

	case 2:
		if (argc != 1) {
			errno = E2BIG;
			return -1;
		}

		if (!strncasecmp(argv[0], "0x", 2))
			val32 = strtol(argv[0] + 2, NULL, 16);
		else
			val32 = atoi(argv[0]);

		array[6] = (val32 & 0xff0000) >> 16;
		array[7] = val32 >> 24;
		array[8] = val32 & 0xff;
		array[9] = (val32 & 0xff00) >> 8;
		break;

	default:
		if (argc != length * 2) {
			errno = EINVAL;
			return -1;
		}

		for (i = 0; i < length * 2; i++)
			if (!strncasecmp(argv[0], "0x", 2))
				array[i + 6] = strtol(argv[i] + 2, NULL, 16);
			else
				array[i + 6] = atoi(argv[i]);
		break;
	}

	err = transport_write(transport, CSR_VARID_PS, array, (length + 3) * 2);
	if (err < 0)
		return err;

	if (reset)
		transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);

	return err;
}

static int cmd_psclr(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t pskey, stores = CSR_STORES_PSRAM;
	int i, err, reset = 0;

	OPT_PSKEY(1, 1, &stores, &reset, NULL);

	if (strncasecmp(argv[0], "0x", 2)) {
		pskey = atoi(argv[0]);

		for (i = 0; storage[i].pskey; i++) {
			if (strcasecmp(storage[i].str, argv[0]))
				continue;

			pskey = storage[i].pskey;
			break;
		}
	} else
		pskey = strtol(argv[0] + 2, NULL, 16);

	memset(array, 0, sizeof(array));
	array[0] = pskey & 0xff;
	array[1] = pskey >> 8;
	array[2] = stores & 0xff;
	array[3] = stores >> 8;

	err = transport_write(transport, CSR_VARID_PS_CLR_STORES, array, 8);
	if (err < 0)
		return err;

	if (reset)
		transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);

	return err;
}

static int cmd_pslist(int transport, int argc, char *argv[])
{
	uint8_t array[8];
	uint16_t pskey = 0x0000, length, stores = CSR_STORES_DEFAULT;
	int err, reset = 0;

	OPT_PSKEY(0, 0, &stores, &reset, NULL);

	while (1) {
		memset(array, 0, sizeof(array));
		array[0] = pskey & 0xff;
		array[1] = pskey >> 8;
		array[2] = stores & 0xff;
		array[3] = stores >> 8;

		err = transport_read(transport, CSR_VARID_PS_NEXT, array, 8);
		if (err < 0)
			break;

		pskey = array[4] + (array[5] << 8);
		if (pskey == 0x0000)
			break;

		memset(array, 0, sizeof(array));
		array[0] = pskey & 0xff;
		array[1] = pskey >> 8;
		array[2] = stores & 0xff;
		array[3] = stores >> 8;

		err = transport_read(transport, CSR_VARID_PS_SIZE, array, 8);
		if (err < 0)
			continue;

		length = array[2] + (array[3] << 8);

		printf("0x%04x - %s (%d bytes)\n", pskey,
					csr_pskeytostr(pskey), length * 2);
	}

	if (reset)
		transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);

	return 0;
}

static int cmd_psread(int transport, int argc, char *argv[])
{
	uint8_t array[256];
	uint16_t pskey = 0x0000, length, stores = CSR_STORES_DEFAULT;
	char *str, val[7];
	int i, err, reset = 0;

	OPT_PSKEY(0, 0, &stores, &reset, NULL);

	while (1) {
		memset(array, 0, sizeof(array));
		array[0] = pskey & 0xff;
		array[1] = pskey >> 8;
		array[2] = stores & 0xff;
		array[3] = stores >> 8;

		err = transport_read(transport, CSR_VARID_PS_NEXT, array, 8);
		if (err < 0)
			break;

		pskey = array[4] + (array[5] << 8);
		if (pskey == 0x0000)
			break;

		memset(array, 0, sizeof(array));
		array[0] = pskey & 0xff;
		array[1] = pskey >> 8;
		array[2] = stores & 0xff;
		array[3] = stores >> 8;

		err = transport_read(transport, CSR_VARID_PS_SIZE, array, 8);
		if (err < 0)
			continue;

		length = array[2] + (array[3] << 8);
		if (length + 6 > (int) sizeof(array) / 2)
			continue;

		memset(array, 0, sizeof(array));
		array[0] = pskey & 0xff;
		array[1] = pskey >> 8;
		array[2] = length & 0xff;
		array[3] = length >> 8;
		array[4] = stores & 0xff;
		array[5] = stores >> 8;

		err = transport_read(transport, CSR_VARID_PS, array, (length + 3) * 2);
		if (err < 0)
			continue;

		str = csr_pskeytoval(pskey);
		if (!strcasecmp(str, "UNKNOWN")) {
			sprintf(val, "0x%04x", pskey);
			str = NULL;
		}

		printf("// %s%s\n&%04x =", str ? "PSKEY_" : "", 
						str ? str : val, pskey);
		for (i = 0; i < length; i++)
			printf(" %02x%02x", array[(i * 2) + 7], array[(i * 2) + 6]);
		printf("\n");
	}

	if (reset)
		transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);

	return 0;
}
//johnny_V24_2_s
static void psset_bdaddr(int transport)
{
	uint16_t pskey = 0x0001;
        uint8_t length = 4;
	uint8_t array[128];
	int i, err;
	uint16_t stores = 0x8;
	uint8_t bdaddr[8] = {0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22}; 

        memset(array, 0, sizeof(array));
        array[0] = pskey & 0xff;
        array[1] = pskey >> 8;
        array[2] = length & 0xff;
        array[3] = length >> 8;
        array[4] = stores & 0xff;
        array[5] = stores >> 8;
        for (i = 0; i< 8; i++)
	{
		array[i+6] = bdaddr[i];
	}
	err = transport_write(transport, CSR_VARID_PS, array, (length + 2)*2);
	if (err < 0)
	{
		printf("pskey bdaddr set err\n");
		exit(1);
	}

}

static void psset_uartbaud(int transport)
{
	uint16_t pskey = 0x1be;
       uint16_t value = 0x1d8;
	uint8_t length = 1;
	uint8_t array[128];
	uint16_t stores = 0x8;
	int err;

        memset(array, 0, sizeof(array));
        array[0] = pskey & 0xff;
        array[1] = pskey >> 8;
        array[2] = length & 0xff;
        array[3] = length >> 8;
        array[4] = stores & 0xff;
        array[5] = stores >> 8;
        array[6] = value & 0xff;
        array[7] = value >> 8;
        err = transport_write(transport, CSR_VARID_PS, array, (length + 3) * 2);
	if (err < 0)
	{
		printf("pskey uartbaud set err\n");
		exit(1);
	}
}

static void psset_anafreq(int transport)
{
        uint16_t pskey = 0x1fe;
        uint16_t value = 0x6590;
        uint8_t length = 1;
	uint8_t array[128];
	uint16_t stores = 0x8;
	int err;

        memset(array, 0, sizeof(array));
        array[0] = pskey & 0xff;
        array[1] = pskey >> 8;
        array[2] = length & 0xff;
        array[3] = length >> 8;
        array[4] = stores & 0xff;
        array[5] = stores >> 8;
        array[6] = value & 0xff;
        array[7] = value >> 8;
        err = transport_write(transport, CSR_VARID_PS, array, (length + 3) * 2);
	if (err < 0)
	{
		printf("pskey anafreq set err\n");
                exit(1);
	}
}

static void psset_warmreset(int transport)
{
	int err;
	err = transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);
        if (err < 0)
        {
                printf("pskey anafreq set err\n");
                exit(1);
        }

}
//johnny_V24_2_e
/*jeff debug*/
static int cmd_psload(int transport, int argc, char *argv[])
{
	uint8_t array[256];
	uint16_t pskey, length, size, stores = CSR_STORES_PSRAM;
	char *str, val[7];
	int err, reset = 0;

       LOGI("cmd_psload++");
	OPT_PSKEY(1, 1, &stores, &reset, NULL);

	psr_read(argv[0]);

	memset(array, 0, sizeof(array));
	size = sizeof(array) - 6;

	while (psr_get(&pskey, array + 6, &size) == 0) {
		str = csr_pskeytoval(pskey);
		if (!strcasecmp(str, "UNKNOWN")) {
			sprintf(val, "0x%04x", pskey);
			str = NULL;
		}

            LOGI("Loading %s%s ... ", str ? "PSKEY_" : "",
							str ? str : val);
		printf("Loading %s%s ... ", str ? "PSKEY_" : "",
							str ? str : val);
		fflush(stdout);

		length = size / 2;

		array[0] = pskey & 0xff;
		array[1] = pskey >> 8;
		array[2] = length & 0xff;
		array[3] = length >> 8;
		array[4] = stores & 0xff;
		array[5] = stores >> 8;

		err = transport_write(transport, CSR_VARID_PS, array, size + 6);

		LOGI("%s\n", err < 0 ? "failed" : "done");
		printf("%s\n", err < 0 ? "failed" : "done");

		memset(array, 0, sizeof(array));
		size = sizeof(array) - 6;
	}

	LOGI("cmd_psload--");
//johnny_V24_2_s	
//	if (reset)
//		transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);
	if (reset){
		err = transport_write(transport, CSR_VARID_WARM_RESET, NULL, 0);
		printf("[jeff] Loading warm_reset cmd... %s\n", err < 0 ? "failed" : "done");        
	}
	return 0;
}

static int psset(int transport,uint16_t pskey, uint16_t *value,uint16_t size)
{
	uint8_t array[256];
	uint16_t length, stores = CSR_STORES_PSRAM;
	int err;

      //LOGI("psset++, pskey:0x%4x,size:%d",pskey,size);
			
	memset(array, 0, sizeof(array));
	length = size;

	array[0] = pskey & 0xff;
	array[1] = pskey >> 8;
	array[2] = length & 0xff;
	array[3] = length >> 8;
	array[4] = stores & 0xff;
	array[5] = stores >> 8;

	memcpy(&array[6],(uint8_t*)value,size*2);

      err = transport_write(transport, CSR_VARID_PS, array, size*2 + 6);
		
	LOGI("psset++, pskey:0x%4x,size:%d, %s\n",pskey,size, (err < 0 ? "failed" : "done"));
	if(err<0)
		LOGE("psset++, pskey:0x%4x,size:%d, failed\n",pskey,size);
	return 0;
}


static int cmd_psload_semco(int transport)
{
    #define PSLEN(array)  (sizeof(array)/sizeof(array[0]) -1)
    //SEMCO BT chip 
    
    uint16_t    bol_1[] =  {0x00f6, 0x0001};
    uint16_t    bol_2[] =  {0x0031, 0x2a00, 0x0000, 0x3000, 0x0000, 0x0400};
    uint16_t    bol_3[] =  {0x023e, 0x0001};
    uint16_t    bol_4[] =  {0x03d4, 0x0007};
    uint16_t    bol_5[] =  {0x0001, 0x0078, 0x9abc, 0x0056, 0x1234};
    uint16_t    bol_6[] =  {0x01f6, 0x0015}; //0x0000:semco, 0x0015:bc04
    uint16_t    bol_7[] =  {0x024b, 0x0000};
    uint16_t    bol_8[] =  {0x023b, 0x0001};
    uint16_t    bol_9[] =  {0x01fe, 0x6590};
    uint16_t    bol_10[] = {0x01be, 0x075f}; //baudrate .115200: 0x01d8, 460800:0x075f
    uint16_t    bol_11[] = {0x0240, 0x0008};	
    uint16_t    bol_12[] = {0x0217, 0xfffe};
    uint16_t    bol_13[] = {0x21c5, 0x1515};
    uint16_t    bol_14[] = {0x0243, 0x0007, 0x0003};
    uint16_t    bol_15[] = {0x03af, 0x0007};	
    uint16_t    bol_16[] = {0x21e9, 0x003f};	
    uint16_t    bol_17[] = {0x0021, 0x0014};	
    uint16_t    bol_18[] = {0x0017, 0x0014};	
    uint16_t    bol_19[] = {0x001d, 0x3010};	
    uint16_t    bol_20[] = {0x03e4, 0x1c99};		
    uint16_t    bol_21[] = {0x002a, 0x0011};	
    uint16_t    bol_22[] = {0x0028, 0x0008, 0x0000, 0x0000};	
    uint16_t    bol_23[] = {0x0203, 0x0002, 0x0036, 0x0004, 0x0082, 0x0006, 0x0043, 0x0008, 0x004b, 0x000a, 0x002c, 0x0010, 0x000f, 0x0014, 0x0029, 0x0020, 0x0026, 0x0024, 0x000d, 0x0028, 0x000a, 0x0034, 0x0003, 0x0046, 0x000c, 0x0064, 0x000a, 0x0074, 0x000a, 0x0082, 0x0004, 0x0000, 0x0000};	
    uint16_t    bol_24[] = {0x0394, 0xffec};	
    uint16_t    bol_25[] = {0x03aa, 0xffd8, 0x0003, 0xffeb, 0x0001, 0xffec, 0x0005, 0xfff6, 0x0005, 0x0014, 0x0000, 0x0028, 0xfffe};
    uint16_t    bol_26[] = {0x03ab, 0xffd8, 0x0003, 0xffeb, 0x0001, 0xffec, 0x0005, 0xfff6, 0x0005, 0x0014, 0x0000, 0x0028, 0xfffe};
    uint16_t    bol_27[] = {0x21e1, 0xffd8, 0x0002, 0xffeb, 0x0001, 0xffec, 0xffff, 0xfff6, 0x0000, 0x0032, 0x0000, 0x003c, 0x0001, 0x0050, 0x0002, 0x0064, 0x0003};
    uint16_t    bol_28[] = {0x03d4, 0x0007};
    //uint16_t    bol_29[] = {};	

    //semco patch.
    uint16_t    bol_30[] = {0x212c, 0x0001, 0xbb38, 0x3d14, 0xe335, 0x03e8, 0x0014, 0x06e0, 0x0100, 0x1d84, 0x03fc, 0x0100, 0x1c14, 0xfa25, 0x0018, 0xff2b, 0xff0e, 0xbb00, 0x3b18, 0x00e2, 0xfdb9};
    uint16_t    bol_31[] = {0x212e, 0x0001, 0xa276, 0xff00, 0x7e25, 0xff00, 0xc815, 0x8fc4, 0x40b4, 0xff00, 0xc825, 0x8915, 0x9000, 0xffc4, 0x3000, 0x00b4, 0x8925, 0xe400, 0x6c15, 0x9000, 0xffc4, 0x3000, 0x00b4, 0xe400, 0x6c25, 0xe400, 0x6d15, 0x9000, 0xffc4, 0x3000, 0x00b4, 0xe400, 0x6d25, 0xffe3, 0xfed6};
    uint16_t    bol_32[] = {0x212f, 0x0001, 0xa32d, 0xff00, 0x7e25, 0x0216, 0xe400, 0x6c18, 0xff00, 0xc811, 0x8fc0, 0x1e84, 0x15f4, 0x3e84, 0x13f4, 0x40b0, 0xff00, 0xc821, 0x9000, 0xff14, 0x8911, 0xe1c1, 0x3000, 0x00b0, 0x8921, 0x0012, 0xe1c1, 0x3000, 0x00b0, 0x01c6, 0x3000, 0x00b4, 0x0be0, 0x7000, 0x0014, 0xff00, 0xc821, 0x8911, 0xe1b1, 0x8921, 0x0012, 0xe1b1, 0x01b6, 0x0022, 0x0126, 0xffe3, 0xa6bb};
    uint16_t    bol_33[] = {0x2133, 0x0001, 0xc904, 0x0018, 0x132b, 0x130e, 0x3d00, 0x219e, 0xe900, 0x3518, 0x0026, 0xe400, 0x6d15, 0xff26, 0x6014, 0x0126, 0xe415, 0x130e, 0xdd00, 0xc09e, 0x019c, 0xe200, 0x2819, 0xff00, 0x7000, 0x009e, 0x8915, 0xe400, 0x6d25, 0xff00, 0xc515, 0xe500, 0xe725, 0xe415, 0x130e, 0xc700, 0xc518, 0x009e, 0xd80f, 0x1f0b};
    uint16_t    bol_34[] = {0x2134, 0x0001, 0xc9cb, 0x2600, 0xce88, 0x06f0, 0xb511, 0x2173, 0x0100, 0x4980, 0x052a, 0xe30b, 0xdf00, 0xf315, 0x8000, 0x0184, 0x17f4, 0xe900, 0x3518, 0x0012, 0x8000, 0x0180, 0x03f0, 0x0026, 0x0fe0, 0x0056, 0x1627, 0x0192, 0xe111, 0x07a4, 0xe800, 0x8031, 0x1523, 0x019c, 0xe200, 0x2619, 0xff00, 0x7000, 0x00e2, 0xe30f, 0x4abb};
    uint16_t    bol_35[] = {0x2135, 0x0001, 0xc8bf, 0xfd84, 0x0ef8, 0x0387, 0x06f8, 0x0327, 0x8915, 0x1000, 0xffc4, 0x0227, 0x0018, 0xff2b, 0xff0e, 0xc900, 0xc718, 0x00e2, 0x0018, 0xff2b, 0xff0e, 0xc900, 0xe518, 0x00e2, 0x58ac};
    uint16_t    bol_36[] = {0x2227, 0x299c, 0x0013, 0x279c, 0x0427, 0x0f00, 0x3314, 0x289c, 0x0527, 0x0314, 0x249c, 0x0627, 0x2000, 0x6914, 0x219c, 0x0587, 0x0224, 0x0527, 0x0514, 0x1b9c, 0x0487, 0x0224, 0x0417, 0x0677, 0x0200, 0x9084, 0x0828, 0x0517, 0x0677, 0x0287, 0x0424, 0x0227, 0x0013, 0x0323, 0x0013, 0x0a33, 0x0183, 0xde2c, 0x0313, 0xff00, 0xc521, 0xf60f, 0x0023, 0xff00, 0xc521, 0x0114, 0x0ba0, 0xfc0b, 0x0325, 0x0014, 0x0f13, 0x0127, 0x0023, 0x0b0e, 0xc700, 0x5f18, 0x009e, 0x0137, 0x0013, 0xff30, 0xf7f0, 0x0f97, 0xfc0f, 0xc70e};
    uint16_t    bol_37[] = {0x2228, 0x1613, 0x1030, 0x2080, 0x362c, 0x0010, 0x1a23, 0xe415, 0x0234, 0x1a0e, 0xc600, 0xf518, 0x009e, 0x1513, 0xe230, 0xe900, 0x3715, 0x1e34, 0x0027, 0xe035, 0x0118, 0x012b, 0x019c, 0xe200, 0x2719, 0xff00, 0x7000, 0x009e, 0xf814, 0x1583, 0x022c, 0x0814, 0x169b, 0x02e8, 0x0074, 0xe900, 0x3635, 0x7f84, 0x0328, 0x0100, 0x8014, 0x3f84, 0x0220, 0x4014, 0xe900, 0x3625, 0x1693, 0xe111, 0x07a4, 0xe800, 0x8031, 0x1523, 0xe415, 0x0234, 0x1a0e, 0xc700, 0xc518, 0x009e, 0x1517, 0xe500, 0xe725, 0xe30f, 0x6a40};
    uint16_t    bol_38[] = {0x2229, 0xf60b, 0x0127, 0x8000, 0xff14, 0x0227, 0x0014, 0x0327, 0x0727, 0x5000, 0x0714, 0xff00, 0x7b25, 0xff00, 0xcf15, 0xc000, 0x0fc4, 0xe400, 0x76b5, 0xff00, 0xcf25, 0xe900, 0x3415, 0x8925, 0xe200, 0x2519, 0xff00, 0x7000, 0x00e2, 0xdb92};
    uint16_t    bol_39[] = {0x222a, 0xf10b, 0x0100, 0x6d10, 0x0200, 0x3514, 0x2a9c, 0xe800, 0x8021, 0x0a23, 0xe230, 0x249c, 0x0b27, 0x0a13, 0x219c, 0x0a13, 0x0b87, 0x0620, 0x0230, 0x0a23, 0x0200, 0x5780, 0xf72c, 0xe800, 0x8051, 0xe900, 0x3721, 0xe415, 0x0010, 0x0c23, 0x220e, 0xc900, 0x8618, 0x009e, 0xfd14, 0xf825, 0x019c, 0xe200, 0x2919, 0xe500, 0xdb11, 0x1000, 0x00c0, 0xff00, 0x7000, 0x00f6, 0xf10f, 0xe015, 0xf60b, 0x0218, 0x0a2b, 0x0818, 0x0b2b, 0xe200, 0x2719, 0xff00, 0x7000, 0x01e2, 0x9c36};
    uint16_t    bol_40[] = {0x222b, 0xe419, 0xe415, 0x0a34, 0xfa25, 0x0116, 0x0012, 0x04e8, 0xf881, 0x06fc, 0x0ce0, 0xf899, 0x03ec, 0x0c87, 0x082c, 0x0c27, 0x8915, 0xf000, 0x00c4, 0x0cb7, 0x8925, 0xf821, 0x0238, 0xfa89, 0xedfc, 0xf10f, 0x338f};

    LOGI("set SEMCO PSKEY...");
	
    psset(transport,bol_1[0], &bol_1[1], PSLEN(bol_1));
    psset(transport,bol_2[0], &bol_2[1], PSLEN(bol_2));
    psset(transport,bol_3[0], &bol_3[1], PSLEN(bol_3));
    psset(transport,bol_4[0], &bol_4[1], PSLEN(bol_4));
    psset(transport,bol_5[0], &bol_5[1], PSLEN(bol_5));
    psset(transport,bol_6[0], &bol_6[1], PSLEN(bol_6));
    psset(transport,bol_7[0], &bol_7[1], PSLEN(bol_7));
    psset(transport,bol_8[0], &bol_8[1], PSLEN(bol_8));
    psset(transport,bol_9[0], &bol_9[1], PSLEN(bol_9));
    psset(transport,bol_10[0], &bol_10[1], PSLEN(bol_10));
    psset(transport,bol_11[0], &bol_11[1], PSLEN(bol_11));	
    psset(transport,bol_12[0], &bol_12[1], PSLEN(bol_12));
    psset(transport,bol_13[0], &bol_13[1], PSLEN(bol_13));
    psset(transport,bol_14[0], &bol_14[1], PSLEN(bol_14));
    psset(transport,bol_15[0], &bol_15[1], PSLEN(bol_15));
    psset(transport,bol_16[0], &bol_16[1], PSLEN(bol_16));
    psset(transport,bol_17[0], &bol_17[1], PSLEN(bol_17));
    psset(transport,bol_18[0], &bol_18[1], PSLEN(bol_18));
    psset(transport,bol_19[0], &bol_19[1], PSLEN(bol_19));
    psset(transport,bol_20[0], &bol_20[1], PSLEN(bol_20));
    psset(transport,bol_21[0], &bol_21[1], PSLEN(bol_21));
    psset(transport,bol_22[0], &bol_22[1], PSLEN(bol_22));
    psset(transport,bol_23[0], &bol_23[1], PSLEN(bol_23));
    psset(transport,bol_24[0], &bol_24[1], PSLEN(bol_24));
    psset(transport,bol_25[0], &bol_25[1], PSLEN(bol_25));
    psset(transport,bol_26[0], &bol_26[1], PSLEN(bol_26));
    psset(transport,bol_27[0], &bol_27[1], PSLEN(bol_27));
    psset(transport,bol_28[0], &bol_28[1], PSLEN(bol_28));
    
    psset(transport,bol_30[0], &bol_30[1], PSLEN(bol_30));
    psset(transport,bol_31[0], &bol_31[1], PSLEN(bol_31));
    psset(transport,bol_32[0], &bol_32[1], PSLEN(bol_32));
    psset(transport,bol_33[0], &bol_33[1], PSLEN(bol_33));
    psset(transport,bol_34[0], &bol_34[1], PSLEN(bol_34));
    psset(transport,bol_35[0], &bol_35[1], PSLEN(bol_35));
    psset(transport,bol_36[0], &bol_36[1], PSLEN(bol_36));
    psset(transport,bol_37[0], &bol_37[1], PSLEN(bol_37));
    psset(transport,bol_38[0], &bol_38[1], PSLEN(bol_38));
    psset(transport,bol_39[0], &bol_39[1], PSLEN(bol_39));
    psset(transport,bol_40[0], &bol_40[1], PSLEN(bol_40));

    return 0;
}

static int cmd_psload_default(int transport)
{

    //SEMCO BT chip 
    //jeff
    uint16_t    bol_1[4] = {0x0078,0x9abc,0x0056,0x1234};
    uint16_t    bol_2 = 0x0015;
    uint16_t    bol_3 = 0x0001;
    uint16_t    bol_4 = 0x0006;
    uint16_t    bol_5 = 0x0001;
    uint16_t    bol_6 = 0x0000;
	  
    uint16_t    bol_8 = 0x6590;
    uint16_t    bol_9 = 0x0000;
    uint16_t    bol_10 = 0x0000;
    uint16_t    bol_11 = 0x0001;	
    uint16_t    bol_12 = 0x0001;
    uint16_t    bol_13 = 0x0001;
    uint16_t    bol_14 = 0x0060;
	

#if 0 //bt slave
    uint16_t    bol_7[2] = {0x0880, 0x0006};     //PSKEY_PCM_CONFIG32
    uint16_t    bol_15[2] = {0x0000, 0x0000};	//PSKEY_PCM_LOW_JITTER_CONFIG
    
#else //bt master 2mhz, 8k sysnc
    uint16_t    bol_7[2] = {0x08C0, 0x0004};        //PSKEY_PCM_CONFIG32
    //uint16_t    bol_15[2] = {0x2020, 0x0177};	//PSKEY_PCM_LOW_JITTER_CONFIG	 ,2Mhz 8K sync
    uint16_t    bol_15[2] = {0x0404, 0x0177};	//PSKEY_PCM_LOW_JITTER_CONFIG	,256K, 8K sync
#endif

 
    uint16_t    bol_16 = 0x0000;
    uint16_t    bol_17 = 0x0006;

    //uint16_t    bol_18 = 0x01d8;  //115k2 (0x01d8)  230k4 (0x03b0)  460k8 (0x075f)
    uint16_t    bol_18 = 0x0ebf;    //0ebf=921600,161e=1382400,1d7e=1843200,
    //uint16_t    bol_18 = 0x075f;
    uint16_t    bol_19 = 0x082e;    
//    uint16_t    bol_20 = 0x0006;

	
    psset(transport,0x0001, bol_1, 0x0004);
    psset(transport,0x01f6, &bol_2, 0x0001);
    psset(transport,0x01f9, &bol_3, 0x0001);
    psset(transport,0x0205, &bol_4, 0x0001);
    psset(transport,0x0246, &bol_5, 0x0001);
    psset(transport,0x023b, &bol_6, 0x0001);
    psset(transport,0x01b3, bol_7, 0x0002);
    psset(transport,0x01fe, &bol_8, 0x0001);
    psset(transport,0x01b1, &bol_9, 0x0001);
    psset(transport,0x01b2, &bol_10, 0x0001);
    psset(transport,0x01ab, &bol_11, 0x0001);
    psset(transport,0x01ac, &bol_12, 0x0001);
    psset(transport,0x01b5, &bol_13, 0x0001);
    psset(transport,0x01b6, &bol_14, 0x0001);
    psset(transport,0x01ba, bol_15, 0x0002);    //PSKEY_PCM_LOW_JITTER_CONFIG
    psset(transport,0x024d, &bol_16, 0x0001);
    psset(transport,0x0017, &bol_17, 0x0001);
    psset(transport,0x01be, &bol_18, 0x0001);    //UART_BAUDRATE
    psset(transport,0x01bf, &bol_19, 0x0001);
    //psset(transport,0x01be, &bol_20, 0x0001);
//johnny_V24_2_e
	return 0;
}

static int cmd_pscheck(int transport, int argc, char *argv[])
{
	uint8_t array[256];
	uint16_t pskey, size;
	int i;

	OPT_HELP(1, NULL);

	psr_read(argv[0]);

	while (psr_get(&pskey, array, &size) == 0) {
		printf("0x%04x =", pskey);
		for (i = 0; i < size; i++)
			printf(" 0x%02x", array[i]);
		printf("\n");
	}

	return 0;
}

static struct {
	char *str;
	int (*func)(int transport, int argc, char *argv[]);
	char *arg;
	char *doc;
} commands[] = {
	{ "builddef",  cmd_builddef,  "",                    "Get build definitions"          },
	{ "keylen",    cmd_keylen,    "<handle>",            "Get current crypt key length"   },
	{ "clock",     cmd_clock,     "",                    "Get local Bluetooth clock"      },
	{ "rand",      cmd_rand,      "",                    "Get random number"              },
	{ "chiprev",   cmd_chiprev,   "",                    "Get chip revision"              },
	{ "buildname", cmd_buildname, "",                    "Get the full build name"        },
	{ "panicarg",  cmd_panicarg,  "",                    "Get panic code argument"        },
	{ "faultarg",  cmd_faultarg,  "",                    "Get fault code argument"        },
	{ "coldreset", cmd_coldreset, "",                    "Perform cold reset"             },
	{ "warmreset", cmd_warmreset, "",                    "Perform warm reset"             },
	{ "disabletx", cmd_disabletx, "",                    "Disable TX on the device"       },
	{ "enabletx",  cmd_enabletx,  "",                    "Enable TX on the device"        },
	{ "singlechan",cmd_singlechan,"<channel>",           "Lock radio on specific channel" },
	{ "hoppingon", cmd_hoppingon, "",                    "Revert to channel hopping"      },
	{ "rttxdata1", cmd_rttxdata1, "<freq> <level>",      "TXData1 radio test"             },
	{ "radiotest", cmd_radiotest, "<freq> <level> <id>", "Run radio tests"                },
	{ "memtypes",  cmd_memtypes,  NULL,                  "Get memory types"               },
	{ "psget",     cmd_psget,     "<key>",               "Get value for PS key"           },
	{ "psset",     cmd_psset,     "<key> <value>",       "Set value for PS key"           },
	{ "psclr",     cmd_psclr,     "<key>",               "Clear value for PS key"         },
	{ "pslist",    cmd_pslist,    NULL,                  "List all PS keys"               },
	{ "psread",    cmd_psread,    NULL,                  "Read all PS keys"               },
	{ "psload",    cmd_psload,    "<file>",              "Load all PS keys from PSR file" },
	{ "pscheck",   cmd_pscheck,   "<file>",              "Check PSR file"                 },
	{ NULL }
};

static void usage(void)
{
	int i, pos = 0;

	printf("bccmd - Utility for the CSR BCCMD interface\n\n");
	printf("Usage:\n"
		"\tbccmd [options] <command>\n\n");

	printf("Options:\n"
		"\t-t <transport>     Select the transport\n"
		"\t-d <device>        Select the device\n"
		"\t-h, --help         Display help\n"
		"\n");

	printf("Transports:\n"
		"\tHCI USB BCSP H4 3WIRE\n\n");

	printf("Commands:\n");
	for (i = 0; commands[i].str; i++)
		printf("\t%-10s %-20s\t%s\n", commands[i].str,
		commands[i].arg ? commands[i].arg : " ",
		commands[i].doc);
	printf("\n");

	printf("Keys:\n\t");
	for (i = 0; storage[i].pskey; i++) {
		printf("%s ", storage[i].str);
		pos += strlen(storage[i].str) + 1;
		if (pos > 60) {
			printf("\n\t");
			pos = 0;
		}
	}
	printf("\n");
}

static struct option main_options[] = {
	{ "transport",	1, 0, 't' },
	{ "device",	1, 0, 'd' },
	{ "help",	0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
	char *device = NULL;
	int i, err, opt, transport = CSR_TRANSPORT_HCI;

      LOGI("jeff Bccmd main++, bt master,921600");
	while ((opt=getopt_long(argc, argv, "+t:d:i:h", main_options, NULL)) != EOF) {
		switch (opt) {
		case 't':
			if (!strcasecmp(optarg, "hci"))
				transport = CSR_TRANSPORT_HCI;
			else if (!strcasecmp(optarg, "usb"))
				transport = CSR_TRANSPORT_USB;
			else if (!strcasecmp(optarg, "bcsp"))
				transport = CSR_TRANSPORT_BCSP;
			else if (!strcasecmp(optarg, "h4"))
				transport = CSR_TRANSPORT_H4;
			else if (!strcasecmp(optarg, "h5"))
				transport = CSR_TRANSPORT_3WIRE;
			else if (!strcasecmp(optarg, "3wire"))
				transport = CSR_TRANSPORT_3WIRE;
			else if (!strcasecmp(optarg, "twutl"))
				transport = CSR_TRANSPORT_3WIRE;
			else
				transport = CSR_TRANSPORT_UNKNOWN;
			break;

		case 'd':
		case 'i':
			device = strdup(optarg);
			break;

		case 'h':
		default:
			usage();
			exit(0);
		}
	}

	argc -= optind;
	argv += optind;
	optind = 0;

	if (argc < 1) {
		usage();
		exit(1);
	}

	if (transport_open(transport, device) < 0)
		exit(1);

	if (device)
		free(device);

//johnny_V24_2_s
/*	for (i = 0; commands[i].str; i++) {
		if (strcasecmp(commands[i].str, argv[0]))
			continue;

		err = commands[i].func(transport, argc, argv);

		transport_close(transport);

		if (err < 0) {
			fprintf(stderr, "Can't execute command: %s (%d)\n",
							strerror(errno), errno);
			exit(1);
		}

		exit(0);
	}

	fprintf(stderr, "Unsupported command\n");
*/
#if 0
	LOGI("Bccmd psset_bdaddr");
	psset_bdaddr(transport);
	usleep(100);	
	LOGI("Bccmd psset_uartbaud");
	psset_uartbaud(transport);
	usleep(100);
      LOGI("Bccmd psset_anafreq");	
	psset_anafreq(transport);
	usleep(100);
#endif

      //jeff
      cmd_psload_default(transport);
	//cmd_psload_semco(transport);
	
	psset_warmreset(transport);	
	transport_close(transport);
	LOGI("Bccmd main--");
	return 0;
//johnny_V24_2_e
	exit(1);
}
