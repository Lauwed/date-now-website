/**
 * @file article.c
 * @brief Postgres data-access implementation for the Article table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/article.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Article WHERE sectionId = $1"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND id = $2"
#define QUERY_SELECT_TMP                                                       \
  "SELECT id, sectionId, position, title, sourceName, sourceUrl, summary "     \
  "FROM Article WHERE sectionId = $1 ORDER BY position ASC"
#define QUERY_SELECT_SINGLE_TMP                                                \
  "SELECT id, sectionId, position, title, sourceName, sourceUrl, summary "     \
  "FROM Article WHERE sectionId = $1 AND id = $2"
#define QUERY_NEXT_POSITION_TMP                                                \
  "SELECT COALESCE(MAX(position), -1) + 1 FROM Article WHERE sectionId = $1"
#define QUERY_POST_TMP                                                         \
  "INSERT INTO Article (sectionId, position, title, sourceName, sourceUrl, "   \
  "summary) VALUES ($1, $2, $3, $4, $5, $6) RETURNING id;"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Article SET title = $1, sourceName = $2, sourceUrl = $3, "           \
  "summary = $4 WHERE id = $5 AND sectionId = $6;"
#define QUERY_REORDER_TMP                                                      \
  "UPDATE Article SET position = $1 WHERE id = $2 AND sectionId = $3;"
#define QUERY_DELETE_TMP "DELETE FROM Article WHERE id = $1 AND sectionId = $2;"

int article_exists(int section_id, int article_id) {
  printf(TERMINAL_SQL_MESSAGE("=== ARTICLE EXISTS SQL ==="));

  char section_id_str[16], article_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  snprintf(article_id_str, sizeof(article_id_str), "%d", article_id);
  const char *values[2] = {section_id_str, article_id_str};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 2, values);
  if (res == NULL) {
    return -1;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", count);

  PQclear(res);

  return count > 0;
}

int get_articles_len(int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ARTICLES COUNT SQL ==="));

  char section_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  const char *values[1] = {section_id_str};
  GET_EXPANDED_QUERY(QUERY_COUNT_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_COUNT_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return count;
}

int get_articles(size_t len, struct article **arr, int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ARTICLES SQL ==="));

  char section_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  const char *values[1] = {section_id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct article *u = malloc(sizeof(struct article));

    int init_rc = article_init(u);
    if (init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The article is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int map_rc = article_map(u, &row, 0, 6);
    if (map_rc != 0) {
      free(u);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("Error mapping row: %d"), i);
      continue;
    }

    arr[count] = u;
    count += 1;
  }

  PQclear(res);

  return 0;
}

int get_article(struct article *article, int section_id, int article_id) {
  if (article_id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ARTICLE SQL ==="));

  char section_id_str[16], article_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  snprintf(article_id_str, sizeof(article_id_str), "%d", article_id);
  const char *values[2] = {section_id_str, article_id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int init_rc = article_init(article);
  if (init_rc != 0) {
    fprintf(stderr, "The article is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  pg_row_t row = {res, 0};
  int map_rc = article_map(article, &row, 0, 6);
  PQclear(res);
  if (map_rc != 0) {
    return HTTP_INTERNAL_ERROR;
  }

  return 0;
}

int add_article(struct article *article) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ARTICLE SQL ==="));

  // Compute next position scoped to the section
  char section_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", article->section_id);
  const char *pos_values[1] = {section_id_str};
  GET_EXPANDED_QUERY(QUERY_NEXT_POSITION_TMP, 1, pos_values);

  PGresult *pos_res = pg_exec(QUERY_NEXT_POSITION_TMP, 1, pos_values);
  if (pos_res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }
  int next_position = atoi(PQgetvalue(pos_res, 0, 0));
  PQclear(pos_res);

  char position_str[16];
  snprintf(position_str, sizeof(position_str), "%d", next_position);
  const char *values[6] = {section_id_str, position_str, article->title,
                           article->source_name, article->source_url,
                           article->summary};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 6, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 6, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  article->id = atoi(PQgetvalue(res, 0, 0));
  article->position = next_position;
  PQclear(res);

  return 0;
}

int edit_article(struct article *article) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT ARTICLE SQL ==="));

  char id_str[16], section_id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", article->id);
  snprintf(section_id_str, sizeof(section_id_str), "%d", article->section_id);
  const char *values[6] = {article->title,     article->source_name,
                           article->source_url, article->summary,
                           id_str,              section_id_str};
  GET_EXPANDED_QUERY(QUERY_PUT_TMP, 6, values);

  PGresult *res = pg_exec(QUERY_PUT_TMP, 6, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int reorder_articles(int section_id, int *ids, size_t count) {
  printf(TERMINAL_SQL_MESSAGE("=== REORDER ARTICLES SQL ==="));

  PGresult *begin_res = PQexec(db, "BEGIN;");
  if (PQresultStatus(begin_res) != PGRES_COMMAND_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BEGIN error: %s\n"),
            PQerrorMessage(db));
    PQclear(begin_res);
    return HTTP_INTERNAL_ERROR;
  }
  PQclear(begin_res);

  char section_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);

  for (size_t i = 0; i < count; i++) {
    char position_str[16], id_str[16];
    snprintf(position_str, sizeof(position_str), "%zu", i);
    snprintf(id_str, sizeof(id_str), "%d", ids[i]);
    const char *values[3] = {position_str, id_str, section_id_str};
    GET_EXPANDED_QUERY(QUERY_REORDER_TMP, 3, values);

    PGresult *res = pg_exec(QUERY_REORDER_TMP, 3, values);
    if (res == NULL) {
      PQexec(db, "ROLLBACK;");
      return HTTP_INTERNAL_ERROR;
    }
    PQclear(res);
  }

  PGresult *commit_res = PQexec(db, "COMMIT;");
  if (PQresultStatus(commit_res) != PGRES_COMMAND_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("COMMIT error: %s\n"),
            PQerrorMessage(db));
    PQclear(commit_res);
    PQexec(db, "ROLLBACK;");
    return HTTP_INTERNAL_ERROR;
  }
  PQclear(commit_res);

  return 0;
}

int delete_article(int section_id, int article_id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ARTICLE SQL ==="));

  char article_id_str[16], section_id_str[16];
  snprintf(article_id_str, sizeof(article_id_str), "%d", article_id);
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  const char *values[2] = {article_id_str, section_id_str};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}
