#include <enums.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/tag.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Tag"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE name = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "name, color "                                                               \
  "FROM Tag"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE name = ?"
#define QUERY_Q_TMP " WHERE name LIKE ?100 OR color LIKE ?100"
#define QUERY_SORT_TMP " ORDER BY name COLLATE NOCASE %s"
#define QUERY_PAGINATION_TMP " LIMIT ?102 OFFSET ?103"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO Tag (name, color) "                                             \
  "VALUES (?, ?);"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Tag "                                                                \
  "SET name = ?, color = ? "                                                   \
  "WHERE name = ?;"
#define QUERY_DELETE_TMP "DELETE FROM Tag WHERE name = ?;"

extern sqlite3 *db;

int tag_exists(char *name) {
  printf(TERMINAL_SQL_MESSAGE("=== TAG EXISTS SQL ==="));

  int query_rc = SQLITE_ROW;
  int tags_count = 0;

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
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

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
      tags_count = sqlite3_column_int(stmt, 0);
      printf("COUNT:\t%d\n", tags_count);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return tags_count > 0;
}

int get_tags_len(const struct mg_str *q) {
  printf(TERMINAL_SQL_MESSAGE("=== GET TAGS COUNT SQL ==="));

  int query_rc = SQLITE_ROW;

  char *q_str = NULL;

  char *query_tmp = QUERY_COUNT_TMP;
  char *query_q_tmp = QUERY_Q_TMP;
  int query_len = strlen(query_tmp) + 2;
  if (q->len > 0) {
    q_str = malloc(q->len + 2);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    query_len += strlen(query_q_tmp);
  }

  char *query = malloc(query_len);
  strcpy(query, query_tmp);
  if (q->len > 0) {
    strcat(query, query_q_tmp);
  }
  strcat(query, ";");

  int tags_count = 0;

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
  if (q_str != NULL) {
    sqlite3_bind_text(stmt, 100, q_str, -1, SQLITE_STATIC);
  }

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
      tags_count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  free(q_str);
  free(query);

  return tags_count;
}

int get_tags(size_t len, struct tag **arr, const struct mg_str *q,
             const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET TAGS SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_SELECT_TMP;
  char *query_params_tmp = QUERY_Q_TMP;
  char *query_pagination_tmp = QUERY_PAGINATION_TMP;

  // Sort tmp
  const char *sort_keyword = "ASC";
  char *query_sort_tmp = NULL;
  if (sort->len > 0) {
    if (strncasecmp(sort->buf, "desc", sort->len) == 0) {
      sort_keyword = "DESC";
    } else if (strncasecmp(sort->buf, "asc", sort->len) == 0) {
      sort_keyword = "ASC";
    } else {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG VALUE FOR SORTING"));
      return HTTP_BAD_REQUEST;
    }

    query_sort_tmp =
        malloc(snprintf(NULL, 0, QUERY_SORT_TMP, sort_keyword) + 1);
    sprintf(query_sort_tmp, QUERY_SORT_TMP, sort_keyword);
  }

  char *q_str = NULL;

  int query_len = strlen(query_tmp) + 2;
  if (q->len > 0) {
    q_str = malloc(q->len + 2);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    query_len += strlen(query_params_tmp);
  }
  if (sort->len > 0) {
    query_len += strlen(query_sort_tmp);
  }
  if (page > 0) {
    query_len += strlen(query_pagination_tmp);
  }

  char *query = malloc(query_len);
  strcpy(query, query_tmp);
  if (q->len > 0) {
    strcat(query, query_params_tmp);
  }
  if (sort->len > 0) {
    strcat(query, query_sort_tmp);
  }
  if (page > 0) {
    strcat(query, query_pagination_tmp);
  }
  strcat(query, ";");
  free(query_sort_tmp);

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
  if (q->len > 0) {
    sqlite3_bind_text(stmt, 100, q_str, -1, SQLITE_STATIC);
  }
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
    free(q_str);
    return query_rc;
  }

  size_t count = 0;
  while (query_rc == SQLITE_ROW && count < len) {
    struct tag *u = NULL;
    u = malloc(sizeof(struct tag));

    int tag_init_rc = tag_init(u);
    if (tag_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The tag is NULL"));
      free(q_str);
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = NULL;
    m = malloc(sizeof(struct media));

    int tag_rc = tag_map(u, stmt, 0, 1);
    if (tag_rc != 0) {
      free(u);

      count += 1;
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
  free(q_str);

  return 0;
}

int get_tag(struct tag *tag, char *name) {
  if (name < 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET TAG SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_SELECT_SINGLE_TMP ";";

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  // Binding
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  } else if (query_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return HTTP_NOT_FOUND;
  }

  while (query_rc == SQLITE_ROW) {
    int tag_init_rc = tag_init(tag);
    if (tag_init_rc != 0) {
      fprintf(stderr, "The tag is NULL\n");
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = NULL;
    m = malloc(sizeof(struct media));

    int tag_rc = tag_map(tag, stmt, 0, 1);
    if (tag_rc != 0) {
      free(tag);

      query_rc = sqlite3_step(stmt);
      continue;
    }

    printf("\n");
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int add_tag(struct tag *tag) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD TAG SQL ==="));

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
  sqlite3_bind_text(stmt, 1, tag->name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, tag->color, -1, SQLITE_STATIC);

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

int edit_tag(struct tag *tag, char *name) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT TAG SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_PUT_TMP;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  // Binding
  sqlite3_bind_text(stmt, 1, tag->name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, tag->color, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);

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

int delete_tag(char *name) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE TAG SQL ==="));

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
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

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
