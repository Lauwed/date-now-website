#pragma once

/**
 * @file sql/issue_sponsor.h
 * @brief SQLite data-access functions for the IssueSponsor join table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether the sponsor association (issue_id, sponsor_name) exists.
 * @param issue_id Issue identifier.
 * @param id       Sponsor name.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 * @note @p id is not freed by this function.
 */
int issue_sponsor_exists(int issue_id, char *id);

/**
 * @brief Returns the count of sponsors associated with an issue.
 * @param q        Unused search query parameter (reserved for future use).
 * @param issue_id Issue identifier.
 * @return Count of associated sponsors, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_issue_sponsors_len(const struct mg_str *q, int issue_id);

/**
 * @brief Fetches a page of issue_sponsor records for a given issue.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct issue_sponsor *` pointers.
 * @param issue_id  Issue identifier.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_issue_sponsor().
 */
int get_issue_sponsors(size_t len, struct issue_sponsor **arr, int issue_id,
                       int page, int page_size);

/**
 * @brief Inserts a new issue_sponsor association record.
 * @param issue_sponsor Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p issue_sponsor is not freed by this function.
 */
int add_issue_sponsor(struct issue_sponsor *issue_sponsor);

/**
 * @brief Deletes an issue_sponsor association by issue id and sponsor name.
 * @param issue_id Issue identifier.
 * @param id       Sponsor name.
 * @return 0 on success, http_res_code on error.
 * @note @p id is not freed by this function.
 */
int delete_issue_sponsor(int issue_id, char *id);
