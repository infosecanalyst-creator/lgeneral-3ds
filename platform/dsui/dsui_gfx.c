/*
 * dsui_gfx.c - software drawing for the 3DS front end
 */
#include <string.h>
#include "dsui_gfx.h"

static SDL_Surface *T = 0;
static int cx0, cy0, cx1, cy1; /* clip, exclusive x1/y1 */

void dg_target( SDL_Surface *s )
{
    T = s;
    dg_noclip();
}
SDL_Surface *dg_get_target( void ) { return T; }

void dg_clip( int x, int y, int w, int h )
{
    cx0 = x < 0 ? 0 : x;
    cy0 = y < 0 ? 0 : y;
    cx1 = x + w; if ( T && cx1 > T->w ) cx1 = T->w;
    cy1 = y + h; if ( T && cy1 > T->h ) cy1 = T->h;
}
void dg_noclip( void )
{
    cx0 = cy0 = 0;
    cx1 = T ? T->w : 0;
    cy1 = T ? T->h : 0;
}

static inline Uint32 mapc( SDL_PixelFormat *f, DSColor c )
{
    return SDL_MapRGB( f, ( c >> 16 ) & 255, ( c >> 8 ) & 255, c & 255 );
}

static inline Uint32 getp( SDL_Surface *s, int x, int y )
{
    Uint8 *p = (Uint8*)s->pixels + y * s->pitch + x * s->format->BytesPerPixel;
    switch ( s->format->BytesPerPixel ) {
        case 2: return *(Uint16*)p;
        case 4: return *(Uint32*)p;
        case 3: return p[0] | ( p[1] << 8 ) | ( p[2] << 16 );
        default: return *p;
    }
}
static inline void putp( SDL_Surface *s, int x, int y, Uint32 v )
{
    Uint8 *p = (Uint8*)s->pixels + y * s->pitch + x * s->format->BytesPerPixel;
    switch ( s->format->BytesPerPixel ) {
        case 2: *(Uint16*)p = (Uint16)v; break;
        case 4: *(Uint32*)p = v; break;
        case 3: p[0] = v & 255; p[1] = ( v >> 8 ) & 255; p[2] = ( v >> 16 ) & 255; break;
        default: *p = (Uint8)v; break;
    }
}
static inline void blendp( SDL_Surface *s, int x, int y, int r, int g, int b, int a )
{
    Uint8 dr, dg_, db;
    Uint32 v = getp( s, x, y );
    SDL_GetRGB( v, s->format, &dr, &dg_, &db );
    dr  = dr  + ( ( r - dr  ) * a >> 8 );
    dg_ = dg_ + ( ( g - dg_ ) * a >> 8 );
    db  = db  + ( ( b - db  ) * a >> 8 );
    putp( s, x, y, SDL_MapRGB( s->format, dr, dg_, db ) );
}

DSColor dg_getpixel( SDL_Surface *s, int x, int y )
{
    Uint8 r, g, b;
    if ( !s || x < 0 || y < 0 || x >= s->w || y >= s->h ) return 0;
    SDL_GetRGB( getp( s, x, y ), s->format, &r, &g, &b );
    return ( r << 16 ) | ( g << 8 ) | b;
}

void dg_fill( int x, int y, int w, int h, DSColor c )
{
    SDL_Rect r;
    int x1 = x + w, y1 = y + h;
    if ( !T ) return;
    if ( x < cx0 ) x = cx0;
    if ( y < cy0 ) y = cy0;
    if ( x1 > cx1 ) x1 = cx1;
    if ( y1 > cy1 ) y1 = cy1;
    if ( x1 <= x || y1 <= y ) return;
    r.x = x; r.y = y; r.w = x1 - x; r.h = y1 - y;
    SDL_FillRect( T, &r, mapc( T->format, c ) );
}

void dg_fill_a( int x, int y, int w, int h, DSColor c, int a )
{
    int i, j, x1 = x + w, y1 = y + h;
    int r = ( c >> 16 ) & 255, g = ( c >> 8 ) & 255, b = c & 255;
    if ( !T ) return;
    if ( a >= 255 ) { dg_fill( x, y, w, h, c ); return; }
    if ( a <= 0 ) return;
    if ( x < cx0 ) x = cx0;
    if ( y < cy0 ) y = cy0;
    if ( x1 > cx1 ) x1 = cx1;
    if ( y1 > cy1 ) y1 = cy1;
    if ( T->format->BytesPerPixel == 2 && T->format->Rmask == 0xf800 ) {
        /* fast RGB565 path */
        for ( j = y; j < y1; j++ ) {
            Uint16 *p = (Uint16*)( (Uint8*)T->pixels + j * T->pitch ) + x;
            for ( i = x; i < x1; i++, p++ ) {
                int dr = ( *p >> 11 ) << 3, dgr = ( ( *p >> 5 ) & 63 ) << 2, db = ( *p & 31 ) << 3;
                dr += ( r - dr ) * a >> 8; dgr += ( g - dgr ) * a >> 8; db += ( b - db ) * a >> 8;
                *p = ( ( dr >> 3 ) << 11 ) | ( ( dgr >> 2 ) << 5 ) | ( db >> 3 );
            }
        }
        return;
    }
    for ( j = y; j < y1; j++ )
        for ( i = x; i < x1; i++ )
            blendp( T, i, j, r, g, b, a );
}

