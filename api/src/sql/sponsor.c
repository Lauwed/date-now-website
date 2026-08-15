/**
 * @file sponsor.c
 * @brief Postgres data-access implementation for the Sponsor table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/sponsor.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Sponsor"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE name = $1"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "name, link "                                                                \
  "FROM Sponsor"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE name = $1"
#define QUERY_Q_TMP " WHERE name LIKE $%1$d OR link LIKE $%1$d"
#define QUERY_SORT_TMP " ORDER BY LOWER(name) %s"
#define QUERY_PAGINATION_TMP " LIMIT $%d OFFSET $%d"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO Sponsor (name, link) "                                          \
  "VALUES ($1, $2);"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Sponsor "                                                            \
  "SET name = $1, link = $2 "                                                  \
  "WHERE name = $3;"
#define QUERY_DELETE_TMP "DELETE FROM Sponsor WHERE name = $1;"

int sponsor_exists(char *name) {
  printf(TERMINAL_SQL_MESSAGE("=== SPONSOR EXISTS SQL ==="));

  const char *values[1] = {name};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int sponsors_count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", sponsors_count);

  PQclear(res);

  return sponsors_count > 0;
}

int get_sponsors_len(const struct mg_str *q) {
  printf(TERMINAL_SQL_MESSAGE("=== GET SPONSORS COUNT SQL ==="));

  char *q_str = NULL;
  const char *values[1];
  int n_values = 0;

  char query[512] = QUERY_COUNT_TMP;
  if (q->len > 0) {
    q_str = malloc(q->len + 3);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, 1);
    strcat(query, clause);

    values[n_values++] = q_str;
  }
  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  free(q_str);
  if (res == NULL) {
    return -1;
  }

  int sponsors_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return sponsors_count;
}

int get_sponsors(size_t len, struct sponsor **arr, const struct mg_str *q,
                 const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET SPONSORS SQL ==="));

  const char *sort_keyword = "ASC";
  if (sort->len > 0) {
    if (strncasecmp(sort->buf, "desc", sort->len) == 0) {
      sort_keyword = "DESC";
    } else if (strncasecmp(sort->buf, "asc", sort->len) == 0) {
      sort_keyword = "ASC";
    } else {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG VALUE FOR SORTING"));
      return HTTP_BAD_REQUEST;
    }
  }

  char *q_str = NULL;
  char offset_str[16], page_size_str[16];
  const char *values[3];
  int n_values = 0;

  char query[768] = QUERY_SELECT_TMP;

  if (q->len > 0) {
    q_str = malloc(q->len + 3);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, n_values + 1);
    strcat(query, clause);

    values[n_values++] = q_str;
  }

  if (sort->len > 0) {
    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_SORT_TMP, sort_keyword);
    strcat(query, clause);
  }

  if (page > 0) {
    int offset = (page - 1) * page_size;
    snprintf(page_size_str, sizeof(page_size_str), "%d", page_size);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);

    char clause[64];
    snprintf(clause, sizeof(clause), QUERY_PAGINATION_TMP, n_values + 1,
             n_values + 2);
    strcat(query, clause);

    values[n_values++] = page_size_str;
    values[n_values++] = offset_str;
  }

  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  free(q_str);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct sponsor *u = malloc(sizeof(struct sponsor));

    int sponsor_init_rc = sponsor_init(u);
    if (sponsor_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The sponsor is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int sponsor_rc = sponsor_map(u, &row, 0, 1);
    if (sponsor_rc != 0) {
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

int get_sponsor(struct sponsor *sponsor, char *name) {
  if (name == NULL) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET SPONSOR SQL ==="));

  const char *values[1] = {name};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int sponsor_init_rc = sponsor_init(sponsor);
  if (sponsor_init_rc != 0) {
    fprintf(stderr, "The sponsor is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  pg_row_t row = {res, 0};
  int sponsor_rc = sponsor_map(sponsor, &row, 0, 1);
  PQclear(res);
  if (sponsor_rc != 0) {
    return HTTP_INTERNAL_ERROR;
  }

  return 0;
}

int add_sponsor(struct sponsor *sponsor) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD SPONSOR SQL ==="));

  const char *values[2] = {sponsor->name, sponsor->link};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int edit_sponsor(struct sponsor *sponsor, char *name) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT SPONSOR SQL ==="));

  const char *values[3] = {sponsor->name, sponsor->link, name};
  GET_EXPANDED_QUERY(QUERY_PUT_TMP, 3, values);

  PGresult *res = pg_exec(QUERY_PUT_TMP, 3, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_sponsor(char *name) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE SPONSOR SQL ==="));

  const char *values[1] = {name};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}
