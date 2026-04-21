#pragma once

/**
 * @file utils.h
 * @brief Utility functions: validation, JSON serialisation, SQLite mapping,
 *        HTTP hydration, structure initialisation and memory management.
 */

#include <lib/mongoose.h>
#include <sqlite3.h>
#include <stddef.h>
#include <structs.h>

/* -------------------------------------------------------------------------
 * Validation and conversion
 * ---------------------------------------------------------------------- */

/**
 * @brief Validates an email address using a regular expression.
 * @param email String to validate (trimmed in-place).
 * @return 0 if valid, -1 if NULL, 1 on regex error, 2 if format is invalid.
 * @note @p email is not freed by this function.
 */
int check_email_validity(char *email);

/**
 * @brief Identifies the HTTP method from a raw request buffer.
 * @param buffer Start of the request buffer.
 * @return Pointer to a static constant "GET", "POST", "PUT", or "DELETE",
 *         or NULL if unrecognised. @note No allocation — do not free.
 */
const char *get_method(const char *buffer);

/**
 * @brief Copies a mg_str into a standard C buffer.
 * @param dest  Destination buffer (must be large enough).
 * @param src   Source mg_str.
 * @return 0 on success, 1 on sprintf failure.
 * @note @p dest is not allocated here — the caller is responsible.
 */
int mg_str_to_str(char *dest, struct mg_str src);

/**
 * @brief Transforms a string into a URL-friendly slug in-place.
 *
 * Replaces non-alphanumeric characters with hyphens and converts to
 * lowercase. The transformation is done in-place in @p str.
 *
 * @param str String to transform (modified in-place).
 * @param len Length of the string.
 * @return Always 0.
 * @note @p str is neither allocated nor freed here.
 */
int str_to_slug(char *str, size_t len);


/* -------------------------------------------------------------------------
 * JSON serialisation
 * ---------------------------------------------------------------------- */

/**
 * @brief Serialises an error_reply to JSON and allocates error_reply::json.
 * @note Allocates @c err->json via cJSON — caller must free it.
 */
void error_reply_to_json(struct error_reply *err);

/**
 * @brief Serialises a list_reply to its JSON envelope and allocates list_reply::json.
 * @note Allocates @c reply->json via cJSON — caller must free it.
 *       @c reply->data must be a valid JSON array string (or NULL).
 */
void list_reply_to_json(struct list_reply *reply);

/**
 * @brief Serialises a media to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 * @note Caller must free the returned string when it is not "null".
 */
char *media_to_json(struct media *media);

/** @brief Serialises an array of media to a JSON array. */
char *medias_to_json(struct media **medias, size_t len);

/**
 * @brief Serialises a user to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 * @note Caller must free the returned string when it is not "null".
 */
char *user_to_json(struct user *user);

/** @brief Serialises an array of users to a JSON array. */
char *users_to_json(struct user **users, size_t len);

/**
 * @brief Serialises a view to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 * @note Caller must free the returned string when it is not "null".
 */
char *view_to_json(struct view *view);

/** @brief Serialises an array of views to a JSON array. */
char *views_to_json(struct view **views, size_t len);

/**
 * @brief Serialises a tag to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 * @note Caller must free the returned string when it is not "null".
 */
char *tag_to_json(struct tag *tag);

/** @brief Serialises an array of tags to a JSON array. */
char *tags_to_json(struct tag **tags, size_t len);

/**
 * @brief Serialises a sponsor to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 * @note Caller must free the returned string when it is not "null".
 */
char *sponsor_to_json(struct sponsor *sponsor);

/** @brief Serialises an array of sponsors to a JSON array. */
char *sponsors_to_json(struct sponsor **sponsors, size_t len);

/**
 * @brief Serialises an issue to JSON (including cover, tags, authors, sponsors).
 * @return Dynamically allocated JSON string, or the static literal "null".
 * @note Caller must free the returned string when it is not "null".
 */
char *issue_to_json(struct issue *issue);

/** @brief Serialises an array of issues to a JSON array. */
char *issues_to_json(struct issue **issues, size_t len);

/**
 * @brief Serialises an issue_author association to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 */
char *issue_author_to_json(struct issue_author *issue);

/** @brief Serialises an array of issue_author associations to a JSON array. */
char *issue_authors_to_json(struct issue_author **issues, size_t len);

/**
 * @brief Serialises an issue_sponsor association to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 */
char *issue_sponsor_to_json(struct issue_sponsor *issue);

/** @brief Serialises an array of issue_sponsor associations to a JSON array. */
char *issue_sponsors_to_json(struct issue_sponsor **issues, size_t len);

/**
 * @brief Serialises an issue_tag association to JSON.
 * @return Dynamically allocated JSON string, or the static literal "null".
 */
char *issue_tag_to_json(struct issue_tag *issue);

/** @brief Serialises an array of issue_tag associations to a JSON array. */
char *issue_tags_to_json(struct issue_tag **issues, size_t len);


/* -------------------------------------------------------------------------
 * Memory management
 * ---------------------------------------------------------------------- */

