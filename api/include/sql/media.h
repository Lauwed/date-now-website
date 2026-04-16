#pragma once

#include <stddef.h>
#include <structs.h>

int media_exists(int id);
int get_medias_len(void);
int get_medias(size_t len, struct media **arr);
int get_media(struct media *media, int id);
int add_media(struct media *media);
int update_media_file(int id, const char *url, double width, double height);
int delete_media(int id);
