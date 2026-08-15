/**
 * @file issue_tag.c
 * @brief Postgres data-access implementation for the IssueTag join table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/issue.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <time.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM IssueTag WHERE issueId = $1"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND tagName = $2"
#define QUERY_SELECT_TMP                                                       \
  "SELECT issueId, tagName FROM IssueTag WHERE issueId = $1"

#define QUERY_PAGINATION_TMP " LIMIT $2 OFFSET $3"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO IssueTag (issueId, tagName) "                                   \
  "VALUES ($1, $2);"

#define QUERY_DELETE_TMP                                                       \
  "DELETE FROM IssueTag WHERE issueId = $1 AND tagName = $2;"

int issue_tag_exists(int issue_id, char *id) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE EXISTS SQL ==="));

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  const char *values[2] = {issue_id_str, id};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 2, values);
  if (res == NULL) {
    return -1;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", issues_count);

  PQclear(res);

  return issues_count > 0;
}

int get_issue_tags_len(const struct mg_str *q, int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES COUNT SQL ==="));
  (void)q;

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  const char *values[1] = {issue_id_str};
  GET_EXPANDED_QUERY(QUERY_COUNT_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_COUNT_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return issues_count;
}

int get_issue_tags(size_t len, struct issue_tag **arr, int issue_id, int page,
                   int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES SQL ==="));

  char issue_id_str[16], page_size_str[16], offset_str[16];
  const char *values[3];
  int n_values = 0;

  char query[256] = QUERY_SELECT_TMP;

  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  values[n_values++] = issue_id_str;

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
    struct issue_tag *u = malloc(sizeof(struct issue_tag));

    int issue_init_rc = issue_tag_init(u);
    if (issue_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The issue tag is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int issue_rc = issue_tag_map(u, &row, 0, 1);
    if (issue_rc != 0) {
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

int add_issue_tag(struct issue_tag *issue) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ISSUE SQL ==="));

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue->issue_id);
  const char *values[2] = {issue_id_str, issue->tag_name};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_issue_tag(int issue_id, char *id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ISSUE SQL ==="));

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  const char *values[2] = {issue_id_str, id};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}
