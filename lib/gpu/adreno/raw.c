/* SPDX-License-Identifier: Apache-2.0 */
#include "hpc/gpu/adreno/raw.h"
#include <errno.h>
#include <string.h>

struct hpc_gpu_adreno_raw_context_t {
  hpc_gpu_host_allocation_callbacks_t allocator;
  hpc_gpu_adreno_kgsl_device_info_t info;
  hpc_gpu_adreno_kgsl_counter_t *counters;
  uint64_t *previous;
  unsigned char *reserved;
  uint32_t count;
  int fd;
  int started;
};

int hpc_gpu_adreno_raw_create_context(uint32_t count,
    const hpc_gpu_adreno_kgsl_counter_t *counters,
    const hpc_gpu_host_allocation_callbacks_t *allocator,
    hpc_gpu_adreno_raw_context_t **out_context) {
  if (!out_context) return -EINVAL;
  *out_context = NULL;
  if (!count || count > 4096 || !counters || !allocator ||
      !allocator->alloc || !allocator->free) return -EINVAL;
  for (uint32_t i = 0; i < count; ++i)
    if (counters[i].group_id >= HPC_GPU_ADRENO_KGSL_GROUP_MAX) return -EINVAL;
  hpc_gpu_adreno_raw_context_t *c = allocator->alloc(allocator->user_data, sizeof(*c));
  if (!c) return -ENOMEM;
  memset(c, 0, sizeof(*c));
  c->allocator = *allocator;
  c->fd = -1;
  c->count = count;
  c->counters = allocator->alloc(allocator->user_data, count * sizeof(*c->counters));
  c->previous = allocator->alloc(allocator->user_data, count * sizeof(*c->previous));
  c->reserved = allocator->alloc(allocator->user_data, count);
  if (c->reserved) memset(c->reserved, 0, count);
  int error = -ENOMEM;
  if (!c->counters || !c->previous || !c->reserved) goto fail;
  memcpy(c->counters, counters, count * sizeof(*counters));
  c->fd = hpc_gpu_adreno_kgsl_open();
  if (c->fd < 0) { error = c->fd; goto fail; }
  error = hpc_gpu_adreno_kgsl_get_device_info(c->fd, &c->info);
  if (error) goto fail;
  *out_context = c;
  return 0;
fail:
  hpc_gpu_adreno_raw_destroy_context(c);
  return error;
}

int hpc_gpu_adreno_raw_get_device_info(const hpc_gpu_adreno_raw_context_t *c,
    hpc_gpu_adreno_kgsl_device_info_t *info) {
  if (!c || !info) return -EINVAL;
  *info = c->info;
  return 0;
}

int hpc_gpu_adreno_raw_stop_counters(hpc_gpu_adreno_raw_context_t *c) {
  if (!c) return -EINVAL;
  int result = 0;
  if (c->reserved && c->fd >= 0) {
    for (uint32_t i = 0; i < c->count; ++i) {
      if (!c->reserved[i]) continue;
      int error = hpc_gpu_adreno_kgsl_put(c->fd, c->counters[i].group_id,
                                       c->counters[i].countable_selector);
      if (!error) c->reserved[i] = 0;
      else if (!result) result = error;
    }
  }
  c->started = 0;
  return result;
}

int hpc_gpu_adreno_raw_destroy_context(hpc_gpu_adreno_raw_context_t *c) {
  if (!c) return 0;
  int result = hpc_gpu_adreno_raw_stop_counters(c);
  if (c->fd >= 0) {
    int error = hpc_gpu_adreno_kgsl_close(c->fd);
    if (!result) result = error;
  }
  hpc_gpu_host_allocation_callbacks_t allocator = c->allocator;
  if (c->reserved) allocator.free(allocator.user_data, c->reserved);
  if (c->previous) allocator.free(allocator.user_data, c->previous);
  if (c->counters) allocator.free(allocator.user_data, c->counters);
  allocator.free(allocator.user_data, c);
  return result;
}

int hpc_gpu_adreno_raw_start_counters(hpc_gpu_adreno_raw_context_t *c) {
  if (!c) return -EINVAL;
  if (c->started) return -EALREADY;
  int error = hpc_gpu_adreno_raw_stop_counters(c);
  if (error) return error;
  for (uint32_t i = 0; i < c->count; ++i) {
    uint32_t j = 0;
    for (; j < i; ++j)
      if (c->counters[j].group_id == c->counters[i].group_id &&
          c->counters[j].countable_selector == c->counters[i].countable_selector) break;
    if (j != i) continue;
    error = hpc_gpu_adreno_kgsl_get(c->fd, c->counters[i].group_id,
                                   c->counters[i].countable_selector);
    if (error) goto fail;
    c->reserved[i] = 1;
  }
  error = hpc_gpu_adreno_kgsl_read(c->fd, c->count, c->counters);
  if (error) goto fail;
  for (uint32_t i = 0; i < c->count; ++i) c->previous[i] = c->counters[i].value;
  c->started = 1;
  return 0;
fail:
  hpc_gpu_adreno_raw_stop_counters(c);
  return error;
}

int hpc_gpu_adreno_raw_query_counters(hpc_gpu_adreno_raw_context_t *c,
                                    uint64_t *values) {
  if (!c || !c->started || !values) return -EINVAL;
  int error = hpc_gpu_adreno_kgsl_read(c->fd, c->count, c->counters);
  if (error) return error;
  int reset = 0;
  for (uint32_t i = 0; i < c->count; ++i)
    if (c->counters[i].value < c->previous[i]) reset = 1;
  for (uint32_t i = 0; i < c->count; ++i) {
    if (!reset) values[i] = c->counters[i].value - c->previous[i];
    c->previous[i] = c->counters[i].value;
  }
  return reset ? -HPC_GPU_ADRENO_ERROR_DISCONTINUITY : 0;
}
