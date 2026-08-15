/**
 * @file utils.c
 * @brief Implementation of all utility functions declared in utils.h:
 *        validation, JSON serialisation, SQLite row mapping, HTTP hydration,
 *        structure initialisation, and memory management.
 */

#include <cjson/cJSON.h>
#include <lib/mongoose.h>
#include <lib/pg.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/utils.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <utils.h>

#define METHODS_LEN 4

/* Postgres row-fetch macros: `row` is a `pg_row_t *` (result set + row
 * index), `index` is the 0-based column index. Values are always returned
 * as text by libpq (unless binary format is requested, which we don't use)
 * — parsed here into the destination's C type. */

#define MAP_DOUBLE(dest, row, index, required)                                 \
  if (!PQgetisnull((row)->res, (row)->row, index)) {                           \
    double d = atof(PQgetvalue((row)->res, (row)->row, index));                \
    printf("%s: %f, ", PQfname((row)->res, index), d);                         \
    dest = d;                                                                  \
  } else if (required) {                                                       \
    return 1;                                                                  \
  }

#define MAP_TEXT(dest, row, index, required)                                   \
  if (!PQgetisnull((row)->res, (row)->row, index)) {                           \
    const char *str = PQgetvalue((row)->res, (row)->row, index);               \
    printf("%s: %s, ", PQfname((row)->res, index), str);                       \
    dest = strndup(str, strlen(str));                                          \
  } else if (required) {                                                       \
    return 1;                                                                  \
  }

#define MAP_INT(dest, row, index, required)                                    \
  if (!PQgetisnull((row)->res, (row)->row, index)) {                           \
    int integer = atoi(PQgetvalue((row)->res, (row)->row, index));             \
    printf("%s: %d, \n", PQfname((row)->res, index), integer);                 \
    dest = integer;                                                            \
  } else if (required) {                                                       \
    return 1;                                                                  \
  } else {                                                                     \
    dest = 0;                                                                  \
  }

/* Postgres BOOLEAN columns come back from PQgetvalue() as the literal text
 * "t"/"f" — not "1"/"0" — so they need their own macro rather than
 * MAP_INT. (Binding *out* to a BOOLEAN column still accepts "1"/"0" text,
 * so no equivalent write-side macro is needed.) */
#define MAP_BOOL(dest, row, index, required)                                   \
  if (!PQgetisnull((row)->res, (row)->row, index)) {                           \
    const char *b = PQgetvalue((row)->res, (row)->row, index);                 \
    printf("%s: %s, \n", PQfname((row)->res, index), b);                       \
    dest = (b[0] == 't');                                                      \
  } else if (required) {                                                       \
    return 1;                                                                  \
  } else {                                                                     \
    dest = 0;                                                                  \
  }

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

int validate_content_blocks(struct mg_str json) {
  if (!mg_validateJSON(json)) {
    return 1;
  }

  cJSON *root = cJSON_ParseWithLength(json.buf, json.len);
  if (root == NULL) {
    return 1;
  }

  if (!cJSON_IsArray(root)) {
    cJSON_Delete(root);
    return 1;
  }

  cJSON *block = NULL;
  cJSON_ArrayForEach(block, root) {
    if (!cJSON_IsObject(block)) {
      cJSON_Delete(root);
      return 1;
    }

    cJSON *type = cJSON_GetObjectItemCaseSensitive(block, "type");
    if (!cJSON_IsString(type) || type->valuestring == NULL) {
      cJSON_Delete(root);
      return 1;
    }

    if (strcmp(type->valuestring, "text") == 0) {
      cJSON *markdown = cJSON_GetObjectItemCaseSensitive(block, "markdown");
      if (!cJSON_IsString(markdown) || markdown->valuestring == NULL) {
        cJSON_Delete(root);
        return 1;
      }
    } else if (strcmp(type->valuestring, "youtube") == 0 ||
               strcmp(type->valuestring, "tweet") == 0) {
      cJSON *url = cJSON_GetObjectItemCaseSensitive(block, "url");
      if (!cJSON_IsString(url) || url->valuestring == NULL) {
        cJSON_Delete(root);
        return 1;
      }
    } else {
      cJSON_Delete(root);
      return 1;
    }
  }

  cJSON_Delete(root);

  return 0;
}

