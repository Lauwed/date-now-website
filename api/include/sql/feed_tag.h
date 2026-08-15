#pragma once

/**
 * @file sql/feed_tag.h
 * @brief SQLite data-access functions for the FeedTag join table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether the tag association (feed_id, tag_name) exists.
 * @param feed_id Feed identifier.
 * @param id      Tag name.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 * @note @p id is not freed by this function.
 */
int feed_tag_exists(int feed_id, char *id);

/**
 * @brief Returns the count of tags associated with a feed.
 * @param q       Unused search query parameter (reserved for future use).
 * @param feed_id Feed identifier.
 * @return Count of associated tags, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_feed_tags_len(const struct mg_str *q, int feed_id);

/**
 * @brief Fetches a page of feed_tag records for a given feed.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct feed_tag *` pointers.
 * @param feed_id   Feed identifier.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_feed_tag().
 */
int get_feed_tags(size_t len, struct feed_tag **arr, int feed_id, int page,
                  int page_size);

/**
 * @brief Inserts a new feed_tag association record.
 * @param feed_tag Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p feed_tag is not freed by this function.
 */
int add_feed_tag(struct feed_tag *feed_tag);

/**
 * @brief Deletes a feed_tag association by feed id and tag name.
 * @param feed_id Feed identifier.
 * @param id      Tag name.
 * @return 0 on success, http_res_code on error.
 * @note @p id is not freed by this function.
 */
int delete_feed_tag(int feed_id, char *id);
