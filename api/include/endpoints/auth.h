#pragma once

/**
 * @file endpoints/auth.h
 * @brief Authentication endpoint handlers (subscription, login, TOTP).
 *
 * All handlers follow the Mongoose event-loop signature and reply directly
 * via @p c. None of the pointer parameters are freed by these functions.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles POST /auth/subscribe — sends a subscription confirmation email.
 *
 * Reads the "email" field from the request body, validates it, checks for
 * duplicates, then generates a SUBSCRIPTION JWT and emails a confirmation
 * link to the address. Replies 409 if the email is already subscribed.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_subscription_mail(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply,
                            const char *secret);

/**
 * @brief Handles POST /auth/subscribe/confirm — confirms a newsletter subscription.
 *
 * Verifies the SUBSCRIPTION JWT from the body, marks the user as subscribed,
 * and replies 200 on success.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void subscribe_user(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles POST /auth/register — registers a new AUTHOR user.
 *
 * Verifies a LOGIN JWT, creates the user with role AUTHOR, and replies 201
 * with `{ "message": "Author successfully registered", "totpseed": "..." }`.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void register_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles POST /auth/totp/regenerate — regenerates the TOTP seed.
 *
 * Requires a valid SESSION token. Generates a new TOTP seed for the
 * authenticated user and replies 200 with the new seed.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 */
void generate_totpseed_user(struct mg_connection *c,
                            struct mg_http_message *msg,
                            struct error_reply *error_reply);

/**
 * @brief Handles POST /auth/login — sends a login confirmation email.
 *
 * Reads the "email" field, looks up the user, generates a LOGIN JWT, and
 * emails a login confirmation link. Replies 200 on success.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles POST /auth/login/confirm — validates the TOTP code and issues
 *        a SESSION token.
 *
 * Verifies the LOGIN JWT from the body, validates the provided TOTP code
 * against the user's seed (current ±1 step), and replies 200 with a SESSION
 * JWT on success.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void login_user(struct mg_connection *c, struct mg_http_message *msg,
                struct error_reply *error_reply, const char *secret);

/**
 * @brief Checks whether the current request carries a valid SESSION JWT.
 *
 * Reads the Authorization header, verifies the JWT signature and type, and
 * sets @p *user_logged to 1 if valid. Replies 401 and sets @p *user_logged
 * to 0 if not.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 * @param user_logged Output flag: set to 1 if authenticated, 0 otherwise.
 */
void is_user_logged(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret,
                    int *user_logged);
