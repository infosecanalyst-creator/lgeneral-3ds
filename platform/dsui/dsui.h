/*
 * dsui.h - native dual-screen front end for LGeneral (New 3DS XL, with a desktop simulator)
 *
 * Layout: one 400x480 surface. Top screen = rows 0-239 (400 wide, intel panel).
 * Bottom screen = rows 240-479, columns 40-359 (320x240 touch map + controls).
 * The engine renders into an offscreen canvas; dsui presents it scaled on the bottom screen.
 */
#ifndef DSUI_H
#define DSUI_H

#ifdef DSUI

#include <SDL.h>

#define DS_W      400
#define DS_H      480
#define DS_TOP_H  240
#define DS_BOT_X  40
#define DS_BOT_Y  240
#define DS_BOT_W  320
#define DS_BOT_H  240

/* engine canvas (bigger than the 50% view so panning is smooth) */
#define DS_CANVAS_W 736
#define DS_CANVAS_H 560

/* video: creates the real screen and the engine canvas, returns the canvas */
SDL_Surface *dsui_video_init( void );
/* engine finished drawing into the canvas (rects may be 0 = full) */
void dsui_canvas_updated( int count, SDL_Rect *rects );
/* called once per engine frame (end of frame) */
void dsui_frame( void );
/* called at the start of engine_check_events, after SDL_PumpEvents; runs input + UI logic.
   returns 1 if a native modal UI owns the input this frame (engine input is skipped) */
int  dsui_tick( void );
/* SDL event filter hook: returns 0 to drop the event */
int  dsui_filter( const SDL_Event *ev );

/* engine notifications */
void dsui_confirm( const char *text );                 /* engine opened its confirm window */
void dsui_label( void *label, const char *text );      /* engine wrote to a label (combat messages) */
void dsui_turn_begins( int human );                    /* a player's turn starts (after turn info) */
void dsui_scenario_loaded( void );                     /* engine_init finished */
void dsui_engine_reset( void );                        /* engine_init starts (map data not valid yet) */
/* blocking status screen; timeout_ms <= 0 waits for input */
void dsui_status_screen( const char *title, const char *text, int timeout_ms );

/* options shared with the engine */
extern int dsui_quick_attack;
extern int dsui_auto_advance;

#endif /* DSUI */
#endif
