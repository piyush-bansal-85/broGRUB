#ifndef BROTHEMECOMPILER_H
#define BROTHEMECOMPILER_H
#include <grub/types.h>

#define BRO_BACKGROUND_CONFIG  "broBg.bro"
#define BRO_FOREGROUND_CONFIG  "broFg.bro"
#define BRO_FALLBACK "others"
#define BRO_PATH "/bro-themes/"
#define IS_SPACE_BYTE(c) (c <= 0x20 || c == 0x7f)

/**
 * @brief contains the data for the bro-themes's data.
 * 
 * it has the key and the value and it compiles the data within the bro-themes' folder and then gives the value.
*/
typedef struct broThemeEntry {
    char* key;
    char* value;
} bte;

/**
 * @brief contains the data for the bro-themes's data.
 * 
 * it has the entries and the count.
*/
typedef struct broThemeData {
    bte *entries;
    grub_size_t count;
} btd;

/**
 * @brief contains the structure of loading the image.
 * 
 * it is generally created to implement the image to the grub.
*/
typedef struct grub_image_structure
{
  char *key;
  struct grub_video_bitmap *bitmap;
  struct grub_image_structure *next;
} bro_image;

/**
 * @param filename Full path to the theme configuration file to parse. use @b `get_bro_theme_path()` to get the path.
 *
 * Opens the specified configuration file and parses its contents into
 * key-value entries separated by ':'.
 *
 * Empty lines and lines beginning with '#' are ignored. Lines without
 * a ':' separator are also skipped.
 *
 * Returns: A pointer to a #btd structure containing the parsed entries,
 * or NULL if the file cannot be opened or memory allocation fails.
 *
 * The returned data must be freed using bro_free_theme_data() when
 * it is no longer needed.
 */
btd * broThemeCompiler(const char *filename);


/**
 * @brief Builds the path to a file inside the `/bro-themes` directory.
 *
 * @param filename Name of the configuration file, e.g. "broBg.txt".
 * @param suffix Suffix to folder before the filename, e.g. "/bro-themes/".
 * @return Dynamically allocated file path, or NULL on failure.
 *
 * @note The caller must release the returned path using grub_free().
 */
char *
get_bro_theme_path(const char *filename, const char *suffix);

char *
bro_trim_copy (const char *s);

/**
 * finds the current value of the 
*/
bro_image *
bro_find_image(bro_image* currentMapping, const char *key);

/**
 * @brief used in order to find the data from the key
 * 
 * if the data is not found, it returns NULL
 * 
 * @param themeData it needs the themedata which is of the type of btd*
 * @param key it needs the key which is of the type of char*
 * 
 * @return returns the bte* which is the key and value with the given key.
*/
bte* bro_find_bte(btd* themeData, const char *key);

/**
 * @brief gives the value of the bte data with the key
 * 
 * if the data not found, it returns NULL
 * 
 * @param themeData it needs the themedata which is of the type of btd*
 * @param key it needs the key which is of the type of char*
 * 
 * @return returns the value of the bte data with the given key.
*/
char* bro_get_bte_data(btd* themeData, const char *key);

/**
 * @brief Frees the memory allocated for the broThemeData structure.
 *
 * @param data Pointer to the broThemeData structure to be freed.
 */
void
bro_free_theme_data(btd *data);

#endif