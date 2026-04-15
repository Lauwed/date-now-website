#include <lib/mongoose.h>
#include <macros/colors.h>
#include <macros/utils.h>
#include <regex.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <utils.h>

#define METHODS_LEN 4

#define MAP_DOUBLE(dest, stmt, index, required)                                \
  if (sqlite3_column_type(stmt, index) == SQLITE_FLOAT) {                      \
    double d = sqlite3_column_double(stmt, index);                             \
    printf("%s: %f, ", sqlite3_column_name(stmt, index), d);                   \
    dest = d;                                                                  \
  } else if (required) {                                                       \
    return 1;                                                                  \
  }

#define MAP_TEXT(dest, stmt, index, required)                                  \
  if (sqlite3_column_type(stmt, index) == SQLITE_TEXT) {                       \
    const char *str = (const char *)sqlite3_column_text(stmt, index);          \
    printf("%s: %s, ", sqlite3_column_name(stmt, index), str);                 \
    dest = strndup(str, strlen(str));                                          \
  } else if (required) {                                                       \
    return 1;                                                                  \
  }

#define MAP_INT(dest, stmt, index, required)                                   \
  printf("type int:  %d, ", sqlite3_column_type(stmt, index));                 \
  if (sqlite3_column_type(stmt, index) == SQLITE_INTEGER) {                    \
    int integer = sqlite3_column_int(stmt, index);                             \
    printf("%s: %d, ", sqlite3_column_name(stmt, index), integer);             \
    dest = integer;                                                            \
  } else if (required) {                                                       \
    return 1;                                                                  \
  } else {                                                                     \
    dest = 0;                                                                  \
  }

#define ERROR_REPLY_JSON "{\"code\":%d,\"message\":\"%s\"}"
#define LIST_REPLY_JSON                                                        \
  "{\"data\":%s,\"count\":%d,\"total\":%d,\"totalPages\":%d}"
#define MEDIA_JSON                                                             \
  "{\"id\":%d,\"alt\":\"%s\",\"url\":\"%s\",\"width\":%f,\"height\":%f}"
#define USER_JSON                                                              \
  "{\"id\":%d,\"username\":%s,\"email\":\"%s\",\"role\":\"%s\",\"link\":%"     \
  "s,\"picture\":%s,\"subscribedAt\":%d,\"isSupporter\":%d,\"createdAt\":%d}"
#define ISSUE_JSON                                                             \
  "{\"id\":%d,\"slug\":\"%s\",\"title\":\"%s\",\"subtitle\":\"%s\",\"cover\":" \
  "%s,\"createdAt\":%d,\"publishedAt\":%d,\"updatedAt\":%d,\"issueNumber\":"   \
  "%d,\"excerpt\":\"%s\",\"content\":\"%s\",\"isSponsored\":%d,\"status\":\"%" \
  "s\",\"openedMailCount\":%d,\"tags\":%s,\"authors\":%s,\"sponsors\":%s}"
#define TAG_JSON "{\"name\":\"%s\",\"color\":\"%s\"}"
#define SPONSOR_JSON "{\"name\":\"%s\",\"link\":\"%s\"}"
#define ISSUE_AUTHOR_JSON "{\"userId\":%d,\"issueId\":%d}"
#define ISSUE_SPONSOR_JSON                                                     \
  "{\"sponsorName\":\"%s\",\"issueId\":%d,\"link\":\"%s\"}"
#define ISSUE_TAG_JSON "{\"tagName\":\"%s\",\"issueId\":%d}"
#define VIEW_JSON "{\"id\":%d,\"time\":%d,\"hashedIp\":\"%s\",\"issueId\":%d}"

const size_t NULL_SIZE = strlen("null") * sizeof(char);
const size_t DOUBLE_QUOTES_SIZE = strlen("\"\"") * sizeof(char);
const size_t COMMA_SIZE = strlen(",") * sizeof(char);

static void trim(char *str) {
  int len = strlen(str);
  while (len > 0 &&
         (str[len - 1] == ' ' || str[len - 1] == '\r' || str[len - 1] == '\n'))
    str[--len] = '\0';

  int start = 0;
  while (str[start] == ' ' || str[start] == '\r' || str[start] == '\n')
    start++;
  if (start > 0)
    memmove(str, str + start, len - start + 1);
}

