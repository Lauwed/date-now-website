#include <endpoints/auth.h>
#include <enums.h>
#include <lib/mongoose.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/media.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <sys/stat.h>
#include <time.h>
#include <utils.h>

#define UPLOADS_DIR "./uploads"
#define ALT_REQUIRED_MESSAGE "The 'alt' field is required"

/* Extract file extension from filename, including the dot. Returns pointer
   into filename, or empty string if no extension found. */
static const char *get_extension(const char *filename, size_t len) {
  for (int i = (int)len - 1; i >= 0; i--) {
    if (filename[i] == '.') {
      return &filename[i];
    }
    if (filename[i] == '/' || filename[i] == '\\') {
      break;
    }
  }
  return "";
}

/* Build a unique filename: "<unix_timestamp>_<rand16bits><ext>" into out[len] */
static void make_unique_filename(const char *original, size_t orig_len,
                                 char *out, size_t outlen) {
  const char *ext = get_extension(original, orig_len);
  unsigned short rnd = (unsigned short)(rand() & 0xFFFF);
  snprintf(out, outlen, "%ld_%04x%s", (long)time(NULL), rnd, ext);
}

/* Write multipart body to disk. Returns 0 on success. */
static int save_file(const char *filename, const char *data, size_t datalen) {
  /* Ensure uploads directory exists */
  struct stat st = {0};
  if (stat(UPLOADS_DIR, &st) == -1) {
    mkdir(UPLOADS_DIR, 0755);
  }

  char path[512];
  snprintf(path, sizeof(path), "%s/%s", UPLOADS_DIR, filename);

  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    perror("fopen");
    return -1;
  }
  fwrite(data, 1, datalen, f);
  fclose(f);
  return 0;
}

/* Delete physical file for a media record (url is like "/uploads/foo.jpg") */
static void delete_file_for_media(struct media *m) {
  if (m->url == NULL)
    return;
  /* url starts with '/', so prepend '.' to make it a relative path */
  char path[512];
  snprintf(path, sizeof(path), ".%s", m->url);
  remove(path);
}

void send_medias_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA LIST ==="));

    int count = get_medias_len();
    if (count < 0) {
      ERROR_REPLY_500;
      return;
    }

    char *data = NULL;
    if (count > 0) {
      struct media **arr = malloc(count * sizeof(struct media *));
      query_code = get_medias((size_t)count, arr);
      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING MEDIAS"));
        HANDLE_QUERY_CODE;
        free(arr);
        return;
      }

      /* Build JSON array */
      size_t json_len = 0;
      for (int i = 0; i < count; i++)
        json_len += media_to_json_len(arr[i]);
      json_len += count; /* commas */

      char *inner = malloc(json_len + 1);
      inner[0] = '\0';
      for (int i = 0; i < count; i++) {
        char *mj = media_to_json(arr[i]);
        strcat(inner, mj);
        if (i < count - 1)
          strcat(inner, ",");
        free(mj);
      }

      data = malloc(json_len + 3);
      sprintf(data, "[%s]", inner);
      free(inner);

      for (int i = 0; i < count; i++)
        free_media(arr[i]);
      free(arr);
    } else {
      data = "[]";
    }

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", data);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIAS SUCCESSFULLY SENT ==="));
    if (count > 0)
      free(data);

  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    if (check_auth(msg) != 0) {
      mg_http_reply(c, 401, JSON_HEADER,
                    "{\"code\":401,\"message\":\"Unauthorized\"}");
      return;
    }

    printf(TERMINAL_ENDPOINT_MESSAGE("=== POST MEDIA ==="));

    /* Expect multipart/form-data with a "file" part and optional "alt" part */
    struct mg_http_part part;
    size_t ofs = 0;
    char *file_data = NULL;
    size_t file_data_len = 0;
    char stored_filename[256] = "";
    char alt_buf[512] = "";

    while ((ofs = mg_http_next_multipart(msg->body, ofs, &part)) > 0) {
      if (mg_strcmp(part.name, mg_str("file")) == 0 && part.filename.len > 0) {
        /* This is the file part */
        char unique[256];
        make_unique_filename(part.filename.buf, part.filename.len, unique,
                             sizeof(unique));
        strncpy(stored_filename, unique, sizeof(stored_filename) - 1);
        file_data = (char *)part.body.buf;
        file_data_len = part.body.len;
      } else if (mg_strcmp(part.name, mg_str("alt")) == 0) {
        snprintf(alt_buf, sizeof(alt_buf), "%.*s", (int)part.body.len,
                 part.body.buf);
      }
    }

    if (stored_filename[0] == '\0' || file_data == NULL) {
      ERROR_REPLY_400("A 'file' part is required");
      return;
    }

    if (save_file(stored_filename, file_data, file_data_len) != 0) {
      ERROR_REPLY_500;
      return;
    }

    struct media *media = malloc(sizeof(struct media));
    media_init(media);

    /* Build URL */
    char url[300];
    snprintf(url, sizeof(url), "/uploads/%s", stored_filename);
    media->url = strdup(url);

    if (alt_buf[0] != '\0') {
      media->alternative_text = strdup(alt_buf);
    } else {
      media->alternative_text = strdup("");
    }

    query_code = add_media(media);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR ADDING MEDIA"));
      HANDLE_QUERY_CODE;
      free(media->url);
      free(media->alternative_text);
      free(media);
      return;
    }

    char *mj = media_to_json(media);
    mg_http_reply(c, 201, JSON_HEADER, "%s\n", mj);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY ADDED ==="));
    free(mj);
    free(media->url);
    free(media->alternative_text);
    free(media);

  } else {
    ERROR_REPLY_405;
  }

  if (error_reply != NULL) {
    free(error_reply->json);
    free(error_reply->message);
  }
  free(error_reply);
}

