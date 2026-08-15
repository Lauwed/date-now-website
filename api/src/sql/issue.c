/**
 * @file issue.c
 * @brief Postgres data-access implementation for the Issue table.
 */

#include <enums.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/issue.h>
#include <sql/issue_author.h>
#include <sql/issue_sponsor.h>
#include <sql/issue_tag.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <time.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM Issue i"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = $1"
#define QUERY_EXISTS_SLUG_TMP QUERY_COUNT_TMP " WHERE slug = $1"
#define QUERY_IDENTITY_EXISTS_TMP                                              \
  QUERY_COUNT_TMP                                                              \
  " WHERE (title = $1 OR slug = $2 OR issueNumber = $3) AND id <> $4"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "i.id, i.slug, i.title, i.subtitle, EXTRACT(EPOCH FROM i.createdAt)::BIGINT, "\
  "EXTRACT(EPOCH FROM i.publishedAt)::BIGINT, "                                \
  "EXTRACT(EPOCH FROM i.updatedAt)::BIGINT, i.issueNumber, i.excerpt, "        \
  "i.isSponsored, "                                                            \
  "i.status, i.openedMailCount, "                                              \
  "COUNT(v.id), "                                                              \
  "m.id, m.textAlternatif, m.url, m.width, m.height "                         \
  "FROM Issue i "                                                              \
  "LEFT JOIN Media m ON m.id = i.cover "                                       \
  "LEFT JOIN View v ON v.issueId = i.id "                                      \
  "LEFT JOIN IssueAuthor a ON a.issueId = i.id "                               \
  "LEFT JOIN IssueTag t ON t.issueId = i.id "                                  \
  "LEFT JOIN IssueSponsor s ON s.issueId = i.id "
/* List query: no relation JOINs to avoid Cartesian-product row duplication. */
#define QUERY_SELECT_NOREL_TMP                                                 \
  "SELECT "                                                                    \
  "i.id, i.slug, i.title, i.subtitle, EXTRACT(EPOCH FROM i.createdAt)::BIGINT, "\
  "EXTRACT(EPOCH FROM i.publishedAt)::BIGINT, "                                \
  "EXTRACT(EPOCH FROM i.updatedAt)::BIGINT, i.issueNumber, i.excerpt, "        \
  "i.isSponsored, "                                                            \
  "i.status, i.openedMailCount, "                                              \
  "COUNT(v.id), "                                                              \
  "m.id, m.textAlternatif, m.url, m.width, m.height "                         \
  "FROM Issue i "                                                              \
  "LEFT JOIN Media m ON m.id = i.cover "                                       \
  "LEFT JOIN View v ON v.issueId = i.id "
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE i.id = $1"
#define QUERY_SELECT_SLUG_TMP QUERY_SELECT_TMP " WHERE i.slug = $1"
#define QUERY_Q_TMP                                                            \
  " WHERE i.title LIKE $%1$d OR CAST(i.issueNumber AS TEXT) LIKE $%1$d"
#define QUERY_SORT_TMP " ORDER BY LOWER(i.title) %s"
#define QUERY_SORT_DEFAULT_TMP " ORDER BY i.issueNumber DESC"
#define QUERY_STATUS_AND_TMP " AND i.status = $%d"
#define QUERY_STATUS_WHERE_TMP " WHERE i.status = $%d"
#define QUERY_PAGINATION_TMP " LIMIT $%d OFFSET $%d"
/* Postgres requires strict GROUP BY (unlike SQLite's lenient extension) —
 * grouping by each table's primary key makes all of its other selected
 * columns functionally dependent, so no other column needs to be listed. */
#define QUERY_GROUP_BY " GROUP BY i.id, m.id"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO Issue (title, slug, subtitle, cover, publishedAt, "             \
  "issueNumber, "                                                              \
  "excerpt, isSponsored, status) "                                             \
  "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, COALESCE($9, 'DRAFT')) "            \
  "RETURNING id;"
#define QUERY_PUT_TMP                                                          \
  "UPDATE Issue "                                                              \
  "SET title = $1, slug = $2, subtitle = $3, cover = $4, "                     \
  "publishedAt = COALESCE($5, CASE "                                          \
  "WHEN status = 'PUBLISHED' THEN CURRENT_TIMESTAMP ELSE NULL END), "          \
  "issueNumber = $6, "                                                         \
  "excerpt = $7, isSponsored = $8, status = COALESCE($9, 'DRAFT'), "           \
  "updatedAt = CURRENT_TIMESTAMP "                                             \
  "WHERE id = $10;"

