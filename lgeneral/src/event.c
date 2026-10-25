/***************************************************************************
                          event.c  -  description
                             -------------------
    begin                : Wed Mar 20 2002
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "lgeneral.h"
#include "event.h"
#ifdef DSUI
#include "dsui.h"
#endif

int buttonup = 0, buttondown = 0;
int motion = 0;
int motion_x = 50, motion_y = 50;
int keystate[SDLK_LAST];
int buttonstate[10];
int sdl_quit = 0;

/*
====================================================================
Locals
====================================================================
*/

/*
====================================================================
Event filter that blocks all events. Used to clear the SDL
event queue.
====================================================================
*/
int all_filter( const SDL_Event *event )
{
    return 0;
}

/*
====================================================================
Returns whether any mouse button is pressed
====================================================================
*/
static int event_is_button_pressed()
{
    return (SDL_GetMouseState(0,0)!=0);
}
static int event_is_key_pressed()
{
    int i;
    Uint8 *keystate=SDL_GetKeyState(0);
    for (i=0;i<SDLK_LAST;i++) 
    {
        if (i==SDLK_NUMLOCK||i==SDLK_CAPSLOCK||i==SDLK_SCROLLOCK) continue;
        if (keystate[i]) return 1;
    }
    return 0;
}

#if defined(__3DS__) && !defined(DSUI)
/*
====================================================================
3DS buttons -> LGeneral mouse/keys (first-boot mapping).
Touch is a normal left click (handled by SDL). Buttons:
  A = left click at pointer   B = right click (unit info / cancel)
  X = strategic map (o)       Y = end turn (e)
  L = previous unit (p)       R = next unit (n)
  Start = Esc (menu)          Select = scenario info (i)
  D-pad / Circle Pad = scroll map (arrow keys)
SDL joystick numbering for 3DS: 0 Start,1 A,2 B,3 X,4 Y,5 L,6 R,7 Select,8 ZL,9 ZR
====================================================================
*/
static void n3ds_key( SDLKey sym, int down )
{
    SDL_Event e;
    keystate[sym] = down;
    if ( !down ) return;
    memset( &e, 0, sizeof( e ) );
    e.type = SDL_KEYDOWN;
    e.key.type = SDL_KEYDOWN;
    e.key.state = SDL_PRESSED;
    e.key.keysym.sym = sym;
    e.key.keysym.unicode = ( sym < 128 ) ? sym : 0;
    SDL_PushEvent( &e );
}
static void n3ds_click( int button, int down )
{
    buttonstate[button] = down;
    if ( down ) buttondown = button; else buttonup = button;
}
static int n3ds_filter( const SDL_Event *event )
{
    static const SDLKey btn_keys[10] = {
        SDLK_ESCAPE, 0, 0, SDLK_o, SDLK_e, SDLK_p, SDLK_n, SDLK_i, 0, 0 };
    static int axis_dir[4];
    int down, b, dir;
    switch ( event->type ) {
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            down = ( event->type == SDL_JOYBUTTONDOWN );
            b = event->jbutton.button;
            if ( b == 1 ) n3ds_click( SDL_BUTTON_LEFT, down );
            else if ( b == 2 ) n3ds_click( SDL_BUTTON_RIGHT, down );
            else if ( b < 10 && btn_keys[b] ) n3ds_key( btn_keys[b], down );
            return 0;
        case SDL_JOYHATMOTION:
            n3ds_key( SDLK_UP,    ( event->jhat.value & SDL_HAT_UP ) != 0 );
            n3ds_key( SDLK_DOWN,  ( event->jhat.value & SDL_HAT_DOWN ) != 0 );
            n3ds_key( SDLK_LEFT,  ( event->jhat.value & SDL_HAT_LEFT ) != 0 );
            n3ds_key( SDLK_RIGHT, ( event->jhat.value & SDL_HAT_RIGHT ) != 0 );
            return 0;
        case SDL_JOYAXISMOTION:
            if ( event->jaxis.axis > 1 ) return 0; /* C-stick: reserved for zoom */
            dir = ( event->jaxis.value > 12000 ) ? 1 : ( event->jaxis.value < -12000 ) ? -1 : 0;
            if ( dir == axis_dir[event->jaxis.axis] ) return 0;
            axis_dir[event->jaxis.axis] = dir;
            if ( event->jaxis.axis == 0 ) {
                n3ds_key( SDLK_LEFT, dir < 0 );
                n3ds_key( SDLK_RIGHT, dir > 0 );
            } else {
                n3ds_key( SDLK_UP, dir < 0 );
                n3ds_key( SDLK_DOWN, dir > 0 );
            }
            return 0;
    }
    return 1;
}
#endif

