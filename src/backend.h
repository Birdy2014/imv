#ifndef IMV_BACKEND_H
#define IMV_BACKEND_H

#include <stddef.h>

struct imv_source;

enum backend_result {
  BACKEND_SUCCESS = 0, /* successful load */
  BACKEND_BAD_PATH = 1, /* no such file, bad permissions, etc. */
  BACKEND_UNSUPPORTED = 2, /* unsupported file format */
};

/* A backend is responsible for taking a path, or a raw data pointer, and
 * converting that into an imv_source. Each backend may be powered by a
 * different image library and support different image formats.
 */
struct imv_backend {
  /* Name of the backend, for debug and user informational purposes */
  const char *name;

  /* Information about the backend, displayed by help dialog */
  const char *description;

  /* Official website address */
  const char *website;

  /* License the backend is used under */
  const char *license;

  /* Input: path to open
   * Output: initialises the imv_source instance passed in
   */
  enum backend_result (*open_path)(const char *path, struct imv_source **src);

  /* Input: pointer to data and length of data
   * Output: initialises the imv_source instance passed in
   */
  enum backend_result (*open_memory)(void *data, size_t len, struct imv_source **src);
};

#endif