#define QUERY_DELETE_TMP "DELETE FROM Issue WHERE id = $1;"

/* Returns "($1,$2,...,$count)" — caller must free. */
static char *build_in_clause(size_t count) {
  size_t sz = count * 6 + 2;
  char *s = malloc(sz);
  if (!s)
    return NULL;
  s[0] = '(';
  size_t pos = 1;
  for (size_t i = 0; i < count; i++) {
    pos += snprintf(s + pos, sz - pos, "$%zu%c", i + 1,
                    (i < count - 1) ? ',' : ')');
  }
  return s;
}

/* Builds a text-value array of ids (as strings) for a batch WHERE ... IN
 * (...) query. Caller must free the returned array and each of its
 * elements. */
static char **build_id_values(size_t count, struct issue **arr) {
  char **values = malloc(count * sizeof(char *));
  for (size_t i = 0; i < count; i++) {
    values[i] = malloc(16);
    snprintf(values[i], 16, "%d", arr[i]->id);
  }
  return values;
}

static void free_id_values(char **values, size_t count) {
  for (size_t i = 0; i < count; i++)
    free(values[i]);
  free(values);
}

static void load_tags_batch(size_t count, struct issue **arr) {
  if (count == 0)
    return;

  char *in = build_in_clause(count);
  if (!in)
    return;

  const char *pfx = "SELECT it.issueId, t.name, t.color FROM IssueTag it "
                    "JOIN Tag t ON t.name = it.tagName "
                    "WHERE it.issueId IN ";
  size_t qsz = strlen(pfx) + strlen(in) + 2;
  char *query = malloc(qsz);
  if (!query) {
    free(in);
    return;
  }
  snprintf(query, qsz, "%s%s;", pfx, in);
  free(in);

  char **values = build_id_values(count, arr);
  GET_EXPANDED_QUERY(query, (int)count, (const char *const *)values);

  PGresult *res = pg_exec(query, (int)count, (const char *const *)values);
  free(query);
  free_id_values(values, count);
  if (res == NULL)
    return;

  int n_rows = PQntuples(res);
  for (int i = 0; i < n_rows; i++) {
    int issue_id = atoi(PQgetvalue(res, i, 0));
    for (size_t j = 0; j < count; j++) {
      if (arr[j]->id != issue_id)
        continue;
      struct tag *t = malloc(sizeof(struct tag));
      if (tag_init(t) != 0) {
        free(t);
        break;
      }
      pg_row_t row = {res, i};
      tag_map(t, &row, 1, 2);
      printf("tag\t%s\t%s\n", t->name, t->color);
      arr[j]->tags = realloc(arr[j]->tags,
                             (arr[j]->tags_count + 1) * sizeof(struct tag *));
      arr[j]->tags[arr[j]->tags_count++] = t;
      break;
    }
  }
  PQclear(res);
}

static void load_authors_batch(size_t count, struct issue **arr) {
  if (count == 0)
    return;
  char *in = build_in_clause(count);
  if (!in)
    return;
  const char *pfx =
      "SELECT ia.issueId, u.id, u.username, u.email, u.role, u.link, "
      "EXTRACT(EPOCH FROM u.subscribedAt)::BIGINT, u.isSupporter, "
      "EXTRACT(EPOCH FROM u.createdAt)::BIGINT, "
      "m.id, m.textAlternatif, m.url, m.width, m.height "
      "FROM IssueAuthor ia "
      "JOIN AppUser u ON u.id = ia.userId "
      "LEFT JOIN Media m ON m.id = u.picture "
      "WHERE ia.issueId IN ";
  size_t qsz = strlen(pfx) + strlen(in) + 2;
  char *query = malloc(qsz);
  if (!query) {
    free(in);
    return;
  }
  snprintf(query, qsz, "%s%s;", pfx, in);
  free(in);

  char **values = build_id_values(count, arr);
  GET_EXPANDED_QUERY(query, (int)count, (const char *const *)values);

  PGresult *res = pg_exec(query, (int)count, (const char *const *)values);
  free(query);
  free_id_values(values, count);
  if (res == NULL)
    return;

  int n_rows = PQntuples(res);
  for (int i = 0; i < n_rows; i++) {
    int issue_id = atoi(PQgetvalue(res, i, 0));
    for (size_t j = 0; j < count; j++) {
      if (arr[j]->id != issue_id)
        continue;
      struct user *u = malloc(sizeof(struct user));
      if (user_init(u) != 0) {
        free(u);
        break;
      }
      pg_row_t row = {res, i};
      if (user_map(u, &row, 1, 8) != 0) {
        free(u);
        break;
      }
      struct media *m = malloc(sizeof(struct media));
      if (media_map(m, &row, 9, 13) != 0) {
        free(m);
      } else {
        u->picture = m;
      }
      arr[j]->authors = realloc(arr[j]->authors, (arr[j]->authors_count + 1) *
                                                     sizeof(struct user *));
      arr[j]->authors[arr[j]->authors_count++] = u;
      break;
    }
  }
  PQclear(res);
}