/*
====================================================================
Event filter. As long as this is active no KEY or MOUSE events
will be available by SDL_PollEvent().
====================================================================
*/
int event_filter( const SDL_Event *event )
{
#ifdef DSUI
    if ( dsui_filter( event ) == 0 ) return 0;
#endif
#if defined(__3DS__) && !defined(DSUI)
    if ( event->type >= SDL_JOYAXISMOTION && event->type <= SDL_JOYBUTTONUP )
        return n3ds_filter( event );
#endif
    switch ( event->type ) {
        case SDL_MOUSEMOTION:
            motion_x = event->motion.x;
            motion_y = event->motion.y;
            buttonstate[event->motion.state] = 1;
            motion = 1;
            return 0;
        case SDL_MOUSEBUTTONUP:
            buttonstate[event->button.button] = 0;
            buttonup = event->button.button;
            return 0;
        case SDL_MOUSEBUTTONDOWN:
            buttonstate[event->button.button] = 1;
            buttondown = event->button.button;
            return 0;
        case SDL_KEYUP:
            keystate[event->key.keysym.sym] = 0;
            return 0;
        case SDL_KEYDOWN:
            keystate[event->key.keysym.sym] = 1;
            return 1;
        case SDL_QUIT:
            sdl_quit = 1;
            return 0;
    }
    return 1;
}

/*
====================================================================
Publics
====================================================================
*/

/*
====================================================================
Enable/Disable event filtering of mouse and keyboard.
====================================================================
*/
void event_enable_filter( void )
{
    SDL_SetEventFilter( event_filter );
    event_clear();
}
void event_disable_filter( void )
{
    SDL_SetEventFilter( 0 );
}
void event_clear( void )
{
    memset( keystate, 0, sizeof( int ) * SDLK_LAST );
    memset( buttonstate, 0, sizeof( int ) * 10 );
    buttonup = buttondown = motion = 0;
}

/*
====================================================================
Check if an motion event occured since last call.
====================================================================
*/
int event_get_motion( int *x, int *y )
{
    if ( motion ) {
        *x = motion_x;
        *y = motion_y;
        motion = 0;
        return 1;
    }
    return 0;
}
/*
====================================================================
Check if a button was pressed and return its value.
====================================================================
*/
int event_get_buttondown( int *button, int *x, int *y )
{
    if ( buttondown ) {
        *button = buttondown;
        *x = motion_x;
        *y = motion_y;
        buttondown = 0;
        return 1;
    }
    return 0;
}
int event_get_buttonup( int *button, int *x, int *y )
{
    if ( buttonup ) {
        *button = buttonup;
        *x = motion_x;
        *y = motion_y;
        buttonup = 0;
        return 1;
    }
    return 0;
}

/*
====================================================================
Get current cursor position.
====================================================================
*/
void event_get_cursor_pos( int *x, int *y )
{
    *x = motion_x; *y = motion_y;
}

/*
====================================================================
Check if 'button' button is currently set.
====================================================================
*/
int event_check_button( int button )
{
    return buttonstate[button];
}

/*
====================================================================
Check if 'key' is currently set.
====================================================================
*/
int event_check_key( int key )
{
    return keystate[key];
}

/*
====================================================================
Check if SDL_QUIT was received.
====================================================================
*/
int event_check_quit()
{
    return sdl_quit;
}

/*
====================================================================
Clear the SDL event key (keydown events)
====================================================================
*/
void event_clear_sdl_queue()
{
    SDL_Event event;
    SDL_SetEventFilter( all_filter );
    while ( SDL_PollEvent( &event ) );
    SDL_SetEventFilter( event_filter );
}

/*
====================================================================
Wait until neither a key nor a button is pressed.
====================================================================
*/
void event_wait_until_no_input()
{
    SDL_PumpEvents();
    while (event_is_button_pressed()||event_is_key_pressed())
    {
        SDL_Delay(20);
        SDL_PumpEvents();
    }
    event_clear();
}

#ifdef DSUI
/* input injection for the native 3DS front end (canvas coordinates) */
void event_inject_motion( int x, int y )
{
    motion_x = x; motion_y = y; motion = 1;
}
void event_inject_button( int button, int down, int x, int y )
{
    motion_x = x; motion_y = y; motion = 1;
    if ( button >= 0 && button < 10 ) buttonstate[button] = down;
    if ( down ) buttondown = button; else buttonup = button;
}
#endif
