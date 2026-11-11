/**
 * This file contains the loading screen before the whole GRUB interface loads.
 * the purpose of this screen is to show the user that the bootloader is initializing.
*/

#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/font.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/bitmap_scale.h>

#include "loading.h"
#include "broThemeCompiler.h"

GRUB_MOD_LICENSE ("GPLv3+");

#define FILE_NAME "broLoadingConfig.bro"

void
draw_loading_screen (grub_gfxmenu_view_t view)
{

  char* configPath = get_bro_theme_path (FILE_NAME, BRO_PATH);
  if (!configPath) return;

  btd * loading_config_data = broThemeCompiler(configPath);
  if (!loading_config_data) return;

  int loading_gap = grub_strtoul(
    bro_get_bte_data(loading_config_data, "loading_gap")
    , NULL, 10);
  int loading_font_size = grub_strtoul(
    bro_get_bte_data(loading_config_data, "loading_font_size")
    , NULL, 10);
  char* loading_text = bro_get_bte_data(loading_config_data, "loading_text");
  char* icon_path = bro_get_bte_data(loading_config_data, "icon_path");

  grub_free (configPath);

  struct grub_video_bitmap *cog = NULL;
  char *cog_path;
  grub_video_color_t color;
  int cog_width = grub_strtoul(
    bro_get_bte_data(loading_config_data, "icon_width")
    , NULL, 10);
  int cog_height = grub_strtoul(
    bro_get_bte_data(loading_config_data, "icon_height")
    , NULL, 10);
  int center_x, center_y;
  int cog_x, cog_y;
  int text_width, text_x, text_y;
  grub_font_t font;

  if (!view) return;

  center_x = view->screen.width / 2;
  center_y = view->screen.height / 2;

  cog_path = get_bro_theme_path (icon_path, NULL);

  if (cog_path) {
      if (grub_video_bitmap_load (&cog, cog_path)) cog = NULL;
      grub_free (cog_path);
  }

  if (cog) {
      cog_width = cog->mode_info.width;
      cog_height = cog->mode_info.height;

      cog_x = center_x - cog_width / 2;
      cog_y = center_y - (cog_height + loading_gap + loading_font_size) / 2;

      grub_video_blit_bitmap (cog, GRUB_VIDEO_BLIT_BLEND,
                              cog_x, cog_y,
                              0, 0, cog_width, cog_height);
  }

  font = grub_font_get (bro_get_bte_data(loading_config_data, "loading_font"));

  if (font) {
      grub_video_rgba_color_t rgba = {
        .red = 255,
        .green = 255,
        .blue = 255,
        .alpha = 255
      };

      color = grub_video_map_rgba_color (rgba);

      text_width = grub_font_get_string_width (font, loading_text);
      text_x = center_x - text_width / 2;
      text_y = center_y + loading_gap;

      grub_font_draw_string (loading_text, font, color,
                             text_x, text_y);
  }

  if (cog) grub_video_bitmap_destroy (cog);

  bro_free_theme_data(loading_config_data);
  grub_video_swap_buffers ();
}