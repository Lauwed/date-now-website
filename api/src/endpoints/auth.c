/**
 * @file auth.c
 * @brief Authentication endpoint handler implementations.
 */

#include <enums.h>
#include <jwt.h>
#include <lib/email.h>
#include <lib/mongoose.h>
#include <lib/totp.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/user.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

void is_user_logged(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret,
                    int *user_logged) {
  struct mg_str *auth_header = mg_http_get_header(msg, "Authorization");
  if (auth_header == NULL) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("AUTHORIZATION HEADER MISSING"));
    return;
  }

  if (auth_header->len <= 7 || strncmp(auth_header->buf, "Bearer ", 7) != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("INVALID AUTHORIZATION HEADER"));
    return;
  }

  char *token = strndup(auth_header->buf + 7, auth_header->len - 7);

  //  Check JWT
  jwt_t *decoded = NULL;
  int is_decoded =
      jwt_decode(&decoded, token, (unsigned char *)secret, strlen(secret));

  if (is_decoded != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(BAD_JWT_MESSAGE));
    return;
  }

  // Check JWT expired
  long exp = jwt_get_grant_int(decoded, "exp");
  if (time(NULL) > exp) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(JWT_EXPIRED_MESSAGE));
    jwt_free(decoded);
    return;
  }

  int type = jwt_get_grant_int(decoded, "type");
  if (type != SESSION) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(WRONG_JWT_TYPE_MESSAGE));
    jwt_free(decoded);
    return;
  }

  // Get Email from JWT
  // Will be freed with jwt_free
  const char *email = jwt_get_grant(decoded, "email");

  // Check if email validity
  int email_valid = check_email_validity((char *)email);
  if (email_valid != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(EMAIL_VALIDITY_ERROR_MESSAGE));
    jwt_free(decoded);
    return;
  }

  // Check if user is author
  struct user *user = malloc(sizeof(struct user));
  int user_init_rc = user_init(user);
  if (user_init_rc != 0) {
    ERROR_REPLY_500;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER IS NULL"));

    return;
  }

  int query_code = get_user_by_email(user, (char *)email);
  if (query_code <= 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER NOT FIND"));
    return;
  }

  // Check if user is an author
  if (strcmp(user->role, "AUTHOR") != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER NOT AUTHOR"));
    return;
  }

  jwt_free(decoded);
  *user_logged = 1;
}

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
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    // Email mandatory
    int offset, length;
    char *email = NULL;
    offset = mg_json_get(msg->body, "$.email", &length);
    if (offset < 0) {
      ERROR_REPLY_400(EMAIL_REQUIRED_MESSAGE);
      return;
    } else {
      email = mg_json_get_str(msg->body, "$.email");
      printf("%s\n", email);

      // Check if email validity
      int email_valid = check_email_validity(email);
      if (email_valid != 0) {
        ERROR_REPLY_400(EMAIL_VALIDITY_ERROR_MESSAGE);
        return;
      }
    }

    // Check if already subscribed
    struct user *existing_user = malloc(sizeof(struct user));
    int existing_rc = get_user_by_email(existing_user, email);
    if (existing_rc == 0 && existing_user->subscribed_at > 0) {
      free(existing_user);
      free(email);
      ERROR_REPLY_409("Email already subscribed");
      return;
    }
    free(existing_user);

    // Generate JWT of confirmation (email, exp: now + 24h)
    jwt_t *jwt = NULL;
    jwt_new(&jwt);
    jwt_add_grant(jwt, "email", email);
    jwt_add_grant_int(jwt, "type", SUBSCRIPTION);
    jwt_add_grant_int(jwt, "exp", time(NULL) + 86400);
    jwt_set_alg(jwt, JWT_ALG_HS256, (unsigned char *)secret, strlen(secret));

    char *jwt_str = jwt_encode_str(jwt);
    jwt_free(jwt);

    // Send mail with link /confirm?token=<jwt>
    char html[512];
    snprintf(html, sizeof(html),
             "Clique ici pour confirmer : "
             "<a href='https://datenow.com/confirm?token=%s'>Confirmer</a>",
             jwt_str);
    int mail_sent = send_mail(
        email, "Confim subscription to Date.now()'s Newsletter", html);

    free(email);
    if (mail_sent != 0) {
      ERROR_REPLY_400(EMAIL_ERROR_MESSAGE);
      return;
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== SUBSCRIPTION MAIL SENT ==="));
    SUCCESS_REPLY_200_MSG("Confirmation mail has been correctly sent");
    return;
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}

