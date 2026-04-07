#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <math.h>
#include <sql/user.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>

#define USER_EXISTS_MESSAGE "The user already exists."
#define EMAIL_REQUIRED_MESSAGE "Email is required."
#define ROLE_FORMAT_MESSAGE "Value of 'role' should be 'USER' or 'AUTHOR'."

void send_users_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET USER LIST ==="));

    // Query params
    char q_buf[1024] = "";
    struct mg_str q = {.buf = NULL, .len = 0};
    int q_decoded_len = mg_http_get_var(&msg->query, "q", q_buf, sizeof(q_buf));
    if (q_decoded_len > 0 && q_decoded_len < 1024) {
      q_buf[q_decoded_len] = '\0';
      q = mg_str(q_buf);
    }

    const struct mg_str sort = mg_http_var(msg->query, mg_str("sort"));
    printf("QUERY PARAMS:\tQUERY - %.*s\t|\tSORT - %.*s\n", (int)q.len, q.buf,
           (int)sort.len, sort.buf);

    // Pagination
    int page, page_size;
    struct mg_str page_str = mg_http_var(msg->query, mg_str("page"));
    if (mg_str_to_num(page_str, 10, &page, sizeof(int)) == false)
      page = -1;
    else {
      struct mg_str page_size_str =
          mg_http_var(msg->query, mg_str("page_size"));
      if (mg_str_to_num(page_size_str, 10, &page_size, sizeof(int)) == false)
        page_size = 10;
    }

    // Reply init
    struct list_reply *reply = malloc(sizeof(struct list_reply));
    reply->page = page;
    reply->page_size = page_size;
    reply->data = NULL;

    reply->total = reply->count = get_users_len(&q);
    reply->total_pages = 0;
    printf("ARRAY COUNT:\tTOTAL - %d\t|\tCOUNT - %d\t|\tTOTAL PAGES - %d\n",
           reply->total, reply->count, reply->total_pages);
    // If pagination
    if (reply->page > 0) {
      // Cancel pagination if page size too big
      if (reply->total < reply->page_size) {
        reply->page = -1;
      } else {
        double tot_pages = (double)reply->total / (double)reply->page_size;
        reply->total_pages = (int)ceil(tot_pages);

        if (reply->total_pages < reply->page) {
          reply->page = reply->total_pages;
        }

        if (reply->page < reply->total_pages) {
          reply->count = reply->page_size;
        } else {
          int remainder = reply->total % reply->page_size;
          reply->count = remainder == 0 ? reply->page_size : remainder;
        }
      }
    }

    printf("PAGINATION:\tPAGE INDEX - %d\t|\tPAGE SIZE - %d\n", page,
           page_size);
    printf("ARRAY COUNT:\tTOTAL - %d\t|\tCOUNT - %d\t|\tTOTAL PAGES - %d\n",
           reply->total, reply->count, reply->total_pages);

    struct user **users = NULL;

    if (reply->count > 0) {
      users = malloc(reply->count * sizeof(struct user *));
      query_code = get_users(reply->count, users, &q, &sort, reply->page,
                             reply->page_size);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
        HANDLE_QUERY_CODE;

        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = users_to_json(users, reply->count);
    list_reply_to_json(reply);

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== USERS SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_users(users, reply->count);
      free(reply->data);
      free(reply->json);
    }
    free(reply);
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BODY_REQUIRED_MESSAGE));
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(JSON_ERROR_MESSAGE));
      return;
    }

    // Body validation
    int offset, length;

    // Email required
    offset = mg_json_get(msg->body, "$.email", &length);
    if (offset < 0) {
      ERROR_REPLY_400(EMAIL_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("EMAIL REQUIRED"));
      return;
    } else {
      // Email and username not existing already
      char *email = malloc(length);
      strncpy(email, msg->body.buf + offset + 1, length - 2);

      char *username = NULL;
      offset = mg_json_get(msg->body, "$.username", &length);
      if (offset >= 0) {
        username = malloc(length);
        strncpy(username, msg->body.buf + offset + 1, length - 2);
      }

      int exists = user_identity_exists(username, email);
      if (exists != 0) {
        ERROR_REPLY_400(USER_EXISTS_MESSAGE);
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER ALREADY EXISTS"));
        return;
      };
    }

    // Check Role value
    offset = mg_json_get(msg->body, "$.role", &length);
    char role[10];
    if (length > 10) {
      ERROR_REPLY_400(ROLE_FORMAT_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG ROLE"));

      return;
    }
    if (offset >= 0) {
      strncpy(role, msg->body.buf + offset + 1, length - 2);
      role[length - 2] = '\0';

      if (strcmp(role, "USER") != 0 && strcmp(role, "AUTHOR") != 0) {
        ERROR_REPLY_400(ROLE_FORMAT_MESSAGE);
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG ROLE"));

        return;
      }
    }

    // Hydrate
    struct user *user = malloc(sizeof(struct user));
    int user_init_rc = user_init(user);
    if (user_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER IS NULL"));

      return;
    }

    user_hydrate(msg, user);

    // Store in DB
    query_code = add_user(user);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      mg_http_reply(c, 201, JSON_HEADER,
                    "{ \"message\": \"User successfully created\" }");
      printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY ADDED ==="));
    }

    free_user(user);
  } else {
    ERROR_REPLY_405;
  }
}