/**
 * @brief Frees all resources of a media and the structure itself.
 * @param media Pointer to the media to free.
 * @return 0.
 * @note @p media is invalid after this call.
 */
int free_media(struct media *media);

/**
 * @brief Frees all resources of a user and the structure itself.
 * @param user Pointer to the user to free.
 * @return 0.
 * @note Recursively frees @c user->picture via free_media() when non-NULL.
 *       @p user is invalid after this call.
 */
int free_user(struct user *user);

/**
 * @brief Frees all resources of a view and the structure itself.
 * @param view Pointer to the view to free.
 * @return 0.
 * @note @p view is invalid after this call.
 */
int free_view(struct view *view);

/**
 * @brief Frees all resources of an issue and the structure itself.
 * @param issue Pointer to the issue to free.
 * @return 0.
 * @note Recursively frees @c cover, @c tags, @c authors, and @c sponsors.
 *       @p issue is invalid after this call.
 */
int free_issue(struct issue *issue);

/**
 * @brief Frees an issue_author association (scalar structure only).
 * @param issue Pointer to free.
 * @return 0.
 */
int free_issue_author(struct issue_author *issue);

/**
 * @brief Frees an issue_sponsor association and its dynamic fields.
 * @param issue Pointer to free.
 * @return 0.
 */
int free_issue_sponsor(struct issue_sponsor *issue);

/**
 * @brief Frees an issue_tag association and its @c tag_name field.
 * @param issue Pointer to free.
 * @return 0.
 */
int free_issue_tag(struct issue_tag *issue);

/**
 * @brief Frees a tag and its @c name and @c color fields.
 * @param tag Pointer to free.
 * @return 0.
 */
int free_tag(struct tag *tag);

/**
 * @brief Frees a sponsor and its @c name and @c link fields.
 * @param sponsor Pointer to free.
 * @return 0.
 */
int free_sponsor(struct sponsor *sponsor);

/**
 * @brief Frees an array of users and each element.
 * @param user Array of pointers.
 * @param len  Number of elements.
 * @return 0 on success, error code on first failure.
 * @note Also frees the @p user array itself.
 */
int free_users(struct user **user, size_t len);

/**
 * @brief Frees an array of views and each element.
 * @param view Array of pointers.
 * @param len  Number of elements.
 * @return 0 on success.
 * @note Also frees the @p view array itself.
 */
int free_views(struct view **view, size_t len);

/**
 * @brief Frees an array of issues and each element.
 * @param issue Array of pointers.
 * @param len   Number of elements.
 * @return 0 on success.
 * @note Also frees the @p issue array itself.
 */
int free_issues(struct issue **issue, size_t len);

/**
 * @brief Frees an array of issue_author associations and each element.
 * @note Also frees the array itself.
 */
int free_issue_authors(struct issue_author **issue, size_t len);

/**
 * @brief Frees an array of issue_sponsor associations and each element.
 * @note Also frees the array itself.
 */
int free_issue_sponsors(struct issue_sponsor **issue, size_t len);

/**
 * @brief Frees an array of issue_tag associations and each element.
 * @note Also frees the array itself.
 */
int free_issue_tags(struct issue_tag **issue, size_t len);

/**
 * @brief Frees an array of tags and each element.
 * @note Also frees the array itself.
 */
int free_tags(struct tag **tag, size_t len);

/**
 * @brief Frees an array of sponsors and each element.
 * @note Also frees the array itself.
 */
int free_sponsors(struct sponsor **sponsor, size_t len);


/* -------------------------------------------------------------------------
 * SQLite → structure mapping
 * ---------------------------------------------------------------------- */

/**
 * @brief Fills an error_reply and generates its JSON.
 * @param err       Pre-allocated pointer provided by the caller.
 * @param code      Application-level error code.
 * @param message   Error message (external pointer, not copied).
 * @param code_http Corresponding HTTP status code.
 * @return 0, or -1 if @p err is NULL.
 * @note Calls error_reply_to_json() which allocates @c err->json.
 */
int error_reply_map(struct error_reply *err, int code, char *message,
                    int code_http);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct media.
 * @param media       Pre-allocated and initialised structure.
 * @param stmt        SQLite statement positioned on a row.
 * @param start_index Index of the first column to read.
 * @param end_index   Index of the last column (inclusive).
 * @return 0 on success, -1 on invalid arguments, 1 if a required column
 *         is missing.
 * @note Text fields (@c alternative_text, @c url) are allocated by
 *       strndup() — freed via free_media().
 */
