/* 
 * Opauqe data structures and definitions
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */
#ifndef LIBOPENSPOTIFY_OPAQUE_H
#define LIBOPENSPOTIFY_OPAQUE_H

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <spotify/api.h>

#include "login.h"
#include "shn.h"



/* For request.c */
typedef enum { 
	REQ_STATE_NEW = 0,
	REQ_STATE_RUNNING,
	REQ_STATE_RETURNED,
	REQ_STATE_PROCESSED
} sp_request_state;

typedef enum {
	REQ_TYPE_LOGIN = 0,
	REQ_TYPE_LOGOUT,
	REQ_TYPE_PLAY_TOKEN_LOST
} sp_request_type;

typedef struct sp_request {
	sp_request_type type;
	sp_request_state state;
	void *input;
	void *output;
	sp_error error;
	struct sp_request *next;
} sp_request;

/* sp_link.c */
struct sp_link {
	sp_linktype type;
	
	union {
		void        *data;
		sp_track    *track;
		sp_album    *album;
		sp_artist   *artist;
		sp_search   *search;
		sp_playlist *playlist;
	} data;
	
	int refs;
};

/* sp_user.c */
struct sp_user {
	char *canonical_name;
	char *display_name;

	struct sp_user *next;
};

/* sp_session.c and most other API functions */
struct sp_session {
	void *userdata;

	struct sp_user *user;

	sp_session_callbacks *callbacks;

	/* Low-level network stuff */
	int sock;

	char username[256];
	char password[256];

	/* Used when logging in */
	struct login_ctx *login;


        /* Stream cipher context */
        unsigned int key_recv_IV;
        unsigned int key_send_IV;
        shn_ctx shn_recv;
        shn_ctx shn_send;

	struct buf *packet;


	/* Requests scoreboard */
	sp_request *requests;


	/* High level connection state */
	sp_connectionstate connectionstate;


#ifdef _WIN32
	HANDLE request_mutex;
	HANDLE idle_wakeup;
	HANDLE thread_main;
	HANDLE thread_network;
#else
	pthread_mutex_t request_mutex;
	pthread_cond_t idle_wakeup;
	pthread_t thread_main;
	pthread_t thread_network;
#endif
};

#endif
