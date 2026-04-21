/**
 * @file media.c
 * @brief Media endpoint handler implementations (list, upload, single resource).
 */

#include <endpoints/auth.h>
#include <endpoints/media.h>
#include <enums.h>
#include <errno.h>
#include <fcntl.h>
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
#include <unistd.h>
#include <utils.h>
#include <MagickWand/MagickWand.h>


#define MAX_UPLOAD_SIZE (5 * 1024 * 1024)
#define UPLOAD_DIR "/var/www/uploads/media"

static int is_image_data(const char *data, size_t len) {
  if (len < 4) return 0;
  const unsigned char *d = (const unsigned char *)data;
  /* JPEG */
  if (d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return 1;
  /* PNG */
  if (d[0] == 0x89 && d[1] == 0x50 && d[2] == 0x4E && d[3] == 0x47) return 1;
  /* GIF */
  if (d[0] == 0x47 && d[1] == 0x49 && d[2] == 0x46) return 1;
  /* WebP: RIFF....WEBP */
  if (len >= 12 && d[0] == 0x52 && d[1] == 0x49 && d[2] == 0x46 &&
      d[3] == 0x46 && d[8] == 0x57 && d[9] == 0x45 && d[10] == 0x42 &&
      d[11] == 0x50)
    return 1;
  /* BMP */
  if (d[0] == 0x42 && d[1] == 0x4D) return 1;
  /* TIFF LE */
  if (d[0] == 0x49 && d[1] == 0x49 && d[2] == 0x2A && d[3] == 0x00) return 1;
  /* TIFF BE */
  if (d[0] == 0x4D && d[1] == 0x4D && d[2] == 0x00 && d[3] == 0x2A) return 1;
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
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
           bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
           bytes[12], bytes[13], bytes[14], bytes[15]);
}

static void remove_media_dir(const char *uuid) {
  char img_path[512], thumb_path[512], dir_path[256];
  snprintf(dir_path, sizeof(dir_path), UPLOAD_DIR "/%s", uuid);
  snprintf(img_path, sizeof(img_path), UPLOAD_DIR "/%s/image.webp", uuid);
  snprintf(thumb_path, sizeof(thumb_path), UPLOAD_DIR "/%s/thumb.webp", uuid);
  unlink(img_path);
  unlink(thumb_path);
  rmdir(dir_path);
}

void send_medias_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret) {
  int query_code;
  struct error_reply _er = {0};
  error_reply = &_er;

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA LIST ==="));

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
    if (total > 0) free(data);
    free(reply->json);
    free(reply);

  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== POST MEDIA UPLOAD ==="));

    /* Auth check */
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);
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

    /* Generate UUID for the upload directory */
    char uuid[37];
    generate_uuid_v4(uuid);

    /* Create upload directory */
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), UPLOAD_DIR "/%s", uuid);
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING MEDIA DIR"));
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }

    /* Write raw upload to a temp file */
    char tmp_path[] = "/tmp/media_upload_XXXXXX";
    int tmp_fd = mkstemp(tmp_path);
    if (tmp_fd < 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING TEMP FILE"));
      remove_media_dir(uuid);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }
    write(tmp_fd, file_part.body.buf, file_part.body.len);
    close(tmp_fd);

    /* Build output paths */
    char img_path[512], thumb_path[512];
    snprintf(img_path, sizeof(img_path), UPLOAD_DIR "/%s/image.webp", uuid);
    snprintf(thumb_path, sizeof(thumb_path), UPLOAD_DIR "/%s/thumb.webp", uuid);

    /* Convert to WebP at max 1200px width using libmagick */
    MagickWand *wand = NewMagickWand();
    if (MagickReadImage(wand, tmp_path) == MagickFalse) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("MAGICK READ FAILED"));
      DestroyMagickWand(wand);
      unlink(tmp_path);
      remove_media_dir(uuid);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
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
    if (MagickWriteImage(wand, img_path) == MagickFalse) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("MAGICK WRITE FAILED"));
      DestroyMagickWand(wand);
      unlink(tmp_path);
      remove_media_dir(uuid);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }
    DestroyMagickWand(wand);

    /* Generate 300px thumbnail (non-fatal if it fails) */
    MagickWand *tn = NewMagickWand();
    if (MagickReadImage(tn, tmp_path) == MagickTrue) {
      size_t tw = MagickGetImageWidth(tn);
      size_t th = MagickGetImageHeight(tn);
      if (tw > 300) {
        size_t new_th = (size_t)((double)th * 300.0 / (double)tw);
        MagickThumbnailImage(tn, 300, new_th);
      }
      MagickSetImageFormat(tn, "WEBP");
      MagickWriteImage(tn, thumb_path);
    }
    DestroyMagickWand(tn);

    unlink(tmp_path);

    /* Update DB with final URL and dimensions */
    char url[256];
    snprintf(url, sizeof(url), "/uploads/media/%s/image.webp", uuid);

    query_code =
        update_media_file((int)media_id, url, (double)img_width, (double)img_height);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR UPDATING MEDIA FILE"));
      HANDLE_QUERY_CODE;
      free(alt_text);
      free(m);
      return;
    }

    /* Re-fetch from DB and return 201 */
    struct media *created = malloc(sizeof(struct media));
    created->id = 0;
    created->alternative_text = NULL;
    created->url = NULL;
    created->width = 0.0;
    created->height = 0.0;

    query_code = get_media(created, (int)media_id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR FETCHING CREATED MEDIA"));
      HANDLE_QUERY_CODE;
      free(created);
      free(alt_text);
      free(m);
      return;
    }

    char *result = media_to_json(created);
    SUCCESS_REPLY_201(result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY CREATED ==="));

    free(result);
    free_media(created);
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
    is_user_logged(c, msg, error_reply, secret, &user_logged);
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
    is_user_logged(c, msg, error_reply, secret, &user_logged);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    /* 409 if referenced by an issue or user */
    if (media_is_referenced(id)) {
      ERROR_REPLY_409("Media is referenced by one or more resources and cannot be deleted");
      return;
    }

    /* Fetch URL to extract UUID directory name before deleting from DB */
    struct media *to_delete = malloc(sizeof(struct media));
    to_delete->id = 0;
    to_delete->alternative_text = NULL;
    to_delete->url = NULL;
    to_delete->width = 0.0;
    to_delete->height = 0.0;
    if (get_media(to_delete, id) == 0 && to_delete->url != NULL) {
      /* URL format: /uploads/media/{uuid}/image.webp */
      const char *prefix = "/uploads/media/";
      const char *p = to_delete->url + strlen(prefix);
      const char *slash = strchr(p, '/');
      char uuid[37] = {0};
      if (slash && (size_t)(slash - p) == 36) {
        memcpy(uuid, p, 36);
        remove_media_dir(uuid);
      }
    }
    free_media(to_delete);

    /* Remove from DB */
    query_code = delete_media(id);
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
