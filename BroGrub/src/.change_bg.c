/**
 * BroGRUB background cache.
 *
 * Preloads every unique image referenced by broBg.bro, scales each image
 * for the current screen resolution, and reuses cached bitmaps when the
 * selected menu entry changes.
 *
 * Configuration keys are matched against GRUB menuentry --class values.
 * If no class matches, the "others" mapping is used.
 */

#include "broThemeCompiler.h"
#include <grub/dl.h>
#include <grub/menu.h>
#include <grub/menu_viewer.h>
#include <grub/gfxmenu_view.h>
#include <grub/video.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/bitmap.h>
#include <grub/file.h>

#ifndef GRUB_FILE
#define GRUB_FILE __FILE__
#endif

GRUB_MOD_LICENSE ("GPLv3+");

/* Small local helpers: this module must not depend on undeclared path or
   parser-cleanup functions from another object file. */
static int
bro_is_space_byte (unsigned char c)
{
  return c <= 0x20 || c == 0x7f;
}

static char *
bro_trim_copy (const char *s)
{
  const unsigned char *begin;
  const unsigned char *end;
  char *out;
  grub_size_t n;

  if (!s)
    return NULL;

  begin = (const unsigned char *) s;
  while (*begin && bro_is_space_byte (*begin))
    begin++;

  end = begin + grub_strlen ((const char *) begin);
  while (end > begin && bro_is_space_byte (end[-1]))
    end--;

  /* Strip UTF-8 non-breaking spaces from either end too. */
  while ((grub_size_t) (end - begin) >= 2
         && begin[0] == 0xc2 && begin[1] == 0xa0)
    begin += 2;
  while ((grub_size_t) (end - begin) >= 2
         && end[-2] == 0xc2 && end[-1] == 0xa0)
    end -= 2;

  n = (grub_size_t) (end - begin);
  out = grub_malloc (n + 1);
  if (!out)
    return NULL;

  grub_memcpy (out, begin, n);
  out[n] = '\0';
  return out;
}

static grub_gfxmenu_view_t bro_cached_view = NULL;

static bbi *bro_images = NULL;
static bbm *bro_mappings = NULL;

static grub_uint32_t bro_cache_width = 0;
static grub_uint32_t bro_cache_height = 0;
static int bro_cache_ready = 0;

/*
 * TRUE when the view's raw_desktop_image and scaled_desktop_image
 * are borrowed pointers into our cache rather than view-owned bitmaps.
 */
static int bro_view_uses_cached_image = 0;

/* ---------------------------------------------------------------
 * Cache lookup
 * --------------------------------------------------------------- */

static bbi *
bro_find_image_by_path(const char *path)
{
    bbi *image;

    if (!path)
        return NULL;

    for (image = bro_images; image; image = image->next)
    {
        if (image->path && grub_strcmp(image->path, path) == 0)
            return image;
    }

    return NULL;
}

static bbm *
bro_find_mapping(const char *key)
{
    bbm *mapping;

    if (!key)
        return NULL;

    for (mapping = bro_mappings; mapping; mapping = mapping->next)
    {
        if (mapping->key && grub_strcmp(mapping->key, key) == 0)
            return mapping;
    }

    return NULL;
}

/* ---------------------------------------------------------------
 * Cache cleanup
 * --------------------------------------------------------------- */

static void
bro_free_background_cache(void)
{
    bbm *mapping;
    bbi *image;

    while (bro_mappings)
    {
        mapping = bro_mappings;
        bro_mappings = mapping->next;

        grub_free(mapping->key);
        grub_free(mapping);
    }

    while (bro_images)
    {
        image = bro_images;
        bro_images = image->next;

        if (image->scaled)
            grub_video_bitmap_destroy(image->scaled);

        if (image->raw)
            grub_video_bitmap_destroy(image->raw);

        grub_free(image->path);
        grub_free(image);
    }

    bro_cache_width = 0;
    bro_cache_height = 0;
    bro_cache_ready = 0;
}

/*
 * Destroy the view safely. If its background pointers refer to cached
 * bitmaps, detach them first so the view cannot destroy cache-owned data.
 */
static void
bro_destroy_cached_view(void)
{
    if (!bro_cached_view) return;

    if (bro_view_uses_cached_image)
    {
        bro_cached_view->scaled_desktop_image = NULL;
        bro_cached_view->raw_desktop_image = NULL;
        bro_view_uses_cached_image = 0;
    }

    grub_gfxmenu_view_destroy(bro_cached_view);
    bro_cached_view = NULL;
}

/* ---------------------------------------------------------------
 * Load one image and scale it once.
 * Takes ownership of the allocated path, including on failure.
 * --------------------------------------------------------------- */

