#pragma once

/**
 * @file sql.h
 * @brief Debug macro for printing an expanded SQLite query to stdout.
 */

/**
 * @brief Prints the fully-expanded SQL text of a prepared statement.
 *
 * Calls sqlite3_expanded_sql() on @p stmt, prints the result, then frees
 * it with sqlite3_free(). Useful for debugging parameterised queries.
 *
 * @param stmt A `sqlite3_stmt *` that has been prepared and bound.
 */
#ifdef DEBUG
#define GET_EXPANDED_QUERY(stmt)                                               \
  char *expanded = sqlite3_expanded_sql(stmt);                                 \
  if (expanded) {                                                              \
    printf("=== QUERY:\n%s\n===\n", expanded);                                 \
    sqlite3_free(expanded);                                                    \
  }
#else
#define GET_EXPANDED_QUERY(stmt) ((void)(stmt))
#endif