void send_media_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA ==="));

    struct media *media = malloc(sizeof(struct media));
    media_init(media);

    query_code = get_media(media, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING MEDIA"));
      HANDLE_QUERY_CODE;
      free(media);
      return;
    }

    char *mj = media_to_json(media);
    mg_http_reply(c, 200, JSON_HEADER, "%s\n", mj);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY SENT ==="));
    free(mj);
    free_media(media);

  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    if (check_auth(msg) != 0) {
      mg_http_reply(c, 401, JSON_HEADER,
                    "{\"code\":401,\"message\":\"Unauthorized\"}");
      return;
    }

    printf(TERMINAL_ENDPOINT_MESSAGE("=== PUT MEDIA ==="));

    if (!media_exists(id)) {
      ERROR_REPLY_404;
      return;
    }

    struct media *media = malloc(sizeof(struct media));
    media_init(media);
    media->id = id;

    /* Fetch current record so we keep the existing URL */
    query_code = get_media(media, id);
    if (query_code != 0) {
      HANDLE_QUERY_CODE;
      free(media);
      return;
    }

    /* Hydrate from JSON body (alt, width, height) */
    media_hydrate(msg, media);

    query_code = edit_media(media);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR EDITING MEDIA"));
      HANDLE_QUERY_CODE;
      free_media(media);
      return;
    }

    char *mj = media_to_json(media);
    mg_http_reply(c, 200, JSON_HEADER, "%s\n", mj);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY UPDATED ==="));
    free(mj);
    free_media(media);

  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    if (check_auth(msg) != 0) {
      mg_http_reply(c, 401, JSON_HEADER,
                    "{\"code\":401,\"message\":\"Unauthorized\"}");
      return;
    }

    printf(TERMINAL_ENDPOINT_MESSAGE("=== DELETE MEDIA ==="));

    if (!media_exists(id)) {
      ERROR_REPLY_404;
      return;
    }

    /* Fetch to get file URL before deleting */
    struct media *media = malloc(sizeof(struct media));
    media_init(media);
    query_code = get_media(media, id);
    if (query_code == 0) {
      delete_file_for_media(media);
    }
    free_media(media);

    query_code = delete_media(id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR DELETING MEDIA"));
      HANDLE_QUERY_CODE;
      return;
    }

    mg_http_reply(c, 204, JSON_HEADER, "");
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY DELETED ==="));

  } else {
    ERROR_REPLY_405;
  }

  if (error_reply != NULL) {
    free(error_reply->json);
    free(error_reply->message);
  }
  free(error_reply);
}
