#ifndef KERNEL_VIDEO_H
#define KERNEL_VIDEO_H

void clearwin(void);
void putstr(const char *str);
void putstr_at(const char *str, unsigned int row);

void putchar_at_cursor(char c);

void video_reset_cursor(void);

#endif
