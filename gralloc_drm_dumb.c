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

#include "gralloc_drm.h"
#include "gralloc_drm_priv.h"

#define UNUSED(...) (void)(__VA_ARGS__)

struct dumb_info {
	struct gralloc_drm_drv_t base;
	int fd;
};

struct dumb_bo
{
	struct gralloc_drm_bo_t base;
	uint32_t size;
};

static void dumb_destroy(struct gralloc_drm_drv_t *drv)
{
	struct dumb_info *info = (struct dumb_info *) drv;

	free(info);
}

static struct gralloc_drm_bo_t *dumb_alloc(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_handle_t *handle)
{
	struct dumb_info *info = (struct dumb_info *) drv;
	struct drm_mode_create_dumb arg;
	struct dumb_bo *bo;
	int width;
	int height;
//	int pitch;
	int bpp;
	int ret;

	bpp = gralloc_drm_get_bpp(handle->format);
	if (!bpp) {
		ALOGE("unrecognized format 0x%x", handle->format);
		return NULL;
	}

	width = handle->width;
	height = handle->height;
	gralloc_drm_align_geometry(handle->format, &width, &height);

	bo = calloc(1, sizeof(*bo));
	if (!bo) {
		ALOGE("failed to allocate dumb bo.");
		return NULL;
	}

	memset(&arg, 0, sizeof(arg));

//	pitch = ALIGN(width * bpp, 8); // 8 bytes algn ?
//	arg.pitch = pitch;
	arg.bpp = bpp;
	arg.width = width;
	arg.height = height;

	ret = drmIoctl(info->fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
	if (ret) {
		ALOGE("failed to create dumb bo.");
		goto err_free;
	}

	handle->stride = arg.pitch;
	bo->size = arg.pitch * height;

	bo->base.handle = handle;
	bo->base.fb_handle = arg.handle;

	return &bo->base;

err_free:
	free(bo);
	return NULL;
}

static int dumb_map(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *_bo,
		int x, int y, int w, int h,
		int enable_write, void **addr)
{
	struct dumb_info *info = (struct dumb_info *) drv;
	struct dumb_bo *bo = (struct dumb_bo *) _bo;
	struct drm_mode_map_dumb arg;
	void *map = NULL;
	int flags;
	int ret;

	UNUSED(x, y, w, h);

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->base.fb_handle;

	ret = drmIoctl(info->fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
	if (ret) {
		ALOGE("failed to map dumb bo, fb_handle=%x.",
				bo->base.fb_handle);
		return ret;
	}

	flags = PROT_READ;
	if (enable_write)
		flags |= PROT_WRITE;

	map = drm_mmap(0, bo->size, flags, MAP_SHARED, info->fd, arg.offset);
	if (map == MAP_FAILED) {
		ALOGE("failed to do drm_mmap, fb_handle=%x.",
				bo->base.fb_handle);
		return -errno;
	}

	*addr = map;

	return 0;
}

static void dumb_unmap(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *_bo)
{
	UNUSED(drv, _bo);

}

static void dumb_free(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *_bo)
{
	struct dumb_info *info = (struct dumb_info *) drv;
	struct dumb_bo *bo = (struct dumb_bo *) _bo;
	struct drm_mode_destroy_dumb arg;
	int ret;

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->base.fb_handle;

	ret = drmIoctl(info->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
	if (ret)
		ALOGE("failed to destroy dumb bo, fb_handle=%x.",
				bo->base.fb_handle);

	free(bo);
}

struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_dumb(int fd)
{
	struct dumb_info *info;
	int ret;
	uint64_t cap = 0;


	ret = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap);
	if (ret || cap == 0) {
		ALOGE("failed to check DRM_CAP_DUMB_BUFFER cap.");
		return NULL;
	}

	info = calloc(1, sizeof(*info));
	if (!info) {
		ALOGE("failed to allocate driver info.");
		return NULL;
	}

	info->fd = fd;
	info->base.destroy = dumb_destroy;
	info->base.alloc = dumb_alloc;
	info->base.free = dumb_free;
	info->base.map = dumb_map;
	info->base.unmap = dumb_unmap;
	//info->base.resolve_format = dumb_resolve_format; TODO: need??

	return &info->base;
}