void dg_hline( int x, int y, int w, DSColor c ) { dg_fill( x, y, w, 1, c ); }
void dg_vline( int x, int y, int h, DSColor c ) { dg_fill( x, y, 1, h, c ); }
void dg_rect( int x, int y, int w, int h, DSColor c )
{
    dg_hline( x, y, w, c ); dg_hline( x, y + h - 1, w, c );
    dg_vline( x, y, h, c ); dg_vline( x + w - 1, y, h, c );
}
void dg_panel( int x, int y, int w, int h )
{
    dg_fill( x, y, w, h, DC_PANEL );
    dg_rect( x, y, w, h, DC_BORDER );
}
void dg_dot( int cx, int cy, int r, DSColor c )
{
    int i, j;
    for ( j = -r; j <= r; j++ )
        for ( i = -r; i <= r; i++ )
            if ( i * i + j * j <= r * r + r )
                dg_fill( cx + i, cy + j, 1, 1, c );
}

/* ---- text ---- */
static const DSGlyph *glyph( const DSFont *f, unsigned char ch )
{
    if ( ch < 32 ) ch = '?';
    return &f->glyphs[ch - 32];
}

static void draw_glyph( const DSFont *f, const DSGlyph *g, int x, int y, int r, int gc, int b, Uint32 solid )
{
    int i, j;
    const unsigned char *d = f->data + g->off;
    int gx = x + g->x, gy = y + g->y;
    for ( j = 0; j < g->h; j++ ) {
        int py = gy + j;
        if ( py < cy0 || py >= cy1 ) continue;
        for ( i = 0; i < g->w; i++ ) {
            int px = gx + i, a = d[j * g->w + i];
            if ( px < cx0 || px >= cx1 || a < 12 ) continue;
            if ( a > 240 ) putp( T, px, py, solid );
            else blendp( T, px, py, r, gc, b, a );
        }
    }
}

int dg_text_n( const DSFont *f, int x, int y, DSColor c, const char *s, int n )
{
    int x0 = x;
    Uint32 solid;
    int r = ( c >> 16 ) & 255, g = ( c >> 8 ) & 255, b = c & 255;
    if ( !T || !s ) return 0;
    solid = mapc( T->format, c );
    for ( ; *s && n != 0; s++, n-- ) {
        const DSGlyph *gl = glyph( f, (unsigned char)*s );
        if ( gl->w && gl->h ) draw_glyph( f, gl, x, y, r, g, b, solid );
        x += gl->adv;
    }
    return x - x0;
}
int dg_text( const DSFont *f, int x, int y, DSColor c, const char *s ) { return dg_text_n( f, x, y, c, s, -1 ); }

int dg_text_w( const DSFont *f, const char *s )
{
    int w = 0;
    if ( !s ) return 0;
    for ( ; *s; s++ ) w += glyph( f, (unsigned char)*s )->adv;
    return w;
}
static int text_w_n( const DSFont *f, const char *s, int n )
{
    int w = 0;
    for ( ; *s && n > 0; s++, n-- ) w += glyph( f, (unsigned char)*s )->adv;
    return w;
}
int dg_text_r( const DSFont *f, int xr, int y, DSColor c, const char *s )
{
    int w = dg_text_w( f, s );
    dg_text( f, xr - w, y, c, s );
    return w;
}
int dg_text_c( const DSFont *f, int xc, int y, DSColor c, const char *s )
{
    int w = dg_text_w( f, s );
    dg_text( f, xc - w / 2, y, c, s );
    return w;
}
void dg_text_fit( const DSFont *f, int x, int y, int w, DSColor c, const char *s )
{
    int n, len;
    if ( !s ) return;
    if ( dg_text_w( f, s ) <= w ) { dg_text( f, x, y, c, s ); return; }
    len = strlen( s );
    for ( n = len; n > 0; n-- )
        if ( text_w_n( f, s, n ) + dg_text_w( f, "..." ) <= w ) break;
    x += dg_text_n( f, x, y, c, s, n );
    dg_text( f, x, y, c, "..." );
}

