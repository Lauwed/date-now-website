/**
 * @file media.c
 * @brief SQLite data-access implementation for the Media table.
 */

#include <enums.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/media.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Media"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = ?"
#define QUERY_SELECT_TMP \
  "SELECT id, textAlternatif, url, width, height FROM Media"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE id = ?"
#define QUERY_POST_TMP \
  "INSERT INTO Media (textAlternatif, url, width, height) VALUES (?, ?, ?, ?);"
#define QUERY_UPDATE_FILE_TMP \
  "UPDATE Media SET url = ?, width = ?, height = ? WHERE id = ?;"
#define QUERY_UPDATE_ALT_TMP \
  "UPDATE Media SET textAlternatif = ? WHERE id = ?;"
#define QUERY_REFERENCED_TMP \
  "SELECT (SELECT COUNT(*) FROM Issue WHERE cover = ?) + " \
  "(SELECT COUNT(*) FROM User WHERE picture = ?);"
#define QUERY_DELETE_TMP "DELETE FROM Media WHERE id = ?;"

int media_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== MEDIA EXISTS SQL ==="));

  int query_rc;
  int count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_EXISTS_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_bind_int(stmt, 1, id);
  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return 0;
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

int get_medias_len(void) {
  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIAS COUNT SQL ==="));

  int query_rc;
  int count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_COUNT_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
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

int get_medias(size_t len, struct media **arr) {
  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIAS SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_SELECT_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  size_t count = 0;
  while (query_rc == SQLITE_ROW && count < len) {
    struct media *m = malloc(sizeof(struct media));
    m->id = 0;
    m->alternative_text = NULL;
    m->url = NULL;
    m->width = 0.0;
    m->height = 0.0;

    int rc = media_map(m, stmt, 0, 4);
    if (rc != 0) {
      free(m);
      count += 1;
      query_rc = sqlite3_step(stmt);
      continue;
    }

    arr[count] = m;
    count += 1;
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  return 0;
}

int get_media(struct media *media, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc =
      sqlite3_prepare_v2(db, QUERY_SELECT_SINGLE_TMP ";", -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
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
  } else if (query_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return HTTP_NOT_FOUND;
  }

  while (query_rc == SQLITE_ROW) {
    int rc = media_map(media, stmt, 0, 4);
    if (rc != 0) {
      sqlite3_finalize(stmt);
      return HTTP_INTERNAL_ERROR;
    }
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);
  return 0;
}

int add_media(struct media *media) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_POST_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, media->alternative_text, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, media->url ? media->url : "", -1, SQLITE_STATIC);
  sqlite3_bind_double(stmt, 3, media->width);
  sqlite3_bind_double(stmt, 4, media->height);

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  media->id = (unsigned)sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 0;
}

int update_media_file(int id, const char *url, double width, double height) {
  printf(TERMINAL_SQL_MESSAGE("=== UPDATE MEDIA FILE SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_UPDATE_FILE_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, url, -1, SQLITE_STATIC);
  sqlite3_bind_double(stmt, 2, width);
  sqlite3_bind_double(stmt, 3, height);
  sqlite3_bind_int(stmt, 4, id);

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int update_media_alt_text(int id, const char *alt_text) {
  printf(TERMINAL_SQL_MESSAGE("=== UPDATE MEDIA ALT TEXT SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_UPDATE_ALT_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_bind_text(stmt, 1, alt_text, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, id);

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return query_rc;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int media_is_referenced(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== MEDIA IS REFERENCED SQL ==="));

  int query_rc;
  int count = 0;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_REFERENCED_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_bind_int(stmt, 1, id);
  sqlite3_bind_int(stmt, 2, id);

  GET_EXPANDED_QUERY(stmt);
  query_rc = sqlite3_step(stmt);

  if (query_rc == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return count > 0;
}

int delete_media(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE MEDIA SQL ==="));

  int query_rc;

  sqlite3_stmt *stmt;
  query_rc = sqlite3_prepare_v2(db, QUERY_DELETE_TMP, -1, &stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\\n"),
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
