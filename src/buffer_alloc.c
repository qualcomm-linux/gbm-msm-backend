/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/dma-heap.h>
#include <msm_kgsl.h>
#include "buffer_alloc.h"
#include <sys/stat.h>

/*
 * Use the QCOM system heap which produces dma-bufs importable by the msm DRM
 * driver (drmPrimeFDToHandle).  The generic "system" heap allocates from
 * system memory that the msm DRM driver cannot import, causing weston's
 * zwp_linux_dmabuf_v1 import to fail with "importing the supplied dmabufs
 * failed" (ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER).
 *
 * Fall back to the generic "system" heap if the QCOM heap is not present.
 */
#define KGSL_DMA_HEAP_PATH_QCOM   "/dev/dma_heap/qcom,system"
#define KGSL_DMA_HEAP_PATH_SYSTEM "/dev/dma_heap/system"

/*
 * allocate_buffer - Allocate GPU memory using KGSL ioctls only.
 *
 * Scanout/display buffers (GBM_BO_USE_SCANOUT):
 *   Step 1: Allocate via dma_heap (/dev/dma_heap/system).
 *           dma_heap returns a dma-buf fd that can be shared with KMS
 *           directly via drmModeAddFB2() — no DRM allocation needed.
 *   Step 2: Import the dma-buf fd into KGSL (IOCTL_KGSL_GPUOBJ_IMPORT)
 *           so the GPU can access the buffer.
 *   Result: *handle = KGSL GPU object id, *dmabuf_fd_out = dma-buf fd.
 *
 * GPU-only buffers (no GBM_BO_USE_SCANOUT):
 *   Allocate directly via IOCTL_KGSL_GPUOBJ_ALLOC.
 *   Result: *handle = KGSL GPU object id, *dmabuf_fd_out = -1.
 *
 * No DRM ioctls are used in either path.
 */
int allocate_buffer(const struct gbm_msm_device *msm_dev, uint32_t size,
                    uint32_t usage, uint32_t *handle, int *dmabuf_fd_out) {
   if (!msm_dev || !handle || !dmabuf_fd_out) {
      return -1;
   }

   *dmabuf_fd_out = -1;

   if (usage & GBM_BO_USE_SCANOUT) {
      /*
       * Scanout buffer path:
       *   dma_heap alloc → dma-buf fd → KGSL import → KGSL id
       *
       * The dma-buf fd is retained in bo->fd and returned to the caller.
       * KMS uses it directly; KGSL uses the imported GPU object id.
       */
      /* Prefer the QCOM heap; fall back to the generic system heap */
      const char *heap_path = KGSL_DMA_HEAP_PATH_QCOM;
      int heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
      if (heap_fd < 0) {
         heap_path = KGSL_DMA_HEAP_PATH_SYSTEM;
         heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
      }
      if (heap_fd < 0) {
         return -1;
      }

      struct dma_heap_allocation_data heap_data;
      memset(&heap_data, 0, sizeof(heap_data));
      heap_data.len      = size;
      heap_data.fd_flags = O_RDWR | O_CLOEXEC;

      if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data)) {
         close(heap_fd);
         return -1;
      }
      close(heap_fd);

      int dmabuf_fd = (int)heap_data.fd;

      /* Import the dma-buf fd into KGSL for GPU access */
      struct kgsl_gpuobj_import_dma_buf dma_buf_data;
      struct kgsl_gpuobj_import import_args;
      memset(&import_args, 0, sizeof(import_args));
      memset(&dma_buf_data, 0, sizeof(dma_buf_data));

      dma_buf_data.fd      = dmabuf_fd;
      import_args.priv     = (uint64_t)(uintptr_t)&dma_buf_data;
      import_args.priv_len = sizeof(dma_buf_data);
      import_args.type     = KGSL_USER_MEM_TYPE_DMABUF;
      import_args.flags    = 0;

      if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_IMPORT, &import_args)) {
         close(dmabuf_fd);
         return -1;
      }

      *handle        = import_args.id;
      *dmabuf_fd_out = dmabuf_fd;
      return 0;
   }

   /*
    * GPU-only buffer path: allocate directly via KGSL.
    *
    * struct kgsl_gpuobj_alloc fields:
    *   size     - allocation size in bytes (input)
    *   va_len   - GPU VA range length; set equal to size (input)
    *   flags    - cache mode | optional IO-coherent flag (input)
    *   id       - returned GPU object id used as the BO handle (output)
    *   mmapsize - returned size to pass to mmap() (output)
    */
   struct kgsl_gpuobj_alloc args;
   memset(&args, 0, sizeof(args));
   args.size   = size;
   args.va_len = size;

   if (usage & GBM_BO_USE_CURSOR)
      args.flags = ((uint64_t)KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT) |
                   KGSL_MEMFLAGS_IOCOHERENT;
   else
      args.flags = ((uint64_t)KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT);

   if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_ALLOC, &args)) {
      return -1;
   }

   *handle = args.id;
   return 0;
}

/*
 * free_buffer - Free a KGSL GPU object via IOCTL_KGSL_GPUOBJ_FREE.
 *
 * The associated dma-buf fd (for scanout buffers) must be closed separately
 * by the caller (gbm_msm_bo_destroy closes bo->fd).
 */
int free_buffer(const struct gbm_msm_device *msm_dev, uint32_t handle) {
   if (!msm_dev || !handle) {
      return -1;
   }

   struct kgsl_gpuobj_free args;
   memset(&args, 0, sizeof(args));
   args.id = handle;

   if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_FREE, &args)) {
      return -1;
   }

   return 0;
}

/*
 * import_gem_buffer - Import an external dma-buf fd as a KGSL GPU object.
 *
 * Uses IOCTL_KGSL_GPUOBJ_IMPORT with type KGSL_USER_MEM_TYPE_DMABUF.
 * Returns the KGSL GPU object id in *handle.
 */
int import_gem_buffer(const struct gbm_msm_device *msm_dev, int fd,
                      uint32_t *handle) {
   if (!msm_dev || (msm_dev->kgsl_fd < 0) || (fd < 0)) {
      return -1;
   }

   struct kgsl_gpuobj_import_dma_buf dma_buf_data;
   struct kgsl_gpuobj_import args;
   memset(&args, 0, sizeof(args));
   memset(&dma_buf_data, 0, sizeof(dma_buf_data));

   dma_buf_data.fd  = fd;
   args.priv        = (uint64_t)(uintptr_t)&dma_buf_data;
   args.priv_len    = sizeof(dma_buf_data);
   args.type        = KGSL_USER_MEM_TYPE_DMABUF;
   args.flags       = 0;

   if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_IMPORT, &args)) {
      return -1;
   }

   *handle = args.id;
   return 0;
}

/*
 * bo_offset - Return the KGSL mmap offset for a GPU object.
 *
 * For KGSL GPU objects the mmap offset is id * PAGE_SIZE.
 * The caller must mmap() against kgsl_fd.
 *
 * Note: for scanout buffers with a dma-buf fd, the caller should mmap
 * the dma-buf fd directly (offset 0) instead of using this function.
 */
int bo_offset(uint32_t handle, uint64_t *offset) {
   *offset = (uint64_t)handle * (uint64_t)getpagesize();
   return 0;
}
