
#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int get_views_len(int issue_id);
int get_views(size_t len, struct view **arr, int issue_id,
              const struct mg_str *sort, int page, int page_size);
int add_view(struct view *view);
