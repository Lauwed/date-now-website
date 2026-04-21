#pragma once

/**
 * @file sql/media.h
 * @brief SQLite data-access functions for the Media table.
 */

#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether a media record with the given id exists.
 * @param id Database identifier.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 */
int media_exists(int id);

/**
 * @brief Returns the total number of media records.
 * @return Count, or a negative http_res_code on error.
 */
int get_medias_len(void);

/**
 * @brief Fetches all media records into a pre-allocated array.
 * @param len Size of @p arr.
 * @param arr Pre-allocated array of `struct media *` pointers.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_media().
 */
int get_medias(size_t len, struct media **arr);

/**
 * @brief Fetches a single media record by id.
 * @param media Pre-allocated and initialised structure to fill.
 * @param id    Database identifier.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note Dynamic fields are allocated by media_map() and must be freed via
 *       free_media().
 */
int get_media(struct media *media, int id);

/**
 * @brief Inserts a new media record.
 * @param media Hydrated structure to insert. @c media->id is set to the
 *              last-insert rowid on success.
 * @return 0 on success, http_res_code on error.
 * @note @p media is not freed by this function.
 */
int add_media(struct media *media);

/**
 * @brief Updates the file fields (url, width, height) of an existing media.
 * @param id     Database identifier.
 * @param url    New relative URL of the file.
 * @param width  New width in pixels.
 * @param height New height in pixels.
 * @return 0 on success, http_res_code on error.
 * @note @p url is not freed by this function.
 */
int update_media_file(int id, const char *url, double width, double height);

/**
 * @brief Updates the alt text of an existing media record.
 * @param id       Database identifier.
 * @param alt_text New alternative text.
 * @return 0 on success, http_res_code on error.
 * @note @p alt_text is not freed by this function.
 */
int update_media_alt_text(int id, const char *alt_text);

/**
 * @brief Checks whether a media is referenced by any other record.
 * @param id Database identifier.
 * @return 1 if referenced, 0 if not, negative on SQL error.
 */
int media_is_referenced(int id);

/**
 * @brief Deletes a media record by id.
 * @param id Database identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_media(int id);
