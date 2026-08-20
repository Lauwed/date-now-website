/**
 * @file media.c
 * @brief Media endpoint handler implementations (list, upload, single
 * resource).
 */

#include <MagickWand/MagickWand.h>
#include <endpoints/auth.h>
#include <endpoints/media.h>
#include <enums.h>
#include <fcntl.h>
#include <lib/blob.h>
#include <lib/mongoose.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <pthread.h>
#include <sql/media.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <unistd.h>
#include <utils.h>

#define MAX_UPLOAD_SIZE (5 * 1024 * 1024)

static int is_image_data(const char *data, size_t len) {
  if (len < 4)
    return 0;
  const unsigned char *d = (const unsigned char *)data;
  /* JPEG */
  if (d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF)
    return 1;
  /* PNG */
  if (d[0] == 0x89 && d[1] == 0x50 && d[2] == 0x4E && d[3] == 0x47)
    return 1;
  /* GIF */
  if (d[0] == 0x47 && d[1] == 0x49 && d[2] == 0x46)
    return 1;
  /* WebP: RIFF....WEBP */
  if (len >= 12 && d[0] == 0x52 && d[1] == 0x49 && d[2] == 0x46 &&
      d[3] == 0x46 && d[8] == 0x57 && d[9] == 0x45 && d[10] == 0x42 &&
      d[11] == 0x50)
    return 1;
  /* BMP */
  if (d[0] == 0x42 && d[1] == 0x4D)
    return 1;
  /* TIFF LE */
  if (d[0] == 0x49 && d[1] == 0x49 && d[2] == 0x2A && d[3] == 0x00)
    return 1;
  /* TIFF BE */
  if (d[0] == 0x4D && d[1] == 0x4D && d[2] == 0x00 && d[3] == 0x2A)
    return 1;
  return 0;
}

static void generate_uuid_v4(char out[37]) {
  unsigned char bytes[16] = {0};
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    read(fd, bytes, sizeof(bytes));
    close(fd);
  }
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  snprintf(out, 37,
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
           "-%02x%02x%02x%02x%02x%02x",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
           bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
           bytes[13], bytes[14], bytes[15]);
}

struct media_convert_ctx {
  unsigned media_id;
  char uuid[37];
  char tmp_path[256];
};

