/**
 * @file pg.c
 * @brief Thin libpq helpers shared by every file under src/sql/.
 */

#include <lib/pg.h>
#include <macros/colors.h>
#include <stdio.h>

PGconn *db = NULL;

PGresult *pg_exec(const char *sql, int n_params, const char *const *values) {
  PGresult *res = PQexecParams(db, sql, n_params, NULL, values, NULL, NULL, 0);

  ExecStatusType status = PQresultStatus(res);
  if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("pg query error: %s\n"),
            PQerrorMessage(db));
    PQclear(res);
    return NULL;
  }

  return res;
}

void pg_log_query(const char *sql, int n_params, const char *const *values) {
#ifdef DEBUG
  printf("=== QUERY:\n%s\nPARAMS:", sql);
  for (int i = 0; i < n_params; i++) {
    printf(" $%d=%s", i + 1, values[i] ? values[i] : "NULL");
  }
  printf("\n===\n");
#else
  (void)sql;
  (void)n_params;
  (void)values;
#endif
}