/* word wrap engine: calls out per line (draw when T set & draw flag) */
static int wrap_run( const DSFont *f, int x, int y, int w, int h, DSColor c, const char *s, int skip, int draw )
{
    int lines = 0, lh = f->height + 1;
    const char *p = s;
    if ( !s ) return 0;
    while ( *p ) {
        const char *line = p, *brk = 0;
        int lw = 0;
        /* explicit break */
        if ( *p == '#' || *p == '\n' ) {
            lines++; p++;
            continue;
        }
        while ( *p && *p != '#' && *p != '\n' ) {
            int cw = glyph( f, (unsigned char)*p )->adv;
            if ( *p == ' ' ) brk = p;
            if ( lw + cw > w && p > line ) {
                if ( brk && brk > line ) p = brk;
                break;
            }
            lw += cw; p++;
        }
        if ( draw && lines >= skip ) {
            int ly = y + ( lines - skip ) * lh;
            if ( ly + f->height <= y + h ) dg_text_n( f, x, ly, c, line, (int)( p - line ) );
        }
        lines++;
        if ( *p == ' ' ) p++;
        else if ( *p == '#' || *p == '\n' ) p++;
    }
    return lines;
}
int dg_text_wrap( const DSFont *f, int x, int y, int w, int h, DSColor c, const char *s, int skip )
{
    return wrap_run( f, x, y, w, h, c, s, skip, 1 );
}
int dg_text_lines( const DSFont *f, int w, const char *s )
{
    return wrap_run( f, 0, 0, w, 0, 0, s, 0, 0 );
}

/* ---- blits ---- */
void dg_blit( SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy )
{
    SDL_Rect sr, dr, old;
    if ( !T || !src ) return;
    sr.x = sx; sr.y = sy; sr.w = w; sr.h = h;
    dr.x = dx; dr.y = dy;
    SDL_GetClipRect( T, &old );
    {
        SDL_Rect c; c.x = cx0; c.y = cy0; c.w = cx1 - cx0; c.h = cy1 - cy0;
        SDL_SetClipRect( T, &c );
    }
    SDL_BlitSurface( src, &sr, T, &dr );
    SDL_SetClipRect( T, &old );
}

void dg_blit_scaled( SDL_Surface *src, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh )
{
    int i, j;
    Uint32 key;
    int haskey;
    if ( !T || !src || dw <= 0 || dh <= 0 ) return;
    key = src->format->colorkey;
    haskey = ( src->flags & SDL_SRCCOLORKEY ) != 0;
    if ( SDL_MUSTLOCK( src ) ) SDL_LockSurface( src );
    for ( j = 0; j < dh; j++ ) {
        int py = dy + j, syy = sy + j * sh / dh;
        if ( py < cy0 || py >= cy1 ) continue;
        for ( i = 0; i < dw; i++ ) {
            int px = dx + i, sxx = sx + i * sw / dw;
            Uint32 v;
            Uint8 r, g, b;
            if ( px < cx0 || px >= cx1 ) continue;
            v = getp( src, sxx, syy );
            if ( haskey && v == key ) continue;
            if ( src->format == T->format || ( src->format->BytesPerPixel == T->format->BytesPerPixel &&
                 src->format->Rmask == T->format->Rmask && src->format->Gmask == T->format->Gmask ) )
                putp( T, px, py, v );
            else {
                SDL_GetRGB( v, src->format, &r, &g, &b );
                putp( T, px, py, SDL_MapRGB( T->format, r, g, b ) );
            }
        }
    }
    if ( SDL_MUSTLOCK( src ) ) SDL_UnlockSurface( src );
}

void dg_copy( SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy )
{
    SDL_Rect sr, dr;
    Uint32 flags = src->flags & SDL_SRCCOLORKEY;
    Uint32 key = src->format->colorkey;
    if ( !T ) return;
    if ( flags ) SDL_SetColorKey( src, 0, 0 );
    sr.x = sx; sr.y = sy; sr.w = w; sr.h = h;
    dr.x = dx; dr.y = dy;
    SDL_BlitSurface( src, &sr, T, &dr );
    if ( flags ) SDL_SetColorKey( src, SDL_SRCCOLORKEY, key );
}

