/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <glib.h>

#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "att.h"
#include "btio.h"
#include "gattrib.h"
#include "glib-helper.h"
#include "gatt.h"
#include "gatttool.h"

static GIOChannel *iochannel = NULL;
static GAttrib *attrib = NULL;
static GMainLoop *event_loop;
static GString *prompt;

static gchar *opt_src = NULL;
static gchar *opt_dst = NULL;
static gchar *opt_sec_level = NULL;
static int opt_psm = 0;
static int opt_mtu = 0;

static void cmd_help(int argcp, char **argvp);

enum state {
	STATE_DISCONNECTED,
	STATE_CONNECTING,
	STATE_CONNECTED
} conn_state;

static char *get_prompt(void)
{
	if (conn_state == STATE_CONNECTING) {
		g_string_assign(prompt, "Connecting... ");
		return prompt->str;
	}

	if (conn_state == STATE_CONNECTED)
		g_string_assign(prompt, "[CON]");
	else
		g_string_assign(prompt, "[   ]");

	if (opt_dst)
		g_string_append_printf(prompt, "[%17s]", opt_dst);
	else
		g_string_append_printf(prompt, "[%17s]", "");

	if (opt_psm)
		g_string_append(prompt, "[BR]");
	else
		g_string_append(prompt, "[LE]");

	g_string_append(prompt, "> ");

	return prompt->str;
}


static void set_state(enum state st)
{
	conn_state = st;
	rl_set_prompt(get_prompt());
	rl_redisplay();
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	if (err) {
		printf("connect error: %s\n", err->message);
		set_state(STATE_DISCONNECTED);
		return;
	}

	attrib = g_attrib_new(iochannel);
	set_state(STATE_CONNECTED);
}

static void primary_all_cb(GSList *services, guint8 status, gpointer user_data)
{
	GSList *l;

	if (status) {
		printf("Discover all primary services failed: %s\n",
							att_ecode2str(status));
		return;
	}

	printf("\n");
	for (l = services; l; l = l->next) {
		struct att_primary *prim = l->data;
		printf("attr handle: 0x%04x, end grp handle: 0x%04x "
			"uuid: %s\n", prim->start, prim->end, prim->uuid);
	}

	rl_forced_update_display();
}

static void primary_by_uuid_cb(GSList *ranges, guint8 status,
							gpointer user_data)
{
	GSList *l;

	if (status) {
		printf("Discover primary services by UUID failed: %s\n",
							att_ecode2str(status));
		return;
	}

	printf("\n");
	for (l = ranges; l; l = l->next) {
		struct att_range *range = l->data;
		g_print("Starting handle: 0x%04x Ending handle: 0x%04x\n",
						range->start, range->end);
	}

	rl_forced_update_display();
}

static void char_cb(GSList *characteristics, guint8 status, gpointer user_data)
{
	GSList *l;

	if (status) {
		printf("Discover all characteristics failed: %s\n",
							att_ecode2str(status));
		return;
	}

	printf("\n");
	for (l = characteristics; l; l = l->next) {
		struct att_char *chars = l->data;

		printf("handle: 0x%04x, char properties: 0x%02x, char value "
				"handle: 0x%04x, uuid: %s\n", chars->handle,
				chars->properties, chars->value_handle,
				chars->uuid);
	}

	rl_forced_update_display();
}

static void char_desc_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	struct att_data_list *list;
	guint8 format;
	int i;

	if (status != 0) {
		printf("Discover all characteristic descriptors failed: "
						"%s\n", att_ecode2str(status));
		return;
	}

	list = dec_find_info_resp(pdu, plen, &format);
	if (list == NULL)
		return;

	printf("\n");
	for (i = 0; i < list->num; i++) {
		char uuidstr[MAX_LEN_UUID_STR];
		uint16_t handle;
		uint8_t *value;
		uuid_t uuid;

		value = list->data[i];
		handle = att_get_u16(value);

		if (format == 0x01)
			sdp_uuid16_create(&uuid, att_get_u16(&value[2]));
		else
			sdp_uuid128_create(&uuid, &value[2]);

		sdp_uuid2strn(&uuid, uuidstr, MAX_LEN_UUID_STR);
		printf("handle: 0x%04x, uuid: %s\n", handle, uuidstr);
	}

	att_data_list_free(list);

	rl_forced_update_display();
}

static void cmd_exit(int argcp, char **argvp)
{
	rl_callback_handler_remove();
	g_main_loop_quit(event_loop);
}

static void cmd_connect(int argcp, char **argvp)
{
	if (conn_state != STATE_DISCONNECTED)
		return;

	if (argcp > 1) {
		g_free(opt_dst);
		opt_dst = strdup(argvp[1]);
	}

	if (opt_dst == NULL) {
		printf("Remote Bluetooth address required\n");
		return;
	}

	set_state(STATE_CONNECTING);
	iochannel = gatt_connect(opt_src, opt_dst, opt_sec_level, opt_psm,
						opt_mtu, connect_cb);
	if (iochannel == NULL)
		set_state(STATE_DISCONNECTED);

	return;
}

