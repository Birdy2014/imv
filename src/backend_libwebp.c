#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "source.h"
#include "source_private.h"

#include <stdlib.h>

#include <webp/decode.h>

struct private {
  FILE *file;
};

static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }

  struct private *private = raw_private;
  if (private->file) {
    fclose(private->file);
  }
  free(private);
}

static void load_image(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  fseek(private->file, 0L, SEEK_END);
  int filesize = ftell(private->file);
  rewind(private->file);

  uint8_t *buffer = malloc(filesize);
  fread(buffer, 1, filesize, private->file);

  int width;
  int height;
  uint8_t *raw_bmp = WebPDecodeBGRA(buffer, filesize, &width, &height);

  free(buffer);

  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = width;
  bmp->height = height;
  // "IMV_ARGB" means BGRA for some reason.
  bmp->format = IMV_ARGB;
  bmp->data = raw_bmp;
  *image = imv_image_create_from_bitmap(bmp);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = load_image,
  .free = free_private
};

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  unsigned char header[15];
  FILE *f = fopen(path, "rb");
  if (!f) {
    return BACKEND_BAD_PATH;
  }
  fread(header, 1, sizeof header, f);
  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WEBPVP8", 7) != 0) {
    fclose(f);
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = calloc(1, sizeof *private);
  private->file = f;

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_libwebp = {
  .name = "libwebp",
  .description = "",
  .website = "https://chromium.googlesource.com/webm/libwebp",
  .license = "BSD-3-Clause license",
  .open_path = &open_path,
};
