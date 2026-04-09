#pragma once

#include <lib/mongoose.h>
#include <lib/sqlite3.h>
#include <stddef.h>
#include <structs.h>

const char *get_method(const char *buffer);
int mg_str_to_str(char *dest, struct mg_str src);
int str_to_slug(char *str, size_t len);

/** JSON */
void error_reply_to_json(struct error_reply *err);
void list_reply_to_json(struct list_reply *reply);

size_t media_to_json_len(struct media *media);
char *media_to_json(struct media *media);

size_t user_to_json_len(struct user *user);
char *user_to_json(struct user *user);
char *users_to_json(struct user **users, size_t len);

size_t view_to_json_len(struct view *view);
char *view_to_json(struct view *view);
char *views_to_json(struct view **views, size_t len);

size_t tag_to_json_len(struct tag *tag);
char *tag_to_json(struct tag *tag);
char *tags_to_json(struct tag **tags, size_t len);

size_t sponsor_to_json_len(struct sponsor *sponsor);
char *sponsor_to_json(struct sponsor *sponsor);
char *sponsors_to_json(struct sponsor **sponsors, size_t len);

size_t issue_to_json_len(struct issue *issue);
char *issue_to_json(struct issue *issue);
char *issues_to_json(struct issue **issues, size_t len);

size_t issue_author_to_json_len(struct issue_author *issue);
char *issue_author_to_json(struct issue_author *issue);
char *issue_authors_to_json(struct issue_author **issues, size_t len);

size_t issue_sponsor_to_json_len(struct issue_sponsor *issue);
char *issue_sponsor_to_json(struct issue_sponsor *issue);
char *issue_sponsors_to_json(struct issue_sponsor **issues, size_t len);

size_t issue_tag_to_json_len(struct issue_tag *issue);
char *issue_tag_to_json(struct issue_tag *issue);
char *issue_tags_to_json(struct issue_tag **issues, size_t len);

/** FREE */
int free_media(struct media *media);
int free_user(struct user *user);
int free_view(struct view *view);
int free_issue(struct issue *issue);
int free_issue_author(struct issue_author *issue);
int free_issue_sponsor(struct issue_sponsor *issue);
int free_issue_tag(struct issue_tag *issue);
int free_tag(struct tag *tag);
int free_sponsor(struct sponsor *sponsor);

int free_users(struct user **user, size_t len);
int free_views(struct view **view, size_t len);
int free_issues(struct issue **issue, size_t len);
int free_issue_authors(struct issue_author **issue, size_t len);
int free_issue_sponsors(struct issue_sponsor **issue, size_t len);
int free_issue_tags(struct issue_tag **issue, size_t len);
int free_tags(struct tag **tag, size_t len);
int free_sponsors(struct sponsor **sponsor, size_t len);

/** MAPPING */
int error_reply_map(struct error_reply *err, int code, char *message,
                    int code_http);
int media_map(struct media *media, sqlite3_stmt *stmt, int start_index,
              int end_index);
int user_map(struct user *user, sqlite3_stmt *stmt, int start_index,
             int end_index);
int view_map(struct view *view, sqlite3_stmt *stmt, int start_index,
             int end_index);
int issue_map(struct issue *issue, sqlite3_stmt *stmt, int start_index,
              int end_index);
int tag_map(struct tag *tag, sqlite3_stmt *stmt, int start_index,
            int end_index);
int sponsor_map(struct sponsor *sponsor, sqlite3_stmt *stmt, int start_index,
                int end_index);

int issue_author_map(struct issue_author *issue, sqlite3_stmt *stmt,
                     int start_index, int end_index);
int issue_sponsor_map(struct issue_sponsor *issue, sqlite3_stmt *stmt,
                      int start_index, int end_index);
int issue_tag_map(struct issue_tag *issue, sqlite3_stmt *stmt, int start_index,
                  int end_index);

/** HYDRATE */
void user_hydrate(struct mg_http_message *msg, struct user *user);
void view_hydrate(struct mg_http_message *msg, struct view *view);
void issue_hydrate(struct mg_http_message *msg, struct issue *issue);
void tag_hydrate(struct mg_http_message *msg, struct tag *tag);
void sponsor_hydrate(struct mg_http_message *msg, struct sponsor *sponsor);

void issue_author_hydrate(struct mg_http_message *msg,
                          struct issue_author *issue);
void issue_sponsor_hydrate(struct mg_http_message *msg,
                           struct issue_sponsor *issue);
void issue_tag_hydrate(struct mg_http_message *msg, struct issue_tag *issue);

/** INIT */
int media_init(struct media *media);
int user_init(struct user *user);
int view_init(struct view *view);
int issue_init(struct issue *issue);
int tag_init(struct tag *tag);
int sponsor_init(struct sponsor *sponsor);

int issue_author_init(struct issue_author *issue);
int issue_sponsor_init(struct issue_sponsor *issue);
int issue_tag_init(struct issue_tag *issue);

/** MEDIA HYDRATE */
void media_hydrate(struct mg_http_message *msg, struct media *media);
void user_totp_hydrate(struct mg_http_message *msg, struct user *user);
