#include <enums.h>
#include <lib/sqlite3.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/issue.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <time.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Issue"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = ?"
#define QUERY_IDENTITY_EXISTS_TMP                                              \
  QUERY_COUNT_TMP " WHERE title = ? OR slug = ? OR issueNumber = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "i.id, i.slug, i.title, i.subtitle, UNIXEPOCH(i.createdAt), "                \
  "COALESCE(UNIXEPOCH(i.publishedAt), i.publishedAt), "                        \
  "COALESCE(UNIXEPOCH(i.updatedAt), i.updatedAt), i.issueNumber, i.excerpt, "  \
  "i.content, "                                                                \
  "i.isSponsored, "                                                            \
  "i.status, i.openedMailCount, "                                              \
  "m.id, m.textAlternatif, m.url, m.width, m.height "                          \
  "FROM Issue i LEFT JOIN Media m ON m.id = i.cover"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE i.id = ?"
#define QUERY_Q_TMP                                                            \
  " WHERE i.title LIKE ?100 OR CAST(i.issueNumber, Text) LIKE ?100 OR "        \
  "i.content LIKE ?100"
#define QUERY_SORT_TMP " ORDER BY i.title COLLATE NOCASE %s"
#define QUERY_PAGINATION_TMP " LIMIT ?102 OFFSET ?103"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO Issue (title, slug, subtitle, publishedAt, issueNumber, "       \
  "excerpt, content, isSponsored, status) "                                    \
  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, COALESCE(?, 'DRAFT'));"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Issue "                                                              \
  "SET title = ?, slug = ?, subtitle = ?, publishedAt = COALESCE(?, CASE "     \
  "WHEN status = 'PUBLISHED' THEN CURRENT_TIMESTAMP ELSE NULL END), "          \
  "issueNumber = ?, "                                                          \
  "excerpt = ?, content = ?, isSponsored = ?, status = COALESCE(?, 'DRAFT'), " \
  "updatedAt = CURRENT_TIMESTAMP "                                             \
  "WHERE id = ?;";

#define QUERY_DELETE_TMP "DELETE FROM Issue WHERE id = ?;"

int issue_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE EXISTS SQL ==="));

  int query_rc = SQLITE_ROW;
  int issues_count = 0;

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
      issues_count = sqlite3_column_int(stmt, 0);
      printf("COUNT:\t%d\n", issues_count);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return issues_count > 0;
}

int issue_identity_exists(char *title, int issue_number, char *slug) {
  if (title == NULL && issue_number <= 0 && slug == NULL) {
    return -1;
  }

  printf(TERMINAL_SQL_MESSAGE("=== ISSUE IDENTITY EXISTS SQL ==="));

  int issues_count = 0;

  char *query_tmp = QUERY_IDENTITY_EXISTS_TMP;

  sqlite3_stmt *stmt = NULL;
  sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);

  // Binding
  sqlite3_bind_text(stmt, 1, title, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, slug, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 3, issue_number);

  GET_EXPANDED_QUERY(stmt);

  int query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  while (query_rc != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
      issues_count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return issues_count > 0;
}

int get_issues_len(const struct mg_str *q) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES COUNT SQL ==="));

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

  int issues_count = 0;

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
      issues_count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  free(q_str);
  free(query);

  return issues_count;
}

int get_issues(size_t len, struct issue **arr, const struct mg_str *q,
               const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES SQL ==="));

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
    struct issue *u = NULL;
    u = malloc(sizeof(struct issue));

    int issue_init_rc = issue_init(u);
    if (issue_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The issue is NULL"));
      free(q_str);
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = NULL;
    m = malloc(sizeof(struct media));

    int issue_rc = issue_map(u, stmt, 0, 7);
    if (issue_rc != 0) {
      free(u);

      count += 1;
      query_rc = sqlite3_step(stmt);
      fprintf(stderr,
              TERMINAL_ERROR_MESSAGE("Error at line: %ld. Error code: %d"),
              count, query_rc);
      continue;
    }

    // Picture
    int cover_rc = media_map(m, stmt, 8, 12);
    if (cover_rc != 0) {
      free(m);
    } else {
      u->cover = m;
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

int get_issue(struct issue *issue, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SQL ==="));

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
    int issue_init_rc = issue_init(issue);
    if (issue_init_rc != 0) {
      fprintf(stderr, "The issue is NULL\n");
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = NULL;
    m = malloc(sizeof(struct media));

    int issue_rc = issue_map(issue, stmt, 0, 13);
    if (issue_rc != 0) {
      free(issue);

      query_rc = sqlite3_step(stmt);
      continue;
    }

    // Picture
    int cover_rc = media_map(m, stmt, 14, 18);
    if (cover_rc != 0) {
      free(m);
    } else {
      issue->cover = m;
    }

    printf("\n");
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int add_issue(struct issue *issue) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ISSUE SQL ==="));

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

  if (issue->status != NULL && strcmp(issue->status, "PUBLISHED") == 0) {
    issue->published_at = time(NULL);
  }

  // Binding
  sqlite3_bind_text(stmt, 1, issue->title, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, issue->slug, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, issue->subtitle, -1, SQLITE_STATIC);
  if (issue->published_at > 0) {
    sqlite3_bind_int(stmt, 4, issue->published_at);
  }
  sqlite3_bind_int(stmt, 5, issue->issue_number);
  sqlite3_bind_text(stmt, 6, issue->excerpt, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, issue->content, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 8, issue->is_sponsored);
  sqlite3_bind_text(stmt, 9, issue->status, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int edit_issue(struct issue *issue) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT ISSUE SQL ==="));

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

  if (issue->status != NULL && strcmp(issue->status, "PUBLISHED") == 0) {
    issue->published_at = time(NULL);
  }

  // Binding
  sqlite3_bind_text(stmt, 1, issue->title, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, issue->slug, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, issue->subtitle, -1, SQLITE_STATIC);
  if (issue->published_at > 0) {
    sqlite3_bind_int(stmt, 4, issue->published_at);
  }
  sqlite3_bind_int(stmt, 5, issue->issue_number);
  sqlite3_bind_text(stmt, 6, issue->excerpt, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, issue->content, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 8, issue->is_sponsored);
  sqlite3_bind_text(stmt, 9, issue->status, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 10, issue->id);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int delete_issue(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ISSUE SQL ==="));

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
