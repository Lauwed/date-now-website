#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <macros/utils.h>
#include <math.h>
#include <sql/issue.h>
#include <sql/issue_sponsor.h>
#include <sql/sponsor.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>

#define LINK_REQUIRED_MESSAGE "The link is required"
#define ISSUE_DOES_NOT_EXIST_MESSAGE "The issue doesn't exist"
#define SPONSOR_DOES_NOT_EXIST_MESSAGE "The sponsor doesn't exist"
#define LINK_ALREADY_EXISTS "The link between the issue and the sponsor already"
#define LINK_DOES_NOT_EXIST "The link doesn't exists"

void send_issue_sponsors_res(struct mg_connection *c,
                             struct mg_http_message *msg, int issue_id,
                             struct error_reply *error_reply,
                             const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if issue exists
  int exists = issue_exists(issue_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ISSUE SPONSOR LIST ==="));

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

    reply->total = reply->count = get_issue_sponsors_len(&q, issue_id);
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

    struct issue_sponsor **issues = NULL;

    if (reply->count > 0) {
      issues = malloc(reply->count * sizeof(struct issue *));
      query_code = get_issue_sponsors(reply->count, issues, issue_id,
                                      reply->page, reply->page_size);

      if (query_code != 0) {
        fprintf(stderr,
                TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUE SPONSORS"));
        HANDLE_QUERY_CODE;

        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = issue_sponsors_to_json(issues, reply->count);
    list_reply_to_json(reply);

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", reply->json);
    printf(
        TERMINAL_SUCCESS_MESSAGE("=== ISSUE SPONSORS SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_issue_sponsors(issues, reply->count);
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

    // Body validation
    int offset, length;

    // REQUIRED PARAMS
    char *link = NULL;

    // Title required
    offset = mg_json_get(msg->body, "$.link", &length);
    if (offset < 0) {
      ERROR_REPLY_400(LINK_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(LINK_REQUIRED_MESSAGE));
      return;
    }

    // Hydrate
    struct issue_sponsor *issue = malloc(sizeof(struct issue_sponsor));
    int issue_init_rc = issue_sponsor_init(issue);
    if (issue_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE IS NULL"));

      return;
    }

    issue->issue_id = issue_id;
    issue_sponsor_hydrate(msg, issue);

    // Check if sponsor exists
    exists = sponsor_exists(issue->sponsor_name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("SPONSOR NOT FOUND"));
      return;
    }

    // Store in DB
    query_code = add_issue_sponsor(issue);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      mg_http_reply(c, 201, JSON_HEADER,
                    "{ \"message\": \"Issue sponsor successfully created\" }");
      printf(
          TERMINAL_SUCCESS_MESSAGE("=== ISSUE SPONSOR SUCCESSFULLY ADDED ==="));
    }

    free_issue_sponsor(issue);
  } else {
    ERROR_REPLY_405;
  }
}

void send_issue_sponsor_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id, char *id,
                            struct error_reply *error_reply,
                            const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  // Check if exists
  int exists = issue_exists(issue_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return;
  }
  exists = sponsor_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("SPONSOR NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    int delete_rc = delete_issue_sponsor(issue_id, id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE ISSUE"));
    }

    printf(
        TERMINAL_SUCCESS_MESSAGE("=== ISSUE SPONSOR SUCCESSFULLY DELETE ==="));
    mg_http_reply(c, 200, JSON_HEADER,
                  "{ \"message\": \"Issue sponsor successfully deleted\" }");
  } else {
    ERROR_REPLY_405;
  }
}
