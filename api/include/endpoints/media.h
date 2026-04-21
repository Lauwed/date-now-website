#pragma once

/**
 * @file endpoints/media.h
 * @brief Media collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /media — list all media or upload a new one.
 *
 * GET: returns a paginated JSON list of all media records. Requires
 *      authentication.
 * POST: accepts a multipart/form-data upload, validates the file type and
 *       size, converts the image to WebP via ImageMagick, stores the file,
 *       inserts the media record, and replies 201 with the created media
 *       object. Replies 413 if the file exceeds the size limit, 415 if the
 *       content type is not a supported image. Requires authentication.
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
 * PUT: updates the alt text or replaces the file. Requires authentication.
 * DELETE: checks that the media is not referenced by any issue, deletes the
 *         file and the record. Requires authentication.
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
