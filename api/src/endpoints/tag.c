/**
 * @file tag.c
 * @brief Tag endpoint handler implementations (list, single resource).
 */

#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <math.h>
#include <sql/tag.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>


void send_tags_res(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET TAG LIST ==="));

    struct mg_str empty_q = {.buf = NULL, .len = 0};
    struct mg_str empty_sort = {.buf = NULL, .len = 0};
    int total = get_tags_len(&empty_q);
    struct tag **tags = NULL;

    if (total > 0) {
      tags = malloc((size_t)total * sizeof(struct tag *));
      query_code = get_tags((size_t)total, tags, &empty_q, &empty_sort, -1, 0);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING TAGS"));
        HANDLE_QUERY_CODE;
        free(tags);
        return;
      }
    }

    char *json = tags_to_json(tags, (size_t)total);
    SUCCESS_REPLY_200(json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== TAGS SUCCESSFULLY SENT ==="));

    if (tags != NULL) {
      free_tags(tags, (size_t)total);
      free(json);
    }
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    // Check if user logged
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

    // Body validation
    int offset, length = 0;

    // Required props
    REQUIRED_BODY_PROPERTY("name", NAME_REQUIRED_MESSAGE);
    char *name = malloc(length - 1);
    strncpy(name, msg->body.buf + offset + 1, length - 2);
    name[length - 2] = '\0';

    REQUIRED_BODY_PROPERTY("color", COLOR_REQUIRED_MESSAGE);

    int exists = tag_exists(name);
    if (exists != 0) {
      ERROR_REPLY_400(TAG_EXISTS_MESSAGE);
      free(name);
      return;
    };

    // Hydrate
    struct tag *tag = malloc(sizeof(struct tag));
    int tag_init_rc = tag_init(tag);
    if (tag_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG IS NULL"));
      free(name);

      return;
    }

    tag_hydrate(msg, tag);

    // Store in DB
    query_code = add_tag(tag);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING TAGS"));
      HANDLE_QUERY_CODE;
      free(name);

      return;
    } else {
      char *result = tag_to_json(tag);
      SUCCESS_REPLY_201(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY ADDED ==="));
    }

    free_tag(tag);
    free(name);
  } else {
    ERROR_REPLY_405;
  }
}

void send_tag_res(struct mg_connection *c, struct mg_http_message *msg,
                  char *name, struct error_reply *error_reply,
                  const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET TAG ==="));

    // Check if exists
    int exists = tag_exists(name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG NOT FOUND"));
      return;
    }

    struct tag *tag = NULL;
    tag = malloc(sizeof(struct tag));

    query_code = get_tag(tag, name);

    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING TAG"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = tag_to_json(tag);

      SUCCESS_REPLY_200(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY SENT ==="));
    }

    free_tag(tag);
  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    // Check if user logged
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

    struct tag *tag = malloc(sizeof(struct tag));

    // Check if exists
    int exists = tag_exists(name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG NOT FOUND"));
      return;
    }

    int offset, length;

    // Required props
    REQUIRED_BODY_PROPERTY("color", COLOR_REQUIRED_MESSAGE);

    // Retrieve actual values of tag
    query_code = get_tag(tag, name);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING TAGS"));
      HANDLE_QUERY_CODE;

      return;
    }

    // Hydrate
    tag_hydrate(msg, tag);

    // Store in DB
    query_code = edit_tag(tag, name);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING TAGS"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = tag_to_json(tag);
      SUCCESS_REPLY_200(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY EDITED ==="));
    }

    free_tag(tag);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    // Check if exists
    int exists = tag_exists(name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG NOT FOUND"));

      return;
    }

    int delete_rc = delete_tag(name);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE TAG"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Tag successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}
