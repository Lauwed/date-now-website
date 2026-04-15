#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_issues_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret);

void send_issue_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply,
                    const char *secret);
