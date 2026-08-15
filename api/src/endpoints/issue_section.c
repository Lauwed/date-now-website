/**
 * @file issue_section.c
 * @brief IssueSection endpoint handler implementations (list, single
 *        resource, reorder).
 */

#include <cjson/cJSON.h>
#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/article.h>
#include <sql/category.h>
#include <sql/issue.h>
#include <sql/issue_section.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <utils.h>

/* Loads the articles of a CATEGORY section into section->articles. Safe to
 * call on a TEXT section (no-op). */
static void attach_articles(struct issue_section *section) {
  if (strcmp(section->type, "CATEGORY") != 0) {
    return;
  }

  int len = get_articles_len(section->id);
  if (len <= 0) {
    return;
  }

  struct article **articles = malloc(len * sizeof(struct article *));
  if (get_articles((size_t)len, articles, section->id) != 0) {
    free(articles);
    return;
  }

  section->articles = articles;
  section->articles_count = (size_t)len;
}

void send_issue_sections_res(struct mg_connection *c,
                             struct mg_http_message *msg, int issue_id,
                             struct error_reply *error_reply,
                             const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  // Check if issue exists
  int exists = issue_exists(issue_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ISSUE SECTION LIST ==="));

    int total = get_issue_sections_len(issue_id);
    if (total < 0) {
      query_code = total;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SECTIONS"));
      HANDLE_QUERY_CODE;
      return;
    }

    struct issue_section **sections = NULL;
    if (total > 0) {
      sections = malloc((size_t)total * sizeof(struct issue_section *));
      query_code = get_issue_sections((size_t)total, sections, issue_id);
      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SECTIONS"));
        HANDLE_QUERY_CODE;
        free(sections);
        return;
      }

      for (int i = 0; i < total; i++) {
        attach_articles(sections[i]);
      }
    }

    struct list_reply *reply = malloc(sizeof(struct list_reply));
    reply->page = -1;
    reply->page_size = 0;
    reply->total = reply->count = total;
    reply->total_pages = 0;
    reply->json = NULL;
    reply->data = issue_sections_to_json(sections, (size_t)total);
    list_reply_to_json(reply);

    SUCCESS_REPLY_200(reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SECTIONS SUCCESSFULLY SENT ==="));

    if (total > 0) {
      free_issue_sections(sections, (size_t)total);
      free(reply->data);
    }
    free(reply->json);
    free(reply);
  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    // Check if user logged
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

    REQUIRED_BODY_PROPERTY("type", TYPE_REQUIRED_MESSAGE);
    char *type = malloc(length - 1);
    strncpy(type, msg->body.buf + offset + 1, length - 2);
    type[length - 2] = '\0';

    bool is_category = strcmp(type, "CATEGORY") == 0;
    bool is_text = strcmp(type, "TEXT") == 0;
    if (!is_category && !is_text) {
      ERROR_REPLY_400(TYPE_FORMAT_MESSAGE);
      free(type);
      return;
    }

    int category_name_offset, category_name_len;
    category_name_offset =
        mg_json_get(msg->body, "$.categoryName", &category_name_len);
    int text_body_offset, text_body_len;
    text_body_offset = mg_json_get(msg->body, "$.textBody", &text_body_len);

    if (is_category) {
      if (category_name_offset < 0) {
        ERROR_REPLY_400(CATEGORY_NAME_REQUIRED_MESSAGE);
        free(type);
        return;
      }
      if (text_body_offset >= 0) {
        ERROR_REPLY_400(SECTION_TYPE_MISMATCH_MESSAGE);
        free(type);
        return;
      }

      char *category_name = malloc(category_name_len - 1);
      strncpy(category_name, msg->body.buf + category_name_offset + 1,
              category_name_len - 2);
      category_name[category_name_len - 2] = '\0';

      int category_found = category_exists(category_name);
      if (!category_found) {
        ERROR_REPLY_404;
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("CATEGORY NOT FOUND"));
        free(type);
        free(category_name);
        return;
      }
      free(category_name);
    } else {
      if (text_body_offset < 0) {
        ERROR_REPLY_400(TEXT_BODY_REQUIRED_MESSAGE);
        free(type);
        return;
      }
      if (category_name_offset >= 0) {
        ERROR_REPLY_400(SECTION_TYPE_MISMATCH_MESSAGE);
        free(type);
        return;
      }

      struct mg_str text_body_raw =
          mg_str_n(msg->body.buf + text_body_offset, (size_t)text_body_len);
      if (validate_content_blocks(text_body_raw) != 0) {
        ERROR_REPLY_400(CONTENT_BLOCKS_INVALID_MESSAGE);
        free(type);
        return;
      }
    }

    free(type);

    // Hydrate
    struct issue_section *section = malloc(sizeof(struct issue_section));
    int init_rc = issue_section_init(section);
    if (init_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("SECTION IS NULL"));
      return;
    }

    section->issue_id = issue_id;
    issue_section_hydrate(msg, section);

    query_code = add_issue_section(section);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING SECTION"));
      HANDLE_QUERY_CODE;
      free_issue_section(section);
      return;
    }

    char *result = issue_section_to_json(section);
    SUCCESS_REPLY_201(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SECTION SUCCESSFULLY ADDED ==="));

    free_issue_section(section);
  } else {
    ERROR_REPLY_405;
  }
}

