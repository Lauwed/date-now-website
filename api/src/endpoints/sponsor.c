/**
 * @file sponsor.c
 * @brief Sponsor endpoint handler implementations (list, single resource).
 */

#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <math.h>
#include <sql/sponsor.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>


void send_sponsors_res(struct mg_connection *c, struct mg_http_message *msg,
                       struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET SPONSOR LIST ==="));

    struct mg_str empty_q = {.buf = NULL, .len = 0};
    struct mg_str empty_sort = {.buf = NULL, .len = 0};
    int total = get_sponsors_len(&empty_q);
    struct sponsor **sponsors = NULL;

    if (total > 0) {
      sponsors = malloc((size_t)total * sizeof(struct sponsor *));
      query_code = get_sponsors((size_t)total, sponsors, &empty_q, &empty_sort, -1, 0);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SPONSORS"));
        HANDLE_QUERY_CODE;
        free(sponsors);
        return;
      }
    }

    char *json = sponsors_to_json(sponsors, (size_t)total);
    SUCCESS_REPLY_200(json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== SPONSORS SUCCESSFULLY SENT ==="));

    if (sponsors != NULL) {
      free_sponsors(sponsors, (size_t)total);
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

    REQUIRED_BODY_PROPERTY("link", LINK_REQUIRED_MESSAGE);

    int exists = sponsor_exists(name);
    if (exists != 0) {
      ERROR_REPLY_400(SPONSOR_EXISTS_MESSAGE);
      free(name);
      return;
    };

    // Hydrate
    struct sponsor *sponsor = malloc(sizeof(struct sponsor));
    int sponsor_init_rc = sponsor_init(sponsor);
    if (sponsor_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("SPONSOR IS NULL"));
      free(name);

      return;
    }

    sponsor_hydrate(msg, sponsor);

    // Store in DB
    query_code = add_sponsor(sponsor);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SPONSORS"));
      HANDLE_QUERY_CODE;
      free(name);

      return;
    } else {
      char *result = sponsor_to_json(sponsor);
      SUCCESS_REPLY_201(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== SPONSOR SUCCESSFULLY ADDED ==="));
    }

    free_sponsor(sponsor);
    free(name);
  } else {
    ERROR_REPLY_405;
  }
}

void send_sponsor_res(struct mg_connection *c, struct mg_http_message *msg,
                      char *name, struct error_reply *error_reply,
                      const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET SPONSOR ==="));

    // Check if exists
    int exists = sponsor_exists(name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("SPONSOR NOT FOUND"));
      return;
    }

    struct sponsor *sponsor = NULL;
    sponsor = malloc(sizeof(struct sponsor));

    query_code = get_sponsor(sponsor, name);

    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SPONSOR"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = sponsor_to_json(sponsor);

      SUCCESS_REPLY_200(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== SPONSOR SUCCESSFULLY SENT ==="));
    }

    free_sponsor(sponsor);
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

    struct sponsor *sponsor = malloc(sizeof(struct sponsor));

    // Check if exists
    int exists = sponsor_exists(name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("SPONSOR NOT FOUND"));
      return;
    }

    int offset, length;

    // Required props
    REQUIRED_BODY_PROPERTY("link", LINK_REQUIRED_MESSAGE);

    // Retrieve actual values of sponsor
    query_code = get_sponsor(sponsor, name);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SPONSORS"));
      HANDLE_QUERY_CODE;

      return;
    }

    // Hydrate
    sponsor_hydrate(msg, sponsor);

    // Store in DB
    query_code = edit_sponsor(sponsor, name);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SPONSORS"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = sponsor_to_json(sponsor);
      SUCCESS_REPLY_200(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== SPONSOR SUCCESSFULLY EDITED ==="));
    }

    free_sponsor(sponsor);
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
    int exists = sponsor_exists(name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("SPONSOR NOT FOUND"));

      return;
    }

    int delete_rc = delete_sponsor(name);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE SPONSOR"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== SPONSOR SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Sponsor successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}
