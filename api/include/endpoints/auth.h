#pragma once

#include <lib/mongoose.h>
#include <structs.h>

/*
 * Vérifie le Bearer token dans le header Authorization.
 * Retourne 0 si authentifié, -1 sinon.
 */
int check_auth(struct mg_http_message *msg);

void send_auth_request_res(struct mg_connection *c, struct mg_http_message *msg,
                           struct error_reply *error_reply);
void send_auth_verify_res(struct mg_connection *c, struct mg_http_message *msg,
                          struct error_reply *error_reply);
void send_auth_session_res(struct mg_connection *c, struct mg_http_message *msg,
                           struct error_reply *error_reply);
