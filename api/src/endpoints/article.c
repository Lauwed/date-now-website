/**
 * @file article.c
 * @brief Article endpoint handler implementations (list, single resource,
 *        reorder), nested under an issue's category section.
 */

#include <cjson/cJSON.h>
#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/article.h>
#include <sql/issue.h>
#include <sql/issue_section.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <utils.h>

/* Checks issue/section existence and that the section is a CATEGORY section.
 * Sends the appropriate error reply and returns 0 on failure, 1 on success. */
static int check_section(int issue_id, int section_id,
                         struct error_reply *error_reply,
                         struct mg_connection *c) {
  int exists = issue_exists(issue_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return 0;
  }
  exists = issue_section_exists(issue_id, section_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("SECTION NOT FOUND"));
    return 0;
  }

  struct issue_section section;
  int query_code = get_issue_section(&section, issue_id, section_id);
  if (query_code != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SECTION"));
    HANDLE_QUERY_CODE;
    return 0;
  }

  bool is_category = strcmp(section.type, "CATEGORY") == 0;
  free(section.type);
  free(section.category_name);
  free(section.text_body);

  if (!is_category) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("SECTION IS NOT A CATEGORY SECTION"));
    return 0;
  }

  return 1;
}

void send_articles_res(struct mg_connection *c, struct mg_http_message *msg,
                       int issue_id, int section_id,
                       struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (!check_section(issue_id, section_id, error_reply, c)) {
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ARTICLE LIST ==="));

    int total = get_articles_len(section_id);
    if (total < 0) {
      query_code = total;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ARTICLES"));
      HANDLE_QUERY_CODE;
      return;
    }

    struct article **articles = NULL;
    if (total > 0) {
      articles = malloc((size_t)total * sizeof(struct article *));
      query_code = get_articles((size_t)total, articles, section_id);
      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ARTICLES"));
        HANDLE_QUERY_CODE;
        free(articles);
        return;
      }
    }

    struct list_reply *reply = malloc(sizeof(struct list_reply));
    reply->page = -1;
    reply->page_size = 0;
    reply->total = reply->count = total;
    reply->total_pages = 0;
    reply->json = NULL;
    reply->data = articles_to_json(articles, (size_t)total);
    list_reply_to_json(reply);

    SUCCESS_REPLY_200(reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ARTICLES SUCCESSFULLY SENT ==="));

    if (total > 0) {
      free_articles(articles, (size_t)total);
      free(reply->data);
    }
    free(reply->json);
    free(reply);
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    int offset, length;

    REQUIRED_BODY_PROPERTY("title", TITLE_REQUIRED_MESSAGE);
    REQUIRED_BODY_PROPERTY("sourceName", SOURCE_NAME_REQUIRED_MESSAGE);
    REQUIRED_BODY_PROPERTY("sourceUrl", SOURCE_URL_REQUIRED_MESSAGE);

    int summary_offset, summary_len;
    summary_offset = mg_json_get(msg->body, "$.summary", &summary_len);
    if (summary_offset < 0) {
      ERROR_REPLY_400(SUMMARY_REQUIRED_MESSAGE);
      return;
    }
    struct mg_str summary_raw =
        mg_str_n(msg->body.buf + summary_offset, (size_t)summary_len);
    if (validate_content_blocks(summary_raw) != 0) {
      ERROR_REPLY_400(CONTENT_BLOCKS_INVALID_MESSAGE);
      return;
    }

    struct article *article = malloc(sizeof(struct article));
    int init_rc = article_init(article);
    if (init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ARTICLE IS NULL"));
      return;
    }

    article->section_id = section_id;
    article_hydrate(msg, article);

    query_code = add_article(article);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING ARTICLE"));
      HANDLE_QUERY_CODE;
      free_article(article);
      return;
    }

    char *result = article_to_json(article);
    SUCCESS_REPLY_201(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ARTICLE SUCCESSFULLY ADDED ==="));

    free_article(article);
  } else {
    ERROR_REPLY_405;
  }
}

