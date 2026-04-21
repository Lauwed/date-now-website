/**
 * @file issue.c
 * @brief Issue endpoint handler implementations (list, single, publish).
 */

#include <endpoints/auth.h>
#include <enums.h>
#include <lib/email.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <macros/utils.h>
#include <math.h>
#include <sql/issue.h>
#include <sql/user.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>

void send_issues_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ISSUE LIST ==="));

    // Query params
    char q_buf[1024] = "";
    struct mg_str q = {.buf = NULL, .len = 0};
    int q_decoded_len = mg_http_get_var(&msg->query, "q", q_buf, sizeof(q_buf));
    if (q_decoded_len > 0 && q_decoded_len < 1024) {
      q_buf[q_decoded_len] = '\0';
      q = mg_str(q_buf);
    }

    const struct mg_str sort = mg_http_var(msg->query, mg_str("sort"));

    // Status filter
    char status_buf[16] = "";
    const char *status = NULL;
    int status_len =
        mg_http_get_var(&msg->query, "status", status_buf, sizeof(status_buf));
    if (status_len > 0) {
      if (strcmp(status_buf, "DRAFT") == 0 ||
          strcmp(status_buf, "PUBLISHED") == 0 ||
          strcmp(status_buf, "ARCHIVE") == 0) {
        status = status_buf;
      } else {
        ERROR_REPLY_400(STATUS_FORMAT_MESSAGE);
        return;
      }
    }

    printf("QUERY PARAMS:\tQUERY - %.*s\t|\tSORT - %.*s\t|\tSTATUS - %s\n",
           (int)q.len, q.buf, (int)sort.len, sort.buf, status ? status : "");

    // Pagination
    int page, page_size;
    struct mg_str page_str = mg_http_var(msg->query, mg_str("page"));
    if (mg_str_to_num(page_str, 10, &page, sizeof(int)) == false)
      page = -1;
    else {
      struct mg_str page_size_str = mg_http_var(msg->query, mg_str("limit"));
      if (mg_str_to_num(page_size_str, 10, &page_size, sizeof(int)) == false)
        page_size = 20;
    }

    // Reply init
    struct list_reply *reply = malloc(sizeof(struct list_reply));
    reply->page = page;
    reply->page_size = page_size;
    reply->data = NULL;

    reply->json = NULL;
    reply->total = reply->count = get_issues_len(&q, status);
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

    struct issue **issues = NULL;

    if (reply->count > 0) {
      issues = malloc(reply->count * sizeof(struct issue *));
      query_code = get_issues(reply->count, issues, &q, status, &sort,
                              reply->page, reply->page_size);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
        HANDLE_QUERY_CODE;

        free(reply->json);
        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = issues_to_json(issues, reply->count);
    list_reply_to_json(reply);

    SUCCESS_REPLY_200(reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUES SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_issues(issues, reply->count);
      free(reply->data);
    }
    free(reply->json);
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
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    // Body validation
    int offset, length;

    // REQUIRED PARAMS
    char *title = NULL;
    char *slug = NULL;
    int issue_number = 0;

    // Title required
    offset = mg_json_get(msg->body, "$.title", &length);
    if (offset < 0) {
      ERROR_REPLY_400(TITLE_REQUIRED_MESSAGE);
      return;
    } else {
      title = malloc(length);
      strncpy(title, msg->body.buf + offset + 1, length - 2);
    }

    // Issue number required
    offset = mg_json_get(msg->body, "$.issueNumber", &length);
    if (offset < 0) {
      ERROR_REPLY_400(ISSUE_NUMBER_REQUIRED_MESSAGE);
      free(title);
      return;
    } else {
      // Issue number not existing already
      char *issue_number_str = malloc(length + 1);
      sprintf(issue_number_str, STR_FMT, length, msg->body.buf + offset);

      issue_number = atoi(issue_number_str);
      free(issue_number_str);
      if (issue_number <= 0) {
        ERROR_REPLY_400(ISSUE_NUMBER_REQUIRED_MESSAGE);
        free(title);
        return;
      }
    }

    offset = mg_json_get(msg->body, "$.slug", &length);
    if (offset > 0) {
      slug = malloc(length);
      strncpy(slug, msg->body.buf + offset + 1, length - 2);
      slug[length - 2] = '\0';
    } else {
      slug = strdup(title);
      str_to_slug(slug, strlen(slug));
    }

    int exists = issue_identity_exists(title, issue_number, slug);
    free(title);
    free(slug);
    if (exists != 0) {
      ERROR_REPLY_400(ISSUE_EXISTS_MESSAGE);
      return;
    };

    // Check status value
    offset = mg_json_get(msg->body, "$.status", &length);
    char status[12];
    if (length > 12) {
      ERROR_REPLY_400(STATUS_FORMAT_MESSAGE);

      return;
    }
    if (offset >= 0) {
      strncpy(status, msg->body.buf + offset + 1, length - 2);
      status[length - 2] = '\0';

      if (strcmp(status, "DRAFT") != 0 && strcmp(status, "PUBLISHED") != 0 &&
          strcmp(status, "ARCHIVE") != 0) {
        ERROR_REPLY_400(STATUS_FORMAT_MESSAGE);

        return;
      }
    }

    // Hydrate
    struct issue *issue = malloc(sizeof(struct issue));
    int issue_init_rc = issue_init(issue);
    if (issue_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE IS NULL"));

      return;
    }

    issue_hydrate(msg, issue);

    if (issue->slug == NULL) {
      issue->slug = strdup(issue->title);
      str_to_slug(issue->slug, strlen(issue->slug));
    }

    // Store in DB
    query_code = add_issue(issue);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
      HANDLE_QUERY_CODE;
      free_issue(issue);
      return;
    }

    struct issue *created = malloc(sizeof(struct issue));
    query_code = get_issue(created, issue->id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
      HANDLE_QUERY_CODE;
      free_issue(issue);
      free_issue(created);
      return;
    }

    char *result = issue_to_json(created);
    SUCCESS_REPLY_201(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SUCCESSFULLY ADDED ==="));

    free_issue(created);
    free_issue(issue);
  } else {
    ERROR_REPLY_405;
  }
}

