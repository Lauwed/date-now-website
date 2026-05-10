/**
 * @file blocked_email_domain.c
 * @brief Blocked email domain endpoint handler implementations.
 */

#include <cjson/cJSON.h>
#include <endpoints/auth.h>
#include <enums.h>
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

/* Serialise a char** array of domain strings to a JSON array string. */
static char *domains_to_json(char **arr, size_t len) {
  if (len == 0 || arr == NULL) return "[]";
  cJSON *a = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(a, cJSON_CreateString(arr[i]));
  char *json = cJSON_PrintUnformatted(a);
  cJSON_Delete(a);
  return json;
}

void send_blocked_email_domains_res(struct mg_connection *c,
                                    struct mg_http_message *msg,
                                    struct error_reply *error_reply,
                                    const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET BLOCKED DOMAIN LIST ==="));

    // Public access controlled by env var
    const char *public_env = getenv("BLOCKED_DOMAIN_LIST_PUBLIC");
    int is_public = (public_env != NULL && public_env[0] == '1');

    if (!is_public) {
      int user_logged = 0;
      is_user_logged(c, msg, error_reply, secret, &user_logged);
      if (user_logged == 0) {
        ERROR_REPLY_401;
        fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
        return;
      }
    }

    char **arr = NULL;
    size_t len = 0;
    query_code = get_blocked_domains(&arr, &len);

    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING BLOCKED DOMAINS"));
      HANDLE_QUERY_CODE;
      return;
    }

    char *json = domains_to_json(arr, len);
    SUCCESS_REPLY_200(json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== BLOCKED DOMAINS SUCCESSFULLY SENT ==="));

    if (arr != NULL) {
      for (size_t i = 0; i < len; i++) free(arr[i]);
      free(arr);
      if (len > 0) free(json);
    }
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    // Require AUTHOR auth
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    int offset, length = 0;
    REQUIRED_BODY_PROPERTY("domain", DOMAIN_REQUIRED_MESSAGE);

    char *domain = malloc((size_t)length - 1);
    strncpy(domain, msg->body.buf + offset + 1, (size_t)length - 2);
    domain[length - 2] = '\0';

    // Check if already exists
    int exists = blocked_domain_exists(domain);
    if (exists > 0) {
      ERROR_REPLY_409(DOMAIN_EXISTS_MESSAGE);
      free(domain);
      return;
    }
    if (exists < 0) {
      ERROR_REPLY_500;
      free(domain);
      return;
    }

    query_code = add_blocked_domain(domain);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR ADDING BLOCKED DOMAIN"));
      HANDLE_QUERY_CODE;
      free(domain);
      return;
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== BLOCKED DOMAIN SUCCESSFULLY ADDED ==="));
    SUCCESS_REPLY_201_MSG("Domain successfully blocked");
    free(domain);
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

  if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    // Require AUTHOR auth
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    query_code = delete_blocked_domain(domain);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR DELETING BLOCKED DOMAIN"));
      HANDLE_QUERY_CODE;
      return;
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== BLOCKED DOMAIN SUCCESSFULLY DELETED ==="));
    SUCCESS_REPLY_200_MSG("Domain successfully unblocked");
  } else {
    ERROR_REPLY_405;
  }
}