// Returns
// -1 -> Email is NULL
// 1 -> Regex error
// 2 -> No match
int check_email_validity(char *email) {
  printf("CHECK EMAIL VALIDITY\tEmail: %s\n", email);

  if (email == NULL) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("EMAIL IS NULL"));
    return -1;
  }
  trim(email);

  regex_t regex;
  int rc;
  const int msgbuf_len = 100;
  char msgbuf[msgbuf_len];

  rc = regcomp(&regex, "^[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}$",
               REG_EXTENDED);
  if (rc) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULD NOT COMPILE REGEX"));
    return 1;
  }

  rc = regexec(&regex, email, 0, NULL, 0);
  if (rc == REG_NOMATCH) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("NO MATCH"));
    regfree(&regex);
    return 2;
  } else if (rc != 0) {
    regerror(rc, &regex, msgbuf, msgbuf_len);
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("REGEX MATCH FAILED: %s"), msgbuf);
    regfree(&regex);
    return 1;
  }

  regfree(&regex);
  return 0;
}

const char *get_method(const char *method_buf) {
  const char *methods[METHODS_LEN] = {"GET", "POST", "PUT", "DELETE"};

  for (int i = 0; i < METHODS_LEN; i++) {
    if (strncmp(method_buf, methods[i], strlen(methods[i])) == 0) {
      return methods[i];
    }
  }

  return NULL;
}

int mg_str_to_str(char *dest, struct mg_str src) {
  int rc = sprintf(dest, STR_FMT, (int)src.len, src.buf);

  return rc > 0 ? 0 : 1;
}

int str_to_slug(char *str, size_t len) {
  int dash_added = 0;
  int i;
  for (i = 0; i < len; i += 1) {
    char current = str[i];

    // If not letter of number
    if ((!((current >= '0' && current <= '9') ||
           (current >= 'a' && current <= 'z'))) &&
        dash_added == 0) {
      str[i] = '-';
      dash_added = 1;
    } else {
      // Check if uppercase
      if (current >= 'A' && current <= 'Z') {
        str[i] = current + 32;
      }

      dash_added = 0;
    }
  }

  return 0;
}

/** JSON PARSE UTILS */
void error_reply_to_json(struct error_reply *err) {
  size_t len = snprintf(NULL, 0, ERROR_REPLY_JSON, err->code, err->message) + 1;

  err->json = malloc(len);
  sprintf(err->json, ERROR_REPLY_JSON, err->code, err->message);
}

void list_reply_to_json(struct list_reply *reply) {
  size_t len = snprintf(NULL, 0, LIST_REPLY_JSON, reply->data, reply->count,
                        reply->total, reply->total_pages) +
               1;

  reply->json = malloc(len);
  sprintf(reply->json, LIST_REPLY_JSON, reply->data, reply->count, reply->total,
          reply->total_pages);
}

size_t media_to_json_len(struct media *media) {
  if (media == NULL) {
    return NULL_SIZE;
  }

  return snprintf(NULL, 0, MEDIA_JSON, media->id, media->alternative_text,
                  media->url, media->width, media->height) +
         1;
}