static void load_sponsors_batch(size_t count, struct issue **arr) {
  if (count == 0)
    return;

  char *in = build_in_clause(count);
  if (!in)
    return;

  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SPONSORS SQL ==="));

  const char *pfx = "SELECT i.issueId, s.name, i.link AS issueLink, s.link "
                    "FROM IssueSponsor i "
                    "JOIN Sponsor s ON s.name = i.sponsorName "
                    "WHERE issueId IN ";
  size_t qsz = strlen(pfx) + strlen(in) + 2;
  char *query = malloc(qsz);
  if (!query) {
    free(in);
    return;
  }
  snprintf(query, qsz, "%s%s;", pfx, in);
  free(in);

  char **values = build_id_values(count, arr);
  GET_EXPANDED_QUERY(query, (int)count, (const char *const *)values);

  PGresult *res = pg_exec(query, (int)count, (const char *const *)values);
  free(query);
  free_id_values(values, count);
  if (res == NULL)
    return;

  int n_rows = PQntuples(res);
  for (int i = 0; i < n_rows; i++) {
    int issue_id = atoi(PQgetvalue(res, i, 0));
    for (size_t j = 0; j < count; j++) {
      if (arr[j]->id != issue_id)
        continue;
      struct issue_sponsor *s = malloc(sizeof(struct issue_sponsor));
      if (issue_sponsor_init(s) != 0) {
        free(s);
        break;
      }
      pg_row_t row = {res, i};
      issue_sponsor_map(s, &row, 0, 3);
      arr[j]->sponsors =
          realloc(arr[j]->sponsors, (arr[j]->sponsors_count + 1) *
                                        sizeof(struct issue_sponsor *));
      arr[j]->sponsors[arr[j]->sponsors_count++] = s;
      break;
    }
  }
  PQclear(res);
}

/* Builds a text-value array of ids for the IssueSection variant of the
 * batch id-list helper (same shape, different array element type). */
static char **build_section_id_values(size_t count, struct issue_section **arr) {
  char **values = malloc(count * sizeof(char *));
  for (size_t i = 0; i < count; i++) {
    values[i] = malloc(16);
    snprintf(values[i], 16, "%d", arr[i]->id);
  }
  return values;
}

static void load_articles_batch(size_t count, struct issue_section **arr) {
  if (count == 0)
    return;

  char *in = build_in_clause(count);
  if (!in)
    return;

  const char *pfx =
      "SELECT id, sectionId, position, title, sourceName, sourceUrl, "
      "summary FROM Article WHERE sectionId IN ";
  size_t qsz = strlen(pfx) + strlen(in) + strlen(" ORDER BY position ASC;") + 1;
  char *query = malloc(qsz);
  if (!query) {
    free(in);
    return;
  }
  snprintf(query, qsz, "%s%s ORDER BY position ASC;", pfx, in);
  free(in);

  char **values = build_section_id_values(count, arr);
  GET_EXPANDED_QUERY(query, (int)count, (const char *const *)values);

  PGresult *res = pg_exec(query, (int)count, (const char *const *)values);
  free(query);
  free_id_values(values, count);
  if (res == NULL)
    return;

  int n_rows = PQntuples(res);
  for (int i = 0; i < n_rows; i++) {
    int section_id = atoi(PQgetvalue(res, i, 1));
    for (size_t j = 0; j < count; j++) {
      if (arr[j]->id != section_id)
        continue;
      struct article *a = malloc(sizeof(struct article));
      if (article_init(a) != 0) {
        free(a);
        break;
      }
      pg_row_t row = {res, i};
      if (article_map(a, &row, 0, 6) != 0) {
        free(a);
        break;
      }
      arr[j]->articles = realloc(
          arr[j]->articles, (arr[j]->articles_count + 1) * sizeof(struct article *));
      arr[j]->articles[arr[j]->articles_count++] = a;
      break;
    }
  }
  PQclear(res);
}