static void cmd_disconnect(int argcp, char **argvp)
{
	if (conn_state == STATE_DISCONNECTED)
		return;

	g_attrib_unref(attrib);
	attrib = NULL;

	g_io_channel_shutdown(iochannel, FALSE, NULL);
	g_io_channel_unref(iochannel);
	iochannel = NULL;

	set_state(STATE_DISCONNECTED);

	return;
}

static void cmd_primary(int argcp, char **argvp)
{
	uuid_t uuid;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		return;
	}

	if (argcp == 1) {
		gatt_discover_primary(attrib, NULL, primary_all_cb, NULL);
		return;
	}

	if (bt_string2uuid(&uuid, argvp[1]) < 0) {
		printf("Invalid UUID\n");
		return;
	}

	gatt_discover_primary(attrib, &uuid, primary_by_uuid_cb, NULL);
}

static int strtohandle(const char *src)
{
	char *e;
	int dst;

	errno = 0;
	dst = strtoll(src, &e, 16);
	if (errno != 0 || *e != '\0')
		return -EINVAL;

	return dst;
}

static void cmd_char(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		return;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			printf("Invalid start handle: %s\n", argvp[1]);
			return;
		}
	}

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			printf("Invalid end handle: %s\n", argvp[2]);
			return;
		}
	}

	gatt_discover_char(attrib, start, end, char_cb, NULL);
}

static void cmd_char_desc(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		return;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			printf("Invalid start handle: %s\n", argvp[1]);
			return;
		}
	}

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			printf("Invalid end handle: %s\n", argvp[2]);
			return;
		}
	}

	gatt_find_info(attrib, start, end, char_desc_cb, NULL);
}

static struct {
	const char *cmd;
	void (*func)(int argcp, char **argvp);
	const char *params;
	const char *desc;
} commands[] = {
	{ "help",		cmd_help,	"",
		"Show this help"},
	{ "exit",		cmd_exit,	"",
		"Exit interactive mode" },
	{ "connect",		cmd_connect,	"[address]",
		"Connect to a remote device" },
	{ "disconnect",		cmd_disconnect,	"",
		"Disconnect from a remote device" },
	{ "primary",		cmd_primary,	"[UUID]",
		"Primary Service Discovery" },
	{ "characteristics",	cmd_char,	"[start hnd] [end hnd]",
		"Characteristics Discovery" },
	{ "char-desc",		cmd_char_desc,	"[start hnd] [end hnd]",
		"Characteristics Descriptor Discovery" },
	{ NULL, NULL, NULL}
};

static void cmd_help(int argcp, char **argvp)
{
	int i;

	for (i = 0; commands[i].cmd; i++)
		printf("%-15s %-25s %s\n", commands[i].cmd,
				commands[i].params, commands[i].desc);
}

static void parse_line(char *line_read)
{
	gchar **argvp;
	int argcp;
	int i;

	if (line_read == NULL) {
		printf("\n");
		cmd_exit(0, NULL);
		return;
	}

	line_read = g_strstrip(line_read);

	if (*line_read == '\0')
		return;

	add_history(line_read);

	g_shell_parse_argv(line_read, &argcp, &argvp, NULL);

	for (i = 0; commands[i].cmd; i++)
		if (strcasecmp(commands[i].cmd, argvp[0]) == 0)
			break;

	if (commands[i].cmd)
		commands[i].func(argcp, argvp);
	else
		printf("%s: command not found\n", argvp[0]);

	g_strfreev(argvp);
}

static gboolean prompt_read(GIOChannel *chan, GIOCondition cond,
							gpointer user_data)
{
	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		g_io_channel_unref(chan);
		return FALSE;
	}

	rl_callback_read_char();

	return TRUE;
}

int interactive(gchar *dst, int psm)
{
	GIOChannel *pchan;
	gint events;

	opt_sec_level = strdup("low");

	opt_dst = strdup(dst);
	opt_psm = psm;

	prompt = g_string_new(NULL);

	event_loop = g_main_loop_new(NULL, FALSE);

	pchan = g_io_channel_unix_new(fileno(stdin));
	g_io_channel_set_close_on_unref(pchan, TRUE);
	events = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL;
	g_io_add_watch(pchan, events, prompt_read, NULL);

	rl_callback_handler_install(get_prompt(), parse_line);

	g_main_loop_run(event_loop);

	rl_callback_handler_remove();
	cmd_disconnect(0, NULL);
	g_io_channel_unref(pchan);
	g_main_loop_unref(event_loop);
	g_string_free(prompt, TRUE);

	g_free(opt_dst);
	g_free(opt_sec_level);

	return 0;
}
