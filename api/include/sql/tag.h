#pragma once

#include <stddef.h>
#include <structs.h>
#include <lib/mongoose.h>

int tag_exists(char *name);
int get_tags_len(const struct mg_str *q);
int get_tags(size_t len, struct tag **arr, const struct mg_str *q, const struct mg_str *sort, int page, int page_size);
int get_tag(struct tag *tag, char *name);
int add_tag(struct tag *tag);
int edit_tag(struct tag *tag, char *name);
int delete_tag(char *name);
