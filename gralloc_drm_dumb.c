/**************************************************************************
 *
 * gralloc_drm_dumb driver which refer to libdrv's dumb.c.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#define LOG_TAG "GRALLOC-DUMB"

#include <cutils/log.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <drm.h>
#include <xf86drm.h>
#include <libdrm_macros.h>
#include <libkms/libkms.h>

#include "gralloc_drm.h"
#include "gralloc_drm_priv.h"

#define UNUSED(...) (void)(__VA_ARGS__)

struct dumb_info {
	struct gralloc_drm_drv_t base;
	int fd;
	struct kms_driver *kms;
};

static void dumb_destroy(struct gralloc_drm_drv_t *drv)
{
	struct dumb_info *info = (struct dumb_info *) drv;

        kms_destroy(info->kms);
	free(info);
}

static struct gralloc_drm_bo_t *dumb_alloc(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_handle_t *handle)
{
	struct dumb_info *info = (struct dumb_info *) drv;
	struct kms_bo *bo;
	int width;
	int height;
	int ret;
	int i = 2;
	unsigned attrs[7] = {
	KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
	};

	ALOGE("alloc %p", handle);

	width = handle->width;
	height = handle->height;
	gralloc_drm_align_geometry(handle->format, &width, &height);

	attrs[i++] = KMS_WIDTH; attrs[i++] = width;
	attrs[i++] = KMS_HEIGHT; attrs[i++] = height;

	attrs[i++] = KMS_TERMINATE_PROP_LIST;

	ret = kms_bo_create(info->kms, attrs, &bo);
	if (ret) {
		ALOGE("failed to create dumb bo.");
		return NULL;
	}

//	handle->stride = arg.pitch;

//	bo->base.handle = handle;
//	bo->base.fb_handle = arg.handle;

	return (struct gralloc_drm_bo_t *)bo;
}

static int dumb_map(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *_bo,
		int x, int y, int w, int h,
		int enable_write, void **addr)
{
	struct kms_bo *bo = (struct kms_bo *) _bo;
	int ret;

	UNUSED(drv, x, y, w, h, enable_write);

	ret = kms_bo_map(bo, addr);
	if (ret) {
		ALOGE("failed to map dumb bo");
	}
	return ret;
}

static void dumb_unmap(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *_bo)
{
	struct kms_bo *bo = (struct kms_bo *) _bo;
	UNUSED(drv);
	kms_bo_unmap(bo);
}

static void dumb_free(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *_bo)
{
	struct kms_bo *bo = (struct kms_bo *) _bo;
	kms_bo_destroy(&bo);
}

struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_dumb(int fd)
{
	struct kms_driver *kms;
	struct dumb_info *info;
	int ret;
	uint64_t cap = 0;

	ret = kms_create(fd, &kms);
	if (ret)
		return NULL;

	info = calloc(1, sizeof(*info));
	if (!info) {
		ALOGE("failed to allocate driver info.");
		return NULL;
	}

	info->fd = fd;
	info->kms = kms;
	info->base.destroy = dumb_destroy;
	info->base.alloc = dumb_alloc;
	info->base.free = dumb_free;
	info->base.map = dumb_map;
	info->base.unmap = dumb_unmap;
	//info->base.resolve_format = dumb_resolve_format; TODO: need??

	return &info->base;
}
