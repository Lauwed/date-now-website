#pragma once

#define GET_EXPANDED_QUERY(stmt)                                               \
  char *expanded = sqlite3_expanded_sql(stmt);                                 \
  if (expanded) {                                                              \
    printf("=== QUERY:\n%s\n===\n", expanded);                                 \
    sqlite3_free(expanded);                                                    \
  }
