/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HPC_GPU_ADRENO_KGSL_H_
#define HPC_GPU_ADRENO_KGSL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* KGSL group IDs, not Mesa's array indices or register addresses. */
enum { HPC_GPU_ADRENO_KGSL_GROUP_MAX = 0x37 };

typedef struct hpc_gpu_adreno_kgsl_device_info_t {
  uint32_t chip_id;
  uint32_t gpu_id;
  uint64_t gmem_sizebytes;
} hpc_gpu_adreno_kgsl_device_info_t;

/* This layout is also used by the KGSL READ ioctl. Values are absolute,
 * system-wide driver counters, not per-process or per-frame deltas. */
typedef struct hpc_gpu_adreno_ioctl_counter_read_counter_t {
  uint32_t group_id;
  uint32_t countable_selector;
  uint64_t value;
} hpc_gpu_adreno_kgsl_counter_t;

/* Functions return zero or negative errno, except open (fd or negative errno).
 * No function changes the global perfcounter switch or GPU clocks. The caller
 * owns one GET/PUT reference for each successful reservation. These functions
 * do not synchronize concurrent users of the same fd. */
int hpc_gpu_adreno_kgsl_open(void);
int hpc_gpu_adreno_kgsl_close(int fd);
int hpc_gpu_adreno_kgsl_get_device_info(
    int fd, hpc_gpu_adreno_kgsl_device_info_t *info);
/* Returns 5/6/7/8, or zero when unknown. Modern chip IDs (e.g. 0x44050001)
 * use the pinned Mesa device table; their high byte is not a generation. */
uint32_t hpc_gpu_adreno_kgsl_generation(uint32_t chip_id);
int hpc_gpu_adreno_kgsl_get(int fd, uint32_t group, uint32_t selector);
int hpc_gpu_adreno_kgsl_put(int fd, uint32_t group, uint32_t selector);

/* QUERY reports slots and current bindings, not supported event selectors.
 * Pass NULL and capacity=0 to obtain max_counters, then query into that size.
 * A nonempty undersized buffer returns -ENOSPC; UINT32_MAX means a free slot. */
int hpc_gpu_adreno_kgsl_query_group(int fd, uint32_t group,
    uint32_t *selectors, uint32_t capacity, uint32_t *max_counters);

/* Batches at most 64 events per ioctl. Reads are not an atomic GPU snapshot.
 * All output values remain unchanged on failure, including a later batch.
 * No counter-width, wrap, reset, normalization or product-support inference. */
int hpc_gpu_adreno_kgsl_read(int fd, uint32_t count,
                           hpc_gpu_adreno_kgsl_counter_t *counters);

#ifdef __cplusplus
}
#endif
#endif
