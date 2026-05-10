/**
 * @file sql/blocked_email_domain.c
 * @brief SQLite data-access implementation for the BlockedEmailDomain table.
 */

#include <enums.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/blocked_email_domain.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP  "SELECT COUNT(*) FROM BlockedEmailDomain"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE domain = ?"
#define QUERY_SELECT_TMP "SELECT domain FROM BlockedEmailDomain ORDER BY domain"
#define QUERY_POST_TMP   "INSERT INTO BlockedEmailDomain (domain) VALUES (?);"
#define QUERY_DELETE_TMP "DELETE FROM BlockedEmailDomain WHERE domain = ?;"

int blocked_domain_exists(const char *domain) {
  printf(TERMINAL_SQL_MESSAGE("=== BLOCKED DOMAIN EXISTS SQL ==="));

  if (domain == NULL) {
    return 0;
  }

  int query_rc;
  int count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_EXISTS_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, domain, -1, SQLITE_STATIC);

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
      count = sqlite3_column_int(stmt, 0);
      printf("COUNT:\t%d\n", count);
    }
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return count > 0;
}

int get_blocked_domains_len(void) {
  printf(TERMINAL_SQL_MESSAGE("=== GET BLOCKED DOMAINS COUNT SQL ==="));

  int query_rc;
  int count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_COUNT_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

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
      count = sqlite3_column_int(stmt, 0);
    }
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return count;
}

int get_blocked_domains(char ***arr, size_t *len) {
  printf(TERMINAL_SQL_MESSAGE("=== GET BLOCKED DOMAINS SQL ==="));

  int total = get_blocked_domains_len();
  if (total < 0) {
    return HTTP_INTERNAL_ERROR;
  }

  *len = (size_t)total;
  if (total == 0) {
    *arr = NULL;
    return 0;
  }

  *arr = malloc((size_t)total * sizeof(char *));
  if (*arr == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_SELECT_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    free(*arr);
    *arr = NULL;
    return HTTP_INTERNAL_ERROR;
  }

  GET_EXPANDED_QUERY(stmt);

  size_t count = 0;
  query_rc = sqlite3_step(stmt);

  while (query_rc == SQLITE_ROW && count < (size_t)total) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_TEXT) {
      const char *str = (const char *)sqlite3_column_text(stmt, 0);
      (*arr)[count] = strdup(str);
      count++;
    }
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  *len = count;

  return 0;
}

int add_blocked_domain(const char *domain) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD BLOCKED DOMAIN SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_POST_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, domain, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int delete_blocked_domain(const char *domain) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE BLOCKED DOMAIN SQL ==="));

  // Check if exists first
  int exists = blocked_domain_exists(domain);
  if (exists <= 0) {
    return HTTP_NOT_FOUND;
  }

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_DELETE_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, domain, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}
