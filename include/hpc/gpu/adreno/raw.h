/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HPC_GPU_ADRENO_RAW_H_
#define HPC_GPU_ADRENO_RAW_H_
#include "hpc/gpu/adreno/kgsl.h"
#include "hpc/gpu/base_utilities.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hpc_gpu_adreno_raw_context_t hpc_gpu_adreno_raw_context_t;
enum { HPC_GPU_ADRENO_ERROR_DISCONTINUITY = 1100 };

/* Allocates host state and opens KGSL; does not reserve counters until start.
 * Input value fields are ignored. Duplicate events share one reservation.
 * All functions return zero or a negative errno/library error. Contexts must
 * be externally synchronized. The allocator is copied and used until destroy. */
int hpc_gpu_adreno_raw_create_context(uint32_t count,
    const hpc_gpu_adreno_kgsl_counter_t *counters,
    const hpc_gpu_host_allocation_callbacks_t *allocator,
    hpc_gpu_adreno_raw_context_t **out_context);
int hpc_gpu_adreno_raw_destroy_context(hpc_gpu_adreno_raw_context_t *context);
int hpc_gpu_adreno_raw_get_device_info(const hpc_gpu_adreno_raw_context_t *context,
    hpc_gpu_adreno_kgsl_device_info_t *info);
int hpc_gpu_adreno_raw_start_counters(hpc_gpu_adreno_raw_context_t *context);
int hpc_gpu_adreno_raw_stop_counters(hpc_gpu_adreno_raw_context_t *context);

/* Returns deltas in input order. On ioctl failure output and baseline remain
 * unchanged. On any decrease, re-primes the complete baseline, leaves output
 * unchanged and returns -HPC_GPU_ADRENO_ERROR_DISCONTINUITY. A decrease may be
 * reset or wrap; no fixed counter width is assumed. Discard that entire window. */
int hpc_gpu_adreno_raw_query_counters(hpc_gpu_adreno_raw_context_t *context,
                                    uint64_t *values);
#ifdef __cplusplus
}
#endif
#endif
