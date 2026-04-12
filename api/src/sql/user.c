#include <enums.h>
#include <sqlite3.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/user.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM User"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = ?"
#define QUERY_IDENTITY_EXISTS_TMP                                              \
  QUERY_COUNT_TEMP " WHERE username = ? OR email = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "u.id, u.username, u.email, u.role, u.link, UNIXEPOCH(u.subscribedAt), "     \
  "u.isSupporter, UNIXEPOCH(u.createdAt), m.id, m.textAlternatif, m.url, "     \
  "m.width, m.height "                                                         \
  "FROM User u LEFT JOIN Media m ON m.id = u.picture";
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE id = ?"
#define QUERY_Q_TMP                                                            \
  " WHERE username LIKE ?100 OR email LIKE ?100 OR link LIKE ?100"
#define QUERY_SORT_TMP " ORDER BY email COLLATE NOCASE %s"
#define QUERY_PAGINATION_TMP " LIMIT ?102 OFFSET ?103"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO User (username, email, role, link)"                             \
  "VALUES (?, ?, COALESCE(?, 'USER'), ?);";
#define QUERY_PUT_TMP                                                          \
  "UPDATE User "                                                               \
  "SET username = ?, email = ?, role = COALESCE(?, 'USER'), "                  \
  "link = ?, isSupporter = ? "                                                 \
  "WHERE id = ?;";

#define QUERY_DELETE_TMP "DELETE FROM User WHERE id = ?;"

int user_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== USER EXISTS SQL ==="));

  int query_rc = SQLITE_ROW;
  int users_count = 0;

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
  sqlite3_bind_int(stmt, 1, id);

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
      users_count = sqlite3_column_int(stmt, 0);
      printf("COUNT:\t%d\n", users_count);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return users_count > 0;
}

int user_identity_exists(char *username, char *email) {
  if (email == NULL) {
    return -1;
  }

  printf(TERMINAL_SQL_MESSAGE("=== USER EMAIL EXISTS SQL ==="));

  int users_count = 0;

  char *query_tmp =
      "SELECT COUNT(*) FROM User WHERE username = ? OR email = ?;";

  sqlite3_stmt *stmt = NULL;
  sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);

  // Binding
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  int query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  while (query_rc != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
      users_count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return users_count > 0;
}

int get_users_len(const struct mg_str *q) {
  printf(TERMINAL_SQL_MESSAGE("=== GET USERS COUNT SQL ==="));

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

  int users_count = 0;

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
      users_count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  free(q_str);
  free(query);

  return users_count;
}

int get_users(size_t len, struct user **arr, const struct mg_str *q,
              const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET USERS SQL ==="));

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
    struct user *u = NULL;
    u = malloc(sizeof(struct user));

    int user_init_rc = user_init(u);
    if (user_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The user is NULL"));
      free(q_str);
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = NULL;
    m = malloc(sizeof(struct media));

    int user_rc = user_map(u, stmt, 0, 7);
    if (user_rc != 0) {
      free(u);

      count += 1;
      query_rc = sqlite3_step(stmt);
      fprintf(stderr,
              TERMINAL_ERROR_MESSAGE("Error at line: %ld. Error code: %d"),
              count, query_rc);
      continue;
    }

    // Picture
    int picture_rc = media_map(m, stmt, 8, 12);
    if (picture_rc != 0) {
      free(m);
    } else {
      u->picture = m;
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

int get_user(struct user *user, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET USER SQL ==="));

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
  sqlite3_bind_int(stmt, 1, id);

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
    int user_init_rc = user_init(user);
    if (user_init_rc != 0) {
      fprintf(stderr, "The user is NULL\n");
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = NULL;
    m = malloc(sizeof(struct media));

    int user_rc = user_map(user, stmt, 0, 7);
    if (user_rc != 0) {
      free(user);

      query_rc = sqlite3_step(stmt);
      continue;
    }

    // Picture
    int picture_rc = media_map(m, stmt, 8, 12);
    if (picture_rc != 0) {
      free(m);
    } else {
      user->picture = m;
    }

    printf("\n");
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int add_user(struct user *user) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD USER SQL ==="));

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
  sqlite3_bind_text(stmt, 1, user->username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, user->email, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, user->role, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, user->link, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int edit_user(struct user *user) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT USER SQL ==="));

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
  sqlite3_bind_text(stmt, 1, user->username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, user->email, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, user->role, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, user->link, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 5, user->is_supporter);
  sqlite3_bind_int(stmt, 6, user->id);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int delete_user(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE USER SQL ==="));

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
  sqlite3_bind_int(stmt, 1, id);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}
