#pragma once

/**
 * @file endpoints/media.h
 * @brief Media collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/** @brief Returned by delete_media_with_blob() when the media is still in use. */
#define MEDIA_STILL_REFERENCED 2

/**
 * @brief Deletes a media record together with its Vercel Blob objects.
 *
 * Refuses to delete anything while the media is still referenced — by an
 * issue cover, a user picture, or an image link inside a section's or an
 * article's markdown (see media_is_referenced()).
 *
 * Callers that replace an image (issue cover, user picture) use this to drop
 * the previous one instead of leaking a row and two Blob objects.
 *
 * @param id Media database identifier.
 * @return 0 on success, MEDIA_STILL_REFERENCED if it is still in use and was
 *         left untouched, 1 on any other failure.
 */
int delete_media_with_blob(int id);

/**
 * @brief Handles GET/POST /media — list all media or upload a new one.
 *
 * GET: returns a paginated JSON list of all media records. Requires
 *      authentication.
 * POST: accepts a multipart/form-data upload, validates the file type and
 *       size, inserts the media record, and replies 202 immediately with
 *       the created media id. A background thread converts the image to
 *       WebP (full-size + thumbnail) via ImageMagick and uploads both to
 *       Vercel Blob, then updates the record's url/thumbUrl/width/height.
 *       Replies 413 if the file exceeds the size limit, 415 if the content
 *       type is not a supported image. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_medias_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /media/:id — fetch, update, or delete a
 *        media record.
 *
 * GET: returns the media object. Requires authentication.
 * PUT: updates the alt text. The file itself cannot be replaced — upload a
 *      new media instead. Requires authentication.
 * DELETE: refuses with 409 while the media is still referenced, otherwise
 *         deletes the Blob objects and the record. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param id          Media database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_media_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply,
                    const char *secret);
