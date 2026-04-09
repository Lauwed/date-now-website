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

#define TAG_EXISTS_MESSAGE "The tag already exists."
#define NAME_REQUIRED_MESSAGE "The name is required."
#define COLOR_REQUIRED_MESSAGE "The color is required."

void send_tags_res(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET TAG LIST ==="));

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
    int page, page_size = 0;
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

    reply->total = reply->count = get_tags_len(&q);
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

    struct tag **tags = NULL;

    if (reply->count > 0) {
      tags = malloc(reply->count * sizeof(struct tag *));
      query_code = get_tags(reply->count, tags, &q, &sort, reply->page,
                            reply->page_size);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING TAGS"));
        HANDLE_QUERY_CODE;

        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = tags_to_json(tags, reply->count);
    list_reply_to_json(reply);

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== TAGS SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_tags(tags, reply->count);
      free(reply->data);
      free(reply->json);
    }
    free(reply);
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    if (check_auth(msg) != 0) {
      mg_http_reply(c, 401, JSON_HEADER, "{\"code\":401,\"message\":\"Unauthorized\"}");
      return;
    }
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
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG ALREADY EXISTS"));
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
      mg_http_reply(c, 201, JSON_HEADER,
                    "{ \"message\": \"Tag successfully created\" }");
      printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY ADDED ==="));
    }

    free_tag(tag);
    free(name);
  } else {
    ERROR_REPLY_405;
  }
}

void send_tag_res(struct mg_connection *c, struct mg_http_message *msg,
                  char *name, struct error_reply *error_reply) {
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

      mg_http_reply(c, 200, JSON_HEADER, "%s\n", result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY SENT ==="));
    }

    free_tag(tag);
  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    if (check_auth(msg) != 0) {
      mg_http_reply(c, 401, JSON_HEADER, "{\"code\":401,\"message\":\"Unauthorized\"}");
      return;
    }
    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(BODY_REQUIRED_MESSAGE));
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(JSON_ERROR_MESSAGE));
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
      mg_http_reply(c, 200, JSON_HEADER,
                    "{ \"message\": \"Tag successfully edited\" }");
      printf(TERMINAL_SUCCESS_MESSAGE("=== TAG SUCCESSFULLY EDITED ==="));
    }

    free_tag(tag);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    if (check_auth(msg) != 0) {
      mg_http_reply(c, 401, JSON_HEADER, "{\"code\":401,\"message\":\"Unauthorized\"}");
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
    mg_http_reply(c, 200, JSON_HEADER,
                  "{ \"message\": \"Tag successfully deleted\" }");
  } else {
    ERROR_REPLY_405;
  }
}
