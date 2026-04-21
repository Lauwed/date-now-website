#pragma once

/**
 * @file enums.h
 * @brief Shared enumerations used across the API.
 */

/**
 * @brief HTTP error codes used as return values from SQL functions.
 *
 * These values allow SQL functions to communicate the type of error back to
 * the endpoint via the HANDLE_QUERY_CODE macro.
 */
enum http_res_code {
  HTTP_NOT_FOUND      = 404,  /**< Resource not found in the database. */
  HTTP_BAD_REQUEST    = 400,  /**< Invalid or missing parameter. */
  HTTP_INTERNAL_ERROR = 500   /**< Unexpected internal SQLite error. */
};

/**
 * @brief JWT token type stored in the "type" claim of the payload.
 *
 * Distinguishes tokens issued for different stages of the authentication
 * flow and prevents them from being reused across stages.
 */
enum jwt_type {
  SUBSCRIPTION = 1,  /**< Subscription confirmation token (valid 24 h). */
  LOGIN        = 2,  /**< Login confirmation token (valid 24 h). */
  SESSION      = 3   /**< Active session token issued after successful TOTP login. */
};
