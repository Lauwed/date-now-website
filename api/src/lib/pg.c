/**
 * @file pg.c
 * @brief Thin libpq helpers shared by every file under src/sql/.
 */

#include <lib/pg.h>
#include <macros/colors.h>
#include <pthread.h>
#include <stdio.h>

PGconn *db = NULL;

/* One libpq connection is shared by the whole process, and the media upload
 * endpoint converts images on a detached thread that writes to the database
 * while the HTTP loop keeps serving requests. libpq forbids using a single
 * connection from two threads at once — without this lock the two paths
 * corrupt the heap. */
static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

PGresult *pg_exec(const char *sql, int n_params, const char *const *values) {
  pthread_mutex_lock(&db_lock);

  /* Providers like Neon suspend the compute (and drop the TCP connection)
   * after idle periods — reconnect before running if that's happened. */
  if (PQstatus(db) != CONNECTION_OK) {
    PQreset(db);
  }

  PGresult *res = PQexecParams(db, sql, n_params, NULL, values, NULL, NULL, 0);
  ExecStatusType status = PQresultStatus(res);

  if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK &&
      PQstatus(db) != CONNECTION_OK) {
    /* Connection died mid-query — reconnect and retry once. */
    PQclear(res);
    PQreset(db);
    res = PQexecParams(db, sql, n_params, NULL, values, NULL, NULL, 0);
    status = PQresultStatus(res);
  }

  if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("pg query error: %s\n"),
            PQerrorMessage(db));
    PQclear(res);
    res = NULL;
  }

  pthread_mutex_unlock(&db_lock);
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
