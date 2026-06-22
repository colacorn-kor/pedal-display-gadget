#pragma once

void content_fs_register(void);
void content_screen_create(void);
void content_screen_destroy(void);
void content_show_image(const char *file, const char *name);
void content_show_gif(const char *file, const char *name);
void content_show_text(const char *text, const char *name);