int media_map(struct media *media, sqlite3_stmt *stmt, int start_index,
              int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct user.
 * @note Text fields (@c username, @c email, @c role, @c link) are
 *       allocated by strndup() — freed via free_user().
 */
int user_map(struct user *user, sqlite3_stmt *stmt, int start_index,
             int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct view.
 * @note The @c hashed_ip field is allocated by strndup() — freed via
 *       free_view().
 */
int view_map(struct view *view, sqlite3_stmt *stmt, int start_index,
             int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct issue.
 * @note Text fields are allocated by strndup() — freed via free_issue().
 *       Sub-structures (@c cover, @c tags, @c authors, @c sponsors) are
 *       NOT populated here — they are loaded separately.
 */
int issue_map(struct issue *issue, sqlite3_stmt *stmt, int start_index,
              int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct tag.
 * @note @c name and @c color are allocated — freed via free_tag().
 */
int tag_map(struct tag *tag, sqlite3_stmt *stmt, int start_index,
            int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct sponsor.
 * @note @c name and @c link are allocated — freed via free_sponsor().
 */
int sponsor_map(struct sponsor *sponsor, sqlite3_stmt *stmt, int start_index,
                int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct issue_author.
 */
int issue_author_map(struct issue_author *issue, sqlite3_stmt *stmt,
                     int start_index, int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct issue_sponsor.
 * @note @c sponsor_name and @c link are allocated — freed via
 *       free_issue_sponsor().
 */
int issue_sponsor_map(struct issue_sponsor *issue, sqlite3_stmt *stmt,
                      int start_index, int end_index);

/**
 * @brief Maps columns of a sqlite3_stmt into a struct issue_tag.
 * @note @c tag_name is allocated — freed via free_issue_tag().
 */
int issue_tag_map(struct issue_tag *issue, sqlite3_stmt *stmt, int start_index,
                  int end_index);


/* -------------------------------------------------------------------------
 * HTTP → structure hydration
 * ---------------------------------------------------------------------- */

/**
 * @brief Populates a struct user from the JSON body of an HTTP request.
 *
 * Reads the "username", "email", "role", "link", "isSupporter", and
 * "pictureId" fields from @p msg->body. Text fields are allocated by
 * malloc() inside the structure — freed via free_user().
 *
 * @param msg  HTTP message containing the JSON body.
 * @param user Pre-allocated and initialised structure (use user_init() first).
 */
void user_hydrate(struct mg_http_message *msg, struct user *user);

/**
 * @brief Populates a struct view from the JSON body of an HTTP request.
 * @note @c hashed_ip is allocated by malloc() — freed via free_view().
 */
void view_hydrate(struct mg_http_message *msg, struct view *view);

/**
 * @brief Populates a struct issue from the JSON body of an HTTP request.
 * @note Text fields are allocated by malloc() — freed via free_issue().
 */
void issue_hydrate(struct mg_http_message *msg, struct issue *issue);

/**
 * @brief Populates a struct tag from the JSON body of an HTTP request.
 * @note @c name and @c color are allocated by malloc() — freed via free_tag().
 */
void tag_hydrate(struct mg_http_message *msg, struct tag *tag);

/**
 * @brief Populates a struct sponsor from the JSON body of an HTTP request.
 * @note @c name and @c link are allocated by malloc() — freed via free_sponsor().
 */
void sponsor_hydrate(struct mg_http_message *msg, struct sponsor *sponsor);

/**
 * @brief Populates a struct issue_author from the JSON body of an HTTP request.
 */
void issue_author_hydrate(struct mg_http_message *msg,
                          struct issue_author *issue);

/**
 * @brief Populates a struct issue_sponsor from the JSON body of an HTTP request.
 * @note @c sponsor_name and @c link are allocated — freed via free_issue_sponsor().
 */
void issue_sponsor_hydrate(struct mg_http_message *msg,
                           struct issue_sponsor *issue);

/**
 * @brief Populates a struct issue_tag from the JSON body of an HTTP request.
 * @note @c tag_name is allocated — freed via free_issue_tag().
 */
void issue_tag_hydrate(struct mg_http_message *msg, struct issue_tag *issue);


/* -------------------------------------------------------------------------
 * Initialisation
 * ---------------------------------------------------------------------- */

/**
 * @brief Initialises all pointers of a struct user to NULL and integers to 0.
 * @param user Pre-allocated structure.
 * @return 0, or -1 if @p user is NULL.
 */
int user_init(struct user *user);

/**
 * @brief Initialises a struct view (hashed_ip = NULL, integer fields = 0).
 * @return 0, or -1 if @p view is NULL.
 */
int view_init(struct view *view);

/**
 * @brief Initialises all pointers of a struct issue to NULL and integers to 0.
 * @return 0, or -1 if @p issue is NULL.
 */
int issue_init(struct issue *issue);

/**
 * @brief Initialises a struct tag (name = NULL, color = NULL).
 * @return 0, or -1 if @p tag is NULL.
 */
int tag_init(struct tag *tag);

/**
 * @brief Initialises a struct sponsor (name = NULL, link = NULL).
 * @return 0, or -1 if @p sponsor is NULL.
 */
int sponsor_init(struct sponsor *sponsor);

/**
 * @brief Initialises a struct issue_author (all integer fields to 0).
 * @return 0, or -1 if @p issue is NULL.
 */
int issue_author_init(struct issue_author *issue);

/**
 * @brief Initialises a struct issue_sponsor (pointers NULL, integers 0).
 * @return 0, or -1 if @p issue is NULL.
 */
int issue_sponsor_init(struct issue_sponsor *issue);

/**
 * @brief Initialises a struct issue_tag (tag_name = NULL, issue_id = 0).
 * @return 0, or -1 if @p issue is NULL.
 */
int issue_tag_init(struct issue_tag *issue);
