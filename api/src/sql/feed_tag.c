/**
 * @file feed_tag.c
 * @brief Postgres data-access implementation for the FeedTag join table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/feed.h>
#include <sql/feed_tag.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <time.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM FeedTag WHERE feedId = $1"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND tagName = $2"
#define QUERY_SELECT_TMP                                                       \
  "SELECT feedId, tagName FROM FeedTag WHERE feedId = $1"

#define QUERY_PAGINATION_TMP " LIMIT $2 OFFSET $3"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO FeedTag (feedId, tagName) "                                     \
  "VALUES ($1, $2);"

#define QUERY_DELETE_TMP                                                       \
  "DELETE FROM FeedTag WHERE feedId = $1 AND tagName = $2;"

int feed_tag_exists(int feed_id, char *id) {
  printf(TERMINAL_SQL_MESSAGE("=== FEED TAG EXISTS SQL ==="));

  char feed_id_str[16];
  snprintf(feed_id_str, sizeof(feed_id_str), "%d", feed_id);
  const char *values[2] = {feed_id_str, id};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 2, values);
  if (res == NULL) {
    return -1;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", count);

  PQclear(res);

  return count > 0;
}

int get_feed_tags_len(const struct mg_str *q, int feed_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET FEED TAGS COUNT SQL ==="));
  (void)q;

  char feed_id_str[16];
  snprintf(feed_id_str, sizeof(feed_id_str), "%d", feed_id);
  const char *values[1] = {feed_id_str};
  GET_EXPANDED_QUERY(QUERY_COUNT_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_COUNT_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return count;
}

int get_feed_tags(size_t len, struct feed_tag **arr, int feed_id, int page,
                  int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET FEED TAGS SQL ==="));

  char feed_id_str[16], page_size_str[16], offset_str[16];
  const char *values[3];
  int n_values = 0;

  char query[256] = QUERY_SELECT_TMP;

  snprintf(feed_id_str, sizeof(feed_id_str), "%d", feed_id);
  values[n_values++] = feed_id_str;

  if (page > 0) {
    int offset = (page - 1) * page_size;
    snprintf(page_size_str, sizeof(page_size_str), "%d", page_size);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);
    strcat(query, QUERY_PAGINATION_TMP);
    values[n_values++] = page_size_str;
    values[n_values++] = offset_str;
  }
  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct feed_tag *u = malloc(sizeof(struct feed_tag));

    int init_rc = feed_tag_init(u);
    if (init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The feed tag is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int map_rc = feed_tag_map(u, &row, 0, 1);
    if (map_rc != 0) {
      free(u);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("Error mapping row: %d"), i);
      continue;
    }

    arr[count] = u;
    count += 1;
  }

  PQclear(res);

  return 0;
}

int add_feed_tag(struct feed_tag *feed_tag) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD FEED TAG SQL ==="));

  char feed_id_str[16];
  snprintf(feed_id_str, sizeof(feed_id_str), "%d", feed_tag->feed_id);
  const char *values[2] = {feed_id_str, feed_tag->tag_name};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_feed_tag(int feed_id, char *id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE FEED TAG SQL ==="));

  char feed_id_str[16];
  snprintf(feed_id_str, sizeof(feed_id_str), "%d", feed_id);
  const char *values[2] = {feed_id_str, id};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}
