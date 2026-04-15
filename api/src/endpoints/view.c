#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <math.h>
#include <sql/view.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>

#define HASHED_IP_REQUIRED_MESSAGE "The hashed IP is required"
#define ISSUE_REQUIRED_MESSAGE "The issue ID is required"

void send_views_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET VIEW LIST ==="));

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

    reply->total = reply->count = get_views_len(&q);
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

    struct view **views = NULL;

    if (reply->count > 0) {
      views = malloc(reply->count * sizeof(struct view *));
      query_code = get_views(reply->count, views, &q, &sort, reply->page,
                             reply->page_size);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING VIEWS"));
        HANDLE_QUERY_CODE;

        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = views_to_json(views, reply->count);
    list_reply_to_json(reply);

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== VIEWS SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_views(views, reply->count);
      free(reply->data);
      free(reply->json);
    }
    free(reply);
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
    offset = mg_json_get(msg->body, "$.hashedIp", &length);
    if (offset < 0) {
      ERROR_REPLY_400(HASHED_IP_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("HASHED IP REQUIRED"));
      return;
    }
    offset = mg_json_get(msg->body, "$.issueId", &length);
    if (offset < 0) {
      ERROR_REPLY_400(ISSUE_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE ID REQUIRED"));
    }

    // Hydrate
    struct view *view = malloc(sizeof(struct view));
    int view_init_rc = view_init(view);
    if (view_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("VIEW IS NULL"));

      return;
    }

    view_hydrate(msg, view);

    // Store in DB
    query_code = add_view(view);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING VIEWS"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      mg_http_reply(c, 201, JSON_HEADER,
                    "{ \"message\": \"User successfully created\" }");
      printf(TERMINAL_SUCCESS_MESSAGE("=== VIEW SUCCESSFULLY ADDED ==="));
    }

    free_view(view);
  } else {
    ERROR_REPLY_405;
  }
}
