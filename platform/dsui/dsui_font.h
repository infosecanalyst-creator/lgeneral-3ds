/* dsui_font.h - pre-rendered anti-aliased bitmap fonts (Latin-1) */
#ifndef DSUI_FONT_H
#define DSUI_FONT_H
typedef struct { int off; unsigned char w, h; signed char x, y; unsigned char adv; } DSGlyph;
typedef struct { int height; int ascent; const DSGlyph *glyphs; const unsigned char *data; } DSFont;
extern const DSFont dsfont_small, dsfont_body, dsfont_bold, dsfont_title;
#endif
