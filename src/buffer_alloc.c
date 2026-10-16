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

#define KGSL_DMA_HEAP_PATH "/dev/dma_heap/system"

int allocate_buffer(const struct gbm_msm_device *msm_dev, uint32_t size,
                    uint32_t usage, uint32_t *handle, int *dmabuf_fd_out)
{
   if (!msm_dev || !handle || !dmabuf_fd_out)
      return -1;

   *dmabuf_fd_out = -1;

   if (usage & GBM_BO_USE_SCANOUT) {
      int heap_fd = open(KGSL_DMA_HEAP_PATH, O_RDONLY | O_CLOEXEC);
      if (heap_fd < 0)
         return -1;

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

   struct kgsl_gpuobj_alloc args;
   memset(&args, 0, sizeof(args));
   args.size   = size;
   args.va_len = size;

   if (usage & GBM_BO_USE_CURSOR)
      args.flags = ((uint64_t)KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT) |
                   KGSL_MEMFLAGS_IOCOHERENT;
   else
      args.flags = ((uint64_t)KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT);

   if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_ALLOC, &args))
      return -1;

   *handle = args.id;
   return 0;
}

int free_buffer(const struct gbm_msm_device *msm_dev, uint32_t handle)
{
   if (!msm_dev || !handle)
      return -1;

   struct kgsl_gpuobj_free args;
   memset(&args, 0, sizeof(args));
   args.id = handle;

   if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_FREE, &args))
      return -1;

   return 0;
}

int import_gem_buffer(const struct gbm_msm_device *msm_dev, int fd,
                      uint32_t *handle)
{
   if (!msm_dev || (msm_dev->kgsl_fd < 0) || (fd < 0))
      return -1;

   struct kgsl_gpuobj_import_dma_buf dma_buf_data;
   struct kgsl_gpuobj_import args;
   memset(&args, 0, sizeof(args));
   memset(&dma_buf_data, 0, sizeof(dma_buf_data));

   dma_buf_data.fd  = fd;
   args.priv        = (uint64_t)(uintptr_t)&dma_buf_data;
   args.priv_len    = sizeof(dma_buf_data);
   args.type        = KGSL_USER_MEM_TYPE_DMABUF;
   args.flags       = 0;

   if (ioctl(msm_dev->kgsl_fd, IOCTL_KGSL_GPUOBJ_IMPORT, &args))
      return -1;

   *handle = args.id;
   return 0;
}

int bo_offset(uint32_t handle, uint64_t *offset)
{
   *offset = (uint64_t)handle * (uint64_t)getpagesize();
   return 0;
}
