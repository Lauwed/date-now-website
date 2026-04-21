#pragma once

#include <stdlib.h>

/**
 * @file structs.h
 * @brief Data structure definitions used throughout the API.
 */

/**
 * @brief HTTP error response, serialisable to JSON.
 *
 * Used by the ERROR_REPLY_* macros to build an error response.
 * The @c json field is allocated by error_reply_to_json() and must be
 * freed by the caller after the response has been sent.
 */
struct error_reply {
  int code;       /**< Application-level error code. */
  int code_http;  /**< HTTP status code to return to the client. */
  char *message;  /**< Error message (external pointer, not allocated here). */
  char *json;     /**< JSON string produced by error_reply_to_json(). @note Dynamically allocated — caller must free after use. */
};

/**
 * @brief Paginated list response.
 *
 * Built by GET-list endpoints. The @c data field is provided by the caller
 * (result of a *_to_json() call), and @c json is allocated by
 * list_reply_to_json().
 */
struct list_reply {
  char *data;        /**< JSON array of items (allocated by caller). */
  int count;         /**< Number of items in the current page. */
  int total;         /**< Total number of items across all pages. */
  size_t page_size;  /**< Requested page size. */
  int page;          /**< Current page number (-1 = no pagination). */
  int total_pages;   /**< Total number of pages. */
  char *json;        /**< Full JSON envelope. @note Allocated by list_reply_to_json() — caller must free after use. */
};

/**
 * @brief Uploaded image media.
 *
 * All pointer fields are allocated by the SQL mapping functions and must
 * be freed via free_media().
 */
struct media {
  unsigned id;              /**< Database identifier. */
  char *alternative_text;   /**< Alt text. @note Allocated by map — freed by free_media(). */
  char *url;                /**< Relative file URL. @note Allocated by map — freed by free_media(). */
  double width;             /**< Width in pixels. */
  double height;            /**< Height in pixels. */
};

/**
 * @brief System user.
 *
 * Pointer fields @c username, @c email, @c role, @c link are allocated by
 * SQL mapping or HTTP hydration functions and freed by free_user().
 * The @c picture field (when non-NULL) is freed recursively by free_user()
 * via free_media().
 */
struct user {
  int id;             /**< Database identifier. */
  char *username;     /**< Display name. @note Dynamically allocated — freed by free_user(). */
  char *email;        /**< Email address. @note Dynamically allocated — freed by free_user(). */
  char *role;         /**< Role: "USER" or "AUTHOR". @note Dynamically allocated — freed by free_user(). */
  char totp_seed[64]; /**< Base32-encoded TOTP seed (fixed-size array, not dynamically allocated). */
  struct media *picture; /**< Profile picture (may be NULL). @note Dynamically allocated — freed recursively by free_user(). */
  char *link;         /**< Optional personal URL (may be NULL). @note Dynamically allocated — freed by free_user(). */
  int subscribed_at;  /**< Newsletter subscription timestamp (0 = not subscribed). */
  int is_supporter;   /**< 1 if the user is a supporter, 0 otherwise. */
  int created_at;     /**< Account creation timestamp (Unix). */
};

/**
 * @brief Issue categorisation tag.
 *
 * @c name is the primary key. Fields are allocated by mapping/hydration
 * and freed by free_tag().
 */
struct tag {
  char *name;   /**< Tag name (primary key, max 64 chars). @note Dynamically allocated — freed by free_tag(). */
  char *color;  /**< Hex colour (#RRGGBB). @note Dynamically allocated — freed by free_tag(). */
};

/**
 * @brief Sponsor that can be associated with issues.
 *
 * @c name is the primary key. Fields are freed by free_sponsor().
 */
struct sponsor {
  char *name;  /**< Sponsor name (primary key). @note Dynamically allocated — freed by free_sponsor(). */
  char *link;  /**< Default sponsor URL. @note Dynamically allocated — freed by free_sponsor(). */
};

/**
 * @brief Association between an issue and an author (IssueAuthor table).
 *
 * Scalar structure — freed by free_issue_author() (simple free()).
 */
struct issue_author {
  int user_id;   /**< Author's user identifier. */
  int issue_id;  /**< Issue identifier. */
};

/**
 * @brief Association between an issue and a tag (IssueTag table).
 *
 * @c tag_name is allocated by the mapping function and freed by
 * free_issue_tag().
 */
struct issue_tag {
  char *tag_name;  /**< Tag name. @note Dynamically allocated — freed by free_issue_tag(). */
  int issue_id;    /**< Issue identifier. */
};

/**
 * @brief Association between an issue and a sponsor (IssueSponsor table).
 *
 * @c sponsor_name and @c link are allocated by mapping and freed by
 * free_issue_sponsor().
 */
struct issue_sponsor {
  char *sponsor_name;  /**< Sponsor name. @note Dynamically allocated — freed by free_issue_sponsor(). */
  int issue_id;        /**< Issue identifier. */
  char *link;          /**< Sponsor-specific advertising link for this association. @note Dynamically allocated — freed by free_issue_sponsor(). */
};

/**
 * @brief Newsletter issue.
 *
 * Text fields (@c slug, @c title, @c subtitle, @c excerpt, @c content,
 * @c status) are dynamically allocated and freed by free_issue().
 * Nested sub-structures (@c cover, @c tags, @c authors, @c sponsors) are
 * freed recursively by free_issue().
 */
struct issue {
  int id;                           /**< Database identifier. */
  char *slug;                       /**< URL-friendly identifier. @note Dynamically allocated — freed by free_issue(). */
  char *title;                      /**< Issue title. @note Dynamically allocated — freed by free_issue(). */
  char *subtitle;                   /**< Subtitle. @note Dynamically allocated — freed by free_issue(). */
  struct media *cover;              /**< Cover image (may be NULL). @note Dynamically allocated — freed recursively by free_issue(). */
  int created_at;                   /**< Creation timestamp (Unix). */
  int published_at;                 /**< Publication timestamp (0 = unpublished). */
  int updated_at;                   /**< Last modification timestamp (Unix). */
  int issue_number;                 /**< Sequential issue number. */
  char *excerpt;                    /**< Short summary. @note Dynamically allocated — freed by free_issue(). */
  char *content;                    /**< Full content (HTML/Markdown). @note Dynamically allocated — freed by free_issue(). */
  int is_sponsored;                 /**< 1 if the issue has sponsors. */
  char *status;                     /**< Status: "DRAFT", "PUBLISHED", or "ARCHIVE". @note Dynamically allocated — freed by free_issue(). */
  int opened_mail_count;            /**< Number of newsletter email opens. */
  struct issue_tag **tags;          /**< Array of associated tags (may be NULL). @note Dynamically allocated — freed recursively by free_issue(). */
  size_t tags_count;                /**< Number of tags in the array. */
  struct user **authors;            /**< Array of authors (may be NULL). @note Dynamically allocated — freed recursively by free_issue(). */
  size_t authors_count;             /**< Number of authors. */
  struct issue_sponsor **sponsors;  /**< Array of sponsors (may be NULL). @note Dynamically allocated — freed recursively by free_issue(). */
  size_t sponsors_count;            /**< Number of sponsors. */
};

/**
 * @brief A page view (visit) of an issue by a visitor.
 *
 * @c hashed_ip is allocated by the mapping function and freed by free_view().
 */
struct view {
  int id;           /**< Database identifier. */
  int time;         /**< Unix timestamp of the visit. */
  char *hashed_ip;  /**< Hashed visitor IP address. @note Dynamically allocated — freed by free_view(). */
  int issue_id;     /**< Identifier of the visited issue. */
};
