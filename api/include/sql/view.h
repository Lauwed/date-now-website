
#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int get_views_len(const struct mg_str *q);
int get_views(size_t len, struct view **arr, const struct mg_str *q,
              const struct mg_str *sort, int page, int page_size);
int add_view(struct view *view);