static void *media_convert_thread(void *arg) {
  struct media_convert_ctx *ctx = arg;

  MagickWand *wand = NewMagickWand();
  if (MagickReadImage(wand, ctx->tmp_path) == MagickFalse) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("MAGICK READ FAILED (async)"));
    DestroyMagickWand(wand);
    unlink(ctx->tmp_path);
    delete_media((int)ctx->media_id);
    free(ctx);
    return NULL;
  }

  size_t w = MagickGetImageWidth(wand);
  size_t h = MagickGetImageHeight(wand);
  if (w > 1200) {
    size_t new_h = (size_t)((double)h * 1200.0 / (double)w);
    MagickThumbnailImage(wand, 1200, new_h);
    w = 1200;
    h = new_h;
  }
  int img_width = (int)w;
  int img_height = (int)h;

  MagickSetImageFormat(wand, "WEBP");
  size_t img_blob_len = 0;
  unsigned char *img_blob = MagickGetImageBlob(wand, &img_blob_len);
  DestroyMagickWand(wand);
  if (img_blob == NULL) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("MAGICK BLOB FAILED (async)"));
    unlink(ctx->tmp_path);
    delete_media((int)ctx->media_id);
    free(ctx);
    return NULL;
  }

  char img_pathname[256];
  snprintf(img_pathname, sizeof(img_pathname), "media/%s/image.webp",
           ctx->uuid);

  char *img_url = NULL;
  if (blob_upload(img_pathname, img_blob, img_blob_len, "image/webp",
                  &img_url) != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BLOB IMAGE UPLOAD FAILED (async)"));
    MagickRelinquishMemory(img_blob);
    unlink(ctx->tmp_path);
    delete_media((int)ctx->media_id);
    free(ctx);
    return NULL;
  }
  MagickRelinquishMemory(img_blob);

  char *thumb_url = NULL;
  MagickWand *tn = NewMagickWand();
  if (MagickReadImage(tn, ctx->tmp_path) == MagickTrue) {
    size_t tw = MagickGetImageWidth(tn);
    size_t th = MagickGetImageHeight(tn);
    if (tw > 300) {
      size_t new_th = (size_t)((double)th * 300.0 / (double)tw);
      MagickThumbnailImage(tn, 300, new_th);
    }
    MagickSetImageFormat(tn, "WEBP");
    size_t thumb_blob_len = 0;
    unsigned char *thumb_blob = MagickGetImageBlob(tn, &thumb_blob_len);
    if (thumb_blob != NULL) {
      char thumb_pathname[256];
      snprintf(thumb_pathname, sizeof(thumb_pathname), "media/%s/thumb.webp",
               ctx->uuid);
      if (blob_upload(thumb_pathname, thumb_blob, thumb_blob_len,
                      "image/webp", &thumb_url) != 0) {
        fprintf(stderr,
                TERMINAL_ERROR_MESSAGE("BLOB THUMB UPLOAD FAILED (async)"));
        thumb_url = NULL;
      }
      MagickRelinquishMemory(thumb_blob);
    }
  }
  DestroyMagickWand(tn);

  unlink(ctx->tmp_path);

  update_media_file((int)ctx->media_id, img_url, thumb_url,
                    (double)img_width, (double)img_height);

  free(img_url);
  free(thumb_url);

  printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA CONVERSION COMPLETE ==="));
  free(ctx);
  return NULL;
}

/**
 * @brief Deletes a media and its Blob objects, unless something still uses it.
 *
 * Shared by the DELETE endpoint and by the replace-on-update paths of Issue
 * and AppUser, so the Blob objects are never left behind.
 *
 * @return 0 on success, MEDIA_STILL_REFERENCED when the media is still in use
 *         (nothing is deleted), 1 on any other failure.
 */
int delete_media_with_blob(int id) {
  if (media_is_referenced(id)) {
    return MEDIA_STILL_REFERENCED;
  }

  struct media *to_delete = malloc(sizeof(struct media));
  if (to_delete == NULL) {
    return 1;
  }
  to_delete->id = 0;
  to_delete->alternative_text = NULL;
  to_delete->url = NULL;
  to_delete->thumb_url = NULL;
  to_delete->width = 0.0;
  to_delete->height = 0.0;

  if (get_media(to_delete, id) == 0) {
    const char *urls[2] = {to_delete->url, to_delete->thumb_url};
    // A Blob that refuses to go is logged, never fatal: keeping a dangling
    // row would be worse than leaking one object.
    if (blob_delete(urls, 2) != 0)
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR DELETING FROM BLOB"));
  }
  free_media(to_delete);

  return delete_media(id) == 0 ? 0 : 1;
}

