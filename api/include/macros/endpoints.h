#pragma once

/**
 * @file endpoints.h
 * @brief Convenience macros for endpoint handlers: HTTP headers, error
 *        constants, error reply shortcuts, and query-code dispatching.
 *
 * All ERROR_REPLY_* macros assume that the current scope contains:
 * - @c c           — a `struct mg_connection *`
 * - @c error_reply — a `struct error_reply *`
 * - @c query_code  — an `int` (for HANDLE_QUERY_CODE and ERROR_REPLY_SQL)
 * - @c msg         — a `struct mg_http_message *` (for REQUIRED_BODY_PROPERTY)
 */

#define JSON_HEADER "Content-Type: application/json\r\n"

#define EMAIL_ERROR_MESSAGE         "ERROR WHILE SENDING EMAIL"
#define EMAIL_VALIDITY_ERROR_MESSAGE "EMAIL IS NOT VALID"
#define BODY_REQUIRED_MESSAGE       "BODY IS REQUIRED"
#define JSON_ERROR_MESSAGE          "JSON IS NOT VALID"
#define BAD_REQUEST_MESSAGE         "Bad request."
#define JWT_EXPIRED_MESSAGE         "JWT expired"
#define BAD_JWT_MESSAGE             "Wrong signature or token invalid"
#define WRONG_JWT_TYPE_MESSAGE      "Wrong type of JWT"
#define UNAUTHORIZED_MESSAGE        "Not authorized"

/** @brief Reply with HTTP 500 and a generic internal error JSON body. */
#define ERROR_REPLY_500                                                        \
  error_reply_map(error_reply, 500, "Internal error", 500);                    \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 500 and a SQL query error JSON body. */
#define ERROR_REPLY_SQL                                                        \
  error_reply_map(error_reply, query_code, "Error SQL Query.", 500);           \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 405 Method Not Allowed. */
#define ERROR_REPLY_405                                                        \
  error_reply_map(error_reply, 405, "Method not allowed", 405);                \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 400 Bad Request and the given @p message. */
#define ERROR_REPLY_400(message)                                               \
  error_reply_map(error_reply, 400, message, 400);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 404 Not Found. */
#define ERROR_REPLY_404                                                        \
  error_reply_map(error_reply, 404, "Not found", 404);                         \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 401 Unauthorized. */
#define ERROR_REPLY_401                                                        \
  error_reply_map(error_reply, 401, "Not authorized", 401);                    \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 409 Conflict and the given @p message. */
#define ERROR_REPLY_409(message)                                               \
  error_reply_map(error_reply, 409, message, 409);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 413 Content Too Large and the given @p message. */
#define ERROR_REPLY_413(message)                                               \
  error_reply_map(error_reply, 413, message, 413);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/** @brief Reply with HTTP 415 Unsupported Media Type and the given @p message. */
#define ERROR_REPLY_415(message)                                               \
  error_reply_map(error_reply, 415, message, 415);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

/**
 * @brief Dispatches an HTTP error reply based on the value of @c query_code.
 *
 * Must follow a SQL function call. Handles HTTP_INTERNAL_ERROR (500),
 * HTTP_NOT_FOUND (404), HTTP_BAD_REQUEST (400), and any other non-zero code
 * (generic SQL error → 500).
 */
#define HANDLE_QUERY_CODE                                                      \
  if (query_code == HTTP_INTERNAL_ERROR) {                                     \
    ERROR_REPLY_500;                                                           \
  } else if (query_code == HTTP_NOT_FOUND) {                                   \
    ERROR_REPLY_404;                                                           \
  } else if (query_code == HTTP_BAD_REQUEST) {                                 \
    error_reply_map(error_reply, 400, "Bad request", 400);                     \
  } else {                                                                     \
    fprintf(stderr, "ERROR SQL QUERY: %d\n", query_code);                      \
    ERROR_REPLY_SQL;                                                           \
  }

/**
 * @brief Asserts that a JSON body property exists; replies 400 and returns
 *        if it is missing.
 * @param prop    JSON path fragment (e.g. "username").
 * @param message Error message sent in the 400 reply.
 */
#define REQUIRED_BODY_PROPERTY(prop, message)                                  \
  offset = mg_json_get(msg->body, "$." prop, &length);                         \
  if (offset < 0) {                                                            \
    ERROR_REPLY_400(message);                                                  \
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(prop "REQUIRED"));                  \
    return;                                                                    \
  }