char *media_to_json(struct media *media) {
  if (media == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(media_to_json_len(media));

  int len = sprintf(json, MEDIA_JSON, media->id, media->alternative_text,
                    media->url, media->width, media->height);

  return json;
}

size_t user_to_json_len(struct user *user) {
  if (user == NULL) {
    return NULL_SIZE;
  }

  char *username = "null";
  if (user->username != NULL) {
    username = malloc(snprintf(NULL, 0, "\"%s\"", user->username) + 1);
    sprintf(username, "\"%s\"", user->username);
  }

  char *link = "null";
  if (user->link != NULL) {
    link = malloc(snprintf(NULL, 0, "\"%s\"", user->link) + 1);
    sprintf(link, "\"%s\"", user->link);
  }

  int len =
      snprintf(NULL, 0, USER_JSON, user->id, username, user->email, user->role,
               link, media_to_json(user->picture), user->subscribed_at,
               user->is_supporter, user->created_at) +
      1;

  if (strcmp(link, "null") != 0)
    free(link);

  if (strcmp(username, "null") != 0)
    free(username);

  return len;
}

char *user_to_json(struct user *user) {
  if (user == NULL) {
    return "null";
  }

  char *username = "null";
  if (user->username != NULL) {
    username = malloc(snprintf(NULL, 0, "\"%s\"", user->username) + 1);
    sprintf(username, "\"%s\"", user->username);
  }

  char *link = "null";
  if (user->link != NULL) {
    link = malloc(snprintf(NULL, 0, "\"%s\"", user->link) + 1);
    sprintf(link, "\"%s\"", user->link);
  }

  char *json = NULL;
  json = malloc(user_to_json_len(user));

  sprintf(json, USER_JSON, user->id, username, user->email, user->role, link,
          media_to_json(user->picture), user->subscribed_at, user->is_supporter,
          user->created_at);

  if (strcmp(link, "null") != 0)
    free(link);

  if (strcmp(username, "null") != 0)
    free(username);

  return json;
}

char *users_to_json(struct user **users, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += user_to_json_len(users[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *users_json = malloc(json_len + 1);
  users_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *user = user_to_json(users[i]);

    strcat(users_json, user);
    if (i < len - 1)
      strcat(users_json, ",");

    if (strcmp(user, "null"))
      free(user);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", users_json) + 1);
    sprintf(json, "[%s]", users_json);
  } else {
    json = "[]";
  }

  free(users_json);

  return json;
}

size_t view_to_json_len(struct view *view) {
  if (view == NULL) {
    return NULL_SIZE;
  }

  int len = snprintf(NULL, 0, VIEW_JSON, view->id, view->time, view->hashed_ip,
                     view->issue_id) +
            1;

  return len;
}

char *view_to_json(struct view *view) {
  if (view == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(view_to_json_len(view));

  sprintf(json, VIEW_JSON, view->id, view->time, view->hashed_ip,
          view->issue_id);

  return json;
}

char *views_to_json(struct view **views, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += view_to_json_len(views[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *views_json = malloc(json_len + 1);
  views_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *view = view_to_json(views[i]);

    strcat(views_json, view);
    if (i < len - 1)
      strcat(views_json, ",");

    if (strcmp(view, "null"))
      free(view);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", views_json) + 1);
    sprintf(json, "[%s]", views_json);
  } else {
    json = "[]";
  }

  free(views_json);

  return json;
}

size_t tag_to_json_len(struct tag *tag) {
  if (tag == NULL) {
    return NULL_SIZE;
  }

  int len = snprintf(NULL, 0, TAG_JSON, tag->name, tag->color) + 1;

  return len;
}

char *tag_to_json(struct tag *tag) {
  if (tag == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(tag_to_json_len(tag));

  sprintf(json, TAG_JSON, tag->name, tag->color);

  return json;
}

char *tags_to_json(struct tag **tags, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += tag_to_json_len(tags[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *tags_json = malloc(json_len + 1);
  tags_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *tag = tag_to_json(tags[i]);

    strcat(tags_json, tag);
    if (i < len - 1)
      strcat(tags_json, ",");

    if (strcmp(tag, "null"))
      free(tag);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", tags_json) + 1);
    sprintf(json, "[%s]", tags_json);
  } else {
    json = "[]";
  }

  free(tags_json);

  return json;
}

size_t sponsor_to_json_len(struct sponsor *sponsor) {
  if (sponsor == NULL) {
    return NULL_SIZE;
  }

  int len = snprintf(NULL, 0, SPONSOR_JSON, sponsor->name, sponsor->link) + 1;

  return len;
}

char *sponsor_to_json(struct sponsor *sponsor) {
  if (sponsor == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(sponsor_to_json_len(sponsor));

  sprintf(json, SPONSOR_JSON, sponsor->name, sponsor->link);

  return json;
}

char *sponsors_to_json(struct sponsor **sponsors, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += sponsor_to_json_len(sponsors[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *sponsors_json = malloc(json_len + 1);
  sponsors_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *sponsor = sponsor_to_json(sponsors[i]);

    strcat(sponsors_json, sponsor);
    if (i < len - 1)
      strcat(sponsors_json, ",");

    if (strcmp(sponsor, "null"))
      free(sponsor);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", sponsors_json) + 1);
    sprintf(json, "[%s]", sponsors_json);
  } else {
    json = "[]";
  }

  free(sponsors_json);

  return json;
}

size_t issue_to_json_len(struct issue *issue) {
  if (issue == NULL) {
    return NULL_SIZE;
  }

  int len =
      snprintf(NULL, 0, ISSUE_JSON, issue->id, issue->slug, issue->title,
               issue->subtitle, media_to_json(issue->cover), issue->created_at,
               issue->published_at, issue->updated_at, issue->issue_number,
               issue->excerpt, issue->content, issue->is_sponsored,
               issue->status, issue->opened_mail_count,
               issue_tags_to_json(issue->tags, issue->tags_count),
               users_to_json(issue->authors, issue->authors_count),
               issue_sponsors_to_json(issue->sponsors, issue->sponsors_count)) +
      1;

  return len;
}

char *issue_to_json(struct issue *issue) {
  if (issue == NULL) {
    return "null";
  }

  char *cover_json = media_to_json(issue->cover);
  char *tags_json = issue_tags_to_json(issue->tags, issue->tags_count);
  char *authors_json = users_to_json(issue->authors, issue->authors_count);
  char *sponsors_json = issue_sponsors_to_json(issue->sponsors, issue->sponsors_count);

  char *json = malloc(snprintf(NULL, 0, ISSUE_JSON, issue->id, issue->slug,
      issue->title, issue->subtitle, cover_json, issue->created_at,
      issue->published_at, issue->updated_at, issue->issue_number,
      issue->excerpt, issue->content, issue->is_sponsored, issue->status,
      issue->opened_mail_count, tags_json, authors_json, sponsors_json) + 1);

  sprintf(json, ISSUE_JSON, issue->id, issue->slug, issue->title,
      issue->subtitle, cover_json, issue->created_at,
      issue->published_at, issue->updated_at, issue->issue_number,
      issue->excerpt, issue->content, issue->is_sponsored, issue->status,
      issue->opened_mail_count, tags_json, authors_json, sponsors_json);

  if (issue->cover != NULL) free(cover_json);
  if (issue->tags_count > 0) free(tags_json);
  if (issue->authors_count > 0) free(authors_json);
  if (issue->sponsors_count > 0) free(sponsors_json);

  return json;
}

char *issues_to_json(struct issue **issues, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += issue_to_json_len(issues[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *issues_json = malloc(json_len + 1);
  issues_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *issue = issue_to_json(issues[i]);

    strcat(issues_json, issue);
    if (i < len - 1)
      strcat(issues_json, ",");

    if (strcmp(issue, "null"))
      free(issue);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", issues_json) + 1);
    sprintf(json, "[%s]", issues_json);
  } else {
    json = "[]";
  }

  free(issues_json);

  return json;
}

size_t issue_author_to_json_len(struct issue_author *issue) {
  if (issue == NULL) {
    return NULL_SIZE;
  }

  int len =
      snprintf(NULL, 0, ISSUE_AUTHOR_JSON, issue->user_id, issue->issue_id) + 1;

  return len;
}

char *issue_author_to_json(struct issue_author *issue) {
  if (issue == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(issue_author_to_json_len(issue));

  sprintf(json, ISSUE_AUTHOR_JSON, issue->user_id, issue->issue_id);

  return json;
}

char *issue_authors_to_json(struct issue_author **issues, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += issue_author_to_json_len(issues[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *issues_json = malloc(json_len + 1);
  issues_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *issue = issue_author_to_json(issues[i]);

    strcat(issues_json, issue);
    if (i < len - 1)
      strcat(issues_json, ",");

    if (strcmp(issue, "null"))
      free(issue);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", issues_json) + 1);
    sprintf(json, "[%s]", issues_json);
  } else {
    json = "[]";
  }

  free(issues_json);

  return json;
}

size_t issue_sponsor_to_json_len(struct issue_sponsor *issue) {
  if (issue == NULL) {
    return NULL_SIZE;
  }

  int len = snprintf(NULL, 0, ISSUE_SPONSOR_JSON, issue->sponsor_name,
                     issue->issue_id, issue->link) +
            1;

  return len;
}

char *issue_sponsor_to_json(struct issue_sponsor *issue) {
  if (issue == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(issue_sponsor_to_json_len(issue));

  sprintf(json, ISSUE_SPONSOR_JSON, issue->sponsor_name, issue->issue_id,
          issue->link);

  return json;
}

char *issue_sponsors_to_json(struct issue_sponsor **issues, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += issue_sponsor_to_json_len(issues[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *issues_json = malloc(json_len + 1);
  issues_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *issue = issue_sponsor_to_json(issues[i]);

    strcat(issues_json, issue);
    if (i < len - 1)
      strcat(issues_json, ",");

    if (strcmp(issue, "null"))
      free(issue);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", issues_json) + 1);
    sprintf(json, "[%s]", issues_json);
  } else {
    json = "[]";
  }

  free(issues_json);

  return json;
}

size_t issue_tag_to_json_len(struct issue_tag *issue) {
  if (issue == NULL) {
    return NULL_SIZE;
  }

  int len =
      snprintf(NULL, 0, ISSUE_TAG_JSON, issue->tag_name, issue->issue_id) + 1;

  return len;
}

char *issue_tag_to_json(struct issue_tag *issue) {
  if (issue == NULL) {
    return "null";
  }

  char *json = NULL;
  json = malloc(issue_tag_to_json_len(issue));

  sprintf(json, ISSUE_TAG_JSON, issue->tag_name, issue->issue_id);

  return json;
}

char *issue_tags_to_json(struct issue_tag **issues, size_t len) {
  char *json = NULL;

  size_t json_len = 0;
  for (int i = 0; i < len; i += 1) {
    json_len += issue_tag_to_json_len(issues[i]);

    if (i < len - 1) {
      json_len += COMMA_SIZE;
    }
  }

  char *issues_json = malloc(json_len + 1);
  issues_json[0] = '\0';
  for (int i = 0; i < len; i += 1) {
    char *issue = issue_tag_to_json(issues[i]);

    strcat(issues_json, issue);
    if (i < len - 1)
      strcat(issues_json, ",");

    if (strcmp(issue, "null"))
      free(issue);
  }

  if (len > 0) {
    json = malloc(snprintf(NULL, 0, "[%s]", issues_json) + 1);
    sprintf(json, "[%s]", issues_json);
  } else {
    json = "[]";
  }

  free(issues_json);

  return json;
}

/** FREE UTILS */

int free_media(struct media *media) {
  free(media->alternative_text);
  free(media->url);

  media->alternative_text = NULL;
  media->url = NULL;

  free(media);
  media = NULL;

  return 0;
}

int free_user(struct user *user) {
  free(user->username);
  free(user->email);
  free(user->role);
  free(user->link);

  if (user->picture != NULL) {
    free_media(user->picture);
  }

  user->username = NULL;
  user->email = NULL;
  user->role = NULL;
  user->link = NULL;

  free(user);
  user = NULL;

  return 0;
}

int free_view(struct view *view) {
  free(view->hashed_ip);

  view->hashed_ip = NULL;

  free(view);
  view = NULL;

  return 0;
}

int free_issue(struct issue *issue) {
  free(issue->slug);
  free(issue->title);
  free(issue->subtitle);
  free(issue->excerpt);
  free(issue->content);
  free(issue->status);

  if (issue->cover != NULL) {
    free_media(issue->cover);
  }

  if (issue->tags != NULL) {
    free_issue_tags(issue->tags, issue->tags_count);
  }
  if (issue->authors != NULL) {
    free_users(issue->authors, issue->authors_count);
  }
  if (issue->sponsors != NULL) {
    free_issue_sponsors(issue->sponsors, issue->sponsors_count);
  }

  issue->slug = NULL;
  issue->title = NULL;
  issue->subtitle = NULL;
  issue->excerpt = NULL;
  issue->content = NULL;
  issue->status = NULL;

  free(issue);
  issue = NULL;

  return 0;
}

int free_issue_author(struct issue_author *issue) {
  free(issue);
  issue = NULL;

  return 0;
}
int free_issue_sponsor(struct issue_sponsor *issue) {
  free(issue->sponsor_name);
  free(issue->link);
  issue->sponsor_name = NULL;
  issue->link = NULL;

  free(issue);
  issue = NULL;

  return 0;
}
int free_issue_tag(struct issue_tag *issue) {
  free(issue->tag_name);
  issue->tag_name = NULL;

  free(issue);
  issue = NULL;

  return 0;
}

int free_tag(struct tag *tag) {
  free(tag->name);
  free(tag->color);

  tag->name = NULL;
  tag->color = NULL;

  free(tag);
  tag = NULL;

  return 0;
}

int free_sponsor(struct sponsor *sponsor) {
  free(sponsor->name);
  free(sponsor->link);

  sponsor->name = NULL;
  sponsor->link = NULL;

  free(sponsor);
  sponsor = NULL;

  return 0;
}

int free_users(struct user **users, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (users[i] != NULL) {
      result_code = free_user(users[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(users);
  users = NULL;

  return result_code;
}

int free_views(struct view **views, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (views[i] != NULL) {
      result_code = free_view(views[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(views);
  views = NULL;

  return result_code;
}

int free_issues(struct issue **issues, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (issues[i] != NULL) {
      result_code = free_issue(issues[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(issues);
  issues = NULL;

  return result_code;
}

int free_tags(struct tag **tags, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (tags[i] != NULL) {
      result_code = free_tag(tags[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(tags);
  tags = NULL;

  return result_code;
}

int free_issue_authors(struct issue_author **issues, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (issues[i] != NULL) {
      result_code = free_issue_author(issues[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(issues);
  issues = NULL;

  return result_code;
}
int free_issue_sponsors(struct issue_sponsor **issues, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (issues[i] != NULL) {
      result_code = free_issue_sponsor(issues[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(issues);
  issues = NULL;

  return result_code;
}
int free_issue_tags(struct issue_tag **issues, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (issues[i] != NULL) {
      result_code = free_issue_tag(issues[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(issues);
  issues = NULL;

  return result_code;
}

int free_sponsors(struct sponsor **sponsors, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (sponsors[i] != NULL) {
      result_code = free_sponsor(sponsors[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(sponsors);
  sponsors = NULL;

  return result_code;
}

/** MAPPING */
int error_reply_map(struct error_reply *err, int code, char *message,
                    int code_http) {
  if (err == NULL)
    return -1;

  err->code = code;
  err->code_http = code_http;
  if (message != NULL)
    err->message = message;

  error_reply_to_json(err);

  return 0;
}

int user_map(struct user *user, sqlite3_stmt *stmt, int start_index,
             int end_index) {
  if (start_index > end_index || user == NULL || stmt == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " USER " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(user->id, stmt, start_index, 1);
  // Username
  MAP_TEXT(user->username, stmt, start_index + 1, 0);
  // Email
  MAP_TEXT(user->email, stmt, start_index + 2, 1);
  // Role
  MAP_TEXT(user->role, stmt, start_index + 3, 1);

  // Link
  MAP_TEXT(user->link, stmt, start_index + 4, 0);

  // Subscribed at
  MAP_INT(user->subscribed_at, stmt, start_index + 5, 0);
  // Is supporter
  MAP_INT(user->is_supporter, stmt, start_index + 6, 1);

  // Created at
  MAP_INT(user->created_at, stmt, start_index + 7, 1);

  return 0;
}

int view_map(struct view *view, sqlite3_stmt *stmt, int start_index,
             int end_index) {
  if (start_index > end_index || view == NULL || stmt == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " USER " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(view->id, stmt, start_index, 1);
  // Username
  MAP_INT(view->time, stmt, start_index + 1, 1);
  // Email
  MAP_TEXT(view->hashed_ip, stmt, start_index + 2, 1);
  // Role
  MAP_INT(view->issue_id, stmt, start_index + 3, 1);

  return 0;
}

int issue_map(struct issue *issue, sqlite3_stmt *stmt, int start_index,
              int end_index) {
  if (start_index > end_index || issue == NULL || stmt == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->id, stmt, start_index, 1);
  MAP_TEXT(issue->slug, stmt, start_index + 1, 1);
  MAP_TEXT(issue->title, stmt, start_index + 2, 1);
  MAP_TEXT(issue->subtitle, stmt, start_index + 3, 1);
  MAP_INT(issue->created_at, stmt, start_index + 4, 1);
  MAP_INT(issue->published_at, stmt, start_index + 5, 0);
  MAP_INT(issue->updated_at, stmt, start_index + 6, 0);
  MAP_INT(issue->issue_number, stmt, start_index + 7, 1);
  MAP_TEXT(issue->excerpt, stmt, start_index + 8, 1);
  MAP_TEXT(issue->content, stmt, start_index + 9, 1);
  MAP_INT(issue->is_sponsored, stmt, start_index + 10, 0);
  MAP_TEXT(issue->status, stmt, start_index + 11, 1);
  MAP_INT(issue->opened_mail_count, stmt, start_index + 12, 0);

  return 0;
}
int issue_author_map(struct issue_author *issue, sqlite3_stmt *stmt,
                     int start_index, int end_index) {
  if (start_index > end_index || issue == NULL || stmt == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE AUTHOR " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->issue_id, stmt, start_index, 1);
  MAP_INT(issue->user_id, stmt, start_index + 1, 1);

  return 0;
}
int issue_sponsor_map(struct issue_sponsor *issue, sqlite3_stmt *stmt,
                      int start_index, int end_index) {
  if (start_index > end_index || issue == NULL || stmt == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE SPONSOR " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->issue_id, stmt, start_index, 1);
  MAP_TEXT(issue->sponsor_name, stmt, start_index + 1, 1);
  MAP_TEXT(issue->link, stmt, start_index + 2, 1);

  return 0;
}
int issue_tag_map(struct issue_tag *issue, sqlite3_stmt *stmt, int start_index,
                  int end_index) {
  if (start_index > end_index || issue == NULL || stmt == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE TAG " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->issue_id, stmt, start_index, 1);
  MAP_TEXT(issue->tag_name, stmt, start_index + 1, 1);

  return 0;
}

int media_map(struct media *media, sqlite3_stmt *stmt, int start_index,
              int end_index) {
  if (start_index > end_index || media == NULL || stmt == NULL) {
    return -1;
  }

  int id_index = start_index;
  int alt_index = start_index + 1;
  int url_index = start_index + 2;
  int width_index = start_index + 3;
  int height_index = start_index + 4;

  MAP_INT(media->id, stmt, id_index, 1);
  MAP_TEXT(media->alternative_text, stmt, alt_index, 1);
  MAP_TEXT(media->url, stmt, url_index, 1);
  MAP_DOUBLE(media->width, stmt, width_index, 0);
  MAP_DOUBLE(media->height, stmt, height_index, 0);

  return 0;
}

int tag_map(struct tag *tag, sqlite3_stmt *stmt, int start_index,
            int end_index) {
  if (start_index > end_index || tag == NULL || stmt == NULL) {
    return -1;
  }

  int name_index = start_index;
  int color_index = start_index + 1;

  MAP_TEXT(tag->name, stmt, name_index, 1);
  MAP_TEXT(tag->color, stmt, color_index, 1);

  return 0;
}

int sponsor_map(struct sponsor *sponsor, sqlite3_stmt *stmt, int start_index,
                int end_index) {
  if (start_index > end_index || sponsor == NULL || stmt == NULL) {
    return -1;
  }

  int name_index = start_index;
  int link_index = start_index + 1;

  MAP_TEXT(sponsor->name, stmt, name_index, 1);
  MAP_TEXT(sponsor->link, stmt, link_index, 1);

  return 0;
}

/** HYDRATE */
void user_hydrate(struct mg_http_message *msg, struct user *user) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"username\"")) == 0) {
      printf("USERNAME: %.*s\n", (int)val.len, val.buf);
      user->username = malloc(val.len);
      sprintf(user->username, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"email\"")) == 0) {
      printf("EMAIL: %.*s\n", (int)val.len, val.buf);
      user->email = malloc(val.len);
      sprintf(user->email, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"role\"")) == 0) {
      printf("ROLE: %.*s\n", (int)val.len, val.buf);
      user->role = malloc(val.len);
      sprintf(user->role, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"link\"")) == 0) {
      printf("LINK: %.*s\n", (int)val.len, val.buf);
      user->link = malloc(val.len);
      sprintf(user->link, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"isSupporter\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        user->is_supporter = number;
      }
    }
  }
}

void view_hydrate(struct mg_http_message *msg, struct view *view) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"hashedIp\"")) == 0) {
      printf("HASHED IP: %.*s\n", (int)val.len, val.buf);
      view->hashed_ip = malloc(val.len);
      sprintf(view->hashed_ip, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"issueId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        view->issue_id = number;
      }
    }
  }
}

void issue_hydrate(struct mg_http_message *msg, struct issue *issue) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"title\"")) == 0) {
      printf("TITLE: %.*s\n", (int)val.len, val.buf);
      issue->title = malloc(val.len);
      sprintf(issue->title, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"slug\"")) == 0) {
      printf("SLUG: %.*s\n", (int)val.len, val.buf);
      issue->slug = malloc(val.len);
      sprintf(issue->slug, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"content\"")) == 0) {
      printf("CONTENT: %.*s\n", (int)val.len, val.buf);
      issue->content = malloc(val.len);
      sprintf(issue->content, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"subtitle\"")) == 0) {
      printf("SUBTITLE: %.*s\n", (int)val.len, val.buf);
      issue->subtitle = malloc(val.len);
      sprintf(issue->subtitle, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"status\"")) == 0) {
      printf("STATUS: %.*s\n", (int)val.len, val.buf);
      issue->status = malloc(val.len);
      sprintf(issue->status, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"excerpt\"")) == 0) {
      printf("EXCERPT: %.*s\n", (int)val.len, val.buf);
      issue->excerpt = malloc(val.len);
      sprintf(issue->excerpt, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"id\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->id = number;
      }
    } else if (mg_strcmp(key, mg_str("\"createdAt\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->created_at = number;
      }
    } else if (mg_strcmp(key, mg_str("\"publishedAt\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->published_at = number;
      }
    } else if (mg_strcmp(key, mg_str("\"updatedAt\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->updated_at = number;
      }
    } else if (mg_strcmp(key, mg_str("\"openedMailCount\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->opened_mail_count = number;
      }
    } else if (mg_strcmp(key, mg_str("\"issueNumber\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->issue_number = number;
      }
    } else if (mg_strcmp(key, mg_str("\"isSponsored\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->is_sponsored = number;
      }
    }
  }
}
void issue_author_hydrate(struct mg_http_message *msg,
                          struct issue_author *issue) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"issueId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->issue_id = number;
      }
    }
    if (mg_strcmp(key, mg_str("\"userId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->user_id = number;
      }
    }
  }
}
void issue_sponsor_hydrate(struct mg_http_message *msg,
                           struct issue_sponsor *issue) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"issueId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->issue_id = number;
      }
    }
    if (mg_strcmp(key, mg_str("\"sponsorName\"")) == 0) {
      printf("SPONSOR NAME: %.*s\n", (int)val.len, val.buf);
      issue->sponsor_name = malloc(val.len);
      sprintf(issue->sponsor_name, "%.*s", (int)val.len - 2, val.buf + 1);
    }
    if (mg_strcmp(key, mg_str("\"link\"")) == 0) {
      printf("LINK: %.*s\n", (int)val.len, val.buf);
      issue->link = malloc(val.len);
      sprintf(issue->link, "%.*s", (int)val.len - 2, val.buf + 1);
    }
  }
}
void issue_tag_hydrate(struct mg_http_message *msg, struct issue_tag *issue) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"issueId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        issue->issue_id = number;
      }
    }
    if (mg_strcmp(key, mg_str("\"tagName\"")) == 0) {
      printf("SPONSOR NAME: %.*s\n", (int)val.len, val.buf);
      issue->tag_name = malloc(val.len);
      sprintf(issue->tag_name, "%.*s", (int)val.len - 2, val.buf + 1);
    }
  }
}

void tag_hydrate(struct mg_http_message *msg, struct tag *tag) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"name\"")) == 0) {
      printf("NAME: %.*s\n", (int)val.len, val.buf);
      tag->name = malloc(val.len);
      sprintf(tag->name, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"color\"")) == 0) {
      printf("COLOR: %.*s\n", (int)val.len, val.buf);
      tag->color = malloc(val.len);
      sprintf(tag->color, "%.*s", (int)val.len - 2, val.buf + 1);
    }
  }
}

void sponsor_hydrate(struct mg_http_message *msg, struct sponsor *sponsor) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"name\"")) == 0) {
      printf("NAME: %.*s\n", (int)val.len, val.buf);
      sponsor->name = malloc(val.len);
      sprintf(sponsor->name, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"link\"")) == 0) {
      printf("LINK: %.*s\n", (int)val.len, val.buf);
      sponsor->link = malloc(val.len);
      sprintf(sponsor->link, "%.*s", (int)val.len - 2, val.buf + 1);
    }
  }
}

/** INIT */
int user_init(struct user *user) {
  if (user == NULL) {
    return -1;
  }

  user->username = NULL;
  user->email = NULL;
  user->role = NULL;

  user->link = NULL;
  user->picture = NULL;

  user->subscribed_at = 0;
  user->is_supporter = 0;

  return 0;
}

int view_init(struct view *view) {
  if (view == NULL) {
    return -1;
  }

  view->hashed_ip = NULL;

  view->time = 0;
  view->issue_id = 0;

  return 0;
}

int issue_init(struct issue *issue) {
  if (issue == NULL) {
    return -1;
  }

  issue->slug = NULL;
  issue->title = NULL;
  issue->subtitle = NULL;

  issue->excerpt = NULL;
  issue->content = NULL;
  issue->status = NULL;

  issue->published_at = 0;
  issue->updated_at = 0;

  issue->cover = NULL;

  issue->tags = NULL;
  issue->tags_count = 0;
  issue->authors = NULL;
  issue->authors_count = 0;
  issue->sponsors = NULL;
  issue->sponsors_count = 0;

  return 0;
}
int issue_author_init(struct issue_author *issue) {
  if (issue == NULL) {
    return -1;
  }
  issue->issue_id = 0;
  issue->user_id = 0;

  return 0;
}
int issue_sponsor_init(struct issue_sponsor *issue) {
  if (issue == NULL) {
    return -1;
  }
  issue->issue_id = 0;
  issue->sponsor_name = NULL;
  issue->link = NULL;

  return 0;
}
int issue_tag_init(struct issue_tag *issue) {
  if (issue == NULL) {
    return -1;
  }
  issue->issue_id = 0;
  issue->tag_name = NULL;

  return 0;
}

int tag_init(struct tag *tag) {
  if (tag == NULL) {
    return -1;
  }

  tag->name = NULL;
  tag->color = NULL;

  return 0;
}

int sponsor_init(struct sponsor *sponsor) {
  if (sponsor == NULL) {
    return -1;
  }

  sponsor->name = NULL;
  sponsor->link = NULL;

  return 0;
}