void send_medias_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA LIST ==="));

    // ?url= resolves one exact full-size URL back to its record. Clients that
    // only kept the URL of an embedded image need the id to delete it.
    char url_buf[2048] = "";
    int url_len = mg_http_get_var(&msg->query, "url", url_buf, sizeof(url_buf));
    if (url_len > 0) {
      url_buf[url_len] = '\0';

      struct media *found = malloc(sizeof(struct media));
      found->id = 0;
      found->alternative_text = NULL;
      found->url = NULL;
      found->thumb_url = NULL;
      found->width = 0.0;
      found->height = 0.0;

      int found_count = get_media_by_url(found, url_buf) == 0 ? 1 : 0;
      struct media *one[1] = {found};

      char *url_data = medias_to_json(found_count > 0 ? one : NULL,
                                      (size_t)found_count);
      struct list_reply *url_reply = malloc(sizeof(struct list_reply));
      url_reply->data = url_data;
      url_reply->count = found_count;
      url_reply->total = found_count;
      url_reply->total_pages = 0;
      url_reply->page = -1;
      url_reply->page_size = 0;
      list_reply_to_json(url_reply);

      SUCCESS_REPLY_200(url_reply->json);
      printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA BY URL SENT ==="));

      free_media(found);
      if (found_count > 0)
        free(url_data);
      free(url_reply->json);
      free(url_reply);
      return;
    }

    int total = get_medias_len();

    struct media **medias = NULL;
    if (total > 0) {
      medias = malloc((size_t)total * sizeof(struct media *));
      query_code = get_medias((size_t)total, medias);
      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING MEDIAS"));
        HANDLE_QUERY_CODE;
        free(medias);
        return;
      }
    }

    char *data = medias_to_json(medias, (size_t)total);
    struct list_reply *reply = malloc(sizeof(struct list_reply));
    reply->data = data;
    reply->count = total;
    reply->total = total;
    reply->total_pages = 0;
    reply->page = -1;
    reply->page_size = 0;
    list_reply_to_json(reply);

    SUCCESS_REPLY_200(reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIAS SUCCESSFULLY SENT ==="));

    if (medias != NULL) {
      for (int i = 0; i < total; i++) {
        free_media(medias[i]);
      }
      free(medias);
    }
    if (total > 0)
      free(data);
    free(reply->json);
    free(reply);

  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== POST MEDIA UPLOAD ==="));

    /* Auth check */
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    /* Parse multipart */
    struct mg_http_part text_part, file_part;
    memset(&text_part, 0, sizeof(text_part));
    memset(&file_part, 0, sizeof(file_part));
    int found_text = 0, found_file = 0;

    struct mg_http_part part;
    size_t ofs = 0;
    while ((ofs = mg_http_next_multipart(msg->body, ofs, &part)) > 0) {
      if (mg_strcmp(part.name, mg_str("textAlternatif")) == 0) {
        text_part = part;
        found_text = 1;
      } else if (mg_strcmp(part.name, mg_str("file")) == 0) {
        file_part = part;
        found_file = 1;
      }
    }

    /* Validate textAlternatif */
    if (!found_text || text_part.body.len == 0) {
      ERROR_REPLY_400(TEXT_ALT_REQUIRED_MESSAGE);
      return;
    }

    /* Validate file */
    if (!found_file || file_part.body.len == 0) {
      ERROR_REPLY_400(FILE_REQUIRED_MESSAGE);
      return;
    }

    /* Check file size */
    if (file_part.body.len > MAX_UPLOAD_SIZE) {
      ERROR_REPLY_413(FILE_TOO_LARGE_MESSAGE);
      return;
    }

    /* Check magic bytes */
    if (!is_image_data(file_part.body.buf, file_part.body.len)) {
      ERROR_REPLY_415(FILE_NOT_IMAGE_MESSAGE);
      return;
    }

    /* Copy textAlternatif (strip trailing whitespace/newlines) */
    char *alt_text = strndup(text_part.body.buf, text_part.body.len);
    size_t alt_len = strlen(alt_text);
    while (alt_len > 0 &&
           (alt_text[alt_len - 1] == '\r' || alt_text[alt_len - 1] == '\n' ||
            alt_text[alt_len - 1] == ' '))
      alt_text[--alt_len] = '\0';

    if (alt_len == 0) {
      free(alt_text);
      ERROR_REPLY_400(TEXT_ALT_REQUIRED_MESSAGE);
      return;
    }

    /* Insert into DB to get the auto-incremented ID */
    struct media *m = malloc(sizeof(struct media));
    m->id = 0;
    m->alternative_text = alt_text;
    m->url = NULL;
    m->thumb_url = NULL;
    m->width = 0.0;
    m->height = 0.0;

    query_code = add_media(m);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR INSERTING MEDIA"));
      HANDLE_QUERY_CODE;
      free(alt_text);
      free(m);
      return;
    }

    unsigned media_id = m->id;

    /* Generate UUID used to namespace this upload's Blob pathname */
    char uuid[37];
    generate_uuid_v4(uuid);

    /* Write raw upload to a temp file (MagickReadImage needs a path) */
    char tmp_path[] = "/tmp/media_upload_XXXXXX";
    int tmp_fd = mkstemp(tmp_path);
    if (tmp_fd < 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING TEMP FILE"));
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }
    write(tmp_fd, file_part.body.buf, file_part.body.len);
    close(tmp_fd);

    /* Spawn background thread to run ImageMagick and update DB */
    struct media_convert_ctx *ctx = malloc(sizeof(struct media_convert_ctx));
    ctx->media_id = media_id;
    memcpy(ctx->uuid, uuid, 37);
    memcpy(ctx->tmp_path, tmp_path, sizeof(tmp_path));

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, media_convert_thread, ctx) != 0) {
      pthread_attr_destroy(&attr);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("THREAD CREATE FAILED"));
      free(ctx);
      unlink(tmp_path);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }
    pthread_attr_destroy(&attr);

    /* Return 202 — URL will be available once the background conversion
     * finishes */
    char response[64];
    snprintf(response, sizeof(response), "{\"id\":%u}", media_id);
    mg_http_reply(c, 202, JSON_HEADER, "%s\n", response);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA UPLOAD ACCEPTED ==="));

    free(alt_text);
    free(m);

  } else {
    ERROR_REPLY_405;
  }
}

