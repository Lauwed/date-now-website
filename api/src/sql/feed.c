/**
 * @file feed.c
 * @brief Postgres data-access implementation for the Feed table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/feed.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Feed"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = $1"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "id, name, link, isRssFeed "                                                 \
  "FROM Feed"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE id = $1"
#define QUERY_Q_TMP " WHERE name LIKE $%1$d OR link LIKE $%1$d"
#define QUERY_SORT_TMP " ORDER BY LOWER(name) %s"
#define QUERY_PAGINATION_TMP " LIMIT $%d OFFSET $%d"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO Feed (name, link, isRssFeed) "                                  \
  "VALUES ($1, $2, $3) RETURNING id;"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Feed "                                                               \
  "SET name = $1, link = $2, isRssFeed = $3 "                                  \
  "WHERE id = $4;"
#define QUERY_DELETE_TMP "DELETE FROM Feed WHERE id = $1;"

int feed_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== FEED EXISTS SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int feeds_count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", feeds_count);

  PQclear(res);

  return feeds_count > 0;
}

int get_feeds_len(const struct mg_str *q) {
  printf(TERMINAL_SQL_MESSAGE("=== GET FEEDS COUNT SQL ==="));

  char *q_str = NULL;
  const char *values[1];
  int n_values = 0;

  char query[512] = QUERY_COUNT_TMP;
  if (q->len > 0) {
    q_str = malloc(q->len + 3);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, 1);
    strcat(query, clause);

    values[n_values++] = q_str;
  }
  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  free(q_str);
  if (res == NULL) {
    return -1;
  }

  int feeds_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return feeds_count;
}

int get_feeds(size_t len, struct feed **arr, const struct mg_str *q,
             const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET FEEDS SQL ==="));

  const char *sort_keyword = "ASC";
  if (sort->len > 0) {
    if (strncasecmp(sort->buf, "desc", sort->len) == 0) {
      sort_keyword = "DESC";
    } else if (strncasecmp(sort->buf, "asc", sort->len) == 0) {
      sort_keyword = "ASC";
    } else {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG VALUE FOR SORTING"));
      return HTTP_BAD_REQUEST;
    }
  }

  char *q_str = NULL;
  char offset_str[16], page_size_str[16];
  const char *values[3];
  int n_values = 0;

  char query[768] = QUERY_SELECT_TMP;

  if (q->len > 0) {
    q_str = malloc(q->len + 3);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, n_values + 1);
    strcat(query, clause);

    values[n_values++] = q_str;
  }

  if (sort->len > 0) {
    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_SORT_TMP, sort_keyword);
    strcat(query, clause);
  }

  if (page > 0) {
    int offset = (page - 1) * page_size;
    snprintf(page_size_str, sizeof(page_size_str), "%d", page_size);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);

    char clause[64];
    snprintf(clause, sizeof(clause), QUERY_PAGINATION_TMP, n_values + 1,
             n_values + 2);
    strcat(query, clause);

    values[n_values++] = page_size_str;
    values[n_values++] = offset_str;
  }

  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  free(q_str);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct feed *u = malloc(sizeof(struct feed));

    int feed_init_rc = feed_init(u);
    if (feed_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The feed is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int feed_rc = feed_map(u, &row, 0, 3);
    if (feed_rc != 0) {
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

int get_feed(struct feed *feed, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET FEED SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int feed_init_rc = feed_init(feed);
  if (feed_init_rc != 0) {
    fprintf(stderr, "The feed is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  pg_row_t row = {res, 0};
  int feed_rc = feed_map(feed, &row, 0, 3);
  PQclear(res);
  if (feed_rc != 0) {
    return HTTP_INTERNAL_ERROR;
  }

  return 0;
}

int add_feed(struct feed *feed) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD FEED SQL ==="));

  char is_rss_feed_str[8];
  snprintf(is_rss_feed_str, sizeof(is_rss_feed_str), "%d", feed->is_rss_feed);
  const char *values[3] = {feed->name, feed->link, is_rss_feed_str};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 3, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 3, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  feed->id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return 0;
}

int edit_feed(struct feed *feed) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT FEED SQL ==="));

  char is_rss_feed_str[8], id_str[16];
  snprintf(is_rss_feed_str, sizeof(is_rss_feed_str), "%d", feed->is_rss_feed);
  snprintf(id_str, sizeof(id_str), "%d", feed->id);
  const char *values[4] = {feed->name, feed->link, is_rss_feed_str, id_str};
  GET_EXPANDED_QUERY(QUERY_PUT_TMP, 4, values);

  PGresult *res = pg_exec(QUERY_PUT_TMP, 4, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_feed(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE FEED SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}