void subscribe_user(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if POST
  if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== CONFIRM SUBSCRIPTION ==="));

    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    int offset, length;
    offset = mg_json_get(msg->body, "$.token", &length);
    if (offset < 0) {
      ERROR_REPLY_400(NO_TOKEN_MESSAGE);
      return;
    }
    char *token_heap = mg_json_get_str(msg->body, "$.token");
    if (token_heap == NULL) {
      ERROR_REPLY_400(NO_TOKEN_MESSAGE);
      return;
    }

    // Check JWT
    jwt_t *decoded = NULL;
    int is_decoded = jwt_decode(&decoded, token_heap, (unsigned char *)secret,
                                strlen(secret));
    free(token_heap);

    if (is_decoded != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BAD_JWT_MESSAGE));
      return;
    }

    // Check JWT expired
    long exp = jwt_get_grant_int(decoded, "exp");
    if (time(NULL) > exp) {
      ERROR_REPLY_400(JWT_EXPIRED_MESSAGE);
      jwt_free(decoded);
      return;
    }

    // JWT Type
    int type = jwt_get_grant_int(decoded, "type");
    if (type != SUBSCRIPTION) {
      ERROR_REPLY_400(WRONG_JWT_TYPE_MESSAGE);
      jwt_free(decoded);
      return;
    }

    // Get Email from JWT
    // Will be freed with jwt_free
    const char *email = jwt_get_grant(decoded, "email");

    // Check if email validity
    int email_valid = check_email_validity((char *)email);
    if (email_valid != 0) {
      ERROR_REPLY_400(EMAIL_VALIDITY_ERROR_MESSAGE);
      return;
    }

    printf("TOKEN GRANTS:\tEXP: %ld\tEMAIL: %s\n", exp, email);

    // Create user in DB
    struct user user = {
        .email = (char *)email, .role = "USER", .subscribed_at = time(NULL)};
    int query_code = add_user(&user);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
      HANDLE_QUERY_CODE;
      jwt_free(decoded);
    } else {
      SUCCESS_REPLY_200_MSG("User successfully subscribed");
      printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY SUBSCRIBED ==="));
      jwt_free(decoded);
      return;
    }

    return;
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}

void register_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply, const char *secret) {
  printf(TERMINAL_ENDPOINT_MESSAGE("=== REGISTER AUTHOR ==="));

  // Check if user logged
  int user_logged = 0;
  is_user_logged(c, msg, error_reply, secret, &user_logged);

  if (user_logged == 0) {
    ERROR_REPLY_401;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
    return;
  }

  // Add user
  if (msg->body.len <= 0) {
    ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
    return;
  } else if (!mg_validateJSON(msg->body)) {
    ERROR_REPLY_400(JSON_ERROR_MESSAGE);
    return;
  }

  // Body validation
  int offset, length;

  // Email required
  offset = mg_json_get(msg->body, "$.email", &length);
  if (offset < 0) {
    ERROR_REPLY_400(EMAIL_REQUIRED_MESSAGE);
    return;
  } else {
    // Email and username not existing already
    char *email = mg_json_get_str(msg->body, "$.email");
    printf("%s\n", email);

    // Check if email validity
    int email_valid = check_email_validity(email);
    if (email_valid != 0) {
      ERROR_REPLY_400(EMAIL_VALIDITY_ERROR_MESSAGE);
      return;
    }

    char *username = NULL;
    offset = mg_json_get(msg->body, "$.username", &length);
    if (offset >= 0) {
      username = malloc(length);
      strncpy(username, msg->body.buf + offset + 1, length - 2);
    }
    if (offset < 0) {
      ERROR_REPLY_400(USERNAME_REQUIRED_MESSAGE);
      return;
    }

    int exists = user_identity_exists(username, email);
    if (exists != 0) {
      ERROR_REPLY_400(USER_EXISTS_MESSAGE);
      return;
    };
  }

  // Hydrate
  struct user *user = malloc(sizeof(struct user));
  int user_init_rc = user_init(user);
  if (user_init_rc != 0) {
    ERROR_REPLY_500;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER IS NULL"));

    return;
  }

  // Generate totpseed
  if (totp_generate_secret(user->totp_seed) != 0) {
    ERROR_REPLY_500;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("OPENSSL ERROR"));

    free_user(user);
    return;
  }

  // Force role to AUTHOR
  user->role = "AUTHOR";
  user_hydrate(msg, user);

  // Store in DB
  int query_code = add_user(user);
  if (query_code != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
    HANDLE_QUERY_CODE;

    return;
  } else {
    mg_http_reply(c, 201, JSON_HEADER,
                  "{ \"message\": \"Author successfully created\", "
                  "\"totpseed\": \"%s\" }",
                  user->totp_seed);
    printf(TERMINAL_SUCCESS_MESSAGE("=== AUTHOR SUCCESSFULLY REGISTER ==="));
  }

  free_user(user);
}