static void load_sections_batch(size_t count, struct issue **arr) {
  if (count == 0)
    return;

  char *in = build_in_clause(count);
  if (!in)
    return;

  const char *pfx =
      "SELECT id, issueId, position, type, categoryName, textBody "
      "FROM IssueSection WHERE issueId IN ";
  size_t qsz = strlen(pfx) + strlen(in) + strlen(" ORDER BY position ASC;") + 1;
  char *query = malloc(qsz);
  if (!query) {
    free(in);
    return;
  }
  snprintf(query, qsz, "%s%s ORDER BY position ASC;", pfx, in);
  free(in);

  char **values = build_id_values(count, arr);
  GET_EXPANDED_QUERY(query, (int)count, (const char *const *)values);

  PGresult *res = pg_exec(query, (int)count, (const char *const *)values);
  free(query);
  free_id_values(values, count);
  if (res == NULL)
    return;

  struct issue_section **all_sections = NULL;
  size_t all_sections_count = 0;

  int n_rows = PQntuples(res);
  for (int i = 0; i < n_rows; i++) {
    int issue_id = atoi(PQgetvalue(res, i, 1));
    for (size_t j = 0; j < count; j++) {
      if (arr[j]->id != issue_id)
        continue;
      struct issue_section *s = malloc(sizeof(struct issue_section));
      if (issue_section_init(s) != 0) {
        free(s);
        break;
      }
      pg_row_t row = {res, i};
      if (issue_section_map(s, &row, 0, 5) != 0) {
        free(s);
        break;
      }
      arr[j]->sections = realloc(arr[j]->sections,
                                 (arr[j]->sections_count + 1) *
                                     sizeof(struct issue_section *));
      arr[j]->sections[arr[j]->sections_count++] = s;

      all_sections = realloc(
          all_sections, (all_sections_count + 1) * sizeof(struct issue_section *));
      all_sections[all_sections_count++] = s;
      break;
    }
  }
  PQclear(res);

  if (all_sections_count > 0) {
    load_articles_batch(all_sections_count, all_sections);
  }
  free(all_sections);
}

int issue_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE EXISTS SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", issues_count);

  PQclear(res);

  return issues_count > 0;
}

int issue_slug_exists(char *slug) {
  printf(TERMINAL_SQL_MESSAGE("=== ISSUE SLUG EXISTS SQL ==="));

  const char *values[1] = {slug};
  GET_EXPANDED_QUERY(QUERY_EXISTS_SLUG_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_SLUG_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", issues_count);

  PQclear(res);

  return issues_count > 0;
}

int issue_identity_exists(char *title, int issue_number, char *slug, int id) {
  if (title == NULL && issue_number <= 0 && slug == NULL) {
    return -1;
  }

  printf(TERMINAL_SQL_MESSAGE("=== ISSUE IDENTITY EXISTS SQL ==="));

  char issue_number_str[16], id_str[16];
  snprintf(issue_number_str, sizeof(issue_number_str), "%d", issue_number);
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[4] = {title, slug, issue_number_str, id_str};
  GET_EXPANDED_QUERY(QUERY_IDENTITY_EXISTS_TMP, 4, values);

  PGresult *res = pg_exec(QUERY_IDENTITY_EXISTS_TMP, 4, values);
  if (res == NULL) {
    return -1;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return issues_count > 0;
}

int get_issues_count(const char *status) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES COUNT (filtered) SQL ==="));

  const char *values[1];
  int n_values = 0;

  char query[256] = QUERY_COUNT_TMP;
  if (status != NULL) {
    char clause[32];
    snprintf(clause, sizeof(clause), QUERY_STATUS_WHERE_TMP, 1);
    strcat(query, clause);
    values[n_values++] = status;
  }
  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  return issues_count;
}

int get_issues_len(const struct mg_str *q, const char *status) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES COUNT SQL ==="));

  char *q_str = NULL;
  const char *values[2];
  int n_values = 0;

  char query[512] = QUERY_COUNT_TMP;
  if (q->len > 0) {
    q_str = malloc(q->len + 3);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, n_values + 1);
    strcat(query, clause);
    values[n_values++] = q_str;

    if (status != NULL) {
      char status_clause[32];
      snprintf(status_clause, sizeof(status_clause), QUERY_STATUS_AND_TMP,
               n_values + 1);
      strcat(query, status_clause);
      values[n_values++] = status;
    }
  } else if (status != NULL) {
    char clause[32];
    snprintf(clause, sizeof(clause), QUERY_STATUS_WHERE_TMP, n_values + 1);
    strcat(query, clause);
    values[n_values++] = status;
  }
  strcat(query, ";");

  GET_EXPANDED_QUERY(query, n_values, values);

  PGresult *res = pg_exec(query, n_values, values);
  free(q_str);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int issues_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return issues_count;
}

