/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _GBM_MSM_H_
#define _GBM_MSM_H_

#include <stdbool.h>
#include <gbm.h>
#include <gbm_backend_abi.h>
#include "drm_fourcc.h"

#define NUM_BACK_BUFFERS 3

#define DRM_FORMAT_MOD_QCOM_32F fourcc_mod_code(QCOM, 32)

struct gbm_msm_bo {
   struct gbm_bo base;
   /*
    * fd: dma-buf file descriptor for scanout/display buffers.
    *     Obtained from dma_heap allocation and used directly by KMS.
    *     -1 for GPU-only buffers (no dma-buf fd needed).
    *     -1 for imported buffers (caller owns the original fd).
    */
   int fd;
   /*
    * kgsl_id: KGSL GPU object id for all buffers.
    *     Used for KGSL operations (IOCTL_KGSL_GPUOBJ_FREE, mmap offset).
    *
    * drm_gem_handle: DRM GEM handle for scanout and imported buffers (0 if none).
    *     Obtained via drmPrimeFDToHandle() from the dma-buf fd.
    *     Stored in base.v0.handle.u32 so that Mesa's gbm_bo_get_handle()
    *     (which returns v0.handle directly, bypassing the backend callback)
    *     returns a valid DRM GEM handle for Weston's drmModeAddFB2() call.
    *
    *     GPU-only buffers have no dma-buf fd, so drm_gem_handle == 0 and
    *     v0.handle.u32 == kgsl_id for those buffers only.
    *
    * THE BUG THIS FIXES:
    *     gbm_msm_bo_import() previously stored the KGSL id in v0.handle.u32.
    *     Weston calls gbm_bo_get_handle() (which bypasses the backend callback
    *     and returns v0.handle directly) and passes the result to
    *     drmModeAddFB2().  Passing a KGSL id instead of a DRM GEM handle
    *     causes drmModeAddFB2() to fail, Weston sends a Wayland error to the
    *     client, and the Adreno EGL driver reports
    *     "DequeueBuffer: Display dispatch queue failed".
    */
   uint32_t kgsl_id;
   uint32_t drm_gem_handle;
   uint32_t size;
   uint32_t aligned_width;
   uint32_t aligned_height;
   uint64_t modifier;
   uint32_t num_planes;
   void *map;
   int map_refcount;
   int (*bo_dump_buffers)(struct gbm_bo *gbo, char *func);
   uint32_t (*bo_get_metabuffer_size)(struct gbm_bo *gbo, int plane);
   void (*bo_get_plane_aligned_width_height)(struct gbm_bo *gbm, int plane, uint32_t *aligned_width, uint32_t *aligned_height);
};


struct gbm_msm_surface {
   struct gbm_surface base;
   struct {
      int age;
      bool locked;
      struct gbm_bo *bo;
   } color_buffers[NUM_BACK_BUFFERS], *back, *current;
   struct gbm_bo *(*surface_get_back_bo)(struct gbm_surface *surface);
   int (*surface_swap_buffers)(struct gbm_surface *surface);
};

const struct gbm_backend *gbmint_get_backend(const struct gbm_core *gbm_core);
#endif
