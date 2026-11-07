/**
 * @file blocked_email_domain.c
 * @brief Blocklist of disposable email domains: endpoint handlers.
 */

#include <cjson/cJSON.h>
#include <endpoints/auth.h>
#include <endpoints/blocked_email_domain.h>
#include <enums.h>
#include <lib/email_validator.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/blocked_email_domain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <utils.h>

/** Returns 1 if the blocklist can be read without a session. */
static int list_is_public(void) {
  const char *public_list = getenv("BLOCKED_DOMAIN_LIST_PUBLIC");
  return public_list != NULL && strcmp(public_list, "1") == 0;
}

/** Returns 1 if the request comes from an authenticated author. */
static int author_is_logged(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply,
                            const char *secret) {
  int user_logged = 0;
  is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

  return user_logged != 0;
}

/** Serialises the domains as a JSON array. The caller frees the string. */
static char *domains_to_json(char **domains, size_t len) {
  cJSON *array = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++) {
    cJSON_AddItemToArray(array, cJSON_CreateString(domains[i]));
  }

  char *json = cJSON_PrintUnformatted(array);
  cJSON_Delete(array);

  return json;
}

static void list_domains(struct mg_connection *c, struct error_reply *error_reply,
                         int *query_code) {
  char **domains = NULL;
  size_t len = 0;

  *query_code = get_blocked_domains(&domains, &len);
  if (*query_code != 0) {
    return;
  }

  char *json = domains_to_json(domains, len);
  free_blocked_domains(domains, len);
  if (json == NULL) {
    ERROR_REPLY_500;
    return;
  }

  SUCCESS_REPLY_200(json);
  printf(TERMINAL_SUCCESS_MESSAGE("=== BLOCKED DOMAINS SUCCESSFULLY SENT ==="));
  free(json);
}

static void add_domain(struct mg_connection *c, struct mg_http_message *msg,
                       struct error_reply *error_reply) {
  if (msg->body.len <= 0) {
    ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
    return;
  } else if (!mg_validateJSON(msg->body)) {
    ERROR_REPLY_400(JSON_ERROR_MESSAGE);
    return;
  }

  char *raw = mg_json_get_str(msg->body, "$.domain");
  if (raw == NULL) {
    ERROR_REPLY_400(DOMAIN_REQUIRED_MESSAGE);
    return;
  }

  char domain[EMAIL_DOMAIN_MAX_LEN + 1];
  int cleaned = email_domain_clean(raw, domain, sizeof(domain));
  free(raw);
  if (cleaned != 0) {
    ERROR_REPLY_400(DOMAIN_FORMAT_MESSAGE);
    return;
  }

  int exists = blocked_domain_exists(domain);
  if (exists < 0) {
    ERROR_REPLY_500;
    return;
  }
  if (exists > 0) {
    ERROR_REPLY_409(DOMAIN_EXISTS_MESSAGE);
    return;
  }

  int query_code = add_blocked_domain(domain);
  if (query_code != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR ADDING BLOCKED DOMAIN"));
    HANDLE_QUERY_CODE;
    return;
  }

  printf(TERMINAL_SUCCESS_MESSAGE("=== BLOCKED DOMAIN SUCCESSFULLY ADDED ==="));
  SUCCESS_REPLY_201_MSG("Domain successfully blocked");
}

void send_blocked_email_domains_res(struct mg_connection *c,
                                    struct mg_http_message *msg,
                                    struct error_reply *error_reply,
                                    const char *secret) {
  int query_code = 0;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET BLOCKED DOMAINS ==="));

    if (!list_is_public() && !author_is_logged(c, msg, error_reply, secret)) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    list_domains(c, error_reply, &query_code);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING BLOCKED DOMAINS"));
      HANDLE_QUERY_CODE;
    }
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== ADD BLOCKED DOMAIN ==="));

    if (!author_is_logged(c, msg, error_reply, secret)) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    add_domain(c, msg, error_reply);
  } else {
    ERROR_REPLY_405;
  }
}

void send_blocked_email_domain_res(struct mg_connection *c,
                                   struct mg_http_message *msg,
                                   const char *domain,
                                   struct error_reply *error_reply,
                                   const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (!mg_match(msg->method, mg_str("DELETE"), NULL)) {
    ERROR_REPLY_405;
    return;
  }

  printf(TERMINAL_ENDPOINT_MESSAGE("=== DELETE BLOCKED DOMAIN ==="));

  if (!author_is_logged(c, msg, error_reply, secret)) {
    ERROR_REPLY_401;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
    return;
  }

  char cleaned[EMAIL_DOMAIN_MAX_LEN + 1];
  if (email_domain_clean(domain, cleaned, sizeof(cleaned)) != 0) {
    ERROR_REPLY_400(DOMAIN_FORMAT_MESSAGE);
    return;
  }

  query_code = delete_blocked_domain(cleaned);
  if (query_code != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR DELETING BLOCKED DOMAIN"));
    HANDLE_QUERY_CODE;
    return;
  }

  printf(TERMINAL_SUCCESS_MESSAGE("=== BLOCKED DOMAIN SUCCESSFULLY DELETED ==="));
  SUCCESS_REPLY_200_MSG("Domain successfully unblocked");
}
