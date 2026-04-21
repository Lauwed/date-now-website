#pragma once

/**
 * @file sql/issue_author.h
 * @brief SQLite data-access functions for the IssueAuthor join table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether the author association (issue_id, user_id) exists.
 * @param issue_id Issue identifier.
 * @param id       User identifier.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 */
int issue_author_exists(int issue_id, int id);

/**
 * @brief Returns the count of authors associated with an issue.
 * @param q        Unused search query parameter (reserved for future use).
 * @param issue_id Issue identifier.
 * @return Count of associated authors, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_issue_authors_len(const struct mg_str *q, int issue_id);

/**
 * @brief Fetches a page of authors for a given issue.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct user *` pointers.
 * @param issue_id  Issue identifier.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_user().
 */
int get_issue_authors(size_t len, struct user **arr, int issue_id,
                      int page, int page_size);

/**
 * @brief Inserts a new author association record.
 * @param issue_author Hydrated structure containing issue_id and user_id.
 * @return 0 on success, http_res_code on error.
 * @note @p issue_author is not freed by this function.
 */
int add_issue_author(struct issue_author *issue_author);

/**
 * @brief Deletes an author association by issue and user ids.
 * @param issue_id Issue identifier.
 * @param id       User identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_issue_author(int issue_id, int id);
