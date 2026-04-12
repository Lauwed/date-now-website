#include <enums.h>
#include <jwt.h>
#include <lib/email.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/user.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

#define EMAIL_REQUIRED_MESSAGE "Email is required"
#define NO_TOKEN_GIVEN "No token given"

void send_subscription_mail(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply,
                            const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if POST
  if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== SEND SUBSCRIBE MAIL ==="));

    // Check if body and validate JSON
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BODY_REQUIRED_MESSAGE));
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(JSON_ERROR_MESSAGE));
      return;
    }

    // Email mandatory
    int offset, length;
    char *email = NULL;
    offset = mg_json_get(msg->body, "$.email", &length);
    if (offset < 0) {
      ERROR_REPLY_400(EMAIL_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(EMAIL_REQUIRED_MESSAGE));
      return;
    } else {
      email = malloc(length);
      strncpy(email, msg->body.buf + offset + 1, length - 2);
    }

    // Generate JWT of confirmation (email, exp: now + 24h)
    jwt_t *jwt = NULL;
    jwt_new(&jwt);
    jwt_add_grant(jwt, "email", email);
    jwt_add_grant_int(jwt, "exp", time(NULL) + 86400);
    jwt_set_alg(jwt, JWT_ALG_HS256, (unsigned char *)secret, strlen(secret));

    char *jwt_str = jwt_encode_str(jwt);
    jwt_free(jwt);

    printf("JWT TOKEN GENERATED:\t%s\n", jwt_str);

    // Send mail with link /confirm?token=<jwt>
    char html[512];
    snprintf(html, sizeof(html),
             "Clique ici pour confirmer : "
             "<a href='https://datenow.com/confirm?token=%s'>Confirmer</a>",
             secret);
    int mail_sent = send_mail(
        email, "Confim subscription to Date.now()'s Newsletter", html);

    free(email);
    if (mail_sent != 0) {
      ERROR_REPLY_400(EMAIL_ERROR_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(EMAIL_ERROR_MESSAGE));
      return;
    }

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", jwt_str);
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}

void subscribe_user(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if GET
  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== CONFIRM SUBSCRIPTION ==="));

    // Check if token
    char token_buf[1024] = "";
    int token_decoded_len =
        mg_http_get_var(&msg->query, "token", token_buf, sizeof(token_buf));
    if (token_decoded_len > 0 && token_decoded_len < 1024) {
      token_buf[token_decoded_len] = '\0';
    } else {
      ERROR_REPLY_400(NO_TOKEN_GIVEN);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(NO_TOKEN_GIVEN));
      return;
    }

    printf("QUERY PARAMS:\tTOKEN: %s\n", token_buf);

    // Check JWT
    jwt_t *decoded = NULL;
    int is_decoded = jwt_decode(&decoded, token_buf, (unsigned char *)secret,
                                strlen(secret));

    if (is_decoded != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BAD_JWT_TOKEN));
      return;
    }

    // Check JWT expired
    long exp = jwt_get_grant_int(decoded, "exp");
    if (time(NULL) > exp) {
      ERROR_REPLY_400(JWT_EXPIRED);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(JWT_EXPIRED));
      jwt_free(decoded);
      return;
    }

    // Get Email from JWT
    // Will be freed with jwt_free
    const char *email = jwt_get_grant(decoded, "email");

    printf("TOKEN GRANTS:\tEXP: %ld\tEMAIL: %s\n", exp, email);

    // Create user in DB
    struct user user = {.email = (char *)email, .role = "USER"};
    int query_code = add_user(&user);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
      HANDLE_QUERY_CODE;
      jwt_free(decoded);

      return;
    } else {
      mg_http_reply(c, 201, JSON_HEADER,
                    "{ \"message\": \"User successfully subscribed\" }");
      printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY SUBSCRIBED ==="));
      jwt_free(decoded);
    }
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}

void register_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply) {}

void send_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply) {}

void login_user(struct mg_connection *c, struct mg_http_message *msg,
                struct error_reply *error_reply) {}