/** JSON SERIALISATION */

void error_reply_to_json(struct error_reply *err) {
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "code", err->code);
  cJSON_AddStringToObject(obj, "message", err->message);
  err->json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
}

void list_reply_to_json(struct list_reply *reply) {
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddRawToObject(obj, "data", reply->data ? reply->data : "[]");
  cJSON_AddNumberToObject(obj, "count", reply->count);
  cJSON_AddNumberToObject(obj, "total", reply->total);
  cJSON_AddNumberToObject(obj, "totalPages", reply->total_pages);
  reply->json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
}

static cJSON *media_to_cjson(struct media *media) {
  if (media == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", media->id);
  cJSON_AddStringToObject(obj, "alt", media->alternative_text);
  cJSON_AddStringToObject(obj, "url", media->url);
  cJSON_AddNumberToObject(obj, "width", media->width);
  cJSON_AddNumberToObject(obj, "height", media->height);
  return obj;
}

char *media_to_json(struct media *media) {
  if (media == NULL)
    return "null";
  cJSON *obj = media_to_cjson(media);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *medias_to_json(struct media **medias, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, media_to_cjson(medias[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *user_to_cjson(struct user *user) {
  if (user == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", user->id);
  if (user->username != NULL)
    cJSON_AddStringToObject(obj, "username", user->username);
  else
    cJSON_AddNullToObject(obj, "username");
  cJSON_AddStringToObject(obj, "email", user->email);
  cJSON_AddStringToObject(obj, "role", user->role);
  if (user->link != NULL)
    cJSON_AddStringToObject(obj, "link", user->link);
  else
    cJSON_AddNullToObject(obj, "link");
  cJSON_AddItemToObject(obj, "picture", media_to_cjson(user->picture));
  cJSON_AddNumberToObject(obj, "subscribedAt", user->subscribed_at);

  cJSON_AddItemToObject(obj, "isSupporter",
                        cJSON_CreateBool(user->is_supporter));
  cJSON_AddNumberToObject(obj, "createdAt", user->created_at);
  cJSON_AddNumberToObject(obj, "trackerPixelConsentDate",
                          user->tracker_pixel_consent_date);
  return obj;
}

char *user_to_json(struct user *user) {
  if (user == NULL)
    return "null";
  cJSON *obj = user_to_cjson(user);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *users_to_json(struct user **users, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, user_to_cjson(users[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *view_to_cjson(struct view *view) {
  if (view == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", view->id);
  cJSON_AddNumberToObject(obj, "time", view->time);
  cJSON_AddStringToObject(obj, "hashedIp", view->hashed_ip);
  cJSON_AddNumberToObject(obj, "issueId", view->issue_id);
  return obj;
}

char *view_to_json(struct view *view) {
  if (view == NULL)
    return "null";
  cJSON *obj = view_to_cjson(view);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *views_to_json(struct view **views, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, view_to_cjson(views[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *tag_to_cjson(struct tag *tag) {
  if (tag == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "name", tag->name);
  cJSON_AddStringToObject(obj, "color", tag->color);
  return obj;
}

char *tag_to_json(struct tag *tag) {
  if (tag == NULL)
    return "null";
  cJSON *obj = tag_to_cjson(tag);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *tags_to_json(struct tag **tags, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, tag_to_cjson(tags[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *feed_to_cjson(struct feed *feed) {
  if (feed == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", feed->id);
  cJSON_AddStringToObject(obj, "name", feed->name);
  cJSON_AddStringToObject(obj, "link", feed->link);
  cJSON_AddItemToObject(obj, "isRssFeed", cJSON_CreateBool(feed->is_rss_feed));
  return obj;
}

char *feed_to_json(struct feed *feed) {
  if (feed == NULL)
    return "null";
  cJSON *obj = feed_to_cjson(feed);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *feeds_to_json(struct feed **feeds, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, feed_to_cjson(feeds[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *feed_tag_to_cjson(struct feed_tag *ft) {
  if (ft == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "feedId", ft->feed_id);
  cJSON_AddStringToObject(obj, "tagName", ft->tag_name);
  return obj;
}

char *feed_tag_to_json(struct feed_tag *feed_tag) {
  if (feed_tag == NULL)
    return "null";
  cJSON *obj = feed_tag_to_cjson(feed_tag);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *feed_tags_to_json(struct feed_tag **feed_tags, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, feed_tag_to_cjson(feed_tags[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *category_to_cjson(struct category *category) {
  if (category == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "name", category->name);
  cJSON_AddStringToObject(obj, "color", category->color);
  return obj;
}

char *category_to_json(struct category *category) {
  if (category == NULL)
    return "null";
  cJSON *obj = category_to_cjson(category);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *categories_to_json(struct category **categories, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, category_to_cjson(categories[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *article_to_cjson(struct article *article) {
  if (article == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", article->id);
  cJSON_AddNumberToObject(obj, "sectionId", article->section_id);
  cJSON_AddNumberToObject(obj, "position", article->position);
  cJSON_AddStringToObject(obj, "title", article->title);
  cJSON_AddStringToObject(obj, "sourceName", article->source_name);
  cJSON_AddStringToObject(obj, "sourceUrl", article->source_url);
  cJSON_AddRawToObject(obj, "summary", article->summary);
  return obj;
}

char *article_to_json(struct article *article) {
  if (article == NULL)
    return "null";
  cJSON *obj = article_to_cjson(article);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *articles_to_json(struct article **articles, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, article_to_cjson(articles[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *issue_section_to_cjson(struct issue_section *section) {
  if (section == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", section->id);
  cJSON_AddNumberToObject(obj, "issueId", section->issue_id);
  cJSON_AddNumberToObject(obj, "position", section->position);
  cJSON_AddStringToObject(obj, "type", section->type);
  if (section->category_name != NULL)
    cJSON_AddStringToObject(obj, "categoryName", section->category_name);
  else
    cJSON_AddNullToObject(obj, "categoryName");
  if (section->text_body != NULL)
    cJSON_AddRawToObject(obj, "textBody", section->text_body);
  else
    cJSON_AddNullToObject(obj, "textBody");
  cJSON *articles_arr = cJSON_CreateArray();
  for (size_t i = 0; i < section->articles_count; i++)
    cJSON_AddItemToArray(articles_arr, article_to_cjson(section->articles[i]));
  cJSON_AddItemToObject(obj, "articles", articles_arr);
  return obj;
}

char *issue_section_to_json(struct issue_section *section) {
  if (section == NULL)
    return "null";
  cJSON *obj = issue_section_to_cjson(section);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *issue_sections_to_json(struct issue_section **sections, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, issue_section_to_cjson(sections[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *sponsor_to_cjson(struct sponsor *sponsor) {
  if (sponsor == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "name", sponsor->name);
  cJSON_AddStringToObject(obj, "link", sponsor->link);
  return obj;
}

char *sponsor_to_json(struct sponsor *sponsor) {
  if (sponsor == NULL)
    return "null";
  cJSON *obj = sponsor_to_cjson(sponsor);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *sponsors_to_json(struct sponsor **sponsors, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, sponsor_to_cjson(sponsors[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *issue_tag_to_cjson(struct issue_tag *it) {
  if (it == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "tagName", it->tag_name);
  cJSON_AddNumberToObject(obj, "issueId", it->issue_id);
  return obj;
}

char *issue_tag_to_json(struct issue_tag *it) {
  if (it == NULL)
    return "null";
  cJSON *obj = issue_tag_to_cjson(it);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *issue_tags_to_json(struct issue_tag **its, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, issue_tag_to_cjson(its[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *issue_author_to_cjson(struct issue_author *ia) {
  if (ia == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "userId", ia->user_id);
  cJSON_AddNumberToObject(obj, "issueId", ia->issue_id);
  return obj;
}

char *issue_author_to_json(struct issue_author *ia) {
  if (ia == NULL)
    return "null";
  cJSON *obj = issue_author_to_cjson(ia);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *issue_authors_to_json(struct issue_author **ias, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, issue_author_to_cjson(ias[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *issue_sponsor_to_cjson(struct issue_sponsor *is) {
  if (is == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "sponsorName", is->sponsor_name);
  cJSON_AddNumberToObject(obj, "issueId", is->issue_id);
  cJSON_AddStringToObject(obj, "issueLink", is->issue_link);
  cJSON_AddStringToObject(obj, "link", is->link);
  return obj;
}

char *issue_sponsor_to_json(struct issue_sponsor *is) {
  if (is == NULL)
    return "null";
  cJSON *obj = issue_sponsor_to_cjson(is);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *issue_sponsors_to_json(struct issue_sponsor **iss, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, issue_sponsor_to_cjson(iss[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return json;
}

static cJSON *issue_to_cjson(struct issue *issue) {
  if (issue == NULL)
    return cJSON_CreateNull();
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", issue->id);
  cJSON_AddStringToObject(obj, "slug", issue->slug);
  cJSON_AddStringToObject(obj, "title", issue->title);
  cJSON_AddStringToObject(obj, "subtitle", issue->subtitle);
  cJSON_AddItemToObject(obj, "cover", media_to_cjson(issue->cover));
  cJSON_AddNumberToObject(obj, "createdAt", issue->created_at);
  cJSON_AddNumberToObject(obj, "publishedAt", issue->published_at);
  cJSON_AddNumberToObject(obj, "updatedAt", issue->updated_at);
  cJSON_AddNumberToObject(obj, "issueNumber", issue->issue_number);
  cJSON_AddNumberToObject(obj, "views", issue->views);
  cJSON_AddStringToObject(obj, "excerpt", issue->excerpt);
  cJSON_AddItemToObject(obj, "isSponsored",
                        cJSON_CreateBool(issue->is_sponsored));
  cJSON_AddStringToObject(obj, "status", issue->status);
  cJSON_AddNumberToObject(obj, "openedMailCount", issue->opened_mail_count);

  cJSON *tags_arr = cJSON_CreateArray();
  for (size_t i = 0; i < issue->tags_count; i++)
    cJSON_AddItemToArray(tags_arr, tag_to_cjson(issue->tags[i]));
  cJSON_AddItemToObject(obj, "tags", tags_arr);

  cJSON *authors_arr = cJSON_CreateArray();
  for (size_t i = 0; i < issue->authors_count; i++)
    cJSON_AddItemToArray(authors_arr, user_to_cjson(issue->authors[i]));
  cJSON_AddItemToObject(obj, "authors", authors_arr);

  cJSON *sponsors_arr = cJSON_CreateArray();
  for (size_t i = 0; i < issue->sponsors_count; i++)
    cJSON_AddItemToArray(sponsors_arr,
                         issue_sponsor_to_cjson(issue->sponsors[i]));
  cJSON_AddItemToObject(obj, "sponsors", sponsors_arr);

  cJSON *sections_arr = cJSON_CreateArray();
  for (size_t i = 0; i < issue->sections_count; i++)
    cJSON_AddItemToArray(sections_arr,
                         issue_section_to_cjson(issue->sections[i]));
  cJSON_AddItemToObject(obj, "sections", sections_arr);

  return obj;
}

char *issue_to_json(struct issue *issue) {
  if (issue == NULL)
    return "null";
  cJSON *obj = issue_to_cjson(issue);
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  return json;
}

char *issues_to_json(struct issue **issues, size_t len) {
  if (len == 0)
    return "[]";
  cJSON *arr = cJSON_CreateArray();
  for (size_t i = 0; i < len; i++)
    cJSON_AddItemToArray(arr, issue_to_cjson(issues[i]));
  char *json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
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
  free(issue->status);

  if (issue->cover != NULL) {
    free_media(issue->cover);
  }

  if (issue->tags != NULL) {
    free_tags(issue->tags, issue->tags_count);
  }
  if (issue->authors != NULL) {
    free_users(issue->authors, issue->authors_count);
  }
  if (issue->sponsors != NULL) {
    free_issue_sponsors(issue->sponsors, issue->sponsors_count);
  }
  if (issue->sections != NULL) {
    free_issue_sections(issue->sections, issue->sections_count);
  }

  issue->slug = NULL;
  issue->title = NULL;
  issue->subtitle = NULL;
  issue->excerpt = NULL;
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
  free(issue->issue_link);
  free(issue->link);
  issue->sponsor_name = NULL;
  issue->issue_link = NULL;
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

int free_feed(struct feed *feed) {
  free(feed->name);
  free(feed->link);

  feed->name = NULL;
  feed->link = NULL;

  free(feed);
  feed = NULL;

  return 0;
}

int free_feed_tag(struct feed_tag *feed_tag) {
  free(feed_tag->tag_name);
  feed_tag->tag_name = NULL;

  free(feed_tag);
  feed_tag = NULL;

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

int free_category(struct category *category) {
  free(category->name);
  free(category->color);

  category->name = NULL;
  category->color = NULL;

  free(category);
  category = NULL;

  return 0;
}

int free_article(struct article *article) {
  free(article->title);
  free(article->source_name);
  free(article->source_url);
  free(article->summary);

  article->title = NULL;
  article->source_name = NULL;
  article->source_url = NULL;
  article->summary = NULL;

  free(article);
  article = NULL;

  return 0;
}

int free_issue_section(struct issue_section *section) {
  free(section->type);
  free(section->category_name);
  free(section->text_body);

  section->type = NULL;
  section->category_name = NULL;
  section->text_body = NULL;

  if (section->articles != NULL) {
    free_articles(section->articles, section->articles_count);
  }
  section->articles = NULL;
  section->articles_count = 0;

  free(section);
  section = NULL;

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

int free_feeds(struct feed **feeds, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (feeds[i] != NULL) {
      result_code = free_feed(feeds[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(feeds);
  feeds = NULL;

  return result_code;
}

int free_feed_tags(struct feed_tag **feed_tags, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (feed_tags[i] != NULL) {
      result_code = free_feed_tag(feed_tags[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(feed_tags);
  feed_tags = NULL;

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

int free_categories(struct category **categories, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (categories[i] != NULL) {
      result_code = free_category(categories[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(categories);
  categories = NULL;

  return result_code;
}

int free_articles(struct article **articles, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (articles[i] != NULL) {
      result_code = free_article(articles[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(articles);
  articles = NULL;

  return result_code;
}

int free_issue_sections(struct issue_section **sections, size_t len) {
  int result_code = 0;
  for (int i = 0; i < len; i += 1) {
    if (sections[i] != NULL) {
      result_code = free_issue_section(sections[i]);

      if (result_code != 0) {
        return result_code;
      }
    }
  }

  free(sections);
  sections = NULL;

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

int user_map(struct user *user, pg_row_t *row, int start_index,
             int end_index) {
  if (start_index > end_index || user == NULL || row == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " USER " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(user->id, row, start_index, 1);
  // Username
  MAP_TEXT(user->username, row, start_index + 1, 0);
  // Email
  MAP_TEXT(user->email, row, start_index + 2, 1);
  // Role
  MAP_TEXT(user->role, row, start_index + 3, 1);

  // Link
  MAP_TEXT(user->link, row, start_index + 4, 0);

  // Subscribed at
  MAP_INT(user->subscribed_at, row, start_index + 5, 0);
  // Is supporter
  MAP_BOOL(user->is_supporter, row, start_index + 6, 1);

  // Created at
  MAP_INT(user->created_at, row, start_index + 7, 1);

  // Tracker consent
  MAP_INT(user->tracker_pixel_consent_date, row, start_index + 8, 0);

  return 0;
}

int view_map(struct view *view, pg_row_t *row, int start_index,
             int end_index) {
  if (start_index > end_index || view == NULL || row == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " USER " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(view->id, row, start_index, 1);
  // Username
  MAP_INT(view->time, row, start_index + 1, 1);
  // Email
  MAP_TEXT(view->hashed_ip, row, start_index + 2, 1);
  // Role
  MAP_INT(view->issue_id, row, start_index + 3, 1);

  return 0;
}

int issue_map(struct issue *issue, pg_row_t *row, int start_index,
              int end_index) {
  if (start_index > end_index || issue == NULL || row == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->id, row, start_index, 1);
  MAP_TEXT(issue->slug, row, start_index + 1, 1);
  MAP_TEXT(issue->title, row, start_index + 2, 1);
  MAP_TEXT(issue->subtitle, row, start_index + 3, 1);
  MAP_INT(issue->created_at, row, start_index + 4, 1);
  MAP_INT(issue->published_at, row, start_index + 5, 0);
  MAP_INT(issue->updated_at, row, start_index + 6, 0);
  MAP_INT(issue->issue_number, row, start_index + 7, 1);
  MAP_TEXT(issue->excerpt, row, start_index + 8, 1);
  MAP_BOOL(issue->is_sponsored, row, start_index + 9, 0);
  MAP_TEXT(issue->status, row, start_index + 10, 1);
  MAP_INT(issue->opened_mail_count, row, start_index + 11, 0);
  MAP_INT(issue->views, row, start_index + 12, 0);
  printf("\n");

  return 0;
}
int issue_author_map(struct issue_author *issue, pg_row_t *row,
                     int start_index, int end_index) {
  if (start_index > end_index || issue == NULL || row == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE AUTHOR " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->issue_id, row, start_index, 1);
  MAP_INT(issue->user_id, row, start_index + 1, 1);

  return 0;
}
int issue_sponsor_map(struct issue_sponsor *issue, pg_row_t *row,
                      int start_index, int end_index) {
  if (start_index > end_index || issue == NULL || row == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE SPONSOR " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->issue_id, row, start_index, 1);
  MAP_TEXT(issue->sponsor_name, row, start_index + 1, 1);
  MAP_TEXT(issue->issue_link, row, start_index + 2, 1);
  MAP_TEXT(issue->link, row, start_index + 3, 1);

  return 0;
}
int issue_tag_map(struct issue_tag *issue, pg_row_t *row, int start_index,
                  int end_index) {
  if (start_index > end_index || issue == NULL || row == NULL) {
    return -1;
  }

  printf(ANSI_BACKGROUND_AMBER " ISSUE TAG " ANSI_RESET_ALL "\n");
  // ID
  MAP_INT(issue->issue_id, row, start_index, 1);
  MAP_TEXT(issue->tag_name, row, start_index + 1, 1);

  return 0;
}

int media_map(struct media *media, pg_row_t *row, int start_index,
              int end_index) {
  if (start_index > end_index || media == NULL || row == NULL) {
    return -1;
  }

  int id_index = start_index;
  int alt_index = start_index + 1;
  int url_index = start_index + 2;
  int width_index = start_index + 3;
  int height_index = start_index + 4;

  MAP_INT(media->id, row, id_index, 1);
  MAP_TEXT(media->alternative_text, row, alt_index, 1);
  MAP_TEXT(media->url, row, url_index, 1);
  MAP_DOUBLE(media->width, row, width_index, 0);
  MAP_DOUBLE(media->height, row, height_index, 0);

  return 0;
}

int feed_map(struct feed *feed, pg_row_t *row, int start_index,
            int end_index) {
  if (start_index > end_index || feed == NULL || row == NULL) {
    return -1;
  }

  MAP_INT(feed->id, row, start_index, 1);
  MAP_TEXT(feed->name, row, start_index + 1, 1);
  MAP_TEXT(feed->link, row, start_index + 2, 1);
  MAP_BOOL(feed->is_rss_feed, row, start_index + 3, 1);

  return 0;
}

int feed_tag_map(struct feed_tag *feed_tag, pg_row_t *row,
                 int start_index, int end_index) {
  if (start_index > end_index || feed_tag == NULL || row == NULL) {
    return -1;
  }

  MAP_INT(feed_tag->feed_id, row, start_index, 1);
  MAP_TEXT(feed_tag->tag_name, row, start_index + 1, 1);

  return 0;
}

int category_map(struct category *category, pg_row_t *row,
                 int start_index, int end_index) {
  if (start_index > end_index || category == NULL || row == NULL) {
    return -1;
  }

  MAP_TEXT(category->name, row, start_index, 1);
  MAP_TEXT(category->color, row, start_index + 1, 1);

  return 0;
}

int article_map(struct article *article, pg_row_t *row, int start_index,
                int end_index) {
  if (start_index > end_index || article == NULL || row == NULL) {
    return -1;
  }

  MAP_INT(article->id, row, start_index, 1);
  MAP_INT(article->section_id, row, start_index + 1, 1);
  MAP_INT(article->position, row, start_index + 2, 1);
  MAP_TEXT(article->title, row, start_index + 3, 1);
  MAP_TEXT(article->source_name, row, start_index + 4, 1);
  MAP_TEXT(article->source_url, row, start_index + 5, 1);
  MAP_TEXT(article->summary, row, start_index + 6, 1);

  return 0;
}

int issue_section_map(struct issue_section *section, pg_row_t *row,
                      int start_index, int end_index) {
  if (start_index > end_index || section == NULL || row == NULL) {
    return -1;
  }

  MAP_INT(section->id, row, start_index, 1);
  MAP_INT(section->issue_id, row, start_index + 1, 1);
  MAP_INT(section->position, row, start_index + 2, 1);
  MAP_TEXT(section->type, row, start_index + 3, 1);
  MAP_TEXT(section->category_name, row, start_index + 4, 0);
  MAP_TEXT(section->text_body, row, start_index + 5, 0);

  return 0;
}

int tag_map(struct tag *tag, pg_row_t *row, int start_index,
            int end_index) {
  if (start_index > end_index || tag == NULL || row == NULL) {
    return -1;
  }

  int name_index = start_index;
  int color_index = start_index + 1;

  MAP_TEXT(tag->name, row, name_index, 1);
  MAP_TEXT(tag->color, row, color_index, 1);

  return 0;
}

int sponsor_map(struct sponsor *sponsor, pg_row_t *row, int start_index,
                int end_index) {
  if (start_index > end_index || sponsor == NULL || row == NULL) {
    return -1;
  }

  int name_index = start_index;
  int link_index = start_index + 1;

  MAP_TEXT(sponsor->name, row, name_index, 1);
  MAP_TEXT(sponsor->link, row, link_index, 1);

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
    } else if (mg_strcmp(key, mg_str("\"pictureId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed && number > 0) {
        if (user->picture == NULL) {
          user->picture = malloc(sizeof(struct media));
          user->picture->alternative_text = NULL;
          user->picture->url = NULL;
          user->picture->width = 0;
          user->picture->height = 0;
        }
        user->picture->id = number;
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
      bool value = false;
      if (mg_json_get_bool(val, "$", &value)) {
        issue->is_sponsored = value;
      }
    } else if (mg_strcmp(key, mg_str("\"coverId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed && number > 0) {
        if (issue->cover == NULL) {
          issue->cover = malloc(sizeof(struct media));
          issue->cover->alternative_text = NULL;
          issue->cover->url = NULL;
          issue->cover->width = 0;
          issue->cover->height = 0;
        }
        issue->cover->id = number;
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

void feed_hydrate(struct mg_http_message *msg, struct feed *feed) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"name\"")) == 0) {
      printf("NAME: %.*s\n", (int)val.len, val.buf);
      feed->name = malloc(val.len);
      sprintf(feed->name, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"link\"")) == 0) {
      printf("LINK: %.*s\n", (int)val.len, val.buf);
      feed->link = malloc(val.len);
      sprintf(feed->link, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"isRssFeed\"")) == 0) {
      if (mg_strcmp(val, mg_str("true")) == 0) {
        feed->is_rss_feed = 1;
      } else if (mg_strcmp(val, mg_str("false")) == 0) {
        feed->is_rss_feed = 0;
      }
    }
  }
}

void feed_tag_hydrate(struct mg_http_message *msg,
                      struct feed_tag *feed_tag) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"feedId\"")) == 0) {
      number_parsed = mg_str_to_num(val, 10, &number, sizeof(int));
      if (number_parsed) {
        feed_tag->feed_id = number;
      }
    }
    if (mg_strcmp(key, mg_str("\"tagName\"")) == 0) {
      printf("TAG NAME: %.*s\n", (int)val.len, val.buf);
      feed_tag->tag_name = malloc(val.len);
      sprintf(feed_tag->tag_name, "%.*s", (int)val.len - 2, val.buf + 1);
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

void category_hydrate(struct mg_http_message *msg, struct category *category) {
  struct mg_str key, val;
  int number;
  bool number_parsed;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"name\"")) == 0) {
      printf("NAME: %.*s\n", (int)val.len, val.buf);
      category->name = malloc(val.len);
      sprintf(category->name, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"color\"")) == 0) {
      printf("COLOR: %.*s\n", (int)val.len, val.buf);
      category->color = malloc(val.len);
      sprintf(category->color, "%.*s", (int)val.len - 2, val.buf + 1);
    }
  }
}

void article_hydrate(struct mg_http_message *msg, struct article *article) {
  struct mg_str key, val;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"title\"")) == 0) {
      printf("TITLE: %.*s\n", (int)val.len, val.buf);
      article->title = malloc(val.len);
      sprintf(article->title, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"sourceName\"")) == 0) {
      printf("SOURCE NAME: %.*s\n", (int)val.len, val.buf);
      article->source_name = malloc(val.len);
      sprintf(article->source_name, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"sourceUrl\"")) == 0) {
      printf("SOURCE URL: %.*s\n", (int)val.len, val.buf);
      article->source_url = malloc(val.len);
      sprintf(article->source_url, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"summary\"")) == 0) {
      // Raw JSON array — copied verbatim, not quote-stripped. Already
      // validated by validate_content_blocks() before this call.
      printf("SUMMARY: %.*s\n", (int)val.len, val.buf);
      article->summary = malloc(val.len + 1);
      snprintf(article->summary, val.len + 1, "%.*s", (int)val.len, val.buf);
    }
  }
}

void issue_section_hydrate(struct mg_http_message *msg,
                           struct issue_section *section) {
  struct mg_str key, val;

  size_t ofs = 0;
  while ((ofs = mg_json_next(msg->body, ofs, &key, &val)) > 0) {
    printf("%.*s -> %.*s\n", (int)key.len, key.buf, (int)val.len, val.buf);

    if (mg_strcmp(key, mg_str("\"type\"")) == 0) {
      printf("TYPE: %.*s\n", (int)val.len, val.buf);
      section->type = malloc(val.len);
      sprintf(section->type, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"categoryName\"")) == 0) {
      printf("CATEGORY NAME: %.*s\n", (int)val.len, val.buf);
      section->category_name = malloc(val.len);
      sprintf(section->category_name, "%.*s", (int)val.len - 2, val.buf + 1);
    } else if (mg_strcmp(key, mg_str("\"textBody\"")) == 0) {
      // Raw JSON array — copied verbatim, not quote-stripped. Already
      // validated by validate_content_blocks() before this call.
      printf("TEXT BODY: %.*s\n", (int)val.len, val.buf);
      section->text_body = malloc(val.len + 1);
      snprintf(section->text_body, val.len + 1, "%.*s", (int)val.len, val.buf);
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
  user->tracker_pixel_consent_date = 0;

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
  issue->status = NULL;

  issue->published_at = 0;
  issue->updated_at = 0;
  issue->views = 0;

  issue->cover = NULL;

  issue->tags = NULL;
  issue->tags_count = 0;
  issue->authors = NULL;
  issue->authors_count = 0;
  issue->sponsors = NULL;
  issue->sponsors_count = 0;
  issue->sections = NULL;
  issue->sections_count = 0;

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
  issue->issue_link = NULL;
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

int feed_init(struct feed *feed) {
  if (feed == NULL) {
    return -1;
  }

  feed->id = 0;
  feed->name = NULL;
  feed->link = NULL;
  feed->is_rss_feed = 0;

  return 0;
}

int feed_tag_init(struct feed_tag *feed_tag) {
  if (feed_tag == NULL) {
    return -1;
  }

  feed_tag->feed_id = 0;
  feed_tag->tag_name = NULL;

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

int category_init(struct category *category) {
  if (category == NULL) {
    return -1;
  }

  category->name = NULL;
  category->color = NULL;

  return 0;
}

int article_init(struct article *article) {
  if (article == NULL) {
    return -1;
  }

  article->id = 0;
  article->section_id = 0;
  article->position = 0;
  article->title = NULL;
  article->source_name = NULL;
  article->source_url = NULL;
  article->summary = NULL;

  return 0;
}

int issue_section_init(struct issue_section *section) {
  if (section == NULL) {
    return -1;
  }

  section->id = 0;
  section->issue_id = 0;
  section->position = 0;
  section->type = NULL;
  section->category_name = NULL;
  section->text_body = NULL;
  section->articles = NULL;
  section->articles_count = 0;

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
