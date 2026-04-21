#pragma once

/**
 * @file endpoints/tag.h
 * @brief Tag collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /tag — list all tags or create a new one.
 *
 * GET: returns the full tag list as a plain JSON array (no pagination wrapper).
 * POST: validates the body, inserts the tag, and returns the created tag
 *       object (201). Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_tags_res(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /tag/:name — fetch, update, or delete a tag.
 *
 * GET: returns the tag object.
 * PUT: updates the tag and returns the updated object. Requires authentication.
 * DELETE: deletes the tag. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param name        Tag name (primary key). Not freed by this function.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_tag_res(struct mg_connection *c, struct mg_http_message *msg,
                  char *name, struct error_reply *error_reply,
                  const char *secret);
