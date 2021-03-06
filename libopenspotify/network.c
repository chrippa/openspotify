/*
 * This file handles libopenspotify's communication with Spotify's service
 * and is run in a seperate thread.
 *
 * Communication with the main thread is done via calls in request.c
 *
 */


#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif
#include <spotify/api.h>

#include "debug.h"
#include "login.h"
#include "network.h"
#include "packet.h"
#include "request.h"
#include "sp_opaque.h"
#include "shn.h"


static int process_request(sp_session *s, sp_request *req);
static int process_login_request(sp_session *s, sp_request *req);
static int process_logout_request(sp_session *s, sp_request *req);


/*
 * The thread routine started by sp_session_init()
 * It does not return but can be terminated by sp_session_release()
 *
 */

#ifdef _WIN32
DWORD WINAPI network_thread(LPVOID data) {
#else
void *network_thread(void *data) {
	pthread_mutex_t idle_mutex;
#endif
	sp_session *s = (sp_session *)data;
	sp_request *req;
	int ret;

#ifdef _WIN32
	/* Initialize Winsock */
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);
#endif

	for(;;) {
		request_cleanup(s);

		/* No locking needed since we're in control of request_cleanup() */
		for(req = s->requests; req; req = req->next) {
			if(req->state != REQ_STATE_NEW && req->state != REQ_STATE_RUNNING)
				continue;

			DSFYDEBUG("Processing request of type %d in state %d\n", req->type, req->state);
			ret = process_request(s, req);
			DSFYDEBUG("Got return value %d from processing of event of type %d in state %d\n", ret, req->type, req->state);
		}


		/* Packets can only be processed once we're logged in */
		if(s->connectionstate != SP_CONNECTION_STATE_LOGGED_IN) {
			if(s->requests == NULL) {
				DSFYDEBUG("Sleeping because there's nothing to do\n");
#ifdef _WIN32
				WaitForSingleObject(s->idle_wakeup, INFINITE);
#else
				pthread_mutex_init(&idle_mutex, NULL);
				pthread_mutex_lock(&idle_mutex);
				pthread_cond_wait(&s->idle_wakeup, &idle_mutex);
				pthread_mutex_destroy(&idle_mutex);
#endif
				DSFYDEBUG("Woke up, a new request was posted\n");
			}

			continue;
		}


		/* Read and process zero or more packets */
		ret = packet_read_and_process(s);
		if(ret < 0) {
			DSFYDEBUG("process_packets() returned %d, disconnecting!\n", ret);
#ifdef _WIN32
			closesocket(s->sock);
#else
			close(s->sock);
#endif
			s->sock = -1;

			s->connectionstate = SP_CONNECTION_STATE_DISCONNECTED;

			request_post_result(s, REQ_TYPE_LOGOUT, SP_ERROR_OTHER_TRANSIENT, NULL);
		}
	}


}


/*
 * Route request handling to the appropriate handlers
 *
 */
static int process_request(sp_session *s, sp_request *req) {
	switch(req->type) {
	case REQ_TYPE_LOGIN:
		return process_login_request(s, req);
		break;

	case REQ_TYPE_LOGOUT:
		return process_logout_request(s, req);
		break;

	default:
		break;
	}

	return 0;
}


/*
 * High-level handling of login requests
 * Uses login specific routines from login.c
 *
 */
static int process_login_request(sp_session *s, sp_request *req) {
	int ret;
	sp_error error;
	unsigned char key_recv[32], key_send[32];

	if(req->state == REQ_STATE_NEW) {
		req->state = REQ_STATE_RUNNING;
		s->login = login_create(s->username, s->password);
		if(s->login == NULL)
			return request_set_result(s, req, SP_ERROR_OTHER_TRANSIENT, NULL);
	}

	ret = login_process(s->login);
	if(ret == 0)
		return 0;
	else if(ret == 1) {
		login_export_session(s->login, &s->sock, key_recv, key_send);
		login_release(s->login);
		s->login = NULL;

		shn_key(&s->shn_recv, key_recv, sizeof(key_recv));
		s->key_recv_IV = 0;

		shn_key(&s->shn_send, key_send, sizeof(key_send));
		s->key_send_IV = 0;

		s->connectionstate = SP_CONNECTION_STATE_LOGGED_IN;

		return request_set_result(s, req, SP_ERROR_OK, NULL);
	}

	switch(s->login->error) {
	case SP_LOGIN_ERROR_DNS_FAILURE:
	case SP_LOGIN_ERROR_NO_MORE_SERVERS:
		error = SP_ERROR_UNABLE_TO_CONTACT_SERVER;
		break;

	case SP_LOGIN_ERROR_UPGRADE_REQUIRED:
		error = SP_ERROR_CLIENT_TOO_OLD;
		break;

	case SP_LOGIN_ERROR_USER_BANNED:
		error = SP_ERROR_USER_BANNED;
		break;

	case SP_LOGIN_ERROR_USER_NOT_FOUND:
	case SP_LOGIN_ERROR_BAD_PASSWORD:
		error = SP_ERROR_BAD_USERNAME_OR_PASSWORD;
		break;

	case SP_LOGIN_ERROR_USER_NEED_TO_COMPLETE_DETAILS:
	case SP_LOGIN_ERROR_USER_COUNTRY_MISMATCH:
	case SP_LOGIN_ERROR_OTHER_PERMANENT:
		error = SP_ERROR_OTHER_PERMAMENT;
		break;

	case SP_LOGIN_ERROR_SOCKET_ERROR:
	default:
		error = SP_ERROR_OTHER_TRANSIENT;
		break;
	}

	login_release(s->login);
	s->login = NULL;

	return request_set_result(s, req, error, NULL);
}


/*
 * High-level handling of logout requests
 * FIXME: Do we need to send some kind of "goodbye" packet first?
 *
 */
static int process_logout_request(sp_session *session, sp_request *req) {

	if(session->sock != -1) {
#ifdef _WIN32
		closesocket(session->sock);
#else
		close(session->sock);
#endif
		session->sock = -1;
	}

	session->connectionstate = SP_CONNECTION_STATE_LOGGED_OUT;

	return request_set_result(session, req, SP_ERROR_OK, NULL);
}
