#pragma once

/**
 * @file sql/issue_tag.h
 * @brief SQLite data-access functions for the IssueTag join table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether the tag association (issue_id, tag_name) exists.
 * @param issue_id Issue identifier.
 * @param id       Tag name.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 * @note @p id is not freed by this function.
 */
int issue_tag_exists(int issue_id, char *id);

/**
 * @brief Returns the count of tags associated with an issue.
 * @param q        Unused search query parameter (reserved for future use).
 * @param issue_id Issue identifier.
 * @return Count of associated tags, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_issue_tags_len(const struct mg_str *q, int issue_id);

/**
 * @brief Fetches a page of issue_tag records for a given issue.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct issue_tag *` pointers.
 * @param issue_id  Issue identifier.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_issue_tag().
 */
int get_issue_tags(size_t len, struct issue_tag **arr, int issue_id, int page,
                   int page_size);

/**
 * @brief Inserts a new issue_tag association record.
 * @param issue_tag Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p issue_tag is not freed by this function.
 */
int add_issue_tag(struct issue_tag *issue_tag);

/**
 * @brief Deletes an issue_tag association by issue id and tag name.
 * @param issue_id Issue identifier.
 * @param id       Tag name.
 * @return 0 on success, http_res_code on error.
 * @note @p id is not freed by this function.
 */
int delete_issue_tag(int issue_id, char *id);
