#pragma once

/**
 * @file sql.h
 * @brief Debug macro for logging a parameterised Postgres query.
 */

/**
 * @brief Logs the SQL text and bound parameter values of a query about to
 *        be executed via pg_exec(). No-op unless built with -DDEBUG (see
 *        pg_log_query() in src/lib/pg.c).
 *
 * @param sql    SQL text with `$1`, `$2`, ... placeholders.
 * @param n      Number of elements in @p values.
 * @param values Array of C-string param values (NULL entries are SQL NULL).
 */
#define GET_EXPANDED_QUERY(sql, n, values) pg_log_query(sql, n, values)
