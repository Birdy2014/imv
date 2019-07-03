#ifndef IMV_BINDS_H
#define IMV_BINDS_H

#include <unistd.h>

struct imv_binds;
struct list;

enum bind_result {
  BIND_SUCCESS,
  BIND_INVALID_KEYS,
  BIND_INVALID_COMMAND,
  BIND_CONFLICTS,
};

/* Create an imv_binds instance */
struct imv_binds *imv_binds_create(void);

/* Clean up an imv_binds instance */
void imv_binds_free(struct imv_binds *binds);

/* Create a key binding */
enum bind_result imv_binds_add(struct imv_binds *binds, const struct list *keys, const char *cmd);

/* Remove all key bindings */
void imv_binds_clear(struct imv_binds *binds);

/* Clear all bindings for a key*/
void imv_binds_clear_key(struct imv_binds *binds, const struct list *keys);

/* Fetch the list of keys pressed so far */
const struct list *imv_bind_input_buffer(struct imv_binds *binds);

/* Abort the current input key sequence */
void imv_bind_clear_input(struct imv_binds *binds);

/* Handle an input event, if a bind is triggered, return its command */
struct list *imv_bind_handle_event(struct imv_binds *binds, const char *event);

/* Convert a string (such as from a config) to a key list */
struct list *imv_bind_parse_keys(const char *keys);

/* Convert a key list to a string */
size_t imv_bind_print_keylist(const struct list *keys, char *buf, size_t len);

#endif
