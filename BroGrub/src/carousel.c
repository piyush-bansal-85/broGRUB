/*
 * carousel.c - Standalone BroGRUB graphical menu viewer.
 *
 * The module hooks into the existing CoreOS gfxmenu try-hook.  It creates
 * the normal gfxmenu view from the selected theme, replaces only the theme's
 * menu-list component with this carousel, and leaves the theme background,
 * title, timeout widgets, messages, and all other components intact.
 *
 * GRUB's normal menu controller still owns selection, timeout, edit, submenu,
 * and boot behaviour.  This viewer only changes how menu entries are drawn.
 */

#include <grub/dl.h>
#include <grub/menu_viewer.h>
#include <grub/normal.h>
#include <grub/err.h>
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/font.h>
#include <grub/menu.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/gfxterm.h>
#include <grub/gui.h>
#include <grub/gfxmenu_view.h>
#include <grub/icon_manager.h>

#include "broThemeCompiler.h"
#include "change_bg.h"
#include "loading.h"

GRUB_MOD_LICENSE ("GPLv3+");

#define CAROUSEL_CONFIG_FILE "carouselConfig.bro"

#ifndef GRUB_FILE
#define GRUB_FILE __FILE__
#endif

struct grub_gui_carousel
{
  struct grub_gui_list list;
  char *id;
  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  grub_gfxmenu_view_t view;
  grub_gfxmenu_icon_manager_t icon_manager;
  bro_image *images;
  int icon_width;
  int icon_height;
  int icon_gap;
  int side_scale_num;
  int side_scale_den;
  int min_width;
  int min_height;
  int carousel_gap;
  int text_gap;
  int selected;
  int previous_selected;
};

static struct grub_gui_component_ops carousel_component_ops;
static struct grub_gui_list_ops carousel_list_ops;

static int
carousel_key_equal (const char *left, const char *right)
{
  if (!left || !right) return 0;

  while (*left && *right)
    {
      char a = *left++;
      char b = *right++;

      if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
      if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';

      if (a != b) return 0;
    }

  return *left == '\0' && *right == '\0';
}

static bro_image *
carousel_find_image (struct grub_gui_carousel *carousel, const char *key)
{
    bro_image *image;

    for (image = carousel->images; image; image = image->next)
        if (carousel_key_equal (image->key, key)) return image;

    return NULL;
}

static void
carousel_free_images (struct grub_gui_carousel *carousel)
{
  bro_image *image;

  while (carousel->images)
    {
      image = carousel->images;
      carousel->images = image->next;

      if (image->bitmap) grub_video_bitmap_destroy (image->bitmap);

      grub_free (image->key);
      grub_free (image);
    }
}

static int
carousel_config_value (btd *data, const char *key, int fallback)
{
  char *value;

  value = bro_get_bte_data (data, key);
  if (!value || !*value)
    return fallback;

  return (int) grub_strtoul (value, NULL, 10);
}

static void
carousel_load_config (struct grub_gui_carousel *carousel)
{
  btd *data;
  char *config;

  config = get_bro_theme_path (CAROUSEL_CONFIG_FILE, BRO_PATH);
  if (!config)
    return;

  data = broThemeCompiler (config);
  grub_free (config);
  if (!data)
    return;

  carousel->icon_width = carousel_config_value (data, "icon_width", carousel->icon_width);
  carousel->icon_height = carousel_config_value (data, "icon_height", carousel->icon_height);
  carousel->icon_gap = carousel_config_value (data, "icon_gap", carousel->icon_gap);
  carousel->side_scale_num = carousel_config_value (data, "carousel_side_scale_num", carousel->side_scale_num);
  carousel->side_scale_den = carousel_config_value (data, "carousel_side_scale_den", carousel->side_scale_den);
  carousel->min_width = carousel_config_value (data, "carousel_min_width", carousel->min_width);
  carousel->min_height = carousel_config_value (data, "carousel_min_height", carousel->min_height);
  carousel->carousel_gap = carousel_config_value (data, "carousel_gap", carousel->carousel_gap);
  carousel->text_gap = carousel_config_value (data, "carousel_text_gap", carousel->text_gap);

  /* Avoid division by zero in side-image scaling. */
  if (carousel->side_scale_den <= 0) carousel->side_scale_den = 6;

  bro_free_theme_data (data);
}

