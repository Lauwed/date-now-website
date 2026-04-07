#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_issue_authors_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id,
                            struct error_reply *error_reply);

void send_issue_author_res(struct mg_connection *c, struct mg_http_message *msg,
                           int issue_id, int id,
                           struct error_reply *error_reply);
