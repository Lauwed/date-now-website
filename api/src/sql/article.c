/**
 * @file article.c
 * @brief SQLite data-access implementation for the Article table.
 */

#include <enums.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/article.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Article WHERE sectionId = ?"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND id = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT id, sectionId, position, title, sourceName, sourceUrl, summary "     \
  "FROM Article WHERE sectionId = ? ORDER BY position ASC"
#define QUERY_SELECT_SINGLE_TMP                                                \
  "SELECT id, sectionId, position, title, sourceName, sourceUrl, summary "     \
  "FROM Article WHERE sectionId = ? AND id = ?"
#define QUERY_NEXT_POSITION_TMP                                                \
  "SELECT COALESCE(MAX(position), -1) + 1 FROM Article WHERE sectionId = ?"
#define QUERY_POST_TMP                                                         \
  "INSERT INTO Article (sectionId, position, title, sourceName, sourceUrl, "   \
  "summary) VALUES (?, ?, ?, ?, ?, ?);"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Article SET title = ?, sourceName = ?, sourceUrl = ?, summary = ? "  \
  "WHERE id = ? AND sectionId = ?;"
#define QUERY_REORDER_TMP                                                      \
  "UPDATE Article SET position = ? WHERE id = ? AND sectionId = ?;"
#define QUERY_DELETE_TMP "DELETE FROM Article WHERE id = ? AND sectionId = ?;"

int article_exists(int section_id, int article_id) {
  printf(TERMINAL_SQL_MESSAGE("=== ARTICLE EXISTS SQL ==="));

  int query_rc = SQLITE_ROW;
  int count = 0;

  char *query_tmp = QUERY_EXISTS_TMP ";";

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  sqlite3_bind_int(stmt, 1, section_id);
  sqlite3_bind_int(stmt, 2, article_id);

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
      count = sqlite3_column_int(stmt, 0);
      printf("COUNT:\t%d\n", count);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return count > 0;
}

int get_articles_len(int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ARTICLES COUNT SQL ==="));

  int query_rc = SQLITE_ROW;
  int count = 0;

  char *query_tmp = QUERY_COUNT_TMP ";";

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  sqlite3_bind_int(stmt, 1, section_id);

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
      count = sqlite3_column_int(stmt, 0);
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return count;
}

int get_articles(size_t len, struct article **arr, int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ARTICLES SQL ==="));

  int query_rc = SQLITE_ROW;

  char *query_tmp = QUERY_SELECT_TMP ";";

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query_tmp, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);

    return query_rc;
  }

  sqlite3_bind_int(stmt, 1, section_id);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  size_t count = 0;
  while (query_rc == SQLITE_ROW && count < len) {
    struct article *u = NULL;
    u = malloc(sizeof(struct article));

    int init_rc = article_init(u);
    if (init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The article is NULL"));
      return HTTP_INTERNAL_ERROR;
    }

    int map_rc = article_map(u, stmt, 0, 6);
    if (map_rc != 0) {
      free(u);

      count += 1;
      query_rc = sqlite3_step(stmt);
      fprintf(stderr,
              TERMINAL_ERROR_MESSAGE("Error at line: %ld. Error code: %d"),
              count, query_rc);
      continue;
    }

    printf("\n");

    arr[count] = u;

    count += 1;
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int get_article(struct article *article, int section_id, int article_id) {
  if (article_id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ARTICLE SQL ==="));

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

  sqlite3_bind_int(stmt, 1, section_id);
  sqlite3_bind_int(stmt, 2, article_id);

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
    int init_rc = article_init(article);
    if (init_rc != 0) {
      fprintf(stderr, "The article is NULL\n");
      return HTTP_INTERNAL_ERROR;
    }

    int map_rc = article_map(article, stmt, 0, 6);
    if (map_rc != 0) {
      free(article);

      query_rc = sqlite3_step(stmt);
      continue;
    }

    printf("\n");
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int add_article(struct article *article) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ARTICLE SQL ==="));

  int query_rc = SQLITE_ROW;

  // Compute next position scoped to the section
  sqlite3_stmt *pos_stmt;
  query_rc =
      sqlite3_prepare_v2(db, QUERY_NEXT_POSITION_TMP ";", -1, &pos_stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(pos_stmt);
    return query_rc;
  }
  sqlite3_bind_int(pos_stmt, 1, article->section_id);
  query_rc = sqlite3_step(pos_stmt);
  int next_position = 0;
  if (query_rc == SQLITE_ROW) {
    next_position = sqlite3_column_int(pos_stmt, 0);
  }
  sqlite3_finalize(pos_stmt);

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
  sqlite3_bind_int(stmt, 1, article->section_id);
  sqlite3_bind_int(stmt, 2, next_position);
  sqlite3_bind_text(stmt, 3, article->title, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, article->source_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, article->source_url, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 6, article->summary, -1, SQLITE_STATIC);

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  article->id = (int)sqlite3_last_insert_rowid(db);
  article->position = next_position;
  sqlite3_finalize(stmt);

  return 0;
}

int edit_article(struct article *article) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT ARTICLE SQL ==="));

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
  sqlite3_bind_text(stmt, 1, article->title, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, article->source_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, article->source_url, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, article->summary, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 5, article->id);
  sqlite3_bind_int(stmt, 6, article->section_id);

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

int reorder_articles(int section_id, int *ids, size_t count) {
  printf(TERMINAL_SQL_MESSAGE("=== REORDER ARTICLES SQL ==="));

  int query_rc = sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BEGIN error: %s\n"),
            sqlite3_errmsg(db));
    return query_rc;
  }

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_REORDER_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    return query_rc;
  }

  for (size_t i = 0; i < count; i++) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_int(stmt, 1, (int)i);
    sqlite3_bind_int(stmt, 2, ids[i]);
    sqlite3_bind_int(stmt, 3, section_id);

    GET_EXPANDED_QUERY(stmt);

    query_rc = sqlite3_step(stmt);
    if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("step error: %s\n"),
              sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
      return query_rc;
    }
  }

  sqlite3_finalize(stmt);

  query_rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("COMMIT error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    return query_rc;
  }

  return 0;
}

int delete_article(int section_id, int article_id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ARTICLE SQL ==="));

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

  sqlite3_bind_int(stmt, 1, article_id);
  sqlite3_bind_int(stmt, 2, section_id);

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
