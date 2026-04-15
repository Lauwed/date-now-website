
#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_issue_sponsors_res(struct mg_connection *c,
                             struct mg_http_message *msg, int issue_id,
                             struct error_reply *error_reply,
                             const char *secret);

void send_issue_sponsor_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id,
                            char *name, struct error_reply *error_reply,
                            const char *secret);
