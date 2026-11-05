/*
 * dsui_int.h - internals shared by the 3DS front end modules
 */
#ifndef DSUI_INT_H
#define DSUI_INT_H

#include "dsui.h"
#include "dsui_gfx.h"
#include "dsui_bridge.h"

/* virtual buttons */
enum {
    K_A = 1 << 0, K_B = 1 << 1, K_X = 1 << 2, K_Y = 1 << 3,
    K_L = 1 << 4, K_R = 1 << 5, K_ZL = 1 << 6, K_ZR = 1 << 7,
    K_START = 1 << 8, K_SELECT = 1 << 9,
    K_UP = 1 << 10, K_DOWN = 1 << 11, K_LEFT = 1 << 12, K_RIGHT = 1 << 13,
    K_CUP = 1 << 14, K_CDOWN = 1 << 15, K_CLEFT = 1 << 16, K_CRIGHT = 1 << 17,
    K_PUP = 1 << 18, K_PDOWN = 1 << 19      /* Circle Pad only (the manual scrolls with it) */
};
#define K_DIRS ( K_UP | K_DOWN | K_LEFT | K_RIGHT )

typedef struct {
    unsigned held, pressed, released, rep;  /* rep = pressed + key repeat for directions */
    int touch;            /* touching now */
    int tx, ty;           /* touch position (bottom screen coords) */
    int tap;              /* a short tap was released this frame */
    int tap_x, tap_y;     /* where it started */
    int drag;             /* dragging (moved beyond threshold) */
    int drag_dx, drag_dy; /* movement since last frame while dragging */
    int down;             /* touch started this frame */
    int down_x, down_y;
    int up;               /* touch ended this frame */
    int long_press;       /* fired once when held still >= 450ms */
    int was_long;         /* the current/just-ended touch fired a long-press */
    int swallow;          /* ignore the rest of this touch (its release is not a tap) */
    Uint32 down_time;
} DSInput;
extern DSInput in;

/* modal screens (dsui_screens.c) */
enum {
    UM_NONE = 0, UM_TITLE, UM_MENU, UM_CONFIRM, UM_PURCHASE, UM_SPLIT, UM_PAGES,
    UM_SCENLIST, UM_SCENSETUP, UM_CAMPLIST, UM_SLOTS, UM_OPTIONS, UM_UNITLIST,
    UM_OVERVIEW, UM_CAMPBRIEF, UM_STATUS, UM_MANUAL, UM_PEDIA
};
extern int ui_mode;
extern int ds_zoom_level;   /* 0 = 50%, 1 = 75%, 2 = 100% */

/* shared state (dsui.c) */
extern SDL_Surface *ds_screen, *ds_canvas;
extern int ds_swapped;
extern int ds_top_dirty, ds_bot_dirty;
void ds_present( void );
void ds_mark_all( void );
void ds_center_on( int mx, int my );
void ds_minimap( int x, int y, int w, int h, int interactive );  /* draws minimap into box */
int  ds_minimap_hit( int x, int y, int w, int h, int px, int py, int *mx, int *my );
void ds_unit_card( int x, int y, int w, int h, Unit *u, const char *title, int full );
void ds_entry_card( int x, int y, int w, int h, Unit_Lib_Entry *e, int cost, const char *note );
void ds_status_bar( void );
int  ds_engine_ready( void );
void ds_button( int x, int y, int w, int h, const char *label, int enabled, int highlight );
int  ds_hit( int px, int py, int x, int y, int w, int h );
void ds_end_turn_request( void );
void ds_clear_selection( void );
int  ds_pressed( int x, int y, int w, int h );
void ds_click( void );
int  dsui_choice_screen( const char *title, const char *text, const char **opts, int n );
void ds_read_input( void );
int  ds_ready_units( void );
const char *ds_class_name( int cls );

/* screens API (dsui_screens.c) */
void scr_open_title( void );
void scr_open_game_menu( void );
void scr_open_confirm( const char *text, int engine_owned, void (*yes)( void ) );
void scr_open_purchase( void );
void scr_open_split( void );
void scr_open_pages( const char *title, char *text_owned );
void scr_open_slots( int save );
void scr_open_options( void );
void scr_open_unitlist( void );
void scr_open_overview( void );
void scr_open_campbrief( void );
void scr_open_help( void );
void scr_open_manual( void );
void scr_open_pedia( Unit_Lib_Entry *show );   /* encyclopedia; optional entry to open at */
void scr_close( void );
void scr_draw_top( void );
void scr_draw_bottom( void );
void scr_input( void );
unsigned scr_view_key( void );
void dsui_options_load( void );
void dsui_options_save( void );

#endif
