/**
 * @file feed.c
 * @brief Feed endpoint handler implementations (list, single resource).
 */

#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <math.h>
#include <sql/feed.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>

void send_feeds_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET FEED LIST ==="));

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
    int page = -1, page_size = 0;
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
    reply->total = reply->count = get_feeds_len(&q);
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

    struct feed **feeds = NULL;

    if (reply->count > 0) {
      feeds = malloc(reply->count * sizeof(struct feed *));
      query_code = get_feeds(reply->count, feeds, &q, &sort, reply->page,
                             reply->page_size);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEEDS"));
        HANDLE_QUERY_CODE;

        free(reply->json);
        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = feeds_to_json(feeds, reply->count);
    list_reply_to_json(reply);

    SUCCESS_REPLY_200(reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== FEEDS SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_feeds(feeds, reply->count);
      free(reply->data);
    }
    free(reply->json);
    free(reply);
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

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
    REQUIRED_BODY_PROPERTY("link", LINK_REQUIRED_MESSAGE);

    // Hydrate
    struct feed *feed = malloc(sizeof(struct feed));
    int feed_init_rc = feed_init(feed);
    if (feed_init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED IS NULL"));

      return;
    }

    feed_hydrate(msg, feed);

    // Store in DB
    query_code = add_feed(feed);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEEDS"));
      HANDLE_QUERY_CODE;

      free_feed(feed);
      return;
    } else {
      char *result = feed_to_json(feed);
      SUCCESS_REPLY_201(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== FEED SUCCESSFULLY ADDED ==="));
    }

    free_feed(feed);
  } else {
    ERROR_REPLY_405;
  }
}

void send_feed_res(struct mg_connection *c, struct mg_http_message *msg,
                   int id, struct error_reply *error_reply,
                   const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET FEED ==="));

    // Check if exists
    int exists = feed_exists(id);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED NOT FOUND"));
      return;
    }

    struct feed *feed = NULL;
    feed = malloc(sizeof(struct feed));

    query_code = get_feed(feed, id);

    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEED"));
      HANDLE_QUERY_CODE;

      return;
    } else {
      char *result = feed_to_json(feed);

      SUCCESS_REPLY_200(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== FEED SUCCESSFULLY SENT ==="));
    }

    free_feed(feed);
  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

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

    // Check if exists
    int exists = feed_exists(id);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED NOT FOUND"));
      return;
    }

    int offset, length;

    // Required props
    REQUIRED_BODY_PROPERTY("name", NAME_REQUIRED_MESSAGE);
    REQUIRED_BODY_PROPERTY("link", LINK_REQUIRED_MESSAGE);

    struct feed *feed = malloc(sizeof(struct feed));

    // Retrieve actual values of feed
    query_code = get_feed(feed, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEED"));
      HANDLE_QUERY_CODE;

      free(feed);
      return;
    }

    // Free previous text fields before re-hydrating
    free(feed->name);
    free(feed->link);
    feed->name = NULL;
    feed->link = NULL;

    // Hydrate
    feed_hydrate(msg, feed);

    // Store in DB
    query_code = edit_feed(feed);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEEDS"));
      HANDLE_QUERY_CODE;

      free_feed(feed);
      return;
    } else {
      char *result = feed_to_json(feed);
      SUCCESS_REPLY_200(result);
      free(result);
      printf(TERMINAL_SUCCESS_MESSAGE("=== FEED SUCCESSFULLY EDITED ==="));
    }

    free_feed(feed);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    // Check if exists
    int exists = feed_exists(id);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED NOT FOUND"));

      return;
    }

    int delete_rc = delete_feed(id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE FEED"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== FEED SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Feed successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}
