/**
 * @file blocked_email_domain.c
 * @brief Postgres data-access implementation for the BlockedEmailDomain table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/blocked_email_domain.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUERY_EXISTS_TMP                                                       \
  "SELECT COUNT(*) FROM BlockedEmailDomain WHERE domain = $1;"
#define QUERY_SELECT_TMP "SELECT domain FROM BlockedEmailDomain ORDER BY domain;"
#define QUERY_POST_TMP                                                         \
  "INSERT INTO BlockedEmailDomain (domain) VALUES ($1) "                       \
  "ON CONFLICT (domain) DO NOTHING;"
#define QUERY_DELETE_TMP                                                       \
  "DELETE FROM BlockedEmailDomain WHERE domain = $1 RETURNING domain;"

int blocked_domain_exists(const char *domain) {
  printf(TERMINAL_SQL_MESSAGE("=== BLOCKED DOMAIN EXISTS SQL ==="));

  if (domain == NULL) {
    return 0;
  }

  const char *values[1] = {domain};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return count > 0;
}

int get_blocked_domains(char ***arr, size_t *len) {
  printf(TERMINAL_SQL_MESSAGE("=== GET BLOCKED DOMAINS SQL ==="));

  *arr = NULL;
  *len = 0;

  GET_EXPANDED_QUERY(QUERY_SELECT_TMP, 0, NULL);

  PGresult *res = pg_exec(QUERY_SELECT_TMP, 0, NULL);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  if (n_rows == 0) {
    PQclear(res);
    return 0;
  }

  char **domains = calloc((size_t)n_rows, sizeof(char *));
  if (domains == NULL) {
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  for (int i = 0; i < n_rows; i++) {
    domains[i] = strdup(PQgetvalue(res, i, 0));
    if (domains[i] == NULL) {
      free_blocked_domains(domains, (size_t)i);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }
  }

  PQclear(res);
  *arr = domains;
  *len = (size_t)n_rows;

  return 0;
}

void free_blocked_domains(char **arr, size_t len) {
  if (arr == NULL) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    free(arr[i]);
  }
  free(arr);
}

int add_blocked_domain(const char *domain) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD BLOCKED DOMAIN SQL ==="));

  const char *values[1] = {domain};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_blocked_domain(const char *domain) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE BLOCKED DOMAIN SQL ==="));

  const char *values[1] = {domain};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int deleted = PQntuples(res);
  PQclear(res);

  return deleted > 0 ? 0 : HTTP_NOT_FOUND;
}
