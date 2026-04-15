#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_sponsors_res(struct mg_connection *c, struct mg_http_message *msg,
                       struct error_reply *error_reply, const char *secret);

void send_sponsor_res(struct mg_connection *c, struct mg_http_message *msg,
                      char *name, struct error_reply *error_reply,
                      const char *secret);
