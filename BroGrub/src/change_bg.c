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
#include "change_bg.h"
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


// static grub_gfxmenu_view_t bro_cached_view = NULL;

static bro_image *bro_images = NULL;

static grub_uint32_t bro_cache_width = 0;
static grub_uint32_t bro_cache_height = 0;
static int bro_cache_ready = 0;

/*
 * TRUE when the view's raw_desktop_image and scaled_desktop_image
 * are borrowed pointers into our cache rather than view-owned bitmaps.
 */
static int bro_view_uses_cached_image = 0;
static grub_gfxmenu_view_t bro_active_view = NULL;
static struct grub_video_bitmap *bro_scaled_background = NULL;


/* ---------------------------------------------------------------
 * Cache cleanup
 * --------------------------------------------------------------- */

static void
bro_free_background_cache(void)
{
    bro_image *image;

    while (bro_images)
    {
        image = bro_images;
        bro_images = image->next;

        if (image->bitmap)
            grub_video_bitmap_destroy(image->bitmap);

        grub_free(image->key);
        grub_free(image);
    }

    bro_cache_width = 0;
    bro_cache_height = 0;
    bro_cache_ready = 0;
}

/* ---------------------------------------------------------------
 * Load one normal image into a bro_image.
 * Takes ownership of key and path, including on failure.
 * --------------------------------------------------------------- */

static bro_image *
bro_load_image(char *key, char *path)
{
    bro_image *image;

    if (!key || !path)
    {
        grub_free(key);
        grub_free(path);
        return NULL;
    }

    image = grub_zalloc(sizeof(*image));
    if (!image)
    {
        grub_free(key);
        grub_free(path);
        return NULL;
    }

    image->key = key;

    if (grub_video_bitmap_load(&image->bitmap, path))
    {
        grub_free(path);
        grub_free(image->key);
        grub_free(image);
        grub_errno = GRUB_ERR_NONE;
        return NULL;
    }

    grub_free(path);

    image->next = bro_images;
    bro_images = image;

    return image;
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
    bro_image *image;
    grub_size_t i;

    bro_free_background_cache();

    /*
     * Resolve broBg.bro beside the active theme.txt through the existing
     * broThemeCompiler path helper.
     */
    config_path = get_bro_theme_path (BRO_BACKGROUND_CONFIG, BRO_PATH);
    if (!config_path)
        return 0;

    theme_data = broThemeCompiler (config_path);
    grub_free (config_path);

    if (!theme_data) return 0;

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

        /* Keep one loaded image for each key. */
        if (bro_find_image(bro_images, key))
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

        image = bro_load_image(key, image_path);
        if (!image)
            continue;
    }

    bro_free_theme_data (theme_data);

    bro_cache_width = width;
    bro_cache_height = height;
    bro_cache_ready = 1;

    return 1;
}

/* ---------------------------------------------------------------
 * Find the cached background for a menu entry.
 *
 * GRUB menu entry classes use a dummy head node in the GRUB source
 * version this code targets, so real classes begin at classes->next.
 * --------------------------------------------------------------- */

static bro_image *
bro_get_entry_background(grub_menu_entry_t entry)
{
    struct grub_menu_entry_class *class_entry;
    bro_image *image;

    if (!entry) return NULL;

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

        image = bro_find_image(bro_images, class_name);
        grub_free (class_name);

        if (image)
            return image;
    }

    return bro_find_image(bro_images, BRO_FALLBACK);
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
    bro_image *image;
    struct grub_video_bitmap *scaled = NULL;

    if (!view || !view->menu) return;

    if (entry_index < 0 || view->menu->size <= 0 || entry_index >= (int) view->menu->size) return;

    menu_entry = grub_menu_get_entry(view->menu, entry_index);
    if (!menu_entry) return;

    image = bro_get_entry_background(menu_entry);

    /*
     * If no configured image matches, keep the existing background.
     */
    if (!image || !image->bitmap) return;

    /*
     * Scale the normal bitmap for this view. bro_image itself stores
     * only the original bitmap.
     */
    if (grub_video_bitmap_create_scaled(
            &scaled,
            view->screen.width,
            view->screen.height,
            image->bitmap,
            GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST))
    {
        grub_errno = GRUB_ERR_NONE;
        return;
    }

    /*
     * The first switch replaces the view's original theme bitmaps.
     * Later switches replace only the previously-created scaled bitmap.
     */
    if (!bro_view_uses_cached_image)
    {
        if (view->scaled_desktop_image)
            grub_video_bitmap_destroy(view->scaled_desktop_image);

        if (view->raw_desktop_image)
            grub_video_bitmap_destroy(view->raw_desktop_image);
    }
    else if (bro_scaled_background)
    {
        grub_video_bitmap_destroy(bro_scaled_background);
    }

    view->raw_desktop_image = image->bitmap;
    view->scaled_desktop_image = scaled;
    bro_scaled_background = scaled;
    bro_view_uses_cached_image = 1;
}


/* ---------------------------------------------------------------
 * Public interface used by carousel.c.
 * The carousel owns the gfxmenu view and its menu viewer.
 * This module only manages cached backgrounds.
 * --------------------------------------------------------------- */

void
bro_background_init(grub_gfxmenu_view_t view, int entry)
{
    if (!view) return;

    bro_active_view = view;

    if (!bro_cache_ready
        || bro_cache_width != view->screen.width
        || bro_cache_height != view->screen.height)
    {
        /*
         * Detach an old cached background before preload frees the cache.
         */
        if (bro_view_uses_cached_image)
        {
            view->scaled_desktop_image = NULL;
            view->raw_desktop_image = NULL;

            if (bro_scaled_background)
                grub_video_bitmap_destroy(bro_scaled_background);

            bro_scaled_background = NULL;
            bro_view_uses_cached_image = 0;
        }

        bro_preload_backgrounds(view->screen.width,
                                view->screen.height);
    }

    if (bro_cache_ready)
        bro_apply_entry_background(view, entry);
}

void
bro_background_set_entry(grub_gfxmenu_view_t view, int entry)
{
    if (!view) return;

    bro_active_view = view;

    if (!bro_cache_ready
        || bro_cache_width != view->screen.width
        || bro_cache_height != view->screen.height)
        bro_background_init(view, entry);
    else
        bro_apply_entry_background(view, entry);
}

void
bro_background_cleanup(grub_gfxmenu_view_t view)
{
    grub_gfxmenu_view_t target = view ? view : bro_active_view;

    /*
     * The view borrows these bitmaps. Detach them before freeing the cache.
     */
    if (target && bro_view_uses_cached_image)
    {
        target->scaled_desktop_image = NULL;
        target->raw_desktop_image = NULL;
    }

    if (bro_scaled_background) grub_video_bitmap_destroy(bro_scaled_background);

    bro_scaled_background = NULL;
    bro_view_uses_cached_image = 0;

    if (!view || bro_active_view == view)
        bro_active_view = NULL;

    bro_free_background_cache();
}

GRUB_MOD_INIT(bg_change)
{
    /* carousel.c owns grub_gfxmenu_try_hook. */
}

GRUB_MOD_FINI(bg_change)
{
    bro_background_cleanup(NULL);
}