static bbi *
bro_load_image(char *path, grub_uint32_t width, grub_uint32_t height)
{
    bbi *image;

    if (!path) return NULL;

    image = grub_zalloc(sizeof(*image));

    if (!image)
    {
        grub_free(path);
        return NULL;
    }

    image->path = path;

    if (grub_video_bitmap_load(&image->raw, image->path))
    {

        grub_free(image->path);
        grub_free(image);
        return NULL;
    }

    if (grub_video_bitmap_create_scaled(
            &image->scaled,
            width,
            height,
            image->raw,
            GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST))
    {

        grub_video_bitmap_destroy(image->raw);
        grub_free(image->path);
        grub_free(image);
        return NULL;
    }

    image->next = bro_images;
    bro_images = image;

    return image;
}

/* ---------------------------------------------------------------
 * Add a key -> cached image mapping.
 * --------------------------------------------------------------- */

static int
bro_add_mapping(const char *key, bbi *image)
{
    bbm *mapping;

    if (!key || !image) return 0;

    mapping = grub_zalloc(sizeof(*mapping));

    if (!mapping) return 0;

    mapping->key = grub_strdup(key);

    if (!mapping->key)
    {
        grub_free(mapping);
        return 0;
    }

    mapping->image = image;
    mapping->next = bro_mappings;
    bro_mappings = mapping;

    return 1;
}

/* ---------------------------------------------------------------
 * Preload all configured backgrounds.
 *
 * Repeated paths share a single loaded/scaled image.
 * A failed image is logged and skipped; it does not abort the menu.
 * --------------------------------------------------------------- */

static int
bro_preload_backgrounds(grub_uint32_t width, grub_uint32_t height)
{
    btd *theme_data;
    char *config_path;
    char *image_path;
    bbi *image;
    grub_size_t i;

    bro_free_background_cache();

    /*
     * Resolve broBg.bro beside the active theme.txt through the existing
     * broThemeCompiler path helper. With the QEMU theme at
     * /boot/grub/themes/customWorld/theme.txt this resolves to
     * /boot/grub/themes/customWorld/broBg.bro.
     */
    config_path = get_bro_theme_path (BRO_BACKGROUND_CONFIG, BRO_PATH);
    if (!config_path)
      {

        return 0;
      }

    theme_data = broThemeCompiler (config_path);
    grub_free (config_path);

    if (!theme_data)
      {

        return 0;
      }

    for (i = 0; i < theme_data->count; i++)
    {
        char *key = bro_trim_copy (theme_data->entries[i].key);
        char *value = bro_trim_copy (theme_data->entries[i].value);

        if (!key || !value || !*key || !*value)
        {
            grub_free (key);
            grub_free (value);
            continue;
        }

        image_path = get_bro_theme_path(value, NULL);
        grub_free (value);

        if (!image_path)
        {

            grub_free (key);
            continue;
        }

        /* Reuse an image already loaded from this exact path. */
        image = bro_find_image_by_path(image_path);

        if (image) grub_free(image_path);
        else
        {
            image = bro_load_image(image_path, width, height);

            if (!image)
            {
                grub_free (key);
                continue;
            }
        }

        if (!bro_add_mapping(key, image))
        {

        }
        grub_free (key);
    }

    bro_free_theme_data (theme_data);

    bro_cache_width = width;
    bro_cache_height = height;
    bro_cache_ready = 1;

    if (!bro_find_mapping(BRO_FALLBACK))
    {

    }

    return 1;
}

/* ---------------------------------------------------------------
 * Find the cached background for a menu entry.
 *
 * GRUB menu entry classes use a dummy head node in the GRUB source
 * version this code targets, so real classes begin at classes->next.
 * --------------------------------------------------------------- */

static bbi *
bro_get_entry_background(grub_menu_entry_t entry)
{
    struct grub_menu_entry_class *class_entry;
    bbm *mapping;

    if (!entry)
        return NULL;

    for (class_entry = entry->classes;
         class_entry;
         class_entry = class_entry->next)
    {
        char *class_name;

        if (!class_entry->name)
            continue;

        /*
         * GRUB class strings can contain leading/trailing whitespace in
         * this build. Normalize the class before exact key lookup.
         */
        class_name = bro_trim_copy (class_entry->name);
        if (!class_name)
            continue;

        mapping = bro_find_mapping (class_name);
        grub_free (class_name);

        if (mapping && mapping->image)
        {

            return mapping->image;
        }
    }

    mapping = bro_find_mapping (BRO_FALLBACK);
    if (mapping && mapping->image)
    {

        return mapping->image;
    }

    return NULL;
}

/* ---------------------------------------------------------------
 * Switch the view to a cached background.
 *
 * Never destroy a cached bitmap during a selection change.
 * --------------------------------------------------------------- */