void send_issue_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply,
                    const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  // Check if exists
  int exists = issue_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ISSUE ==="));

    struct issue *issue = NULL;
    issue = malloc(sizeof(struct issue));

    query_code = get_issue(issue, id);

    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUE"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = issue_to_json(issue);

      SUCCESS_REPLY_200(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SUCCESSFULLY SENT ==="));
    }

    free_issue(issue);
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

    // Hydrate
    struct issue *issue = malloc(sizeof(struct issue));

    int offset, length;

    // REQUIRED PARAMS
    char *title = NULL;
    char *slug = NULL;
    int issue_number = 0;

    // Title required
    offset = mg_json_get(msg->body, "$.title", &length);
    if (offset >= 0) {
      title = malloc(length);
      strncpy(title, msg->body.buf + offset + 1, length - 2);
    }

    // Issue number required
    offset = mg_json_get(msg->body, "$.issueNumber", &length);
    if (offset >= 0) {
      // Issue number not existing already
      char *issue_number_str = malloc(length + 1);
      mg_http_get_var(&msg->body, "issueNumber", issue_number_str, length + 1);

      int issue_number = atoi(issue_number_str);
      free(issue_number_str);
      if (issue_number <= 0) {
        ERROR_REPLY_400(ISSUE_NUMBER_REQUIRED_MESSAGE);
        free(title);
        return;
      }
    }

    offset = mg_json_get(msg->body, "$.slug", &length);
    if (offset >= 0) {
      slug = malloc(length);
      strncpy(slug, msg->body.buf + offset + 1, length - 2);
      slug[length - 2] = '\0';
    }

    if (title != NULL || issue_number > 0 || slug != NULL) {
      int exists = issue_identity_exists(title, issue_number, slug);
      if (exists != 0) {
        ERROR_REPLY_400(ISSUE_EXISTS_MESSAGE);
        free(title);
        free(slug);
        return;
      };
    }

    // Check status value
    offset = mg_json_get(msg->body, "$.status", &length);
    char status[12];
    if (length > 12) {
      ERROR_REPLY_400(STATUS_FORMAT_MESSAGE);
      free(title);
      free(slug);

      return;
    }
    if (offset >= 0) {
      strncpy(status, msg->body.buf + offset + 1, length - 2);
      status[length - 2] = '\0';

      if (strcmp(status, "DRAFT") != 0 && strcmp(status, "PUBLISHED") != 0 &&
          strcmp(status, "ARCHIVE") != 0) {
        ERROR_REPLY_400(STATUS_FORMAT_MESSAGE);
        free(title);
        free(slug);

        return;
      }
    }

    // Generate new slug only if the issue is still in DRAFT
    if (title != NULL && slug == NULL && strcmp(status, "DRAFT") == 0) {
      // Generate slug on the new title
      slug = strdup(title);
      str_to_slug(slug, strlen(slug));
    }
    free(title);

    query_code = get_issue(issue, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
      HANDLE_QUERY_CODE;

      return;
    }

    issue_hydrate(msg, issue);
    if (slug != NULL) {
      issue->slug = slug;
    }

    // Store in DB
    query_code = edit_issue(issue);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
      HANDLE_QUERY_CODE;

      return;
    }

    query_code = get_issue(issue, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ISSUES"));
      HANDLE_QUERY_CODE;
      free_issue(issue);
      return;
    }

    char *result = issue_to_json(issue);
    SUCCESS_REPLY_200(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SUCCESSFULLY EDITED ==="));

    free_issue(issue);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    int delete_rc = delete_issue(id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE ISSUE"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Issue successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}

void publish_issue_res(struct mg_connection *c, struct mg_http_message *msg,
                       int id, struct error_reply *error_reply,
                       const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (!mg_match(msg->method, mg_str("POST"), NULL)) {
    ERROR_REPLY_405;
    return;
  }

  printf(TERMINAL_ENDPOINT_MESSAGE("=== PUBLISH ISSUE ==="));

  // Auth
  int user_logged = 0;
  is_user_logged(c, msg, error_reply, secret, &user_logged);
  if (user_logged == 0) {
    ERROR_REPLY_401;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
    return;
  }

  // Check issue exists
  int exists = issue_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return;
  }

  // Load issue to check current status
  struct issue *issue = malloc(sizeof(struct issue));
  query_code = get_issue(issue, id);
  if (query_code != 0) {
    free(issue);
    HANDLE_QUERY_CODE;
    return;
  }

  // 409 if already published
  if (issue->status != NULL && strcmp(issue->status, "PUBLISHED") == 0) {
    ERROR_REPLY_409(ISSUE_ALREADY_PUBLISHED_MESSAGE);
    free_issue(issue);
    return;
  }

  char *title =
      issue->title != NULL ? strdup(issue->title) : strdup("Date.now()");
  free_issue(issue);

  // Publish in DB
  query_code = publish_issue(id);
  if (query_code != 0) {
    free(title);
    ERROR_REPLY_500;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR PUBLISHING ISSUE"));
    return;
  }

  // Send newsletter to all subscribers
  size_t subscribers_len = 0;
  char **emails = NULL;
  query_code = get_subscriber_emails(&subscribers_len, &emails);
  if (query_code == 0 && emails != NULL) {
    char subject[256];
    snprintf(subject, sizeof(subject), "Date.now() - %s", title);

    char html[512];
    snprintf(html, sizeof(html),
             "Un nouveau numero est disponible : "
             "<a href=https://datenow.com>%s</a>",
             title);

    for (size_t i = 0; i < subscribers_len; i++) {
      send_mail(emails[i], subject, html);
      free(emails[i]);
    }
    free(emails);
  }

  free(title);

  SUCCESS_REPLY_200_MSG("Issue published and newsletter sent");
  printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SUCCESSFULLY PUBLISHED ==="));
}
