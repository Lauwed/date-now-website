#pragma once

#include <stddef.h>

/**
 * @file blob.h
 * @brief Vercel Blob storage client (libcurl) — upload/delete objects via
 *        the Vercel Blob REST API (https://blob.vercel-storage.com).
 */

/**
 * @brief Uploads a buffer to Vercel Blob.
 *
 * Requires the @c BLOB_READ_WRITE_TOKEN environment variable (a Vercel Blob
 * read/write token).
 *
 * @param pathname     Destination path/key within the Blob store (e.g.
 *                      "media/<uuid>/image.webp").
 * @param data          Raw bytes to upload.
 * @param len           Length of @p data in bytes.
 * @param content_type  MIME type of the content (e.g. "image/webp").
 * @param out_url       On success, set to a malloc'd absolute Blob CDN URL —
 *                      caller must free() it. Untouched on failure.
 * @return 0 on success, non-zero on error (network, auth, or API error).
 * @note None of the input parameters are freed by this function.
 */
int blob_upload(const char *pathname, const unsigned char *data, size_t len,
                const char *content_type, char **out_url);

/**
 * @brief Deletes one or more objects from Vercel Blob by URL.
 *
 * Requires the @c BLOB_READ_WRITE_TOKEN environment variable.
 *
 * @param urls  Array of absolute Blob URLs to delete.
 * @param count Number of elements in @p urls. NULL entries are skipped.
 * @return 0 on success, non-zero on error.
 * @note None of the input parameters are freed by this function.
 */
int blob_delete(const char * const *urls, size_t count);