int get_issues(size_t len, struct issue **arr, const struct mg_str *q,
               const char *status, const struct mg_str *sort, int page,
               int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUES SQL ==="));

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
  char page_size_str[16], offset_str[16];
  const char *values[4];
  int n_values = 0;

  char query[1536] = QUERY_SELECT_NOREL_TMP;

  if (q->len > 0) {
    q_str = malloc(q->len + 3);
    sprintf(q_str, "%%%.*s%%", (int)q->len, q->buf);

    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_Q_TMP, n_values + 1);
    strcat(query, clause);
    values[n_values++] = q_str;

    if (status != NULL) {
      char status_clause[32];
      snprintf(status_clause, sizeof(status_clause), QUERY_STATUS_AND_TMP,
               n_values + 1);
      strcat(query, status_clause);
      values[n_values++] = status;
    }
  } else if (status != NULL) {
    char clause[32];
    snprintf(clause, sizeof(clause), QUERY_STATUS_WHERE_TMP, n_values + 1);
    strcat(query, clause);
    values[n_values++] = status;
  }

  strcat(query, QUERY_GROUP_BY);

  if (sort->len > 0) {
    char clause[128];
    snprintf(clause, sizeof(clause), QUERY_SORT_TMP, sort_keyword);
    strcat(query, clause);
  } else {
    strcat(query, QUERY_SORT_DEFAULT_TMP);
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
    struct issue *u = malloc(sizeof(struct issue));

    int issue_init_rc = issue_init(u);
    if (issue_init_rc != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("The issue is NULL"));
      free(u);
      PQclear(res);
      return HTTP_INTERNAL_ERROR;
    }

    struct media *m = malloc(sizeof(struct media));

    pg_row_t row = {res, i};
    int issue_rc = issue_map(u, &row, 0, 12);
    if (issue_rc != 0) {
      free(m);
      free(u);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("Error mapping row: %d"), i);
      continue;
    }

    // Picture
    int cover_rc = media_map(m, &row, 13, 17);
    if (cover_rc != 0) {
      free(m);
    } else {
      u->cover = m;
    }

    arr[count] = u;
    count += 1;
  }

  PQclear(res);

  if (count > 0) {
    load_tags_batch(count, arr);
    load_authors_batch(count, arr);
    load_sponsors_batch(count, arr);
    load_sections_batch(count, arr);
  }

  return 0;
}

