/**
 * @file media.c
 * @brief Postgres data-access implementation for the Media table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/media.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Media"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = $1"
#define QUERY_SELECT_TMP \
  "SELECT id, textAlternatif, url, width, height FROM Media"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE id = $1"
#define QUERY_POST_TMP \
  "INSERT INTO Media (textAlternatif, url, width, height) VALUES ($1, $2, $3, $4) RETURNING id;"
#define QUERY_UPDATE_FILE_TMP \
  "UPDATE Media SET url = $1, width = $2, height = $3 WHERE id = $4;"
#define QUERY_UPDATE_ALT_TMP \
  "UPDATE Media SET textAlternatif = $1 WHERE id = $2;"
#define QUERY_REFERENCED_TMP \
  "SELECT (SELECT COUNT(*) FROM Issue WHERE cover = $1) + " \
  "(SELECT COUNT(*) FROM AppUser WHERE picture = $1);"
#define QUERY_DELETE_TMP "DELETE FROM Media WHERE id = $1;"

int media_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== MEDIA EXISTS SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 1, values);
  if (res == NULL) {
    return 0;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", count);

  PQclear(res);
  return count > 0;
}

int get_medias_len(void) {
  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIAS COUNT SQL ==="));

  GET_EXPANDED_QUERY(QUERY_COUNT_TMP, 0, NULL);

  PGresult *res = pg_exec(QUERY_COUNT_TMP, 0, NULL);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  return count;
}

int get_medias(size_t len, struct media **arr) {
  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIAS SQL ==="));

  GET_EXPANDED_QUERY(QUERY_SELECT_TMP, 0, NULL);

  PGresult *res = pg_exec(QUERY_SELECT_TMP, 0, NULL);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct media *m = malloc(sizeof(struct media));
    m->id = 0;
    m->alternative_text = NULL;
    m->url = NULL;
    m->width = 0.0;
    m->height = 0.0;

    pg_row_t row = {res, i};
    int rc = media_map(m, &row, 0, 4);
    if (rc != 0) {
      free(m);
      continue;
    }

    arr[count] = m;
    count += 1;
  }

  PQclear(res);
  return 0;
}

int get_media(struct media *media, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET MEDIA SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  pg_row_t row = {res, 0};
  int rc = media_map(media, &row, 0, 4);
  PQclear(res);
  if (rc != 0) {
    return HTTP_INTERNAL_ERROR;
  }

  return 0;
}

int add_media(struct media *media) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD MEDIA SQL ==="));

  char width_str[32], height_str[32];
  snprintf(width_str, sizeof(width_str), "%f", media->width);
  snprintf(height_str, sizeof(height_str), "%f", media->height);
  const char *values[4] = {media->alternative_text,
                           media->url ? media->url : "", width_str,
                           height_str};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 4, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 4, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  media->id = (unsigned)atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  return 0;
}

int update_media_file(int id, const char *url, double width, double height) {
  printf(TERMINAL_SQL_MESSAGE("=== UPDATE MEDIA FILE SQL ==="));

  char width_str[32], height_str[32], id_str[16];
  snprintf(width_str, sizeof(width_str), "%f", width);
  snprintf(height_str, sizeof(height_str), "%f", height);
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[4] = {url, width_str, height_str, id_str};
  GET_EXPANDED_QUERY(QUERY_UPDATE_FILE_TMP, 4, values);

  PGresult *res = pg_exec(QUERY_UPDATE_FILE_TMP, 4, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);
  return 0;
}

int update_media_alt_text(int id, const char *alt_text) {
  printf(TERMINAL_SQL_MESSAGE("=== UPDATE MEDIA ALT TEXT SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[2] = {alt_text, id_str};
  GET_EXPANDED_QUERY(QUERY_UPDATE_ALT_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_UPDATE_ALT_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);
  return 0;
}

int media_is_referenced(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== MEDIA IS REFERENCED SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_REFERENCED_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_REFERENCED_TMP, 1, values);
  if (res == NULL) {
    return 0;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  return count > 0;
}

int delete_media(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE MEDIA SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);
  return 0;
}
