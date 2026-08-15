#pragma once

/**
 * @file endpoints/feed.h
 * @brief Feed collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /feed — list all feeds or create a new one.
 *
 * GET: returns a paginated JSON list of feeds.
 * POST: validates the body, inserts the feed, and returns the created feed
 *       object (201). Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_feeds_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /feed/:id — fetch, update, or delete a feed.
 *
 * GET: returns the feed object.
 * PUT: updates the feed and returns the updated object. Requires authentication.
 * DELETE: deletes the feed. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param id          Feed database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_feed_res(struct mg_connection *c, struct mg_http_message *msg,
                   int id, struct error_reply *error_reply,
                   const char *secret);