void dg_downscale2( SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy )
{
    int i, j;
    if ( !T || !src ) return;
    /* clamp destination to target and source bounds */
    if ( dx < 0 ) { sx -= dx * 2; w += dx; dx = 0; }
    if ( dy < 0 ) { sy -= dy * 2; h += dy; dy = 0; }
    if ( dx + w > T->w ) w = T->w - dx;
    if ( dy + h > T->h ) h = T->h - dy;
    if ( sx < 0 ) { dx += ( -sx + 1 ) / 2; w -= ( -sx + 1 ) / 2; sx += ( ( -sx + 1 ) / 2 ) * 2; }
    if ( sy < 0 ) { dy += ( -sy + 1 ) / 2; h -= ( -sy + 1 ) / 2; sy += ( ( -sy + 1 ) / 2 ) * 2; }
    if ( sx + w * 2 > src->w ) w = ( src->w - sx ) / 2;
    if ( sy + h * 2 > src->h ) h = ( src->h - sy ) / 2;
    if ( w <= 0 || h <= 0 ) return;
    if ( src->format->BytesPerPixel == 2 && T->format->BytesPerPixel == 2 ) {
        for ( j = 0; j < h; j++ ) {
            Uint16 *a = (Uint16*)( (Uint8*)src->pixels + ( sy + j * 2 ) * src->pitch ) + sx;
            Uint16 *b = (Uint16*)( (Uint8*)a + src->pitch );
            Uint16 *d = (Uint16*)( (Uint8*)T->pixels + ( dy + j ) * T->pitch ) + dx;
            for ( i = 0; i < w; i++, a += 2, b += 2 ) {
                /* average of 4 RGB565 pixels */
                Uint32 p0 = a[0], p1 = a[1], p2 = b[0], p3 = b[1];
                Uint32 rb = ( ( p0 & 0xf81f ) + ( p1 & 0xf81f ) + ( p2 & 0xf81f ) + ( p3 & 0xf81f ) ) >> 2;
                Uint32 g  = ( ( p0 & 0x07e0 ) + ( p1 & 0x07e0 ) + ( p2 & 0x07e0 ) + ( p3 & 0x07e0 ) ) >> 2;
                d[i] = (Uint16)( ( rb & 0xf81f ) | ( g & 0x07e0 ) );
            }
        }
        return;
    }
    if ( src->format->BytesPerPixel == 4 && T->format->BytesPerPixel == 4 ) {
        for ( j = 0; j < h; j++ ) {
            Uint32 *a = (Uint32*)( (Uint8*)src->pixels + ( sy + j * 2 ) * src->pitch ) + sx;
            Uint32 *b = (Uint32*)( (Uint8*)a + src->pitch );
            Uint32 *d = (Uint32*)( (Uint8*)T->pixels + ( dy + j ) * T->pitch ) + dx;
            for ( i = 0; i < w; i++, a += 2, b += 2 ) {
                Uint32 p0 = a[0], p1 = a[1], p2 = b[0], p3 = b[1];
                Uint32 rb = ( ( p0 & 0xff00ff ) + ( p1 & 0xff00ff ) + ( p2 & 0xff00ff ) + ( p3 & 0xff00ff ) ) >> 2;
                Uint32 g  = ( ( p0 & 0x00ff00 ) + ( p1 & 0x00ff00 ) + ( p2 & 0x00ff00 ) + ( p3 & 0x00ff00 ) ) >> 2;
                d[i] = ( rb & 0xff00ff ) | ( g & 0x00ff00 ) | ( p0 & 0xff000000 );
            }
        }
        return;
    }
    /* generic */
    for ( j = 0; j < h; j++ )
        for ( i = 0; i < w; i++ ) {
            Uint8 r0, g0, b0;
            int r = 0, g = 0, b = 0, k;
            for ( k = 0; k < 4; k++ ) {
                SDL_GetRGB( getp( src, sx + i * 2 + ( k & 1 ), sy + j * 2 + ( k >> 1 ) ), src->format, &r0, &g0, &b0 );
                r += r0; g += g0; b += b0;
            }
            putp( T, dx + i, dy + j, SDL_MapRGB( T->format, r >> 2, g >> 2, b >> 2 ) );
        }
}

void dg_mask_tint( int x, int y, int w, int h, const int *mask, int mw, int mh, int step,
                   DSColor c, int alpha, DSColor oc, int outline )
{
    int i, j;
    int r = ( c >> 16 ) & 255, g = ( c >> 8 ) & 255, b = c & 255;
    Uint32 osolid;
    if ( !T || !mask ) return;
    osolid = mapc( T->format, oc );
    for ( j = 0; j < h; j++ ) {
        int py = y + j, my = j * step;
        if ( py < cy0 || py >= cy1 || my >= mh ) continue;
        for ( i = 0; i < w; i++ ) {
            int px = x + i, mxx = i * step;
            if ( px < cx0 || px >= cx1 || mxx >= mw ) continue;
            if ( !mask[my * mw + mxx] ) continue;
            if ( outline ) {
                int edge = ( i == 0 || j == 0 || i == w - 1 || j == h - 1 ||
                             ( mxx - step < 0 ) || !mask[my * mw + mxx - step] ||
                             ( mxx + step >= mw ) || !mask[my * mw + mxx + step] ||
                             ( my - step < 0 ) || !mask[( my - step ) * mw + mxx] ||
                             ( my + step >= mh ) || !mask[( my + step ) * mw + mxx] );
                if ( edge ) { putp( T, px, py, osolid ); continue; }
            }
            if ( alpha > 0 ) blendp( T, px, py, r, g, b, alpha );
        }
    }
}
