#pragma once

/**
 * @file sql/issue_section.h
 * @brief SQLite data-access functions for the IssueSection table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether a section (id, issueId) exists.
 * @param issue_id   Issue identifier.
 * @param section_id Section identifier.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 */
int issue_section_exists(int issue_id, int section_id);

/**
 * @brief Returns the count of sections belonging to an issue.
 * @param issue_id Issue identifier.
 * @return Count of sections, or a negative http_res_code on error.
 */
int get_issue_sections_len(int issue_id);

/**
 * @brief Fetches all sections of an issue, ordered by position, into a
 *        pre-allocated array. Does not load their articles.
 * @param len      Size of @p arr.
 * @param arr      Pre-allocated array of `struct issue_section *` pointers.
 * @param issue_id Issue identifier.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_issue_section().
 */
int get_issue_sections(size_t len, struct issue_section **arr, int issue_id);

/**
 * @brief Fetches a single section by id. Does not load its articles.
 * @param section    Pre-allocated structure to fill.
 * @param issue_id   Issue identifier.
 * @param section_id Section identifier.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note Dynamic fields in @p section must be freed via free_issue_section().
 */
int get_issue_section(struct issue_section *section, int issue_id,
                      int section_id);

/**
 * @brief Inserts a new section, appending it at the end of the issue
 *        (position = MAX(position)+1 scoped to the issue).
 * @param section Hydrated structure to insert (issue_id must be set).
 * @return 0 on success, http_res_code on error.
 * @note @p section is not freed by this function. @c section->id and
 *       @c section->position are set to the inserted row's values on
 *       success.
 */
int add_issue_section(struct issue_section *section);

/**
 * @brief Updates the content of an existing section (categoryName or
 *        textBody). Never touches @c position.
 * @param section Structure containing the updated values.
 * @return 0 on success, http_res_code on error.
 */
int edit_issue_section(struct issue_section *section);

/**
 * @brief Rewrites the position of every section in @p ids to its index in
 *        the array, in a single transaction.
 * @param issue_id Issue identifier (defensive scoping).
 * @param ids      Ordered array of section ids.
 * @param count    Number of ids in @p ids.
 * @return 0 on success, http_res_code on error.
 */
int reorder_issue_sections(int issue_id, int *ids, size_t count);

/**
 * @brief Deletes a section by id. Cascades to its articles.
 * @param issue_id   Issue identifier.
 * @param section_id Section identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_issue_section(int issue_id, int section_id);
