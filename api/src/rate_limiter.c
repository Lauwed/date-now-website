#include <rate_limiter.h>
#include <string.h>
#include <time.h>

#define RL_MAX_ENTRIES 512

typedef struct {
  uint8_t  ip[16];
  int      is_ip6;
  int      count;
  time_t   window_start;
} rl_entry_t;

static rl_entry_t rl_table[RL_MAX_ENTRIES];
static int        rl_count = 0;

int rate_limit_check(struct mg_addr *addr, int max_requests,
                     int window_seconds) {
  time_t now = time(NULL);

  for (int i = 0; i < rl_count; i++) {
    if (rl_table[i].is_ip6 == (int)addr->is_ip6 &&
        memcmp(rl_table[i].ip, addr->addr.ip, 16) == 0) {
      if (now - rl_table[i].window_start >= window_seconds) {
        rl_table[i].window_start = now;
        rl_table[i].count = 1;
        return 0;
      }
      rl_table[i].count++;
      return rl_table[i].count > max_requests ? 1 : 0;
    }
  }

  int idx;
  if (rl_count < RL_MAX_ENTRIES) {
    idx = rl_count++;
  } else {
    // Evict the entry whose window started earliest
    idx = 0;
    for (int i = 1; i < RL_MAX_ENTRIES; i++) {
      if (rl_table[i].window_start < rl_table[idx].window_start) idx = i;
    }
  }

  memcpy(rl_table[idx].ip, addr->addr.ip, 16);
  rl_table[idx].is_ip6      = (int)addr->is_ip6;
  rl_table[idx].count       = 1;
  rl_table[idx].window_start = now;
  return 0;
}
