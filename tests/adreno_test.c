/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "linux/adreno_driver_ioctl.h"
#include "hpc/gpu/adreno/a7xx.h"
#include "hpc/gpu/adreno/a8xx.h"

static int opens, closes, get_count, put_count, reads, allocations, fail_alloc;
static int fail_get, fail_read, fail_info, fail_put;
static uint32_t chip = 0x07030000;
static uint64_t tick = 1000;
static uint32_t last_group, last_selector;

int hpc_test_open(const char *path, int flags, ...) {
  (void)flags;
  assert(!strcmp(path, "/dev/kgsl-3d0"));
  ++opens;
  return 23;
}
int hpc_test_close(int fd) { assert(fd == 23); ++closes; return 0; }
int hpc_test_ioctl(int fd, unsigned long request, ...) {
  assert(fd == 23);
  va_list args; va_start(args, request); void *ptr = va_arg(args, void *); va_end(args);
  if (request == ADRENO_IOCTL_DEVICE_GET_PROPERTY) {
    if (fail_info) { errno = EIO; return -1; }
    struct adreno_device_get_property *p = ptr;
    assert(p->num_bytes == sizeof(struct adreno_device_info));
    struct adreno_device_info *info = p->value;
    info->chip_id = chip; info->gpu_id = 730; info->gmem_sizebytes = 1024;
  } else if (request == ADRENO_IOCTL_COUNTER_GET) {
    struct adreno_counter_get *p = ptr;
    last_group = p->group_id; last_selector = p->countable_selector;
    if (++get_count == fail_get) { errno = EBUSY; return -1; }
  } else if (request == ADRENO_IOCTL_COUNTER_PUT) {
    if (++put_count == fail_put) { errno = EIO; return -1; }
  } else if (request == ADRENO_IOCTL_COUNTER_READ) {
    struct adreno_counter_read *p = ptr;
    assert(p->num_counters > 0 && p->num_counters <= 64);
    if (++reads == fail_read) { errno = EPERM; return -1; }
    for (uint32_t i = 0; i < p->num_counters; ++i)
      p->counters[i].value = tick + p->counters[i].countable_selector;
  } else if (request == ADRENO_IOCTL_COUNTER_QUERY) {
    struct adreno_counter_query *p = ptr;
    p->max_counters = 3;
    for (unsigned i = 0; i < p->count && i < 3; ++i)
      p->countables[i] = i == 0 ? 7 : UINT32_MAX;
  } else { assert(0); }
  return 0;
}
static void *allocate(void *user, size_t n) {
  (void)user;
  if (++allocations == fail_alloc) return NULL;
  return malloc(n);
}
static void release(void *user, void *ptr) { (void)user; free(ptr); }
static const hpc_gpu_host_allocation_callbacks_t allocator = {NULL, allocate, release};
static void reset(void) {
  assert(opens == closes);
  opens = closes = get_count = put_count = reads = allocations = 0;
  fail_alloc = fail_get = fail_read = fail_info = fail_put = 0;
  chip = 0x07030000; tick = 1000;
}

static void transport(void) {
  reset(); int fd = hpc_gpu_adreno_kgsl_open();
  hpc_gpu_adreno_kgsl_device_info_t info;
  assert(!hpc_gpu_adreno_kgsl_get_device_info(fd, &info));
  assert(info.chip_id == chip && info.gmem_sizebytes == 1024);
  assert(!hpc_gpu_adreno_kgsl_get(fd, 54, 513));
  assert(last_group == 54 && last_selector == 513);
  assert(hpc_gpu_adreno_kgsl_get(fd, 55, 0) == -EINVAL);
  uint32_t count = 0, slots[3];
  assert(!hpc_gpu_adreno_kgsl_query_group(fd, 54, NULL, 0, &count) && count == 3);
  assert(hpc_gpu_adreno_kgsl_query_group(fd, 54, slots, 1, &count) == -ENOSPC);
  assert(!hpc_gpu_adreno_kgsl_query_group(fd, 54, slots, 3, &count));
  assert(slots[0] == 7 && slots[1] == UINT32_MAX);
  hpc_gpu_adreno_kgsl_counter_t data[130];
  for (unsigned i = 0; i < 130; ++i) data[i] = (hpc_gpu_adreno_kgsl_counter_t){0, i, 99};
  fail_read = 2;
  assert(hpc_gpu_adreno_kgsl_read(fd, 130, data) == -EPERM);
  for (unsigned i = 0; i < 130; ++i) assert(data[i].value == 99);
  fail_read = 0; reads = 0;
  assert(!hpc_gpu_adreno_kgsl_read(fd, 130, data) && reads == 3);
  for (unsigned i = 0; i < 130; ++i) assert(data[i].value == 1000 + i);
  assert(!hpc_gpu_adreno_kgsl_close(fd));
}

