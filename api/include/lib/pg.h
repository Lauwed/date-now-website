#pragma once

/**
 * @file lib/pg.h
 * @brief Thin libpq helpers shared by every file under src/sql/.
 *
 * Replaces the previous SQLite embedded-connection model with a single
 * long-lived PGconn for the main event loop (background pthreads open
 * their own short-lived connection instead of sharing this one — libpq
 * connections are not safe to use concurrently from multiple threads).
 */

#include <libpq-fe.h>

/** @brief The main event loop's PGconn, opened once in main.c. */
extern PGconn *db;

/**
 * @brief A single row of a PGresult, paired with its result set.
 *
 * Passed in place of the old `sqlite3_stmt *` to every `*_map()` function —
 * same call-site shape (`struct*, row, index, required`), only the
 * underlying storage changed from a streaming cursor to a materialised
 * result row.
 */
typedef struct {
  PGresult *res;
  int row;
} pg_row_t;

/**
 * @brief Executes a parameterised query (all params passed as text).
 * @param sql       SQL text with `$1`, `$2`, ... placeholders.
 * @param n_params  Number of elements in @p values.
 * @param values    Array of C-string param values; a NULL entry binds SQL
 *                  NULL for that parameter.
 * @return A PGresult* on success (caller must PQclear() it), or NULL on
 *         error (the error is already logged to stderr via PQerrorMessage).
 * @note Succeeds for both SELECT (PGRES_TUPLES_OK) and
 *       INSERT/UPDATE/DELETE (PGRES_COMMAND_OK, optionally with a
 *       RETURNING clause, which also reports PGRES_TUPLES_OK).
 */
PGresult *pg_exec(const char *sql, int n_params, const char *const *values);

/**
 * @brief Debug-only query logger, called right before pg_exec() — mirrors
 *        the old GET_EXPANDED_QUERY(stmt) macro's role. No-op unless built
 *        with -DDEBUG.
 */
void pg_log_query(const char *sql, int n_params, const char *const *values);
