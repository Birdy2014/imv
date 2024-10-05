#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "source.h"
#include "source_private.h"

#include <arpa/inet.h>

struct qoi_header {
  char magic[4];
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorspace;
};

uint8_t last_r(uint8_t *image, int output_byte_index) {
  if (output_byte_index - 4 < 0) {
    return 0;
  }
  return image[output_byte_index - 4];
}

uint8_t last_g(uint8_t *image, int output_byte_index) {
  if (output_byte_index - 3 < 0) {
    return 0;
  }
  return image[output_byte_index - 3];
}

uint8_t last_b(uint8_t *image, int output_byte_index) {
  if (output_byte_index - 2 < 0) {
    return 0;
  }
  return image[output_byte_index - 2];
}

uint8_t last_a(uint8_t *image, int output_byte_index) {
  if (output_byte_index - 1 < 0) {
    return 255;
  }
  return image[output_byte_index - 1];
}

uint8_t *parse_qoi(uint8_t *data, int size, int *width, int *height) {
  if (size < sizeof(struct qoi_header)) {
    return NULL;
  }

  struct qoi_header *header = (struct qoi_header*)data;
  *width = ntohl(header->width);
  *height = ntohl(header->height);

  uint8_t *image = malloc(*width * *height * 4);
  int input_byte_index = 14;
  int output_byte_index = 0;

  uint8_t prev_pixels[64 * 4];
  for (int i = 0; i < 64; ++i) {
    prev_pixels[i * 4] = 0;
    prev_pixels[i * 4 + 1] = 0;
    prev_pixels[i * 4 + 2] = 0;
    prev_pixels[i * 4 + 3] = 255;
  }

  while (input_byte_index < size && output_byte_index < *width * *height * 4) {
    if (data[input_byte_index] == 254) {
      // QOI_OP_RGB
      image[output_byte_index] = data[input_byte_index + 1]; // R
      image[output_byte_index + 1] = data[input_byte_index + 2]; // G
      image[output_byte_index + 2] = data[input_byte_index + 3]; // B
      image[output_byte_index + 3] = last_a(image, output_byte_index); // A

      input_byte_index += 4;
      output_byte_index += 4;
    } else if (data[input_byte_index] == 255) {
      // QOI_OP_RGBA
      image[output_byte_index] = data[input_byte_index + 1]; // R
      image[output_byte_index + 1] = data[input_byte_index + 2]; // G
      image[output_byte_index + 2] = data[input_byte_index + 3]; // B
      image[output_byte_index + 3] = data[input_byte_index + 4]; // A

      input_byte_index += 5;
      output_byte_index += 4;
    } else if ((data[input_byte_index] & 128) == 0 && (data[input_byte_index] & 64) == 0) {
      // QOI_OP_INDEX
      int color_index = data[input_byte_index] & 63;
      image[output_byte_index] = prev_pixels[color_index * 4];
      image[output_byte_index + 1] = prev_pixels[color_index * 4 + 1];
      image[output_byte_index + 2] = prev_pixels[color_index * 4 + 2];
      image[output_byte_index + 3] = prev_pixels[color_index * 4 + 3];

      input_byte_index += 1;
      output_byte_index += 4;
    } else if ((data[input_byte_index] & 128) == 0 && (data[input_byte_index] & 64) != 0) {
      // QOI_OP_DIFF
      image[output_byte_index] = last_r(image, output_byte_index) + ((data[input_byte_index] >> 4) & 3) - 2u;
      image[output_byte_index + 1] = last_g(image, output_byte_index) + ((data[input_byte_index] >> 2) & 3) - 2u;
      image[output_byte_index + 2] = last_b(image, output_byte_index) + (data[input_byte_index] & 3) - 2u;
      image[output_byte_index + 3] = last_a(image, output_byte_index);

      input_byte_index += 1;
      output_byte_index += 4;
    } else if ((data[input_byte_index] & 128) != 0 && (data[input_byte_index] & 64) == 0) {
      // QOI_OP_LUMA
      uint8_t green_diff = (data[input_byte_index] & 63) - 32u;
      image[output_byte_index] = last_r(image, output_byte_index) + ((data[input_byte_index + 1] >> 4) & 15) + green_diff - 8u;
      image[output_byte_index + 1] = last_g(image, output_byte_index) + green_diff;
      image[output_byte_index + 2] = last_b(image, output_byte_index) + (data[input_byte_index + 1] & 15) + green_diff - 8u;
      image[output_byte_index + 3] = last_a(image, output_byte_index);

      input_byte_index += 2;
      output_byte_index += 4;
    } else if ((data[input_byte_index] & 128) != 0 && (data[input_byte_index] & 64) != 0) {
      // QOI_OP_RUN
      for (int i = -1; i < (data[input_byte_index] & 63); ++i) {
        image[output_byte_index] = last_r(image, output_byte_index);
        image[output_byte_index + 1] = last_g(image, output_byte_index);
        image[output_byte_index + 2] = last_b(image, output_byte_index);
        image[output_byte_index + 3] = last_a(image, output_byte_index);
        output_byte_index += 4;
      }

      input_byte_index += 1;
    }

    uint8_t r = last_r(image, output_byte_index);
    uint8_t g = last_g(image, output_byte_index);
    uint8_t b = last_b(image, output_byte_index);
    uint8_t a = last_a(image, output_byte_index);
    uint8_t index_position = (r * 3 + g * 5 + b * 7 + a * 11) % 64;
    prev_pixels[index_position * 4] = r;
    prev_pixels[index_position * 4 + 1] = g;
    prev_pixels[index_position * 4 + 2] = b;
    prev_pixels[index_position * 4 + 3] = a;
  }

  return image;
}

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
  uint8_t *data = parse_qoi(buffer, filesize, &width, &height);

  free(buffer);

  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = width;
  bmp->height = height;
  // "IMV_ABGR" means RGBA for some reason.
  bmp->format = IMV_ABGR;
  bmp->data = data;
  *image = imv_image_create_from_bitmap(bmp);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = load_image,
  .free = free_private
};

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  unsigned char header[4];
  FILE *f = fopen(path, "rb");
  if (!f) {
    return BACKEND_BAD_PATH;
  }
  fread(header, 1, sizeof header, f);
  if (memcmp(header, "qoif", 4) != 0) {
    fclose(f);
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = calloc(1, sizeof *private);
  private->file = f;

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_qoi = {
  .name = "qoi",
  .description = "",
  .website = "https://qoiformat.org",
  .license = "",
  .open_path = &open_path,
};