static void lifecycle_and_deltas(void) {
  reset();
  hpc_gpu_adreno_raw_context_t *c = NULL;
  hpc_gpu_adreno_kgsl_counter_t events[] = {{0, 0, 0}, {54, 9, 0}, {0, 0, 0}};
  assert(!hpc_gpu_adreno_raw_create_context(3, events, &allocator, &c));
  assert(get_count == 0);
  fail_get = 2;
  assert(hpc_gpu_adreno_raw_start_counters(c) == -EBUSY);
  assert(get_count == 2 && put_count == 1);
  fail_get = 0;
  assert(!hpc_gpu_adreno_raw_start_counters(c) && get_count == 4);
  assert(hpc_gpu_adreno_raw_start_counters(c) == -EALREADY);
  uint64_t values[3] = {99, 99, 99}; tick += 700;
  assert(!hpc_gpu_adreno_raw_query_counters(c, values));
  assert(values[0] == 700 && values[1] == 700 && values[2] == 700);
  tick += 100; fail_read = reads + 1;
  assert(hpc_gpu_adreno_raw_query_counters(c, values) == -EPERM);
  assert(values[0] == 700); fail_read = 0; tick += 100;
  assert(!hpc_gpu_adreno_raw_query_counters(c, values) && values[0] == 200);
  tick = 0;
  assert(hpc_gpu_adreno_raw_query_counters(c, values) == -HPC_GPU_ADRENO_ERROR_DISCONTINUITY);
  assert(values[0] == 200 && values[1] == 200);
  tick = 90;
  assert(!hpc_gpu_adreno_raw_query_counters(c, values) && values[1] == 90);
  fail_put = put_count + 1;
  assert(hpc_gpu_adreno_raw_stop_counters(c) == -EIO);
  assert(put_count == 3); // both reservations attempted despite the first error
  assert(!hpc_gpu_adreno_raw_destroy_context(c) && put_count == 4);
}

static void failures_and_families(void) {
  hpc_gpu_adreno_kgsl_counter_t event = {0, 0, 0};
  hpc_gpu_adreno_raw_context_t *c;
  for (int i = 1; i <= 4; ++i) {
    reset(); fail_alloc = i; c = (void *)1;
    assert(hpc_gpu_adreno_raw_create_context(1, &event, &allocator, &c) == -ENOMEM && !c);
  }
  reset(); fail_info = 1;
  assert(hpc_gpu_adreno_raw_create_context(1, &event, &allocator, &c) == -EIO);
  assert(!c && opens == closes);
  reset();
  hpc_gpu_adreno_a7xx_counter_t a7[] = {HPC_GPU_ADRENO_A7XX_PERF_CP_ALWAYS_COUNT,
      HPC_GPU_ADRENO_A7XX_BV_PERF_CP_BUSY_CYCLES};
  assert(!hpc_gpu_adreno_a7xx_create_context(2, a7, &allocator, &c));
  assert(!hpc_gpu_adreno_raw_start_counters(c));
  assert(last_group == 44 && last_selector == 2);
  assert(!hpc_gpu_adreno_raw_destroy_context(c));
  reset(); chip = 0x08030001;
  assert(hpc_gpu_adreno_a7xx_create_context(2, a7, &allocator, &c) == -HPC_GPU_ERROR_INCOMPATIBLE_DEVICE && !c);
  hpc_gpu_adreno_a8xx_counter_t a8 = HPC_GPU_ADRENO_A8XX_PERF_CP_ALWAYS_COUNT;
  assert(!hpc_gpu_adreno_a8xx_create_context(1, &a8, &allocator, &c));
  assert(!hpc_gpu_adreno_raw_start_counters(c));
  assert(last_group == 0 && last_selector == 1);
  assert(!hpc_gpu_adreno_raw_destroy_context(c));
  reset(); chip = 0x07000200;
  assert(hpc_gpu_adreno_a7xx_create_context(2, a7, &allocator, &c) == -HPC_GPU_ERROR_INCOMPATIBLE_DEVICE && !c);
  reset();
}

int main(void) {
  transport(); lifecycle_and_deltas(); failures_and_families();
  puts("Adreno transport, lifecycle, rollback, discontinuity and family tests passed");
  return 0;
}
