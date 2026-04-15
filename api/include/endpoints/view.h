
#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_views_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret);
