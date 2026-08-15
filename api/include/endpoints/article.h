#pragma once

/**
 * @file endpoints/article.h
 * @brief Article collection and single-resource endpoint handlers, nested
 *        under an issue's category section.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /issue/:issueId/section/:sectionId/article.
 *
 * GET: returns all articles of the section, ordered by position. Public,
 *      no authentication required. 404 if the section isn't a CATEGORY
 *      section or doesn't belong to the issue.
 * POST: validates the body (title, sourceName, sourceUrl, summary),
 *       appends the article at the end of the section, and replies 201.
 *       Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param section_id  Section database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_articles_res(struct mg_connection *c, struct mg_http_message *msg,
                       int issue_id, int section_id,
                       struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE
 *        /issue/:issueId/section/:sectionId/article/:articleId.
 *
 * GET: returns the article. Public.
 * PUT: updates any of title/sourceName/sourceUrl/summary. Never changes
 *      position. Requires authentication.
 * DELETE: deletes the article. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param section_id  Section database identifier.
 * @param article_id  Article database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_article_res(struct mg_connection *c, struct mg_http_message *msg,
                      int issue_id, int section_id, int article_id,
                      struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles PUT /issue/:issueId/section/:sectionId/article/reorder.
 *
 * Body: `{"order": [articleId, articleId, ...]}` — must contain exactly the
 * set of article ids currently belonging to the section. Rewrites their
 * position to their index in the array, in a single transaction. Requires
 * authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param section_id  Section database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_articles_reorder_res(struct mg_connection *c,
                               struct mg_http_message *msg, int issue_id,
                               int section_id,
                               struct error_reply *error_reply,
                               const char *secret);