static void
bro_apply_entry_background(grub_gfxmenu_view_t view, int entry_index)
{
    grub_menu_entry_t menu_entry;
    bbi *image;

    if (!view || !view->menu)
        return;

    if (entry_index < 0 || view->menu->size <= 0 || entry_index >= (int) view->menu->size)
    {

        return;
    }

    menu_entry = grub_menu_get_entry(view->menu, entry_index);
    if (!menu_entry)
        return;

    image = bro_get_entry_background(menu_entry);

    /*
     * If resolution/configuration loading failed, keep the existing
     * background rather than replacing it with NULL.
     */
    if (!image) return;

    /*
     * The first switch replaces the view's original theme bitmaps.
     * Later switches only replace borrowed pointers; the cache retains
     * ownership of every cached image.
     */
    if (!bro_view_uses_cached_image)
    {
        if (view->scaled_desktop_image)
            grub_video_bitmap_destroy(view->scaled_desktop_image);

        if (view->raw_desktop_image)
            grub_video_bitmap_destroy(view->raw_desktop_image);
    }

    view->raw_desktop_image = image->raw;
    view->scaled_desktop_image = image->scaled;

    bro_view_uses_cached_image = 1;
}

/* ---------------------------------------------------------------
 * Menu selection callback.
 * --------------------------------------------------------------- */

static void
bro_set_chosen_entry(int entry, void *data)
{
    grub_gfxmenu_view_t view = data;

    if (!view)
        return;

    view->selected = entry;

    bro_apply_entry_background(view, entry);

    grub_gfxmenu_redraw_menu(view);

    grub_video_set_area_status(GRUB_VIDEO_AREA_DISABLED);
    grub_gfxmenu_view_redraw(view, &view->screen);
    grub_video_swap_buffers();

    if (view->double_repaint)
    {
        grub_video_set_area_status(GRUB_VIDEO_AREA_DISABLED);
        grub_gfxmenu_view_redraw(view, &view->screen);
    }
}

static void
bro_viewer_fini(void *data)
{
    (void) data;
}

/* ---------------------------------------------------------------
 * Create/reuse the graphical menu view.
 * --------------------------------------------------------------- */

static grub_err_t
bro_gfxmenu_try(int entry, grub_menu_t menu, int nested)
{
    const char *theme_path;
    struct grub_video_mode_info mode_info;
    struct grub_menu_viewer *viewer;

    theme_path = grub_env_get("theme");

    if (!theme_path)
        return grub_error(GRUB_ERR_FILE_NOT_FOUND, "theme is not set");

    viewer = grub_zalloc(sizeof(*viewer));

    if (!viewer)
        return grub_errno;

    if (grub_video_get_info(&mode_info))
    {
        grub_free(viewer);
        return grub_errno;
    }

    /*
     * If the view or resolution changes, detach borrowed images before
     * destroying the view, then rebuild the cache for the new size.
     */
    if (!bro_cached_view
        || grub_strcmp(bro_cached_view->theme_path, theme_path) != 0
        || bro_cached_view->screen.width != mode_info.width
        || bro_cached_view->screen.height != mode_info.height)
    {
        bro_destroy_cached_view();
        bro_free_background_cache();

        bro_cached_view = grub_gfxmenu_view_new(
            theme_path,
            mode_info.width,
            mode_info.height
        );

    }

    if (!bro_cached_view)
    {
        grub_free(viewer);
        return grub_errno;
    }

    bro_cached_view->selected = entry;
    bro_cached_view->menu = menu;
    bro_cached_view->nested = nested;
    bro_cached_view->first_timeout = -1;

    bro_cached_view->double_repaint =
        (mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_DOUBLE_BUFFERED)
        && !(mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_UPDATING_SWAP);

    grub_video_set_viewport(0, 0, mode_info.width, mode_info.height);

    if (bro_cached_view->double_repaint)
    {
        grub_video_swap_buffers();
        grub_video_set_viewport(0, 0, mode_info.width, mode_info.height);
    }

    /*
     * Preload all configured backgrounds once per view/resolution.
     * If preload fails, leave the theme's original background intact.
     */
    if (!bro_cache_ready
        || bro_cache_width != mode_info.width
        || bro_cache_height != mode_info.height)
    {
        bro_preload_backgrounds(mode_info.width, mode_info.height);
    }

    /*
     * Apply the initial selected entry before the first draw.
     * This also ensures the fallback is applied at startup if needed.
     */
    if (bro_cache_ready)
        bro_apply_entry_background(bro_cached_view, entry);

    grub_gfxmenu_view_draw(bro_cached_view);

    viewer->data = bro_cached_view;
    viewer->set_chosen_entry = bro_set_chosen_entry;
    viewer->print_timeout = grub_gfxmenu_print_timeout;
    viewer->clear_timeout = grub_gfxmenu_clear_timeout;
    viewer->fini = bro_viewer_fini;

    grub_menu_register_viewer(viewer);

    return GRUB_ERR_NONE;
}

/* ---------------------------------------------------------------
 * Module initialization and cleanup.
 * --------------------------------------------------------------- */

GRUB_MOD_INIT(bg_change)
{
    grub_gfxmenu_try_hook = bro_gfxmenu_try;
}

GRUB_MOD_FINI(bg_change)
{
    bro_destroy_cached_view();
    bro_free_background_cache();

    if (grub_gfxmenu_try_hook == bro_gfxmenu_try)
        grub_gfxmenu_try_hook = NULL;
}