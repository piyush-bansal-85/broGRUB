#ifndef CHANGE_BG_H
#define CHANGE_BG_H

#include <grub/gfxmenu_view.h>

void bro_background_init(grub_gfxmenu_view_t view, int entry);
void bro_background_set_entry(grub_gfxmenu_view_t view, int entry);
void bro_background_cleanup(grub_gfxmenu_view_t view);

#endif