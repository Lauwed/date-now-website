#pragma once

#include <lib/mongoose.h>
#include <structs.h>

void send_subscription_mail(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply,
                            const char *secret);

void subscribe_user(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret);

void register_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply, const char *secret);

void generate_totpseed_user(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply);

void send_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret);

void login_user(struct mg_connection *c, struct mg_http_message *msg,
                struct error_reply *error_reply, const char *secret);

void is_user_logged(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret,
                    int *user_logged);
