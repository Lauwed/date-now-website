#include <lib/mongoose.h>
#include <structs.h>

void send_subscription_mail(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply) {
  // Check if POST
  // Check if body and validate JSON
  // Email mandatory
  //
  // Generate JWT of confirmation (email, exp: now + 24h)
  // Send mail with link /confirm?token=<jwt>
}

void subscribe_user(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply) {
  // Checki if GET
  // Check if token
  //
  // Check JWT - If expired refuse
  // Get Email from JWT
  // Create user in DB
}

void register_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply) {}

void send_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply) {}

void login_user(struct mg_connection *c, struct mg_http_message *msg,
                struct error_reply *error_reply) {}
