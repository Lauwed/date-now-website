#pragma once

/**
 * @file endpoints/feed_tag.h
 * @brief FeedTag collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /feed/:feedId/tag — list or add tags.
 *
 * GET: returns a paginated JSON list of feed_tag records for the feed.
 * POST: validates the body, checks for duplicate associations, inserts the
 *       record, and replies 201. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param feed_id     Feed database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_feed_tags_res(struct mg_connection *c, struct mg_http_message *msg,
                        int feed_id, struct error_reply *error_reply,
                        const char *secret);

/**
 * @brief Handles DELETE /feed/:feedId/tag/:name — removes a tag association.
 *
 * Deletes the FeedTag association. Requires authentication.
 * Replies 200 with `{ "message": "Tag successfully deleted" }`.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param feed_id     Feed database identifier.
 * @param name        Tag name. Not freed by this function.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_feed_tag_res(struct mg_connection *c, struct mg_http_message *msg,
                       int feed_id, char *name,
                       struct error_reply *error_reply, const char *secret);
