#include "broThemeCompiler.h"
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/env.h>
#include <grub/types.h>
#include <grub/normal.h>

GRUB_MOD_LICENSE ("GPLv3+");

btd *broThemeCompiler(const char *filename) {
    grub_file_t file;
    btd *data;
    char *line;

    file = grub_file_open(filename, GRUB_FILE_TYPE_NONE);

    if (!file) return NULL;

    data = grub_zalloc(sizeof(*data));

    if (!data) {
        grub_file_close(file);
        return NULL;
    }

    data->entries = NULL;
    data->count = 0;

    while ((line = grub_file_getline(file)) != NULL) {
        char *start = line;
        char *separator;
        char *key_end;
        char *value;
        char *value_end;
        bte *new_entries;

        /* Skip leading whitespace. */
        while (grub_isspace((unsigned char)*start)) start++;

        /* Ignore blank lines and comments. */
        if (*start == '\0' || *start == '/') {
            grub_free(line);
            continue;
        }

        separator = grub_strchr(start, ':');

        if (!separator) {
            grub_free(line);
            continue;
        }

        /* Trim whitespace at the end of the key. */
        key_end = separator;
        while (key_end > start && grub_isspace((unsigned char)key_end[-1]))
            key_end--;

        *key_end = '\0';

        /* Trim whitespace at the start and end of the value. */
        value = separator + 1;
        while (grub_isspace((unsigned char)*value)) value++;

        value_end = value + grub_strlen(value);
        while (value_end > value && grub_isspace((unsigned char)value_end[-1])) value_end--;

        *value_end = '\0';

        new_entries = grub_realloc(
            data->entries,
            sizeof(bte) * (data->count + 1)
        );

        if (!new_entries) {
            grub_free(line);
            /* Free existing entries too before returning. */
            grub_file_close(file);
            /* Call your data cleanup routine here. */
            return NULL;
        }

        data->entries = new_entries;
        data->entries[data->count].key = grub_strdup(start);
        data->entries[data->count].value = grub_strdup(value);

        if (!data->entries[data->count].key ||
            !data->entries[data->count].value) {
            grub_free(data->entries[data->count].key);
            grub_free(data->entries[data->count].value);
            grub_free(line);
            grub_file_close(file);
            /* Call your data cleanup routine here. */
            return NULL;
        }

        data->count++;
        grub_free(line);
    }

    grub_file_close(file);

    return data;
}

char *
get_bro_theme_path(const char *filename, const char *suffix)
{
    const char *theme_path = grub_env_get("theme");
    char *theme_dir;
    char *separator;
    char *config_path;
    grub_size_t dir_len;
    grub_size_t suffix_len = 0;
    grub_size_t filename_len;

    if (!theme_path || !filename) return NULL;

    theme_dir = grub_strdup(theme_path);
    if (!theme_dir) return NULL;

    /* Find the last '/' in the theme path. */
    separator = grub_strrchr(theme_dir, '/');
    if (!separator)
    {
        grub_free(theme_dir);
        return NULL;
    }

    /* Remove the theme filename, leaving its directory. */
    *separator = '\0';

    dir_len = grub_strlen(theme_dir);
    filename_len = grub_strlen(filename);

    /* A NULL suffix means no suffix should be added. */
    if (suffix) suffix_len = grub_strlen(suffix);

    config_path = grub_malloc(dir_len + suffix_len + filename_len + 1);
    if (!config_path)
    {
        grub_free(theme_dir);
        return NULL;
    }

    grub_snprintf(
        config_path,
        dir_len + suffix_len + filename_len + 1,
        "%s%s%s",
        theme_dir,
        suffix ? suffix : "",
        filename
    );

    grub_free(theme_dir);
    return config_path;
}

/* Small local helpers: this module must not depend on undeclared path or
   parser-cleanup functions from another object file. */
char *
bro_trim_copy (const char *s) {
  const unsigned char *begin;
  const unsigned char *end;
  char *out;
  grub_size_t n;

  if (!s) return NULL;

  begin = (const unsigned char *) s;
  while (*begin && IS_SPACE_BYTE (*begin)) begin++;

  end = begin + grub_strlen ((const char *) begin);
  while (end > begin && IS_SPACE_BYTE (end[-1])) end--;

  /* Strip UTF-8 non-breaking spaces from either end too. */
  while ((grub_size_t) (end - begin) >= 2
         && begin[0] == 0xc2 && begin[1] == 0xa0) begin += 2;
  while ((grub_size_t) (end - begin) >= 2
         && end[-2] == 0xc2 && end[-1] == 0xa0) end -= 2;

  n = (grub_size_t) (end - begin);
  out = grub_malloc (n + 1);
  if (!out) return NULL;

  grub_memcpy (out, begin, n);
  out[n] = '\0';
  return out;
}

bro_image *
bro_find_image(bro_image* currentMapping, const char *key)
{
    bro_image *image;

    if (!key) return NULL;

    for (image = currentMapping; image; image = image->next) {
        if (image->key && grub_strcmp(image->key, key) == 0) return image;
    }

    return NULL;
}

bte* bro_find_bte(btd* themeData, const char *key){
    bte *entry;
    grub_size_t count = 0;
    while (count < themeData->count) {
        entry = &themeData->entries[count];
        if (entry->key && grub_strcmp(entry->key, key) == 0) return entry;
        count++;
    }
    return NULL;
}

char*
bro_get_bte_data (btd *themeData, const char *key)
{
    bte *entry = bro_find_bte (themeData, key);
    return entry ? entry->value : NULL;
}

void
bro_free_theme_data(btd *data)
{
    grub_size_t i;

    if (!data) return;

    for (i = 0; i < data->count; i++) {
        grub_free(data->entries[i].key);
        grub_free(data->entries[i].value);
    }

    grub_free(data->entries);
    grub_free(data);
}