void send_issue_section_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id,
                            int section_id, struct error_reply *error_reply,
                            const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  int exists = issue_exists(issue_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
    return;
  }
  exists = issue_section_exists(issue_id, section_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("SECTION NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET ISSUE SECTION ==="));

    struct issue_section *section = malloc(sizeof(struct issue_section));
    query_code = get_issue_section(section, issue_id, section_id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SECTION"));
      HANDLE_QUERY_CODE;
      return;
    }

    attach_articles(section);

    char *result = issue_section_to_json(section);
    SUCCESS_REPLY_200(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SECTION SUCCESSFULLY SENT ==="));

    free_issue_section(section);
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

    struct issue_section *section = malloc(sizeof(struct issue_section));
    query_code = get_issue_section(section, issue_id, section_id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING SECTION"));
      HANDLE_QUERY_CODE;
      return;
    }

    if (strcmp(section->type, "CATEGORY") == 0) {
      int offset, length;
      offset = mg_json_get(msg->body, "$.categoryName", &length);
      if (offset < 0 || length - 2 <= 0) {
        ERROR_REPLY_400(CATEGORY_NAME_REQUIRED_MESSAGE);
        free_issue_section(section);
        return;
      }
      char *category_name = malloc(length - 1);
      strncpy(category_name, msg->body.buf + offset + 1, length - 2);
      category_name[length - 2] = '\0';

      int category_found = category_exists(category_name);
      if (!category_found) {
        ERROR_REPLY_404;
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("CATEGORY NOT FOUND"));
        free(category_name);
        free_issue_section(section);
        return;
      }

      free(section->category_name);
      section->category_name = category_name;
    } else {
      int text_body_offset, text_body_len;
      text_body_offset = mg_json_get(msg->body, "$.textBody", &text_body_len);
      if (text_body_offset < 0) {
        ERROR_REPLY_400(TEXT_BODY_REQUIRED_MESSAGE);
        free_issue_section(section);
        return;
      }

      struct mg_str text_body_raw =
          mg_str_n(msg->body.buf + text_body_offset, (size_t)text_body_len);
      if (validate_content_blocks(text_body_raw) != 0) {
        ERROR_REPLY_400(CONTENT_BLOCKS_INVALID_MESSAGE);
        free_issue_section(section);
        return;
      }

      free(section->text_body);
      section->text_body = malloc((size_t)text_body_len + 1);
      snprintf(section->text_body, (size_t)text_body_len + 1, "%.*s",
              text_body_len, msg->body.buf + text_body_offset);
    }

    query_code = edit_issue_section(section);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR EDITING SECTION"));
      HANDLE_QUERY_CODE;
      free_issue_section(section);
      return;
    }

    char *result = issue_section_to_json(section);
    SUCCESS_REPLY_200(result);
    free(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SECTION SUCCESSFULLY EDITED ==="));

    free_issue_section(section);
  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);

    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    int delete_rc = delete_issue_section(issue_id, section_id);
    if (delete_rc != 0) {
      ERROR_REPLY_500;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("COULDN'T DELETE SECTION"));
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SECTION SUCCESSFULLY DELETE ==="));
    SUCCESS_REPLY_200_MSG("Section successfully deleted");
  } else {
    ERROR_REPLY_405;
  }
}

void send_issue_sections_reorder_res(struct mg_connection *c,
                                     struct mg_http_message *msg,
                                     int issue_id,
                                     struct error_reply *error_reply,
                                     const char *secret) {
  struct error_reply _er = {0};
  error_reply = &_er;

  int exists = issue_exists(issue_id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ISSUE NOT FOUND"));
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

  int total = get_issue_sections_len(issue_id);
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

  int query_code = reorder_issue_sections(issue_id, ids, (size_t)order_count);
  free(ids);

  if (query_code != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR REORDERING SECTIONS"));
    HANDLE_QUERY_CODE;
    return;
  }

  printf(TERMINAL_SUCCESS_MESSAGE("=== ISSUE SECTIONS SUCCESSFULLY REORDERED ==="));
  SUCCESS_REPLY_200_MSG("Sections successfully reordered");
}
