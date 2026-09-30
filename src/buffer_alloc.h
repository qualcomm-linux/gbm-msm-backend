/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <stdbool.h>
#include "gbm_msm_int.h"

/*
 * allocate_buffer - Allocate GPU memory using KGSL ioctls only.
 *
 * Scanout/display buffers (GBM_BO_USE_SCANOUT):
 *   1. Allocate via dma_heap (/dev/dma_heap/system) → dma-buf fd
 *   2. Import dma-buf fd into KGSL (IOCTL_KGSL_GPUOBJ_IMPORT) → KGSL id
 *   3. *handle = KGSL id, *dmabuf_fd_out = dma-buf fd (caller owns it)
 *   The dma-buf fd can be shared directly with KMS for display.
 *
 * GPU-only buffers (no GBM_BO_USE_SCANOUT):
 *   Allocate via IOCTL_KGSL_GPUOBJ_ALLOC.
 *   *handle = KGSL id, *dmabuf_fd_out = -1.
 *
 * No DRM ioctls are used.
 */
int allocate_buffer(const struct gbm_msm_device *msm_dev, uint32_t size,
                    uint32_t usage, uint32_t *handle, int *dmabuf_fd_out);

/*
 * import_gem_buffer - Import an external dma-buf fd as a KGSL GPU object.
 * Uses IOCTL_KGSL_GPUOBJ_IMPORT. Returns KGSL id in *handle.
 */
int import_gem_buffer(const struct gbm_msm_device *msm_dev, int fd,
                      uint32_t *handle);

/*
 * free_buffer - Free a KGSL GPU object via IOCTL_KGSL_GPUOBJ_FREE.
 * The associated dma-buf fd (if any) must be closed separately by the caller.
 */
int free_buffer(const struct gbm_msm_device *msm_dev, uint32_t handle);

/*
 * bo_offset - Return the KGSL mmap offset for a GPU object.
 * Offset = id * PAGE_SIZE; caller must mmap() against kgsl_fd.
 */
int bo_offset(uint32_t handle, uint64_t *offset);