void send_article_res(struct mg_connection *c, struct mg_http_message *msg,
                      int issue_id, int section_id, int article_id,
                      struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (!check_section(issue_id, section_id, error_reply, c)) {
    return;
  }

  int exists = article_exists(section_id, article_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ARTICLE NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ARTICLE ==="));

    struct article *article = malloc(sizeof(struct article));
    query_code = get_article(article, section_id, article_id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ARTICLE"));
      HANDLE_QUERY_CODE;
      return;
    }

    char *result = article_to_json(article);
    SUCCESS_REPLY_200(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ARTICLE SUCCESSFULLY SENT ==="));

    free_article(article);
  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    if (msg->body.len <= 0) {
      ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
      return;
    } else if (!mg_validateJSON(msg->body)) {
      ERROR_REPLY_400(JSON_ERROR_MESSAGE);
      return;
    }

    int offset, length;

    REQUIRED_BODY_PROPERTY("title", TITLE_REQUIRED_MESSAGE);
    REQUIRED_BODY_PROPERTY("sourceName", SOURCE_NAME_REQUIRED_MESSAGE);
    REQUIRED_BODY_PROPERTY("sourceUrl", SOURCE_URL_REQUIRED_MESSAGE);

    int summary_offset, summary_len;
    summary_offset = mg_json_get(msg->body, "$.summary", &summary_len);
    if (summary_offset < 0) {
      ERROR_REPLY_400(SUMMARY_REQUIRED_MESSAGE);
      return;
    }
    struct mg_str summary_raw =
        mg_str_n(msg->body.buf + summary_offset, (size_t)summary_len);
    if (validate_content_blocks(summary_raw) != 0) {
      ERROR_REPLY_400(CONTENT_BLOCKS_INVALID_MESSAGE);
      return;
    }

    struct article *article = malloc(sizeof(struct article));
    query_code = get_article(article, section_id, article_id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING ARTICLE"));
      HANDLE_QUERY_CODE;
      free(article);
      return;
    }

    free(article->title);
    free(article->source_name);
    free(article->source_url);
    free(article->summary);
    article->title = NULL;
    article->source_name = NULL;
    article->source_url = NULL;
    article->summary = NULL;

    article_hydrate(msg, article);

    query_code = edit_article(article);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR EDITING ARTICLE"));
      HANDLE_QUERY_CODE;
      free_article(article);
      return;
    }

    char *result = article_to_json(article);
    SUCCESS_REPLY_200(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ARTICLE SUCCESSFULLY EDITED ==="));

    free_article(article);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    int delete_rc = delete_article(section_id, article_id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE ARTICLE"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== ARTICLE SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Article successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}

void send_articles_reorder_res(struct mg_connection *c,
                               struct mg_http_message *msg, int issue_id,
                               int section_id,
                               struct error_reply *error_reply,
                               const char *secret) {
  struct error_reply _er = {0};
  error_reply = &_er;

  if (!check_section(issue_id, section_id, error_reply, c)) {
    return;
  }

  if (!mg_match(msg->method, mg_str("PUT"), NULL)) {
    ERROR_REPLY_405;
    return;
  }

  int user_logged = 0;
  is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

  if (user_logged == 0) {
    ERROR_REPLY_401;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
    return;
  }

  if (msg->body.len <= 0) {
    ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
    return;
  } else if (!mg_validateJSON(msg->body)) {
    ERROR_REPLY_400(JSON_ERROR_MESSAGE);
    return;
  }

  int offset, length;
  offset = mg_json_get(msg->body, "$.order", &length);
  if (offset < 0) {
    ERROR_REPLY_400(REORDER_REQUIRED_MESSAGE);
    return;
  }

  cJSON *root = cJSON_ParseWithLength(msg->body.buf + offset, (size_t)length);
  if (root == NULL || !cJSON_IsArray(root)) {
    ERROR_REPLY_400(REORDER_REQUIRED_MESSAGE);
    if (root != NULL)
      cJSON_Delete(root);
    return;
  }

  int total = get_articles_len(section_id);
  int order_count = cJSON_GetArraySize(root);
  if (total < 0 || order_count != total) {
    ERROR_REPLY_400(REORDER_MISMATCH_MESSAGE);
    cJSON_Delete(root);
    return;
  }

  int *ids = malloc((size_t)order_count * sizeof(int));
  cJSON *item = NULL;
  int i = 0;
  bool valid = true;
  cJSON_ArrayForEach(item, root) {
    if (!cJSON_IsNumber(item)) {
      valid = false;
      break;
    }
    ids[i++] = item->valueint;
  }
  cJSON_Delete(root);

  if (!valid) {
    ERROR_REPLY_400(REORDER_MISMATCH_MESSAGE);
    free(ids);
    return;
  }

  int query_code = reorder_articles(section_id, ids, (size_t)order_count);
  free(ids);

  if (query_code != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR REORDERING ARTICLES"));
    HANDLE_QUERY_CODE;
    return;
  }

  printf(TERMINAL_SUCCESS_MESSAGE("=== ARTICLES SUCCESSFULLY REORDERED ==="));
  SUCCESS_REPLY_200_MSG("Articles successfully reordered");
}
