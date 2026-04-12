#pragma once

#define JSON_HEADER "Content-Type: application/json\r\n"

#define EMAIL_ERROR_MESSAGE "ERROR WHILE SENDING EMAIL"
#define BODY_REQUIRED_MESSAGE "BODY IS REQUIRED"
#define JSON_ERROR_MESSAGE "JSON IS NOT VALID"
#define BAD_REQUEST_MESSAGE "Bad request."
#define JWT_EXPIRED "JWT Token expired"
#define BAD_JWT_TOKEN "Wrong signature or token invalid"

#define ERROR_REPLY_500                                                        \
  error_reply_map(error_reply, 500, "Internal error", 500);                    \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);
#define ERROR_REPLY_SQL                                                        \
  error_reply_map(error_reply, query_code, "Error SQL Query.", 500);           \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);
#define ERROR_REPLY_405                                                        \
  error_reply_map(error_reply, 405, "Method not allowed", 405);                \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);
#define ERROR_REPLY_400(message)                                               \
  error_reply_map(error_reply, 400, message, 400);                             \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);
#define ERROR_REPLY_404                                                        \
  error_reply_map(error_reply, 404, "Not found", 404);                         \
  mg_http_reply(c, error_reply->code_http, JSON_HEADER, error_reply->json);

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

#define REQUIRED_BODY_PROPERTY(prop, message)                                  \
  offset = mg_json_get(msg->body, "$." prop, &length);                         \
  if (offset < 0) {                                                            \
    ERROR_REPLY_400(message);                                                  \
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(prop "REQUIRED"));                  \
    return;                                                                    \
  }
