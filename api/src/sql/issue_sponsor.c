/**
 * @file issue_sponsor.c
 * @brief SQLite data-access implementation for the IssueSponsor join table.
 */

#include <enums.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/issue.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <structs.h>
#include <time.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM IssueSponsor WHERE issueId = ?"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND sponsorName = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT issueId, sponsorName, link FROM IssueSponsor WHERE issueId = ?"

#define QUERY_PAGINATION_TMP " LIMIT ?102 OFFSET ?103"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO IssueSponsor (issueId, sponsorName, link) "                     \
  "VALUES (?, ?, ?);"

#define QUERY_DELETE_TMP                                                       \
  "DELETE FROM IssueSponsor WHERE issueId = ? AND sponsorName = ?;"

int issue_sponsor_exists(int issue_id, char *id) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE EXISTS SQL ==="));

  int query_rc = SQLITE_ROW;
  int issues_count = 0;

  char *query_tmp = QUERY_EXISTS_TMP ";";

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  // Binding
  sqlite3_bind_int(stmt, 1, issue_id);
  sqlite3_bind_text(stmt, 2, id, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  while (query_rc != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
      issues_count = sqlite3_column_int(stmt, 0);
      printf("COUNT:\t%d\n", issues_count);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return issues_count > 0;
}

int get_issue_sponsors_len(const struct mg_str *q, int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES COUNT SQL ==="));

  int query_rc = SQLITE_ROW;

  char *q_str = NULL;

  char *query_tmp = QUERY_COUNT_TMP;

  char *query = malloc(strlen(query_tmp) + 1);
  strcpy(query, query_tmp);
  strcat(query, ";");

  int issues_count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    free(q_str);

    return query_rc;
  }

  // Binding
  sqlite3_bind_int(stmt, 1, issue_id);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    free(q_str);
    sqlite3_finalize(stmt);
    return query_rc;
  }

  while (query_rc != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
      issues_count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  free(q_str);
  free(query);

  return issues_count;
}

int get_issue_sponsors(size_t len, struct issue_sponsor **arr, int issue_id,
                       int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_SELECT_TMP;
  char *query_pagination_tmp = QUERY_PAGINATION_TMP;
  int query_len = strlen(query_tmp) + 2;

  if (page > 0) {
    query_len += strlen(query_pagination_tmp);
  }

  char *query = malloc(query_len);
  strcpy(query, query_tmp);
  if (page > 0) {
    strcat(query, query_pagination_tmp);
  }
  strcat(query, ";");

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  // Binding
  sqlite3_bind_int(stmt, 1, issue_id);
  if (page > 0) {
    int offset = (page - 1) * page_size;
    sqlite3_bind_int(stmt, 102, page_size);
    sqlite3_bind_int(stmt, 103, offset);
  }

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  size_t count = 0;
  while (query_rc == SQLITE_ROW && count < len) {
    struct issue_sponsor *u = NULL;
    u = malloc(sizeof(struct issue));

    int issue_init_rc = issue_sponsor_init(u);
    if (issue_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The issue sponsor is NULL"));
      return HTTP_INTERNAL_ERROR;
    }

    int issue_rc = issue_sponsor_map(u, stmt, 0, 1);
    if (issue_rc != 0) {
      free(u);

      query_rc = sqlite3_step(stmt);
      fprintf(stderr,
              TERMINAL_ERROR_MESSAGE("Error at line: %ld. Error code: %d"),
              count, query_rc);
      continue;
    }

    printf("\n");

    // Add a to arr
    arr[count] = u;

    count += 1;
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int add_issue_sponsor(struct issue_sponsor *issue) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ISSUE SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_POST_TMP;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  // Binding
  sqlite3_bind_int(stmt, 1, issue->issue_id);
  sqlite3_bind_text(stmt, 2, issue->sponsor_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, issue->link, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int delete_issue_sponsor(int issue_id, char *id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ISSUE SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_DELETE_TMP;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  // Binding
  sqlite3_bind_int(stmt, 1, issue_id);
  sqlite3_bind_text(stmt, 2, id, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}
