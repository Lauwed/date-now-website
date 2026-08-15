/**
 * @file feed_tag.c
 * @brief FeedTag endpoint handler implementations (list, add, remove).
 */

#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <macros/utils.h>
#include <math.h>
#include <sql/feed.h>
#include <sql/feed_tag.h>
#include <sql/tag.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <structs.h>
#include <utils.h>

void send_feed_tags_res(struct mg_connection *c, struct mg_http_message *msg,
                        int feed_id, struct error_reply *error_reply,
                        const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  // Check if feed exists
  int exists = feed_exists(feed_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET FEED TAG LIST ==="));

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

    reply->json = NULL;
    reply->total = reply->count = get_feed_tags_len(&q, feed_id);
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

    struct feed_tag **feed_tags = NULL;

    if (reply->count > 0) {
      feed_tags = malloc(reply->count * sizeof(struct feed_tag *));
      query_code = get_feed_tags(reply->count, feed_tags, feed_id,
                                 reply->page, reply->page_size);

      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEED TAGS"));
        HANDLE_QUERY_CODE;

        free(reply->json);
        free(reply->data);
        free(reply);
        return;
      }
    }

    reply->data = feed_tags_to_json(feed_tags, reply->count);
    list_reply_to_json(reply);

    SUCCESS_REPLY_200(reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== FEED TAGS SUCCESSFULLY SENT ==="));

    if (reply->count > 0) {
      free_feed_tags(feed_tags, reply->count);
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

    // Hydrate
    struct feed_tag *feed_tag = malloc(sizeof(struct feed_tag));
    int init_rc = feed_tag_init(feed_tag);
    if (init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED TAG IS NULL"));

      return;
    }

    feed_tag->feed_id = feed_id;
    feed_tag_hydrate(msg, feed_tag);

    // Check if tag exists
    exists = tag_exists(feed_tag->tag_name);
    if (!exists) {
      ERROR_REPLY_404;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG NOT FOUND"));
      free_feed_tag(feed_tag);
      return;
    }

    // Check for duplicate association
    exists = feed_tag_exists(feed_id, feed_tag->tag_name);
    if (exists) {
      ERROR_REPLY_400(TAG_EXISTS_MESSAGE);
      free_feed_tag(feed_tag);
      return;
    }

    // Store in DB
    query_code = add_feed_tag(feed_tag);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING FEEDS"));
      HANDLE_QUERY_CODE;

      free_feed_tag(feed_tag);
      return;
    } else {
      SUCCESS_REPLY_201_MSG("Feed tag successfully created");
      printf(TERMINAL_SUCCESS_MESSAGE("=== FEED TAG SUCCESSFULLY ADDED ==="));
    }

    free_feed_tag(feed_tag);
  } else {
    ERROR_REPLY_405;
  }
}

void send_feed_tag_res(struct mg_connection *c, struct mg_http_message *msg,
                       int feed_id, char *id,
                       struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  // Check if exists
  int exists = feed_exists(feed_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("FEED NOT FOUND"));
    return;
  }
  exists = tag_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("TAG NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    // Check if user logged
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    int delete_rc = delete_feed_tag(feed_id, id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE FEED TAG"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== FEED TAG SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Feed tag successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}
