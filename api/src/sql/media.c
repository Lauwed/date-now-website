#include <enums.h>
#include <lib/sqlite3.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/media.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Media"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT id, textAlternatif, url, width, height FROM Media"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE id = ?"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO Media (textAlternatif, url, width, height) "                    \
  "VALUES (?, ?, ?, ?);"

#define QUERY_PUT_TMP                                                          \
  "UPDATE Media SET textAlternatif = ?, url = ?, width = ?, height = ? "       \
  "WHERE id = ?;"

#define QUERY_DELETE_TMP "DELETE FROM Media WHERE id = ?;"

int media_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== MEDIA EXISTS SQL ==="));

  int query_rc;
  int count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_EXISTS_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_int(stmt, 1, id);
  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);
  while (query_rc != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
      count = sqlite3_column_int(stmt, 0);
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  return count > 0;
}

int get_medias_len(void) {
  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIAS COUNT SQL ==="));

  int query_rc;
  int count = 0;

  char *query = malloc(strlen(QUERY_COUNT_TMP) + 2);
  strcpy(query, QUERY_COUNT_TMP);
  strcat(query, ";");

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    free(query);
    return query_rc;
  }

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);
  while (query_rc != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
      count = sqlite3_column_int(stmt, 0);
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  free(query);
  return count;
}

int get_medias(size_t len, struct media **arr) {
  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIAS SQL ==="));

  int query_rc;

  char *query = malloc(strlen(QUERY_SELECT_TMP) + 2);
  strcpy(query, QUERY_SELECT_TMP);
  strcat(query, ";");

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    free(query);
    return query_rc;
  }

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    free(query);
    return query_rc;
  }

  size_t count = 0;
  while (query_rc == SQLITE_ROW && count < len) {
    struct media *m = malloc(sizeof(struct media));
    media_init(m);

    int rc = media_map(m, stmt, 0, 4);
    if (rc != 0) {
      free(m);
    } else {
      arr[count] = m;
      count++;
    }

    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  free(query);
  return 0;
}

int get_media(struct media *media, int id) {
  if (id <= 0)
    return HTTP_BAD_REQUEST;

  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc =
      sqlite3_prepare_v2(db, QUERY_SELECT_SINGLE_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_int(stmt, 1, id);
  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return HTTP_NOT_FOUND;
  }
  if (query_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  media_init(media);
  media_map(media, stmt, 0, 4);

  sqlite3_finalize(stmt);
  return 0;
}

int add_media(struct media *media) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_POST_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, media->alternative_text, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, media->url, -1, SQLITE_STATIC);
  if (media->width > 0.0)
    sqlite3_bind_double(stmt, 3, media->width);
  if (media->height > 0.0)
    sqlite3_bind_double(stmt, 4, media->height);

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  media->id = (int)sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 0;
}

int edit_media(struct media *media) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_PUT_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, media->alternative_text, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, media->url, -1, SQLITE_STATIC);
  if (media->width > 0.0)
    sqlite3_bind_double(stmt, 3, media->width);
  if (media->height > 0.0)
    sqlite3_bind_double(stmt, 4, media->height);
  sqlite3_bind_int(stmt, 5, media->id);

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int delete_media(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_DELETE_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

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
