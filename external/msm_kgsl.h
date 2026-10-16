/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Ported from kgsl-dlkm/include/uapi/linux/msm_kgsl.h for use in
 * userspace (msm-gbm-backend).
 */

#ifndef _UAPI_MSM_KGSL_H
#define _UAPI_MSM_KGSL_H

#include <linux/types.h>
#include <linux/ioctl.h>

/* __user is a kernel-only sparse annotation; define it away in userspace */
#ifndef __user
#define __user
#endif

/* --- Memory allocation flags --- */
#define KGSL_MEMFLAGS_SECURE       (1ULL << 3)
#define KGSL_MEMFLAGS_GPUREADONLY  (1ULL << 24)
#define KGSL_MEMFLAGS_GPUWRITEONLY (1ULL << 25)
#define KGSL_MEMFLAGS_FORCE_32BIT  (1ULL << 32)

/* Memory caching hints */
#define KGSL_CACHEMODE_MASK         0x0C000000U
#define KGSL_CACHEMODE_SHIFT        26
#define KGSL_CACHEMODE_WRITECOMBINE 0
#define KGSL_CACHEMODE_UNCACHED     1
#define KGSL_CACHEMODE_WRITETHROUGH 2
#define KGSL_CACHEMODE_WRITEBACK    3

#define KGSL_MEMFLAGS_USE_CPU_MAP  (1ULL << 28)
#define KGSL_MEMFLAGS_SPARSE_PHYS  (1ULL << 29)
#define KGSL_MEMFLAGS_SPARSE_VIRT  (1ULL << 30)
#define KGSL_MEMFLAGS_IOCOHERENT   (1ULL << 31)
#define KGSL_MEMFLAGS_GUARD_PAGE   (1ULL << 33)
#define KGSL_MEMFLAGS_VBO          (1ULL << 34)

/* Memory types */
#define KGSL_MEMTYPE_MASK          0x0000FF00
#define KGSL_MEMTYPE_SHIFT         8
#define KGSL_MEMTYPE_OBJECTANY     0
#define KGSL_MEMTYPE_FRAMEBUFFER   1

/* Alignment hint */
#define KGSL_MEMALIGN_MASK         0x00FF0000
#define KGSL_MEMALIGN_SHIFT        16

enum kgsl_user_mem_type {
	KGSL_USER_MEM_TYPE_PMEM   = 0x00000000,
	KGSL_USER_MEM_TYPE_ASHMEM = 0x00000001,
	KGSL_USER_MEM_TYPE_ADDR   = 0x00000002,
	KGSL_USER_MEM_TYPE_ION    = 0x00000003,
	KGSL_USER_MEM_TYPE_DMABUF = 0x00000003,
	KGSL_USER_MEM_TYPE_MAX    = 0x00000007,
};

/* ioctls */
#define KGSL_IOC_TYPE 0x09

/**
 * struct kgsl_gpuobj_alloc - Argument to IOCTL_KGSL_GPUOBJ_ALLOC
 * @size:         Size in bytes of the object to allocate (in)
 * @flags:        mask of KGSL_MEMFLAG_* bits (in)
 * @va_len:       Size in bytes of the virtual region to allocate (in)
 * @mmapsize:     Returns the mmap() size of the object (out)
 * @id:           Returns the GPU object ID of the new object (out)
 * @metadata_len: Length of the metadata to copy from the user (in)
 * @metadata:     Pointer to the user specified metadata (in)
 */
struct kgsl_gpuobj_alloc {
	__u64 size;
	__u64 flags;
	__u64 va_len;
	__u64 mmapsize;
	unsigned int id;
	unsigned int metadata_len;
	__u64 metadata;
};

#define KGSL_GPUOBJ_ALLOC_METADATA_MAX 64

#define IOCTL_KGSL_GPUOBJ_ALLOC \
	_IOWR(KGSL_IOC_TYPE, 0x45, struct kgsl_gpuobj_alloc)

/**
 * struct kgsl_gpuobj_free - Argument to IOCTL_KGSL_GPUOBJ_FREE
 * @flags: Mask of KGSL_GPUOBJ_FREE_ON_EVENT (in)
 * @priv:  Pointer to private object if KGSL_GPUOBJ_FREE_ON_EVENT set (in)
 * @id:    ID of the GPU object to free (in)
 * @type:  Type of asynchronous event to free on (in)
 * @len:   Length of the data passed in priv (in)
 */
struct kgsl_gpuobj_free {
	__u64 flags;
	__u64 __user priv;
	unsigned int id;
	unsigned int type;
	unsigned int len;
};

#define KGSL_GPUOBJ_FREE_ON_EVENT 1

#define IOCTL_KGSL_GPUOBJ_FREE \
	_IOW(KGSL_IOC_TYPE, 0x46, struct kgsl_gpuobj_free)

/**
 * struct kgsl_gpuobj_info - argument to IOCTL_KGSL_GPUOBJ_INFO
 * @gpuaddr: GPU address of the object (out)
 * @flags:   Current flags for the object (out)
 * @size:    Size of the object (out)
 * @va_len:  VA size of the object (out)
 * @va_addr: Virtual address of the object if mapped (out)
 * @id:      GPU object ID to query (in)
 */
struct kgsl_gpuobj_info {
	__u64 gpuaddr;
	__u64 flags;
	__u64 size;
	__u64 va_len;
	__u64 va_addr;
	unsigned int id;
};

#define IOCTL_KGSL_GPUOBJ_INFO \
	_IOWR(KGSL_IOC_TYPE, 0x47, struct kgsl_gpuobj_info)

/**
 * struct kgsl_gpuobj_import - argument to IOCTL_KGSL_GPUOBJ_IMPORT
 * @priv:     Pointer to the private data for the import type (in)
 * @priv_len: Length of the private data (in)
 * @flags:    Mask of KGSL_MEMFLAG_ flags (in)
 * @type:     Type of the import - KGSL_USER_MEM_TYPE_* (in)
 * @id:       Returns the ID of the new GPU object (out)
 */
struct kgsl_gpuobj_import {
	__u64 __user priv;
	__u64 priv_len;
	__u64 flags;
	unsigned int type;
	unsigned int id;
};

/**
 * struct kgsl_gpuobj_import_dma_buf - import a dmabuf object
 * @fd: File descriptor for the dma-buf object (in)
 */
struct kgsl_gpuobj_import_dma_buf {
	int fd;
};

#define IOCTL_KGSL_GPUOBJ_IMPORT \
	_IOWR(KGSL_IOC_TYPE, 0x48, struct kgsl_gpuobj_import)

#define KGSL_GPUMEM_CACHE_CLEAN    (1 << 0)
#define KGSL_GPUMEM_CACHE_TO_GPU   KGSL_GPUMEM_CACHE_CLEAN
#define KGSL_GPUMEM_CACHE_INV      (1 << 1)
#define KGSL_GPUMEM_CACHE_FROM_GPU KGSL_GPUMEM_CACHE_INV
#define KGSL_GPUMEM_CACHE_FLUSH    (KGSL_GPUMEM_CACHE_CLEAN | KGSL_GPUMEM_CACHE_INV)
#define KGSL_GPUMEM_CACHE_RANGE    (1 << 31U)

#endif /* _UAPI_MSM_KGSL_H */