static void
carousel_load_images (struct grub_gui_carousel *carousel)
{
  btd *theme_data;
  char *config;
  grub_size_t i;

  carousel_free_images (carousel);

  config = get_bro_theme_path (BRO_FOREGROUND_CONFIG, BRO_PATH);
  if (!config) return;

  theme_data = broThemeCompiler (config);
  grub_free (config);

  if (!theme_data) return;

  for (i = 0; i < theme_data->count; i++)
    {
      bro_image *image;
      char *key;
      char *path;

      key = bro_trim_copy (theme_data->entries[i].key);
      path = get_bro_theme_path (theme_data->entries[i].value, NULL);


      if (!key || !*key || !path) {
          grub_free (key);
          grub_free (path);
          continue;
      }

      /* Keep one image per key. */
      if (carousel_find_image (carousel, key))
        {
          grub_free (key);
          grub_free (path);
          continue;
        }

      image = grub_zalloc (sizeof (*image));
      if (!image)
        {
          grub_free (key);
          grub_free (path);
          continue;
        }

      image->key = key;

      /*
       * The path only needs to exist while the bitmap is being loaded.
       * bro_image keeps the key and the loaded bitmap, not the config path.
       */
      if (grub_video_bitmap_load (&image->bitmap, path))
        {
          grub_free (path);
          grub_free (image->key);
          grub_free (image);
          grub_errno = GRUB_ERR_NONE;
          continue;
        }

      grub_free (path);

      image->next = carousel->images;
      carousel->images = image;
    }

  bro_free_theme_data (theme_data);
  {
    bro_image *loaded;
    unsigned loaded_count = 0;
    for (loaded = carousel->images; loaded; loaded = loaded->next)
      loaded_count++;
  }
}

static bro_image *
image_for_entry (struct grub_gui_carousel *carousel,
                          grub_menu_entry_t entry)
{
  struct grub_menu_entry_class *class_entry;
  bro_image *image;

  if (!entry) return NULL;

  for (class_entry = entry->classes; class_entry;
       class_entry = class_entry->next)
    {
      if (!class_entry->name)  continue;

      image = carousel_find_image (carousel, class_entry->name);
      if (image) return image;
    }

  image = carousel_find_image (carousel, BRO_FALLBACK);
  return image;
}

static void
carousel_blit_scaled (struct grub_video_bitmap *bitmap,
                      int x, int y, int width, int height)
{
  struct grub_video_bitmap *scaled = NULL;


  if (!bitmap || width <= 0 || height <= 0) return;

  if (grub_video_bitmap_create_scaled (&scaled, width, height, bitmap,
                                       GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST))
    {
      grub_errno = GRUB_ERR_NONE;
      return; 
    }

  grub_video_blit_bitmap (scaled, GRUB_VIDEO_BLIT_BLEND,
                                x, y, 0, 0, width, height);
    
  grub_video_bitmap_destroy (scaled);
}

static int
carousel_entry_count (grub_menu_t menu)
{
  grub_menu_entry_t entry;
  int count = 0;

  if (!menu) return 0;

  for (entry = menu->entry_list; entry; entry = entry->next) count++;

  return count;
}

static grub_menu_entry_t
carousel_get_entry (grub_menu_t menu, int index)
{
  grub_menu_entry_t entry;
  int i;

  if (!menu || index < 0) return NULL;

  entry = menu->entry_list;
  for (i = 0; entry && i < index; i++) entry = entry->next;

  return entry;
}

static void
carousel_draw_title (struct grub_gui_carousel *carousel,
                     grub_menu_entry_t entry, int center_x, int baseline_y)
{
  grub_video_color_t color;
  struct grub_video_bitmap *icon;
  int width;
  int icon_width = carousel->icon_width;
  int icon_height = carousel->icon_height;
  int icon_gap = carousel->icon_gap;
  int total_width;
  int icon_baseLine_y = baseline_y - (carousel->text_gap - 10);
  int start_x;

  if (!carousel->view || !entry || !entry->title
      || !carousel->view->message_font) return;

  color = grub_video_map_rgba_color (carousel->view->message_color);

  width = grub_font_get_string_width (carousel->view->message_font,
                                      entry->title);

  icon = carousel->icon_manager
    ? grub_gfxmenu_icon_manager_get_icon (carousel->icon_manager, entry)
    : NULL;

  total_width = width;
  if (icon) total_width += icon_width + icon_gap;

  start_x = center_x - total_width / 2;

  if (icon)
    {
      grub_video_blit_bitmap (icon, GRUB_VIDEO_BLIT_BLEND,
                              start_x, icon_baseLine_y,
                              0, 0, icon_width, icon_height);
      start_x += icon_width + icon_gap;
    }

  grub_font_draw_string (entry->title, carousel->view->message_font,
                         color, start_x, baseline_y);
}

