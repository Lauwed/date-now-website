#pragma once

#include <stdlib.h>

/* Forward declarations for issue relations */
struct issue_author;
struct issue_tag;
struct issue_sponsor;

struct error_reply {
  int code;
  int code_http;
  char *message;
  char *json;
};

struct list_reply {
  char *data;
  int count;
  int total;
  size_t page_size;
  int page;
  int total_pages;
  char *json;
};

struct media {
  unsigned id;
  char *alternative_text;
  char *url;
  double width;
  double height;
};

struct user {
  int id;
  char *username;
  char *email;
  char *role;

  char totp_seed[255];

  struct media *picture;
  char *link;

  int subscribed_at;
  int is_supporter;

  int created_at;
};

struct tag {
  char *name;
  char *color;
};

struct sponsor {
  char *name;
  char *link;
};

struct issue {
  int id;
  char *slug;
  char *title;
  char *subtitle;
  struct media *cover;
  int created_at;
  int published_at;
  int updated_at;
  int issue_number;
  char *excerpt;
  char *content;
  int is_sponsored;
  char *status;
  int opened_mail_count;

  /* Relations (populated on demand) */
  struct issue_author **authors;
  int authors_count;
  struct issue_tag **tags;
  int tags_count;
  struct issue_sponsor **sponsors;
  int sponsors_count;
};

struct issue_author {
  int user_id;
  int issue_id;
};

struct issue_tag {
  char *tag_name;
  int issue_id;
};

struct issue_sponsor {
  char *sponsor_name;
  int issue_id;
  char *link;
};

struct view {
  int id;
  int time;
  char *hashed_ip;
  int issue_id;
};

struct auth_session {
  int id;
  int user_id;
  char *token_hash;
  int created_at;
  int expires_at;
};
