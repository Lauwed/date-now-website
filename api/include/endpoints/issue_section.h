#pragma once

/**
 * @file endpoints/issue_section.h
 * @brief IssueSection collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /issue/:issueId/section — list or create sections.
 *
 * GET: returns all sections of the issue, ordered by position, each
 *      including its articles when type is "CATEGORY". Public, no
 *      authentication required.
 * POST: validates the body (type + categoryName XOR textBody), appends the
 *       section at the end of the issue, and replies 201. Requires
 *       authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_sections_res(struct mg_connection *c,
                             struct mg_http_message *msg, int issue_id,
                             struct error_reply *error_reply,
                             const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /issue/:issueId/section/:sectionId.
 *
 * GET: returns the section, with articles when type is "CATEGORY". Public.
 * PUT: updates categoryName (CATEGORY sections) or textBody (TEXT sections).
 *      Never changes type or position. Requires authentication.
 * DELETE: deletes the section, cascading to its articles. Requires
 *         authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param section_id  Section database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_section_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id,
                            int section_id, struct error_reply *error_reply,
                            const char *secret);

/**
 * @brief Handles PUT /issue/:issueId/section/reorder.
 *
 * Body: `{"order": [sectionId, sectionId, ...]}` — must contain exactly the
 * set of section ids currently belonging to the issue. Rewrites their
 * position to their index in the array, in a single transaction. Requires
 * authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_sections_reorder_res(struct mg_connection *c,
                                     struct mg_http_message *msg,
                                     int issue_id,
                                     struct error_reply *error_reply,
                                     const char *secret);