static void
carousel_paint (void *self, const grub_video_rect_t *region)
{
  struct grub_gui_carousel *carousel = self;
  grub_menu_entry_t entry;
  int count, index;
  int center_x, center_y;
  int active_width, active_height;
  int side_width, side_height;
  int gap, x, y;
  bro_image *image;


  if (!carousel->view || !carousel->view->menu) return;

  if (region && !grub_video_have_common_points (&carousel->bounds, region)) return;

  /* gfxmenu_view_redraw() already clips painting to the component region. */
  count = carousel_entry_count (carousel->view->menu);
  if (count <= 0) return;

  index = carousel->selected;
  if (index < 0) index = 0;
  if (index >= count) index = count - 1;

  center_x = carousel->view->screen.width / 2;
  center_y = carousel->bounds.y + carousel->bounds.height / 2;

    active_width = carousel->bounds.width * 90 / 100;
  if (active_width > 850)
    active_width = 850;
  if (active_width < 160)
    active_width = 160;

  active_height = carousel->bounds.height * 95 / 100;
  if (active_height > 500)
    active_height = 500;
  if (active_height < 120)
    active_height = 120;

  side_width = active_width * carousel->side_scale_num
               / carousel->side_scale_den;
  side_height = active_height * carousel->side_scale_num
                / carousel->side_scale_den;
  gap = carousel->carousel_gap;

  /*
   * Lay out every menu entry on one horizontal track.  Entries farther
   * from the selection remain smaller; the selected entry is the large
   * center card.  The canvas viewport clips cards outside this component.
   */
  for (int item_index = 0; item_index < count; item_index++)
    {
      int distance = item_index - index;
      int item_width, item_height;

      entry = carousel_get_entry (carousel->view->menu, item_index);
      image = image_for_entry (carousel, entry);

      if (distance == 0)
        {
          item_width = active_width;
          item_height = active_height;
          x = center_x - item_width / 2;
        }
      else if (distance < 0)
        {
          item_width = side_width;
          item_height = side_height;
          x = center_x - active_width / 2 - gap
              - side_width + (distance + 1) * (side_width + gap);
        }
      else
        {
          item_width = side_width;
          item_height = side_height;
          x = center_x + active_width / 2 + gap
              + (distance - 1) * (side_width + gap);
        }

      y = center_y - item_height / 2;

      if (image) carousel_blit_scaled (image->bitmap, x, y,
                              item_width, item_height);

      if (distance == 0) carousel_draw_title (carousel, entry, x + item_width / 2,
                           y + item_height + carousel->text_gap);
    }

}

static void
carousel_destroy (void *self)
{
  struct grub_gui_carousel *carousel = self;

  carousel_free_images (carousel);
  if (carousel->icon_manager)
    grub_gfxmenu_icon_manager_destroy (carousel->icon_manager);
  grub_free (carousel->id);
  grub_free (carousel);
}

static const char *
carousel_get_id (void *self)
{
  struct grub_gui_carousel *carousel = self;
  return carousel->id;
}

static int
carousel_is_instance (void *self, const char *type)
{
  (void) self;
  return (grub_strcmp (type, "carousel") == 0
          || grub_strcmp (type, "list") == 0);
}

static void
carousel_set_parent (void *self, grub_gui_container_t parent)
{
  struct grub_gui_carousel *carousel = self;
  carousel->parent = parent;
}

static grub_gui_container_t
carousel_get_parent (void *self)
{
  struct grub_gui_carousel *carousel = self;
  return carousel->parent;
}

static void
carousel_set_bounds (void *self, const grub_video_rect_t *bounds)
{
  struct grub_gui_carousel *carousel = self;

  if (bounds)
    {
      carousel->bounds = *bounds;
      carousel->list.component.x = bounds->x;
      carousel->list.component.y = bounds->y;
      carousel->list.component.w = bounds->width;
      carousel->list.component.h = bounds->height;
    }
}

static void
carousel_get_bounds (void *self, grub_video_rect_t *bounds)
{
  struct grub_gui_carousel *carousel = self;

  if (bounds) *bounds = carousel->bounds;
}

static void
carousel_get_minimal_size (void *self, unsigned *width, unsigned *height)
{
  struct grub_gui_carousel *carousel = self;

  if (width) *width = carousel->min_width;
  if (height) *height = carousel->min_height;
}

static grub_err_t
carousel_set_property (void *self, const char *name, const char *value)
{
  struct grub_gui_carousel *carousel = self;

  if (grub_strcmp (name, "id") == 0)
    {
      char *copy = grub_strdup (value);
      if (!copy)
        return grub_errno;
      grub_free (carousel->id);
      carousel->id = copy;
      return GRUB_ERR_NONE;
    }

  return grub_error (GRUB_ERR_BAD_ARGUMENT,
                     "unknown carousel property '%s'", name);
}

