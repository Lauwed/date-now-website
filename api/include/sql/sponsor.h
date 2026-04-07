#pragma once

#include <stddef.h>
#include <structs.h>
#include <lib/mongoose.h>

int sponsor_exists(char *name);
int get_sponsors_len(const struct mg_str *q);
int get_sponsors(size_t len, struct sponsor **arr, const struct mg_str *q, const struct mg_str *sort, int page, int page_size);
int get_sponsor(struct sponsor *sponsor, char *name);
int add_sponsor(struct sponsor *sponsor);
int edit_sponsor(struct sponsor *sponsor, char *name);
int delete_sponsor(char *name);
