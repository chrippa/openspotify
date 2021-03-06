/*
 * $Id: handlers.c 398 2009-07-28 23:16:37Z noah-w $
 *
 * Default handlers for different types of commands
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "packet.h"
#include "sp_opaque.h"
#include "util.h"

int handle_secret_block (sp_session * session, unsigned char *payload, int len)
{
	unsigned int *t;

	if (len != 336) {
		DSFYDEBUG ("Got cmd=0x02 with len %d, expected 336!\n", len);
		return -1;
	}

	t = (unsigned int *) payload;
	DSFYDEBUG ("Initial time %u (%ld seconds from now)\n",
		 ntohl (*t), time (NULL) - ntohl (*t));
	t++;
	DSFYDEBUG ("Future time %u (%ld seconds in the future)\n",
		 ntohl (*t), ntohl (*t) - time (NULL));

	t++;
	DSFYDEBUG ("Next value is %u\n", ntohl (*t));
	t++;
	DSFYDEBUG ("Next value is %u\n", ntohl (*t));

	#if 0 /* FIXME */
	assert (memcmp (session->rsa_pub_exp, payload + 16, 128) == 0);
	#endif

	/* At payload+16+128 is a  144 bytes (1536-bit) RSA signature */

	/*
	 * Actually the cache hash is sent before the server has sent any
	 * packets. It's just put here out of convenience, because this is
	 * one of the first packets ever by the server, and also not
	 * repeated during a session.
	 *
	 */

	return cmd_send_cache_hash (session);
}

int handle_ping (sp_session * session, unsigned char *payload, int len)
{

	/* Store timestamp and respond to the request */
	time_t t;
	assert (len == 4);
	memcpy (&t, payload, 4);
#if 0 /* FIXME */
	session->user_info.last_ping = ntohl (t);
#endif

	return cmd_ping_reply (session);
}

int handle_channel (int cmd, unsigned char *payload, int len)
{
	if (cmd == CMD_CHANNELERR) {
		DSFYDEBUG ("Channel %d got error %d (0x%02x)\n",
			 ntohs (*(unsigned short *) payload),
			 ntohs (*(unsigned short *) (payload + 2)),
			 ntohs (*(unsigned short *) (payload + 2)))
	}
	
	return channel_process (payload, len, cmd == CMD_CHANNELERR);
}

int handle_aeskey (unsigned char *payload, int len)
{
	CHANNEL *ch;
	int ret;

	DSFYDEBUG ("Server said 0x0d (AES key) for channel %d\n",
		   ntohs (*(unsigned short *) (payload + 2)))
		if ((ch =
		     channel_by_id (ntohs
				    (*(unsigned short *) (payload + 2)))) !=
			   NULL) {
		ret = ch->callback (ch, payload + 4, len - 4);
		channel_unregister (ch);
	}
	else {
		DSFYDEBUG
			("Command 0x0d: Failed to find channel with ID %d\n",
			 ntohs (*(unsigned short *) (payload + 2)));
	}

	return ret;
}

static int handle_countrycode (sp_session * session, unsigned char *payload, int len)
{
#if 0 /* FIXME */
	int i;
	for (i = 0; i < len && i < (int)sizeof session->user_info.country; i++)
		session->user_info.country[i] = payload[i];
	session->user_info.country[i] = 0;
#endif
	return 0;
}

static int handle_prodinfo (sp_session * session, unsigned char *payload, int len)
{
#if 0 /* FIXME */
	xml_parse_prodinfo(&session->user_info, payload, len);
#endif
	return 0;
}

int handle_packet (sp_session * session,
		   int cmd, unsigned char *payload, int len)
{
	int error = 0;

	switch (cmd) {
	case CMD_SECRETBLK:
		error = handle_secret_block (session, payload, len);
		break;

	case CMD_PING:
		error = handle_ping (session, payload, len);
		break;

	case CMD_CHANNELDATA:
		error = handle_channel (cmd, payload, len);
		break;

	case CMD_CHANNELERR:
		error = handle_channel (cmd, payload, len);
		break;

	case CMD_AESKEY:
		error = handle_aeskey (payload, len);
		break;

	case CMD_SHAHASH:
		break;

	case CMD_COUNTRYCODE:
		error = handle_countrycode (session, payload, len);
		break;

	case CMD_P2P_INITBLK:
		DSFYDEBUG ("Server said 0x21 (P2P initalization block)\n")
			break;

	case CMD_NOTIFY:
		/* HTML-notification, shown in a yellow bar in the official client */
		break;

	case CMD_PRODINFO:
		/* Payload is uncompressed XML */
		error = handle_prodinfo (session, payload, len);
		break;

	case CMD_WELCOME:
		break;

	case CMD_PAUSE:
		/* Play token lost */
		break;
	}

	return error;
}