static void
carousel_repaint (void *self, int second_pass)
{
  struct grub_gui_carousel *carousel = self;
  (void) second_pass;

  if (carousel->view)
    grub_gfxmenu_redraw_menu (carousel->view);
}

static void
carousel_set_view_info (void *self, grub_gfxmenu_view_t view)
{
  struct grub_gui_carousel *carousel = self;

  carousel->view = view;
  if (view)
    {
      if (carousel->icon_manager)
        grub_gfxmenu_icon_manager_set_theme_path (
            carousel->icon_manager, view->theme_path);

      carousel->selected = view->selected;
      carousel->previous_selected = view->selected;
    }


  if (!carousel->images) carousel_load_images (carousel);
}

static void
carousel_refresh_list (void *self, grub_gfxmenu_view_t view)
{
  struct grub_gui_carousel *carousel = self;

  if (!view) return;

  carousel->view = view;
  carousel->previous_selected = carousel->selected;
  carousel->selected = view->selected;

  if (carousel->selected < 0)
    carousel->selected = 0;

  /* The view redraw paints the canvas immediately after this callback. */
}

grub_gui_component_t
grub_gui_carousel_new (void);
grub_gui_component_t
grub_gui_carousel_new (void)
{
  struct grub_gui_carousel *carousel;

  carousel = grub_zalloc (sizeof (*carousel));
  if (!carousel)
    return NULL;

  carousel->list.component.ops = &carousel_component_ops;
  carousel->list.ops = &carousel_list_ops;
  carousel->list.component.x = 0;
  carousel->list.component.y = 0;
  carousel->list.component.w = 0;
  carousel->list.component.h = 0;
  carousel->selected = 0;
  carousel->previous_selected = 0;
  carousel_load_config (carousel);
  carousel->id = grub_strdup ("carousel");

  if (!carousel->id)
    {
      grub_free (carousel);
      return NULL;
    }

  carousel->icon_manager = grub_gfxmenu_icon_manager_new ();
  if (!carousel->icon_manager)
    {
      grub_free (carousel->id);
      grub_free (carousel);
      return NULL;
    }

  grub_gfxmenu_icon_manager_set_icon_size (
      carousel->icon_manager, carousel->icon_width, carousel->icon_height);

  return (grub_gui_component_t) carousel;
}

static struct grub_gui_component_ops carousel_component_ops =
{
  .destroy = carousel_destroy,
  .get_id = carousel_get_id,
  .is_instance = carousel_is_instance,
  .paint = carousel_paint,
  .set_parent = carousel_set_parent,
  .get_parent = carousel_get_parent,
  .set_bounds = carousel_set_bounds,
  .get_bounds = carousel_get_bounds,
  .get_minimal_size = carousel_get_minimal_size,
  .set_property = carousel_set_property,
  .repaint = carousel_repaint
};

static struct grub_gui_list_ops carousel_list_ops =
{
  .set_view_info = carousel_set_view_info,
  .refresh_list = carousel_refresh_list
};


/* -------------------------------------------------------------------------
 * Standalone gfxmenu viewer integration.
 * ------------------------------------------------------------------------- */

struct carousel_replace_data
{
  grub_gui_component_t old_list;
};

static void
carousel_find_list (grub_gui_component_t component, void *userdata)
{
  struct carousel_replace_data *data = userdata;

  if (!data->old_list && component->ops->is_instance (component, "list"))
    data->old_list = component;
}

static grub_err_t
carousel_replace_theme_list (grub_gfxmenu_view_t view)
{
  struct carousel_replace_data found;
  grub_gui_component_t old_list;
  grub_gui_component_t new_component;
  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  const char *old_id;
  struct grub_gui_carousel *carousel;

  found.old_list = NULL;
  grub_gui_iterate_recursively ((grub_gui_component_t) view->canvas,
                                carousel_find_list, &found);
  old_list = found.old_list;

  if (!old_list)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "BroGRUB carousel: theme has no menu list component");

  parent = old_list->ops->get_parent (old_list);
  if (!parent)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "BroGRUB carousel: menu list has no parent");

  bounds = view->screen;
  old_id = old_list->ops->get_id (old_list);

  new_component = grub_gui_carousel_new ();
  if (!new_component)
    return grub_errno;

  carousel = (struct grub_gui_carousel *) new_component;
  if (old_id && *old_id)
    {
      char *id_copy = grub_strdup (old_id);
      if (!id_copy)
        {
          new_component->ops->destroy (new_component);
          return grub_errno;
        }
      grub_free (carousel->id);
      carousel->id = id_copy;
    }

  new_component->ops->set_bounds (new_component, &bounds);
  new_component->ops->set_parent (new_component, parent);

  /* Replace only the list; every other theme component remains untouched. */
  parent->ops->remove (parent, old_list);
  old_list->ops->destroy (old_list);
  parent->ops->add (parent, new_component);

  carousel->list.ops->set_view_info (carousel, view);
  carousel->list.ops->refresh_list (carousel, view);
  return GRUB_ERR_NONE;
}