void send_user_res(struct mg_connection *c, struct mg_http_message *msg, int id,
                   struct error_reply *error_reply) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if exists
  int exists = user_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET USER ==="));

    struct user *user = NULL;
    user = malloc(sizeof(struct user));

    query_code = get_user(user, id);

    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USER"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = user_to_json(user);

      mg_http_reply(c, 200, JSON_HEADER, "%s\n", result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY SENT ==="));
    }

    free_user(user);
  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BODY_REQUIRED_MESSAGE));
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(JSON_ERROR_MESSAGE));
      return;
    }

    // Hydrate
    struct user *user = malloc(sizeof(struct user));

    int offset, length;

    // Email required
    offset = mg_json_get(msg->body, "$.email", &length);
    if (offset >= 0) {
      // Email and username not existing already
      char *email = malloc(length);
      strncpy(email, msg->body.buf + offset + 1, length - 2);

      char *username = NULL;
      offset = mg_json_get(msg->body, "$.username", &length);
      if (offset >= 0) {
        username = malloc(length);
        strncpy(username, msg->body.buf + offset + 1, length - 2);
      }

      int exists = user_identity_exists(username, email);
      if (exists != 0) {
        ERROR_REPLY_400(USER_EXISTS_MESSAGE);
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("USER ALREADY EXISTS"));
        return;
      };
    }

    // Check Role value
    offset = mg_json_get(msg->body, "$.role", &length);
    char role[10];
    if (length > 10) {
      ERROR_REPLY_400(ROLE_FORMAT_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG ROLE"));

      return;
    }
    if (offset >= 0) {
      strncpy(role, msg->body.buf + offset + 1, length - 2);
      role[length - 2] = '\0';

      if (strcmp(role, "USER") != 0 && strcmp(role, "AUTHOR") != 0) {
        ERROR_REPLY_400(ROLE_FORMAT_MESSAGE);
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG ROLE"));
        return;
      }
    }

    query_code = get_user(user, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
      HANDLE_QUERY_CODE;

      return;
    }

    user_hydrate(msg, user);

    // Store in DB
    query_code = edit_user(user);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING USERS"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      mg_http_reply(c, 200, JSON_HEADER,
                    "{ \"message\": \"User successfully edited\" }");
      printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY EDITED ==="));
    }

    free_user(user);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    int delete_rc = delete_user(id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE USER"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== USER SUCCESSFULLY DELETE ==="));
    mg_http_reply(c, 200, JSON_HEADER,
                  "{ \"message\": \"User successfully deleted\" }");
  } else {
    ERROR_REPLY_405;
  }
}
