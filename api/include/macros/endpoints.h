#pragma once

/**
 * @file endpoints.h
 * @brief Convenience macros for endpoint handlers: HTTP headers, error
 *        constants, error reply shortcuts, success reply shortcuts, and
 *        query-code dispatching.
 *
 * All ERROR_REPLY_* and SUCCESS_REPLY_* macros assume the current scope
 * contains:
 * - @c c           — a `struct mg_connection *`
 * - @c error_reply — a `struct error_reply *`  (ERROR_REPLY_* only)
 * - @c query_code  — an `int`                   (HANDLE_QUERY_CODE / ERROR_REPLY_SQL only)
 * - @c msg         — a `struct mg_http_message *` (REQUIRED_BODY_PROPERTY only)
 *
 * Memory contract for ERROR_REPLY_* macros:
 *   error_reply_map() allocates error_reply->json. Each macro frees it
 *   immediately after mg_http_reply(), which copies data into the Mongoose
 *   write buffer before returning.
 */

#define JSON_HEADER "Content-Type: application/json\r\n"

/* Generic request errors */
#define BODY_REQUIRED_MESSAGE        "BODY IS REQUIRED"
#define JSON_ERROR_MESSAGE           "JSON IS NOT VALID"
#define BAD_REQUEST_MESSAGE          "Bad request."
#define UNAUTHORIZED_MESSAGE         "Not authorized"

/* Auth / JWT errors */
#define EMAIL_ERROR_MESSAGE          "ERROR WHILE SENDING EMAIL"
#define EMAIL_VALIDITY_ERROR_MESSAGE "EMAIL IS NOT VALID"
#define JWT_EXPIRED_MESSAGE          "JWT expired"
#define BAD_JWT_MESSAGE              "Wrong signature or token invalid"
#define WRONG_JWT_TYPE_MESSAGE       "Wrong type of JWT"

/* Shared resource validation messages */
#define USER_EXISTS_MESSAGE          "The user already exists."
#define EMAIL_REQUIRED_MESSAGE       "Email is required."
#define NAME_REQUIRED_MESSAGE        "The name is required."
#define LINK_REQUIRED_MESSAGE        "The link is required."

/* Auth endpoint messages */
#define NO_TOKEN_MESSAGE              "No token given."
#define USERNAME_REQUIRED_MESSAGE     "Username is required."
#define TOKEN_REQUIRED_MESSAGE        "Token is required."
#define CODE_REQUIRED_MESSAGE         "Code is required."

/* Issue endpoint messages */
#define ISSUE_EXISTS_MESSAGE             "The issue already exists."
#define ISSUE_ALREADY_PUBLISHED_MESSAGE  "Issue is already published."
#define TITLE_REQUIRED_MESSAGE           "Title is required."
#define ISSUE_NUMBER_REQUIRED_MESSAGE    "Issue number is required."
#define STATUS_FORMAT_MESSAGE            \
  "Value of 'status' should be 'DRAFT', 'PUBLISHED' or 'ARCHIVE'."

/* User endpoint messages */
#define ROLE_FORMAT_MESSAGE  "Value of 'role' should be 'USER' or 'AUTHOR'."

/* Tag endpoint messages */
#define TAG_EXISTS_MESSAGE     "The tag already exists."
#define COLOR_REQUIRED_MESSAGE "The color is required."

/* Sponsor endpoint messages */
#define SPONSOR_EXISTS_MESSAGE "The sponsor already exists."

/* Media endpoint messages */
#define TEXT_ALT_REQUIRED_MESSAGE "textAlternatif is required and must not be empty."
#define FILE_REQUIRED_MESSAGE     "file is required."
#define FILE_TOO_LARGE_MESSAGE    "File must not exceed 5MB."
#define FILE_NOT_IMAGE_MESSAGE    "File must be an image."

/* View endpoint messages */
#define HASHED_IP_REQUIRED_MESSAGE "The hashed IP is required."
#define ISSUE_REQUIRED_MESSAGE     "The issue ID is required."

/* -------------------------------------------------------------------------
 * Error reply macros
 * Each macro sends the response and frees the allocated JSON string.
 * ---------------------------------------------------------------------- */

