/**
 * @file view.c
 * @brief Postgres data-access implementation for the View (page-view) table.
 */


#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/view.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM View"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "u.id, EXTRACT(EPOCH FROM u.time)::BIGINT, u.hashedIp, u.issueId "           \
  "FROM View u"
#define QUERY_Q_TMP " WHERE issueId = $%1$d"
#define QUERY_SORT_TMP " ORDER BY u.time %s"
#define QUERY_PAGINATION_TMP " LIMIT $%d OFFSET $%d"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO View (time, hashedIp, issueId) "                                \
  "VALUES (CURRENT_TIMESTAMP, $1, $2) RETURNING id;"

int get_views_len(int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET VIEWS COUNT SQL ==="));

  char issue_id_str[16];
  const char *values[1];
  int n_values = 0;

  char query[256] = QUERY_COUNT_TMP;
  if (issue_id > 0) {
    snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
    char clause[64];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, 1);
    strcat(query, clause);
    values[n_values++] = issue_id_str;
  }
  strcat(query, ";");
  printf("%s\n", query);

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  if (res == NULL) {
    return -1;
  }

  int views_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return views_count;
}

int get_views(size_t len, struct view **arr, int issue_id,
              const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET VIEWS SQL ==="));

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

  char issue_id_str[16], offset_str[16], page_size_str[16];
  const char *values[3];
  int n_values = 0;

  char query[768] = QUERY_SELECT_TMP;

  if (issue_id > 0) {
    snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
    char clause[64];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, n_values + 1);
    strcat(query, clause);
    values[n_values++] = issue_id_str;
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
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct view *u = malloc(sizeof(struct view));

    int view_init_rc = view_init(u);
    if (view_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The view is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int view_rc = view_map(u, &row, 0, 3);
    if (view_rc != 0) {
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

int add_view(struct view *view) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD VIEW SQL ==="));

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", view->issue_id);
  const char *values[2] = {view->hashed_ip, issue_id_str};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  view->id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return 0;
}
