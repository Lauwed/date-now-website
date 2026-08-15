/**
 * @file issue_section.c
 * @brief SQLite data-access implementation for the IssueSection table.
 */

#include <enums.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/issue_section.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

extern sqlite3 *db;

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM IssueSection WHERE issueId = ?"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " AND id = ?"
#define QUERY_SELECT_TMP                                                       \
  "SELECT id, issueId, position, type, categoryName, textBody "                \
  "FROM IssueSection WHERE issueId = ? ORDER BY position ASC"
#define QUERY_SELECT_SINGLE_TMP                                                \
  "SELECT id, issueId, position, type, categoryName, textBody "                \
  "FROM IssueSection WHERE issueId = ? AND id = ?"
#define QUERY_NEXT_POSITION_TMP                                                \
  "SELECT COALESCE(MAX(position), -1) + 1 FROM IssueSection WHERE issueId = ?"
#define QUERY_POST_TMP                                                         \
  "INSERT INTO IssueSection (issueId, position, type, categoryName, "          \
  "textBody) VALUES (?, ?, ?, ?, ?);"
#define QUERY_PUT_TMP                                                          \
  "UPDATE IssueSection SET categoryName = ?, textBody = ? "                    \
  "WHERE id = ? AND issueId = ?;"
#define QUERY_REORDER_TMP                                                      \
  "UPDATE IssueSection SET position = ? WHERE id = ? AND issueId = ?;"
#define QUERY_DELETE_TMP "DELETE FROM IssueSection WHERE id = ? AND issueId = ?;"

int issue_section_exists(int issue_id, int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE SECTION EXISTS SQL ==="));

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

  sqlite3_bind_int(stmt, 1, issue_id);
  sqlite3_bind_int(stmt, 2, section_id);

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

int get_issue_sections_len(int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SECTIONS COUNT SQL ==="));

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

  sqlite3_bind_int(stmt, 1, issue_id);

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

int get_issue_sections(size_t len, struct issue_section **arr, int issue_id) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SECTIONS SQL ==="));

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

  sqlite3_bind_int(stmt, 1, issue_id);

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
    struct issue_section *u = NULL;
    u = malloc(sizeof(struct issue_section));

    int init_rc = issue_section_init(u);
    if (init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The issue section is NULL"));
      return HTTP_INTERNAL_ERROR;
    }

    int map_rc = issue_section_map(u, stmt, 0, 5);
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

int get_issue_section(struct issue_section *section, int issue_id,
                      int section_id) {
  if (section_id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SECTION SQL ==="));

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

  sqlite3_bind_int(stmt, 1, issue_id);
  sqlite3_bind_int(stmt, 2, section_id);

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
    int init_rc = issue_section_init(section);
    if (init_rc != 0) {
      fprintf(stderr, "The issue section is NULL\n");
      return HTTP_INTERNAL_ERROR;
    }

    int map_rc = issue_section_map(section, stmt, 0, 5);
    if (map_rc != 0) {
      free(section);

      query_rc = sqlite3_step(stmt);
      continue;
    }

    printf("\n");
    query_rc = sqlite3_step(stmt);
  }

  sqlite3_finalize(stmt);

  return 0;
}

int add_issue_section(struct issue_section *section) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ISSUE SECTION SQL ==="));

  int query_rc = SQLITE_ROW;

  // Compute next position scoped to the issue
  sqlite3_stmt *pos_stmt;
  query_rc =
      sqlite3_prepare_v2(db, QUERY_NEXT_POSITION_TMP ";", -1, &pos_stmt, NULL);
  if (query_rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(pos_stmt);
    return query_rc;
  }
  sqlite3_bind_int(pos_stmt, 1, section->issue_id);
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
  sqlite3_bind_int(stmt, 1, section->issue_id);
  sqlite3_bind_int(stmt, 2, next_position);
  sqlite3_bind_text(stmt, 3, section->type, -1, SQLITE_STATIC);
  if (section->category_name != NULL) {
    sqlite3_bind_text(stmt, 4, section->category_name, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 4);
  }
  if (section->text_body != NULL) {
    sqlite3_bind_text(stmt, 5, section->text_body, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 5);
  }

  GET_EXPANDED_QUERY(stmt);

  query_rc = sqlite3_step(stmt);

  if (query_rc != SQLITE_ROW && query_rc != SQLITE_DONE) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return query_rc;
  }

  section->id = (int)sqlite3_last_insert_rowid(db);
  section->position = next_position;
  sqlite3_finalize(stmt);

  return 0;
}

int edit_issue_section(struct issue_section *section) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT ISSUE SECTION SQL ==="));

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
  if (section->category_name != NULL) {
    sqlite3_bind_text(stmt, 1, section->category_name, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 1);
  }
  if (section->text_body != NULL) {
    sqlite3_bind_text(stmt, 2, section->text_body, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 2);
  }
  sqlite3_bind_int(stmt, 3, section->id);
  sqlite3_bind_int(stmt, 4, section->issue_id);

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

int reorder_issue_sections(int issue_id, int *ids, size_t count) {
  printf(TERMINAL_SQL_MESSAGE("=== REORDER ISSUE SECTIONS SQL ==="));

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
    sqlite3_bind_int(stmt, 3, issue_id);

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

int delete_issue_section(int issue_id, int section_id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ISSUE SECTION SQL ==="));

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

  sqlite3_bind_int(stmt, 1, section_id);
  sqlite3_bind_int(stmt, 2, issue_id);

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
