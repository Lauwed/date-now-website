#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_subscription_mail(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply);

void subscribe_user(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply);

void register_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply);

void send_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply);

void login_user(struct mg_connection *c, struct mg_http_message *msg,
                struct error_reply *error_reply);