void send_media_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply,
                    const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  int exists = media_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("MEDIA NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA ==="));

    struct media *m = malloc(sizeof(struct media));
    m->id = 0;
    m->alternative_text = NULL;
    m->url = NULL;
    m->thumb_url = NULL;
    m->width = 0.0;
    m->height = 0.0;

    query_code = get_media(m, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING MEDIA"));
      HANDLE_QUERY_CODE;
      free(m);
      return;
    }

    char *result = media_to_json(m);
    SUCCESS_REPLY_200(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY SENT ==="));

    free(result);
    free_media(m);

  } else if (mg_match(msg->method, mg_str("PUT"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== PUT MEDIA ==="));

    /* Auth check */
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
    }

    char *alt_text = mg_json_get_str(msg->body, "$.textAlternatif");
    if (alt_text == NULL || strlen(alt_text) == 0) {
      free(alt_text);
      ERROR_REPLY_400(TEXT_ALT_REQUIRED_MESSAGE);
      return;
    }

    query_code = update_media_alt_text(id, alt_text);
    free(alt_text);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR UPDATING MEDIA"));
      HANDLE_QUERY_CODE;
      return;
    }

    struct media *updated = malloc(sizeof(struct media));
    updated->id = 0;
    updated->alternative_text = NULL;
    updated->url = NULL;
    updated->thumb_url = NULL;
    updated->width = 0.0;
    updated->height = 0.0;

    query_code = get_media(updated, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR FETCHING UPDATED MEDIA"));
      HANDLE_QUERY_CODE;
      free(updated);
      return;
    }

    char *result = media_to_json(updated);
    SUCCESS_REPLY_200(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY UPDATED ==="));

    free(result);
    free_media(updated);

  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== DELETE MEDIA ==="));

    /* Auth check */
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged, NULL);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    query_code = delete_media_with_blob(id);
    if (query_code == MEDIA_STILL_REFERENCED) {
      ERROR_REPLY_409(
          "Media is referenced by one or more resources and cannot be deleted");
      return;
    }
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR DELETING MEDIA"));
      ERROR_REPLY_500;
      return;
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY DELETED ==="));
    SUCCESS_REPLY_200_MSG("Media successfully deleted");

  } else {
    ERROR_REPLY_405;
  }
}
