#pragma once

/**
 * @file sql/article.h
 * @brief SQLite data-access functions for the Article table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether an article (id, sectionId) exists.
 * @param section_id Section identifier.
 * @param article_id Article identifier.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 */
int article_exists(int section_id, int article_id);

/**
 * @brief Returns the count of articles belonging to a section.
 * @param section_id Section identifier.
 * @return Count of articles, or a negative http_res_code on error.
 */
int get_articles_len(int section_id);

/**
 * @brief Fetches all articles of a section, ordered by position, into a
 *        pre-allocated array.
 * @param len        Size of @p arr.
 * @param arr        Pre-allocated array of `struct article *` pointers.
 * @param section_id Section identifier.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_article().
 */
int get_articles(size_t len, struct article **arr, int section_id);

/**
 * @brief Fetches a single article by id.
 * @param article    Pre-allocated structure to fill.
 * @param section_id Section identifier.
 * @param article_id Article identifier.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note Dynamic fields in @p article must be freed via free_article().
 */
int get_article(struct article *article, int section_id, int article_id);

/**
 * @brief Inserts a new article, appending it at the end of the section
 *        (position = MAX(position)+1 scoped to the section).
 * @param article Hydrated structure to insert (section_id must be set).
 * @return 0 on success, http_res_code on error.
 * @note @p article is not freed by this function. @c article->id and
 *       @c article->position are set to the inserted row's values on
 *       success.
 */
int add_article(struct article *article);

/**
 * @brief Updates the content of an existing article. Never touches
 *        @c position.
 * @param article Structure containing the updated values.
 * @return 0 on success, http_res_code on error.
 */
int edit_article(struct article *article);

/**
 * @brief Rewrites the position of every article in @p ids to its index in
 *        the array, in a single transaction.
 * @param section_id Section identifier (defensive scoping).
 * @param ids        Ordered array of article ids.
 * @param count      Number of ids in @p ids.
 * @return 0 on success, http_res_code on error.
 */
int reorder_articles(int section_id, int *ids, size_t count);

/**
 * @brief Deletes an article by id.
 * @param section_id Section identifier.
 * @param article_id Article identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_article(int section_id, int article_id);
