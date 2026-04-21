#pragma once

/**
 * @file sql/issue.h
 * @brief SQLite data-access functions for the Issue table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether an issue with the given id exists.
 * @param id Database identifier.
 * @return 1 if the issue exists, 0 if not, negative on SQL error.
 */
int issue_exists(int id);

/**
 * @brief Checks whether an issue with the same title, number, or slug exists.
 * @param title        Title to check.
 * @param issue_number Issue number to check.
 * @param slug         Slug to check.
 * @return 1 if a conflicting record exists, 0 if not, negative on SQL error.
 * @note None of the pointer parameters are freed by this function.
 */
int issue_identity_exists(char *title, int issue_number, char *slug);

/**
 * @brief Returns the total number of issues matching an optional query and
 *        status filter.
 * @param q      Optional full-text search query (empty/NULL = no filter).
 * @param status Optional status filter: "DRAFT", "PUBLISHED", or "ARCHIVE"
 *               (NULL = no filter).
 * @return Count of matching issues, or a negative http_res_code on error.
 * @note Neither @p q nor @p status is freed by this function.
 */
int get_issues_len(const struct mg_str *q, const char *status);

/**
 * @brief Fetches a page of issues into a pre-allocated array.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct issue *` pointers.
 * @param q         Optional full-text search query.
 * @param status    Optional status filter (NULL = all statuses).
 * @param sort      Optional sort expression.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated (including nested
 *       sub-structures) and must be freed via free_issue(). Input pointers
 *       are not freed by this function.
 */
int get_issues(size_t len, struct issue **arr, const struct mg_str *q,
               const char *status, const struct mg_str *sort, int page,
               int page_size);

/**
 * @brief Fetches a single issue by id, including its tags, authors, and sponsors.
 * @param issue Pre-allocated and initialised structure to fill.
 * @param id    Database identifier.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note Dynamic fields (including nested sub-structures) are allocated and
 *       must be freed via free_issue().
 */
int get_issue(struct issue *issue, int id);

/**
 * @brief Inserts a new issue record.
 * @param issue Hydrated structure to insert. @c issue->id is set to the
 *              last-insert rowid on success.
 * @return 0 on success, http_res_code on error.
 * @note @p issue is not freed by this function.
 */
int add_issue(struct issue *issue);

/**
 * @brief Updates an existing issue record.
 * @param issue Structure containing updated values and the target @c id.
 * @return 0 on success, http_res_code on error.
 * @note @p issue is not freed by this function.
 */
int edit_issue(struct issue *issue);

/**
 * @brief Deletes an issue by id.
 * @param id Database identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_issue(int id);

/**
 * @brief Sets the @c published_at timestamp of an issue to the current time.
 * @param id Database identifier.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 */
int publish_issue(int id);
