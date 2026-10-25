/*
 * dsui_gfx.h - small software drawing layer for the 3DS front end
 * (works on 16bpp RGB565 and 32bpp surfaces; colors are 0xRRGGBB)
 */
#ifndef DSUI_GFX_H
#define DSUI_GFX_H

#include <SDL.h>
#include "dsui_font.h"

typedef unsigned int DSColor;

/* palette */
#define DC_BG        0x10141c
#define DC_PANEL     0x1c2230
#define DC_PANEL2    0x252d3e
#define DC_BORDER    0x3c4860
#define DC_TEXT      0xe6e8ec
#define DC_DIM       0x969eaa
#define DC_ACCENT    0xe8b43c
#define DC_FRIEND    0x5aaaff
#define DC_ENEMY     0xf05a50
#define DC_GOOD      0x6ec878
#define DC_WARN      0xf0aa3c
#define DC_DISABLED  0x5a6070
#define DC_BLACK     0x000000
#define DC_WHITE     0xffffff

void dg_target( SDL_Surface *s );               /* drawing target (locks nothing; SW surfaces only) */
SDL_Surface *dg_get_target( void );
void dg_clip( int x, int y, int w, int h );     /* clip rect in target coordinates */
void dg_noclip( void );

void dg_fill( int x, int y, int w, int h, DSColor c );
void dg_fill_a( int x, int y, int w, int h, DSColor c, int alpha );
void dg_rect( int x, int y, int w, int h, DSColor c );
void dg_hline( int x, int y, int w, DSColor c );
void dg_vline( int x, int y, int h, DSColor c );
void dg_panel( int x, int y, int w, int h );    /* filled panel with border */
void dg_dot( int cx, int cy, int r, DSColor c ); /* filled circle */

int  dg_text( const DSFont *f, int x, int y, DSColor c, const char *s );     /* top-left; returns width */
int  dg_text_n( const DSFont *f, int x, int y, DSColor c, const char *s, int n );
int  dg_text_w( const DSFont *f, const char *s );
int  dg_text_r( const DSFont *f, int xr, int y, DSColor c, const char *s );  /* right aligned at xr */
int  dg_text_c( const DSFont *f, int xc, int y, DSColor c, const char *s );  /* centered at xc */
void dg_text_fit( const DSFont *f, int x, int y, int w, DSColor c, const char *s ); /* ellipsize */
/* word-wrapped text; '#' and '\n' break lines (LGeneral uses '#').
   skip_lines skips that many wrapped lines (for paging); returns total line count */
int  dg_text_wrap( const DSFont *f, int x, int y, int w, int h, DSColor c, const char *s, int skip_lines );
int  dg_text_lines( const DSFont *f, int w, const char *s );

/* blit with the source's colorkey (unit icons etc.) */
void dg_blit( SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy );
/* nearest-neighbour scaled blit honoring colorkey: src rect -> dst rect */
void dg_blit_scaled( SDL_Surface *src, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh );
/* read pixel as 0xRRGGBB */
DSColor dg_getpixel( SDL_Surface *s, int x, int y );

/* tint the pixels where mask[(j*step)*mw + i*step] != 0 inside a w x h box at x,y;
   outline draws only the mask border in 'oc' (alpha 255). alpha 0 = no fill */
void dg_mask_tint( int x, int y, int w, int h, const int *mask, int mw, int mh, int step,
                   DSColor c, int alpha, DSColor oc, int outline );

/* presenter helpers: copy (1:1) or 2:1 box-downscale a source rect onto the target at dx,dy.
   w,h are the destination size. */
void dg_copy( SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy );
void dg_downscale2( SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy );

#endif
