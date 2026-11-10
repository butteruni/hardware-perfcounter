/* SPDX-License-Identifier: Apache-2.0 */
#include "hpc/gpu/adreno/kgsl.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "linux/adreno_driver_ioctl.h"

uint32_t hpc_gpu_adreno_kgsl_generation(uint32_t chip_id) {
  static const struct { uint32_t chip, generation; } known[] = {
#include "chip_ids.inc"
  };
  for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
    if (known[i].chip == chip_id) return known[i].generation;
  for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
    if ((known[i].chip & 255u) == 255u &&
        (known[i].chip & 0xffffff00u) == (chip_id & 0xffffff00u))
      return known[i].generation;
  /* A702/A704 are A6-derived despite their marketing number. */
  if ((chip_id & 0xffffff00u) == 0x07000200u ||
      (chip_id & 0xffffff00u) == 0x07000400u) return 6;
  uint32_t core = chip_id >> 24;
  return core >= 5 && core <= 8 ? core : 0;
}

int hpc_gpu_adreno_kgsl_open(void) {
  int fd = open("/dev/kgsl-3d0", O_RDWR | O_CLOEXEC);
  return fd < 0 ? -errno : fd;
}

int hpc_gpu_adreno_kgsl_close(int fd) {
  return close(fd) < 0 ? -errno : 0;
}

int hpc_gpu_adreno_kgsl_get_device_info(
    int fd, hpc_gpu_adreno_kgsl_device_info_t *info) {
  if (!info) return -EINVAL;
  struct adreno_device_info devinfo = {0};
  struct adreno_device_get_property payload = {0};
  payload.type = ADRENO_PROPERTY_DEVICE_INFO;
  payload.value = &devinfo;
  payload.num_bytes = sizeof(devinfo);
  if (ioctl(fd, ADRENO_IOCTL_DEVICE_GET_PROPERTY, &payload) < 0) return -errno;
  info->chip_id = devinfo.chip_id;
  info->gpu_id = devinfo.gpu_id;
  info->gmem_sizebytes = devinfo.gmem_sizebytes;
  return 0;
}

int hpc_gpu_adreno_kgsl_get(int fd, uint32_t group, uint32_t selector) {
  if (group >= HPC_GPU_ADRENO_KGSL_GROUP_MAX) return -EINVAL;
  struct adreno_counter_get payload = {0};
  payload.group_id = group;
  payload.countable_selector = selector;
  return ioctl(fd, ADRENO_IOCTL_COUNTER_GET, &payload) < 0 ? -errno : 0;
}

int hpc_gpu_adreno_kgsl_put(int fd, uint32_t group, uint32_t selector) {
  if (group >= HPC_GPU_ADRENO_KGSL_GROUP_MAX) return -EINVAL;
  struct adreno_counter_put payload = {0};
  payload.group_id = group;
  payload.countable_selector = selector;
  return ioctl(fd, ADRENO_IOCTL_COUNTER_PUT, &payload) < 0 ? -errno : 0;
}

int hpc_gpu_adreno_kgsl_query_group(int fd, uint32_t group,
    uint32_t *selectors, uint32_t capacity, uint32_t *max_counters) {
  if (!max_counters || group >= HPC_GPU_ADRENO_KGSL_GROUP_MAX ||
      (!selectors && capacity)) return -EINVAL;
  struct adreno_counter_query payload = {0};
  payload.group_id = group;
  payload.countables = selectors;
  payload.count = capacity;
  if (ioctl(fd, ADRENO_IOCTL_COUNTER_QUERY, &payload) < 0) return -errno;
  *max_counters = payload.max_counters;
  return selectors && capacity < payload.max_counters ? -ENOSPC : 0;
}

int hpc_gpu_adreno_kgsl_read(int fd, uint32_t count,
                           hpc_gpu_adreno_kgsl_counter_t *counters) {
  if (!count || !counters || count > 4096) return -EINVAL;
  for (uint32_t i = 0; i < count; ++i)
    if (counters[i].group_id >= HPC_GPU_ADRENO_KGSL_GROUP_MAX) return -EINVAL;
  size_t bytes = count * sizeof(*counters);
  hpc_gpu_adreno_kgsl_counter_t *scratch = malloc(bytes);
  if (!scratch) return -ENOMEM;
  memcpy(scratch, counters, bytes);
  int result = 0;
  for (uint32_t offset = 0; offset < count; offset += 64) {
    struct adreno_counter_read payload = {0};
    payload.counters = scratch + offset;
    payload.num_counters = count - offset < 64 ? count - offset : 64;
    if (ioctl(fd, ADRENO_IOCTL_COUNTER_READ, &payload) < 0) {
      result = -errno;
      break;
    }
  }
  if (!result)
    for (uint32_t i = 0; i < count; ++i) counters[i].value = scratch[i].value;
  free(scratch);
  return result;
}
