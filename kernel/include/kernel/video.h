#ifndef KERNEL_VIDEO_H
#define KERNEL_VIDEO_H

void clearwin(void);
void putstr(const char *str);
void putstr_at(const char *str, unsigned int row);

void putchar_at_cursor(char c);

void video_reset_cursor(void);
void video_set_cursor(unsigned int row, unsigned int col);
void video_draw_row(unsigned int row, const char *buf, unsigned int len);
unsigned int video_cols(void);
unsigned int video_rows(void);

#endif
