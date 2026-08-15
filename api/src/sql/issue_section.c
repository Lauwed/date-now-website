/**
 * @file issue_section.c
 * @brief Postgres data-access implementation for the IssueSection table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/issue_section.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM IssueSection WHERE issueId = $1"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND id = $2"
#define QUERY_SELECT_TMP                                                       \
  "SELECT id, issueId, position, type, categoryName, textBody "                \
  "FROM IssueSection WHERE issueId = $1 ORDER BY position ASC"
#define QUERY_SELECT_SINGLE_TMP                                                \
  "SELECT id, issueId, position, type, categoryName, textBody "                \
  "FROM IssueSection WHERE issueId = $1 AND id = $2"
#define QUERY_NEXT_POSITION_TMP                                                \
  "SELECT COALESCE(MAX(position), -1) + 1 FROM IssueSection WHERE issueId = $1"
#define QUERY_POST_TMP                                                         \
  "INSERT INTO IssueSection (issueId, position, type, categoryName, "          \
  "textBody) VALUES ($1, $2, $3, $4, $5) RETURNING id;"
#define QUERY_PUT_TMP                                                          \
  "UPDATE IssueSection SET categoryName = $1, textBody = $2 "                  \
  "WHERE id = $3 AND issueId = $4;"
#define QUERY_REORDER_TMP                                                      \
  "UPDATE IssueSection SET position = $1 WHERE id = $2 AND issueId = $3;"
#define QUERY_DELETE_TMP "DELETE FROM IssueSection WHERE id = $1 AND issueId = $2;"

int issue_section_exists(int issue_id, int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE SECTION EXISTS SQL ==="));

  char issue_id_str[16], section_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  const char *values[2] = {issue_id_str, section_id_str};
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

int get_issue_sections_len(int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SECTIONS COUNT SQL ==="));

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  const char *values[1] = {issue_id_str};
  GET_EXPANDED_QUERY(QUERY_COUNT_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_COUNT_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return count;
}

int get_issue_sections(size_t len, struct issue_section **arr, int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SECTIONS SQL ==="));

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  const char *values[1] = {issue_id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  size_t count = 0;
  for (int i = 0; i < n_rows && count < len; i++) {
    struct issue_section *u = malloc(sizeof(struct issue_section));

    int init_rc = issue_section_init(u);
    if (init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The issue section is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    pg_row_t row = {res, i};
    int map_rc = issue_section_map(u, &row, 0, 5);
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

int get_issue_section(struct issue_section *section, int issue_id,
                      int section_id) {
  if (section_id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SECTION SQL ==="));

  char issue_id_str[16], section_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  const char *values[2] = {issue_id_str, section_id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int init_rc = issue_section_init(section);
  if (init_rc != 0) {
    fprintf(stderr, "The issue section is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  pg_row_t row = {res, 0};
  int map_rc = issue_section_map(section, &row, 0, 5);
  PQclear(res);
  if (map_rc != 0) {
    return HTTP_INTERNAL_ERROR;
  }

  return 0;
}

int add_issue_section(struct issue_section *section) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ISSUE SECTION SQL ==="));

  // Compute next position scoped to the issue
  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", section->issue_id);
  const char *pos_values[1] = {issue_id_str};
  GET_EXPANDED_QUERY(QUERY_NEXT_POSITION_TMP, 1, pos_values);

  PGresult *pos_res = pg_exec(QUERY_NEXT_POSITION_TMP, 1, pos_values);
  if (pos_res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }
  int next_position = atoi(PQgetvalue(pos_res, 0, 0));
  PQclear(pos_res);

  char position_str[16];
  snprintf(position_str, sizeof(position_str), "%d", next_position);
  const char *values[5] = {issue_id_str, position_str, section->type,
                           section->category_name, section->text_body};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 5, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 5, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  section->id = atoi(PQgetvalue(res, 0, 0));
  section->position = next_position;
  PQclear(res);

  return 0;
}

int edit_issue_section(struct issue_section *section) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT ISSUE SECTION SQL ==="));

  char id_str[16], issue_id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", section->id);
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", section->issue_id);
  const char *values[4] = {section->category_name, section->text_body,
                           id_str, issue_id_str};
  GET_EXPANDED_QUERY(QUERY_PUT_TMP, 4, values);

  PGresult *res = pg_exec(QUERY_PUT_TMP, 4, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int reorder_issue_sections(int issue_id, int *ids, size_t count) {
  printf(TERMINAL_SQL_MESSAGE("=== REORDER ISSUE SECTIONS SQL ==="));

  PGresult *begin_res = PQexec(db, "BEGIN;");
  if (PQresultStatus(begin_res) != PGRES_COMMAND_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BEGIN error: %s\n"),
            PQerrorMessage(db));
    PQclear(begin_res);
    return HTTP_INTERNAL_ERROR;
  }
  PQclear(begin_res);

  char issue_id_str[16];
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);

  for (size_t i = 0; i < count; i++) {
    char position_str[16], id_str[16];
    snprintf(position_str, sizeof(position_str), "%zu", i);
    snprintf(id_str, sizeof(id_str), "%d", ids[i]);
    const char *values[3] = {position_str, id_str, issue_id_str};
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

int delete_issue_section(int issue_id, int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ISSUE SECTION SQL ==="));

  char section_id_str[16], issue_id_str[16];
  snprintf(section_id_str, sizeof(section_id_str), "%d", section_id);
  snprintf(issue_id_str, sizeof(issue_id_str), "%d", issue_id);
  const char *values[2] = {section_id_str, issue_id_str};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 2, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 2, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}
