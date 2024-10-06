#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "log.h"
#include "source.h"
#include "source_private.h"

#include <fcntl.h>
#include <nsgif.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct private {
  nsgif_t* gif;
  void *data;
  size_t len;
};

static void* bitmap_create(int width, int height)
{
  const size_t bytes_per_pixel = 4;
  return calloc(width * height, bytes_per_pixel);
}

static void bitmap_destroy(void *bitmap)
{
  free(bitmap);
}

static unsigned char* bitmap_get_buffer(void *bitmap)
{
  return bitmap;
}

static void bitmap_set_opaque(void *bitmap, bool opaque)
{
  (void)bitmap;
  (void)opaque;
}

static bool bitmap_test_opaque(void *bitmap)
{
  (void)bitmap;
  return false;
}

static void bitmap_mark_modified(void *bitmap)
{
  (void)bitmap;
}

static nsgif_bitmap_cb_vt bitmap_callbacks = {
  bitmap_create,
  bitmap_destroy,
  bitmap_get_buffer,
  bitmap_set_opaque,
  bitmap_test_opaque,
  bitmap_mark_modified
};


static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }

  struct private *private = raw_private;
  nsgif_destroy(private->gif);
  munmap(private->data, private->len);
  free(private);
}

static void push_current_image(struct private *private,
    struct imv_image **image, int *frametime)
{
  const nsgif_info_t *info = nsgif_get_info(private->gif);

  uint32_t delay_cs;
  uint32_t frame_new;
  nsgif_rect_t area;

  nsgif_error code = nsgif_frame_prepare(private->gif, &area, &delay_cs, &frame_new);
  if (code != NSGIF_OK) {
    imv_log(IMV_DEBUG, "libnsgif: failed to prepare frame\n");
    return;
  }

  if (delay_cs == -1) {
    // The gif is not animated
    delay_cs = 0;
  }

  nsgif_bitmap_t* bitmap;

  code = nsgif_frame_decode(private->gif, frame_new, &bitmap);
  if (code != NSGIF_OK) {
    imv_log(IMV_DEBUG, "libnsgif: failed to decode frame\n");
    return;
  }

  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = info->width;
  bmp->height = info->height;
  bmp->format = IMV_ABGR;
  size_t len = 4 * bmp->width * bmp->height;
  bmp->data = malloc(len);
  memcpy(bmp->data, bitmap, len);

  *image = imv_image_create_from_bitmap(bmp);
  *frametime = delay_cs * 10.0;
}

static void first_frame(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  push_current_image(private, image, frametime);
}

static void next_frame(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  push_current_image(private, image, frametime);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = first_frame,
  .load_next_frame = next_frame,
  .free = free_private
};

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  struct private *private = calloc(1, sizeof *private);
  nsgif_create(&bitmap_callbacks, NSGIF_BITMAP_FMT_R8G8B8A8, &private->gif);

  nsgif_error code = nsgif_data_scan(private->gif, len, data);

  if (code != NSGIF_OK) {
    nsgif_destroy(private->gif);
    free(private);
    imv_log(IMV_DEBUG, "libsngif: unsupported file\n");
    return BACKEND_UNSUPPORTED;
  }

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  imv_log(IMV_DEBUG, "libnsgif: open_path(%s)\n", path);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return BACKEND_BAD_PATH;
  }

  off_t len = lseek(fd, 0, SEEK_END);
  if (len < 0) {
    close(fd);
    return BACKEND_BAD_PATH;
  }

  void *data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (data == MAP_FAILED || !data) {
    return BACKEND_BAD_PATH;
  }

  struct private *private = calloc(1, sizeof *private);
  private->data = data;
  private->len = len;
  nsgif_create(&bitmap_callbacks, NSGIF_BITMAP_FMT_R8G8B8A8, &private->gif);

  nsgif_error code = nsgif_data_scan(private->gif, private->len, private->data);

  if (code != NSGIF_OK) {
    nsgif_destroy(private->gif);
    munmap(private->data, private->len);
    free(private);
    imv_log(IMV_DEBUG, "libsngif: unsupported file\n");
    return BACKEND_UNSUPPORTED;
  }

  nsgif_data_complete(private->gif);

  const nsgif_info_t *info = nsgif_get_info(private->gif);

  imv_log(IMV_DEBUG, "libnsgif: num_frames=%d\n", info->frame_count);
  imv_log(IMV_DEBUG, "libnsgif: width=%d\n", info->width);
  imv_log(IMV_DEBUG, "libnsgif: height=%d\n", info->height);

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}


const struct imv_backend imv_backend_libnsgif = {
  .name = "libnsgif",
  .description = "Tiny GIF decoding library from the NetSurf project",
  .website = "https://www.netsurf-browser.org/projects/libnsgif/",
  .license = "MIT",
  .open_path = &open_path,
  .open_memory = &open_memory,
};
