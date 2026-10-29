#ifndef TEST_RENDER_DEBUG_H
#define TEST_RENDER_DEBUG_H
void scr_setbgcolor(unsigned);
void scr_setfontcolor(unsigned);
void scr_setCursor(int);
void scr_clear(void);
void scr_setXY(int,int);
int scr_printf(const char *,...);
#endif
