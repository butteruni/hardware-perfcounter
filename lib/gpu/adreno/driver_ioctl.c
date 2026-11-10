/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "driver_ioctl.h"
#include "hpc/gpu/adreno/kgsl.h"

int hpc_gpu_adreno_ioctl_open_gpu_device(void) { return hpc_gpu_adreno_kgsl_open(); }
int hpc_gpu_adreno_ioctl_close_gpu_device(int fd) { return hpc_gpu_adreno_kgsl_close(fd); }
uint32_t hpc_gpu_adreno_ioctl_get_gpu_device_id(int fd) {
  hpc_gpu_adreno_kgsl_device_info_t info;
  int error = hpc_gpu_adreno_kgsl_get_device_info(fd, &info);
  if (error) return (uint32_t)error;
  return ((info.chip_id >> 24) & 255) * 100 +
         ((info.chip_id >> 16) & 255) * 10 + ((info.chip_id >> 8) & 255);
}
int hpc_gpu_adreno_ioctl_activate_counter(int fd, uint32_t group, uint32_t selector) {
  return hpc_gpu_adreno_kgsl_get(fd, group, selector);
}
int hpc_gpu_adreno_ioctl_deactivate_counter(int fd, uint32_t group, uint32_t selector) {
  return hpc_gpu_adreno_kgsl_put(fd, group, selector);
}
int hpc_gpu_adreno_ioctl_query_counters(int fd, uint32_t count,
    hpc_gpu_adreno_ioctl_counter_read_counter_t *counters, uint64_t *values) {
  int error = hpc_gpu_adreno_kgsl_read(fd, count, counters);
  if (error) return error;
  for (uint32_t i = 0; i < count; ++i) values[i] = counters[i].value;
  return 0;
}