int get_issue(struct issue *issue, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE SQL ==="));

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

  int issue_init_rc = issue_init(issue);
  if (issue_init_rc != 0) {
    fprintf(stderr, "The issue is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  struct media *m = malloc(sizeof(struct media));
  pg_row_t row = {res, 0};
  int issue_rc = issue_map(issue, &row, 0, 12);
  if (issue_rc != 0) {
    free(m);
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  int cover_rc = media_map(m, &row, 13, 17);
  if (cover_rc != 0) {
    free(m);
  } else {
    issue->cover = m;
  }

  PQclear(res);

  struct issue *single[1] = {issue};
  load_tags_batch(1, single);
  load_authors_batch(1, single);
  load_sponsors_batch(1, single);
  load_sections_batch(1, single);

  return 0;
}

int get_issue_by_slug(struct issue *issue, char *slug) {
  if (slug == NULL || strlen(slug) == 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET ISSUE BY SLUG SQL ==="));

  const char *values[1] = {slug};
  GET_EXPANDED_QUERY(QUERY_SELECT_SLUG_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SLUG_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int issue_init_rc = issue_init(issue);
  if (issue_init_rc != 0) {
    fprintf(stderr, "The issue is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  struct media *m = malloc(sizeof(struct media));
  pg_row_t row = {res, 0};
  int issue_rc = issue_map(issue, &row, 0, 12);
  if (issue_rc != 0) {
    free(m);
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  int cover_rc = media_map(m, &row, 13, 17);
  if (cover_rc != 0) {
    free(m);
  } else {
    issue->cover = m;
  }

  PQclear(res);

  struct issue *single[1] = {issue};
  load_tags_batch(1, single);
  load_authors_batch(1, single);
  load_sponsors_batch(1, single);
  load_sections_batch(1, single);

  return 0;
}

int add_issue(struct issue *issue) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD ISSUE SQL ==="));

  if (issue->status != NULL && strcmp(issue->status, "PUBLISHED") == 0) {
    issue->published_at = time(NULL);
  }

  char cover_str[16], published_at_str[16], issue_number_str[16],
      is_sponsored_str[8];
  const char *cover_val = NULL, *published_at_val = NULL;
  if (issue->cover != NULL && issue->cover->id > 0) {
    snprintf(cover_str, sizeof(cover_str), "%d", issue->cover->id);
    cover_val = cover_str;
  }
  if (issue->published_at > 0) {
    snprintf(published_at_str, sizeof(published_at_str), "%d",
             issue->published_at);
    published_at_val = published_at_str;
  }
  snprintf(issue_number_str, sizeof(issue_number_str), "%d",
           issue->issue_number);
  snprintf(is_sponsored_str, sizeof(is_sponsored_str), "%d",
           issue->is_sponsored);

  const char *values[9] = {
      issue->title,     issue->slug,      issue->subtitle,
      cover_val,        published_at_val, issue_number_str,
      issue->excerpt,   is_sponsored_str, issue->status};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 9, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 9, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  issue->id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return 0;
}

int edit_issue(struct issue *issue) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT ISSUE SQL ==="));

  if (issue->status != NULL && strcmp(issue->status, "PUBLISHED") == 0) {
    issue->published_at = time(NULL);
  }

  char cover_str[16], published_at_str[16], issue_number_str[16],
      is_sponsored_str[8], id_str[16];
  const char *cover_val = NULL, *published_at_val = NULL;
  if (issue->cover != NULL && issue->cover->id > 0) {
    snprintf(cover_str, sizeof(cover_str), "%d", issue->cover->id);
    cover_val = cover_str;
  }
  if (issue->published_at > 0) {
    snprintf(published_at_str, sizeof(published_at_str), "%d",
             issue->published_at);
    published_at_val = published_at_str;
  }
  snprintf(issue_number_str, sizeof(issue_number_str), "%d",
           issue->issue_number);
  snprintf(is_sponsored_str, sizeof(is_sponsored_str), "%d",
           issue->is_sponsored);
  snprintf(id_str, sizeof(id_str), "%d", issue->id);

  const char *values[10] = {
      issue->title,     issue->slug,      issue->subtitle,
      cover_val,        published_at_val, issue_number_str,
      issue->excerpt,   is_sponsored_str, issue->status,
      id_str};
  GET_EXPANDED_QUERY(QUERY_PUT_TMP, 10, values);

  PGresult *res = pg_exec(QUERY_PUT_TMP, 10, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int publish_issue(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== PUBLISH ISSUE SQL ==="));

  const char *query =
      "UPDATE Issue SET status = 'PUBLISHED', "
      "publishedAt = CURRENT_TIMESTAMP, updatedAt = CURRENT_TIMESTAMP "
      "WHERE id = $1;";

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(query, 1, values);

  PGresult *res = pg_exec(query, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_issue(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE ISSUE SQL ==="));

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