void generate_totpseed_user(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply,
                            const char *secret) {
  // Check if user logged
  int user_logged = 0;
  is_user_logged(c, msg, error_reply, secret, &user_logged);

  if (user_logged == 0) {
    ERROR_REPLY_401;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
    return;
  }

  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if POST
  if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GENERATE NEW USER TOTP SEED ==="));

    // Check if body and validate JSON
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    // Email mandatory
    int offset, length;
    char *email = NULL;
    offset = mg_json_get(msg->body, "$.email", &length);
    if (offset < 0) {
      ERROR_REPLY_400(EMAIL_REQUIRED_MESSAGE);
      return;
    } else {
      email = mg_json_get_str(msg->body, "$.email");
      printf("%s\n", email);

      // Check if email validity
      int email_valid = check_email_validity(email);
      if (email_valid != 0) {
        ERROR_REPLY_400(EMAIL_VALIDITY_ERROR_MESSAGE);
        return;
      }
    }

    // Check if user esists
    if (user_identity_exists(NULL, email)) {
      struct user *user = malloc(sizeof(struct user));
      // Get User
      int query_code = get_user_by_email(user, email);
      free(email);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USER"));
        HANDLE_QUERY_CODE;

        return;
      } else {
        // Generate totpseed
        if (totp_generate_secret(user->totp_seed) != 0) {
          ERROR_REPLY_500;
          fprintf(stderr, TERMINAL_ERROR_MESSAGE("OPENSSL ERROR"));

          free_user(user);
          return;
        }

        // Update user
        query_code = edit_user(user);
        if (query_code != 0) {
          fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR UPDATING USER"));
          HANDLE_QUERY_CODE;
        } else {
          char seed_json[100];
          sprintf(seed_json, "{\"totpseed\": \"%s\"}", user->totp_seed);

          SUCCESS_REPLY_200(seed_json);
          printf(TERMINAL_SUCCESS_MESSAGE(
              "=== USER SEED SUCCESSFULLY UPDATED ==="));
        }

        free_user(user);
        return;
      }
    }

    ERROR_REPLY_404;
    return;
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}

void send_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if POST
  if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== SEND SUBSCRIBE MAIL ==="));

    // Check if body and validate JSON
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    // Email mandatory
    int offset, length;
    char *email = NULL;
    offset = mg_json_get(msg->body, "$.email", &length);
    if (offset < 0) {
      ERROR_REPLY_400(EMAIL_REQUIRED_MESSAGE);
      return;
    } else {
      email = mg_json_get_str(msg->body, "$.email");
      printf("%s\n", email);

      // Check if email validity
      int email_valid = check_email_validity(email);
      if (email_valid != 0) {
        ERROR_REPLY_400(EMAIL_VALIDITY_ERROR_MESSAGE);
        return;
      }
    }

    // Generate JWT of confirmation (email, exp: now + 24h)
    jwt_t *jwt = NULL;
    jwt_new(&jwt);
    jwt_add_grant(jwt, "email", email);
    jwt_add_grant_int(jwt, "type", LOGIN);
    jwt_add_grant_int(jwt, "exp", time(NULL) + 86400);
    jwt_set_alg(jwt, JWT_ALG_HS256, (unsigned char *)secret, strlen(secret));

    char *jwt_str = jwt_encode_str(jwt);
    jwt_free(jwt);

    // Send mail with link login/totp?token=<jwt>
    char html[512];
    snprintf(html, sizeof(html),
             "Clique ici pour confirmer : "
             "<a href='https://datenow.com/login/totp?token=%s'>Log in</a>",
             jwt_str);
    int mail_sent = send_mail(email, "Log in request to Date.now()", html);

    free(email);
    if (mail_sent != 0) {
      ERROR_REPLY_400(EMAIL_ERROR_MESSAGE);
      return;
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== LOGIN MAIL SENT ==="));
    SUCCESS_REPLY_200_MSG("Login mail has been correctly sent");
    return;
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}