/** @brief Reply with HTTP 500 and a generic internal error JSON body. */
#define ERROR_REPLY_500                                                        \
  error_reply_map(error_reply, 500, "Internal error", 500);                    \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;

/** @brief Reply with HTTP 500 and a SQL query error JSON body. */
#define ERROR_REPLY_SQL                                                        \
  error_reply_map(error_reply, query_code, "Error SQL Query.", 500);           \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;

/** @brief Reply with HTTP 405 Method Not Allowed. */
#define ERROR_REPLY_405                                                        \
  error_reply_map(error_reply, 405, "Method not allowed", 405);                \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;

/** @brief Reply with HTTP 400 Bad Request and the given @p message. */
#define ERROR_REPLY_400(message)                                               \
  error_reply_map(error_reply, 400, message, 400);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;                                                     \
  fprintf(stderr, TERMINAL_ERROR_MESSAGE(message));

/** @brief Reply with HTTP 404 Not Found. */
#define ERROR_REPLY_404                                                        \
  error_reply_map(error_reply, 404, "Not found", 404);                         \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;

/** @brief Reply with HTTP 401 Unauthorized. */
#define ERROR_REPLY_401                                                        \
  error_reply_map(error_reply, 401, "Not authorized", 401);                    \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;

/** @brief Reply with HTTP 409 Conflict and the given @p message. */
#define ERROR_REPLY_409(message)                                               \
  error_reply_map(error_reply, 409, message, 409);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;                                                     \
  fprintf(stderr, TERMINAL_ERROR_MESSAGE(message));

/** @brief Reply with HTTP 429 Too Many Requests. */
#define ERROR_REPLY_429                                                        \
  mg_http_reply(c, 429, JSON_HEADER,                                           \
                "{\"code\":429,\"error\":\"Too many requests\"}");

/** @brief Reply with HTTP 413 Content Too Large and the given @p message. */
#define ERROR_REPLY_413(message)                                               \
  error_reply_map(error_reply, 413, message, 413);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;                                                     \
  fprintf(stderr, TERMINAL_ERROR_MESSAGE(message));

/** @brief Reply with HTTP 415 Unsupported Media Type and the given @p message. */
#define ERROR_REPLY_415(message)                                               \
  error_reply_map(error_reply, 415, message, 415);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);    \
  free(error_reply->json);                                                      \
  error_reply->json = NULL;                                                     \
  fprintf(stderr, TERMINAL_ERROR_MESSAGE(message));

/* -------------------------------------------------------------------------
 * Success reply macros
 * @c json_str must be a NUL-terminated string; it is NOT freed by these
 * macros — the caller remains responsible for freeing it if dynamically
 * allocated.
 * ---------------------------------------------------------------------- */

/**
 * @brief Reply HTTP 200 with a serialised JSON string (object or array).
 * @param json_str Result of a *_to_json() call or reply->json.
 */
#define SUCCESS_REPLY_200(json_str)                                            \
  mg_http_reply(c, 200, JSON_HEADER, "%s\n", json_str);

/**
 * @brief Reply HTTP 201 with a serialised JSON string (created resource).
 * @param json_str Result of a *_to_json() call.
 */
#define SUCCESS_REPLY_201(json_str)                                            \
  mg_http_reply(c, 201, JSON_HEADER, "%s\n", json_str);

/**
 * @brief Reply HTTP 200 with a simple @c {"message":"..."} JSON body.
 * @param message Static string literal (not freed).
 */
#define SUCCESS_REPLY_200_MSG(message)                                         \
  mg_http_reply(c, 200, JSON_HEADER, "{\"message\":\"%s\"}\n", message);

/**
 * @brief Reply HTTP 201 with a simple @c {"message":"..."} JSON body.
 * @param message Static string literal (not freed).
 */
#define SUCCESS_REPLY_201_MSG(message)                                         \
  mg_http_reply(c, 201, JSON_HEADER, "{\"message\":\"%s\"}\n", message);

/* -------------------------------------------------------------------------
 * Query-code and body-property helpers
 * ---------------------------------------------------------------------- */

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
    ERROR_REPLY_400(BAD_REQUEST_MESSAGE);                                      \
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
    return;                                                                    \
  }