static grub_gfxmenu_view_t carousel_cached_view;

static void
carousel_set_chosen_entry (int entry, void *data)
{
  grub_gfxmenu_view_t view = data;

  if (!view) return;

  view->selected = entry;

  bro_background_set_entry(view, entry);

  grub_gfxmenu_redraw_menu (view);
  grub_video_set_area_status (GRUB_VIDEO_AREA_DISABLED);
  grub_gfxmenu_view_redraw (view, &view->screen);
  grub_video_swap_buffers ();
  if (view->double_repaint)
    {
      grub_video_set_area_status (GRUB_VIDEO_AREA_DISABLED);
      grub_gfxmenu_view_redraw (view, &view->screen);
    }
}

static void
carousel_viewer_fini (void *data)
{
  /* The view is cached and reused for submenu/menu refreshes. */
  (void) data;
}

static void
carousel_destroy_cached_view (void)
{
  if (carousel_cached_view)
    {
      bro_background_cleanup(carousel_cached_view);
      grub_gfxmenu_view_destroy (carousel_cached_view);
      carousel_cached_view = NULL;
    }
}

static grub_err_t
carousel_try_hook (int entry, grub_menu_t menu, int nested)
{
  const char *theme_path = grub_env_get ("theme");
  struct grub_video_mode_info mode_info;
  struct grub_menu_viewer *viewer;


  if (!theme_path || !*theme_path)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND,
                       "BroGRUB carousel requires the theme variable");

  if (grub_video_get_info (&mode_info))
    {
      return grub_errno;
    }

  viewer = grub_zalloc (sizeof (*viewer));
  if (!viewer)
    return grub_errno;

  if (!carousel_cached_view
      || !carousel_cached_view->theme_path
      || grub_strcmp (carousel_cached_view->theme_path, theme_path) != 0
      || carousel_cached_view->screen.width != mode_info.width
      || carousel_cached_view->screen.height != mode_info.height)
    {
      carousel_destroy_cached_view ();
      carousel_cached_view = grub_gfxmenu_view_new (theme_path,
                                                     mode_info.width,
                                                     mode_info.height);
      if (!carousel_cached_view)
        {
          grub_free (viewer);
          return grub_errno;
        }

        draw_loading_screen (carousel_cached_view);

      carousel_cached_view->double_repaint =
        (mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_DOUBLE_BUFFERED)
        && !(mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_UPDATING_SWAP);

      if (carousel_replace_theme_list (carousel_cached_view))
        {
          carousel_destroy_cached_view ();
          grub_free (viewer);
          return grub_errno;
        }
    }

  carousel_cached_view->selected = entry;
  carousel_cached_view->menu = menu;
  carousel_cached_view->nested = nested;
  carousel_cached_view->first_timeout = -1;

  bro_background_init(carousel_cached_view, entry);

  grub_video_set_viewport (0, 0, mode_info.width, mode_info.height);
  if (carousel_cached_view->double_repaint)
    {
      grub_video_swap_buffers ();
      grub_video_set_viewport (0, 0, mode_info.width, mode_info.height);
    }

  grub_gfxmenu_view_draw (carousel_cached_view);
viewer->data = carousel_cached_view;
  viewer->set_chosen_entry = carousel_set_chosen_entry;
  viewer->print_timeout = grub_gfxmenu_print_timeout;
  viewer->clear_timeout = grub_gfxmenu_clear_timeout;
  viewer->fini = carousel_viewer_fini;
  grub_menu_register_viewer (viewer);

  return GRUB_ERR_NONE;
}

GRUB_MOD_INIT (carousel)
{
  grub_gfxmenu_try_hook = carousel_try_hook;
}

GRUB_MOD_FINI (carousel)
{
  if (grub_gfxmenu_try_hook == carousel_try_hook)
    grub_gfxmenu_try_hook = NULL;

  carousel_destroy_cached_view ();
}