void login_user(struct mg_connection *c, struct mg_http_message *msg,
                struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if POST
  if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== LOGIN ==="));

    // Check if body and validate JSON
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    int offset, length;
    // Token mandatory
    char *token = NULL;
    offset = mg_json_get(msg->body, "$.token", &length);
    if (offset < 0) {
      ERROR_REPLY_400(TOKEN_REQUIRED_MESSAGE);
      return;
    } else {
      token = malloc(length);
      strncpy(token, msg->body.buf + offset + 1, length - 2);
    }

    // Code mandatory
    int code = 0;
    offset = mg_json_get(msg->body, "$.code", &length);
    if (offset < 0) {
      ERROR_REPLY_400(CODE_REQUIRED_MESSAGE);
      return;
    } else {
      char *code_str = malloc(length);
      strncpy(code_str, msg->body.buf + offset + 1, length - 2);

      code = strtol(code_str, (char **)NULL, 10);
      if (!code) {
        ERROR_REPLY_400("Code is not a number.");
        return;
      }
    }

    // Check JWT
    jwt_t *decoded = NULL;
    int is_decoded =
        jwt_decode(&decoded, token, (unsigned char *)secret, strlen(secret));

    if (is_decoded != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BAD_JWT_MESSAGE));
      return;
    }

    // Check JWT expired
    long exp = jwt_get_grant_int(decoded, "exp");
    if (time(NULL) > exp) {
      ERROR_REPLY_400(JWT_EXPIRED_MESSAGE);
      jwt_free(decoded);
      return;
    }

    int type = jwt_get_grant_int(decoded, "type");
    if (type != LOGIN) {
      ERROR_REPLY_400(WRONG_JWT_TYPE_MESSAGE);
      jwt_free(decoded);
      return;
    }

    // Get Email from JWT
    // Will be freed with jwt_free
    const char *email = jwt_get_grant(decoded, "email");

    // Check if email validity
    int email_valid = check_email_validity((char *)email);
    if (email_valid != 0) {
      ERROR_REPLY_400(EMAIL_VALIDITY_ERROR_MESSAGE);
      return;
    }

    printf("TOKEN GRANTS:\tEXP: %ld\tEMAIL: %s\n", exp, email);

    // Get user TOTP seed
    char *seed = NULL;
    int query_code = get_user_totp_seed((char *)email, &seed);
    if (query_code != 0 || seed == NULL) {
      ERROR_REPLY_400("Invalid or expired TOTP code");
      return;
    }

    // Check TOTP Code
    time_t now = time(NULL);
    uint32_t totp_prev = totp_generate_at(seed, 30, now - 30);
    uint32_t totp_curr = totp_generate(seed, 30);
    uint32_t totp_next = totp_generate_at(seed, 30, now + 30);

    if ((uint32_t)code != totp_prev && (uint32_t)code != totp_curr &&
        (uint32_t)code != totp_next) {
      ERROR_REPLY_400("Invalid or expired TOTP code");
      return;
    }

    // Generate session JWT - email + exp
    jwt_t *jwt = NULL;
    jwt_new(&jwt);
    jwt_add_grant(jwt, "email", email);
    jwt_add_grant_int(jwt, "type", SESSION);
    jwt_add_grant_int(jwt, "exp", time(NULL) + 86400);
    jwt_set_alg(jwt, JWT_ALG_HS256, (unsigned char *)secret, strlen(secret));

    // Send session JWT
    char *jwt_str = jwt_encode_str(jwt);
    printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY LOGGED IN ==="));
    mg_http_reply(
        c, 200, JSON_HEADER,
        "{ \"message\": \"Successfully logged in\", \"token\": \"%s\" }",
        jwt_str);
    jwt_free(decoded);
    return;
  }

  ERROR_REPLY_405;
  fprintf(stderr, TERMINAL_ERROR_MESSAGE("METHOD NOT ALLOWED"));
  return;
}
