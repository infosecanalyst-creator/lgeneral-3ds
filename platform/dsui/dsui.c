/*
 * dsui.c - native dual-screen front end for LGeneral: presenter, input, map controls,
 *          top-screen intel panel.
 *
 * Touch (bottom screen): tap unit = select, tap hex = move, tap enemy = battle forecast,
 * tap it again = attack, drag from a unit = move it, drag elsewhere = pan, long-press = full
 * stats, tap a shared hex again = cycle air/ground. Action strip on the right edge while a
 * unit is selected. Buttons mirror everything (see README).
 */
#ifdef DSUI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
#endif

#include "dsui_int.h"

SDL_Surface *ds_screen = 0, *ds_canvas = 0;
int ds_top_dirty = 1, ds_bot_dirty = 1;
int ui_mode = UM_NONE;
int dsui_quick_attack = 0;
int dsui_auto_advance = 1;
DSInput in;

extern int *hex_mask;
extern SDL_Surface *nation_flags;
extern int nation_flag_width, nation_flag_height;

/* ---------------- view ---------------- */
static int zoom = 2;            /* 2 = 50% (whole canvas view 640x480), 1 = 100% */
static int vx, vy;              /* view origin in canvas pixels */
static int seen_map_x = -9999, seen_map_y = -9999;

/* ---------------- map interaction state ---------------- */
static int cur_mx = -1, cur_my = -1, cursor_on = 0;
static Unit *fc_target = 0;     /* battle forecast target */
static Unit *fc_attacker = 0;   /* unit the forecast was computed for */
static int fc_mx = -1, fc_my = -1, fc_strat = 0, fc_att = 0, fc_def = 0;
static Unit *detail_unit = 0;   /* long-press / Info: full stats on top screen */
static Unit *inspect_unit = 0;  /* tapped enemy (no unit selected): card + reach */
static unsigned char *reach = 0;/* map_w*map_h; 1 = move, 2 = attack reach */
static int reach_w = 0, reach_h = 0, reach_on = 0, threat_all = 0;
static Unit *drag_unit = 0;     /* drag-to-move source */
static int panning = 0;
static Unit *last_sel = 0;
static int menu_pending = 0;      /* Start pressed during a CPU turn */
static Unit *seen_sel = 0;        /* to bring a newly selected unit into view */
static int last_sel_spent = 0;
static unsigned last_sig = 0;
static int engine_ready = 0;     /* map data valid (set after engine_init) */
static char confirm_text[256];

/* message log (combat events) */
#define MSG_N 4
static char msgs[MSG_N][96];
static int msg_count = 0;

/* minimap colour cache */
static DSColor *mm_col = 0;
static int mm_w = 0, mm_h = 0, mm_weather = -1;
static void *mm_map = 0;

static void view_size( int *w, int *h ) { *w = DS_BOT_W * zoom; *h = DS_BOT_H * zoom; }

int ds_hit( int px, int py, int x, int y, int w, int h )
{
    return px >= x && py >= y && px < x + w && py < y + h;
}

void ds_mark_all( void ) { ds_top_dirty = ds_bot_dirty = 1; }

/* ======================================================================
   Video
   ====================================================================== */
SDL_Surface *dsui_video_init( void )
{
    SDL_PixelFormat *f;
#ifdef __3DS__
    ds_screen = SDL_SetVideoMode( DS_W, DS_H, 16, SDL_DUALSCR );
#else
    ds_screen = SDL_SetVideoMode( DS_W, DS_H, 16, SDL_SWSURFACE );
#endif
    if ( !ds_screen ) {
        fprintf( stderr, "dsui: video mode failed: %s\n", SDL_GetError() );
        return 0;
    }
#ifndef __3DS__
    setvbuf( stdout, NULL, _IONBF, 0 );
#endif
    f = ds_screen->format;
    ds_canvas = SDL_CreateRGBSurface( SDL_SWSURFACE, DS_CANVAS_W, DS_CANVAS_H, f->BitsPerPixel,
                                      f->Rmask, f->Gmask, f->Bmask, f->Amask );
    if ( !ds_canvas ) return 0;
    SDL_FillRect( ds_canvas, 0, 0 );
    SDL_FillRect( ds_screen, 0, 0 );
    vx = ( DS_CANVAS_W - DS_BOT_W * zoom ) / 2;
    vy = ( DS_CANVAS_H - DS_BOT_H * zoom ) / 2;
    dsui_options_load();
    printf( "dsui: screen %dx%d %dbpp, canvas %dx%d\n", ds_screen->w, ds_screen->h,
            f->BitsPerPixel, DS_CANVAS_W, DS_CANVAS_H );
    return ds_canvas;
}

/* ======================================================================
   Input
   ====================================================================== */
#ifndef __3DS__
static unsigned desk_keys = 0;
static int desk_touch = 0, desk_tx = 0, desk_ty = 0;
static unsigned key_bit( SDLKey k )
{
    switch ( k ) {
        case SDLK_UP: return K_UP;      case SDLK_DOWN: return K_DOWN;
        case SDLK_LEFT: return K_LEFT;  case SDLK_RIGHT: return K_RIGHT;
        case SDLK_z: return K_A;        case SDLK_x: return K_B;
        case SDLK_s: return K_X;        case SDLK_a: return K_Y;
        case SDLK_q: return K_L;        case SDLK_w: return K_R;
        case SDLK_1: return K_ZL;       case SDLK_2: return K_ZR;
        case SDLK_RETURN: return K_START;
        case SDLK_BACKSPACE: case SDLK_TAB: return K_SELECT;
        case SDLK_i: return K_CUP;      case SDLK_k: return K_CDOWN;
        case SDLK_j: return K_CLEFT;    case SDLK_l: return K_CRIGHT;
        default: return 0;
    }
}
#endif

int dsui_filter( const SDL_Event *ev )
{
    switch ( ev->type ) {
#ifndef __3DS__
        case SDL_MOUSEMOTION:
            desk_tx = ev->motion.x - DS_BOT_X; desk_ty = ev->motion.y - DS_BOT_Y;
            return 0;
        case SDL_MOUSEBUTTONDOWN:
            if ( ev->button.button == SDL_BUTTON_LEFT && ev->button.y >= DS_BOT_Y &&
                 ev->button.x >= DS_BOT_X && ev->button.x < DS_BOT_X + DS_BOT_W ) {
                desk_touch = 1;
                desk_tx = ev->button.x - DS_BOT_X; desk_ty = ev->button.y - DS_BOT_Y;
            }
            return 0;
        case SDL_MOUSEBUTTONUP:
            if ( ev->button.button == SDL_BUTTON_LEFT ) desk_touch = 0;
            return 0;
        case SDL_KEYDOWN:
            desk_keys |= key_bit( ev->key.keysym.sym );
            return 0;
        case SDL_KEYUP:
            desk_keys &= ~key_bit( ev->key.keysym.sym );
            return 0;
#else
        case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        case SDL_KEYDOWN: case SDL_KEYUP:
#endif
        case SDL_JOYAXISMOTION: case SDL_JOYBALLMOTION: case SDL_JOYHATMOTION:
        case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
            return 0;
    }
    return 1;
}

void ds_read_input( void )
{
    static int prev_touch = 0, lp_fired = 0, last_tx = 0, last_ty = 0;
    static Uint32 rep_time = 0;
    unsigned h = 0;
    int t = 0, tx = 0, ty = 0;
    Uint32 now = SDL_GetTicks();
#ifdef __3DS__
    u32 k = hidKeysHeld();
    if ( k & KEY_A ) h |= K_A;           if ( k & KEY_B ) h |= K_B;
    if ( k & KEY_X ) h |= K_X;           if ( k & KEY_Y ) h |= K_Y;
    if ( k & KEY_L ) h |= K_L;           if ( k & KEY_R ) h |= K_R;
    if ( k & KEY_ZL ) h |= K_ZL;         if ( k & KEY_ZR ) h |= K_ZR;
    if ( k & KEY_START ) h |= K_START;   if ( k & KEY_SELECT ) h |= K_SELECT;
    if ( k & ( KEY_DUP | KEY_CPAD_UP ) ) h |= K_UP;
    if ( k & ( KEY_DDOWN | KEY_CPAD_DOWN ) ) h |= K_DOWN;
    if ( k & ( KEY_DLEFT | KEY_CPAD_LEFT ) ) h |= K_LEFT;
    if ( k & ( KEY_DRIGHT | KEY_CPAD_RIGHT ) ) h |= K_RIGHT;
    if ( k & KEY_CSTICK_UP ) h |= K_CUP;       if ( k & KEY_CSTICK_DOWN ) h |= K_CDOWN;
    if ( k & KEY_CSTICK_LEFT ) h |= K_CLEFT;   if ( k & KEY_CSTICK_RIGHT ) h |= K_CRIGHT;
    if ( k & KEY_TOUCH ) {
        touchPosition tp;
        hidTouchRead( &tp );
        t = 1; tx = tp.px; ty = tp.py;
    }
#else
    h = desk_keys;
    t = desk_touch; tx = desk_tx; ty = desk_ty;
    if ( tx < 0 ) tx = 0; if ( tx > DS_BOT_W - 1 ) tx = DS_BOT_W - 1;
    if ( ty < 0 ) ty = 0; if ( ty > DS_BOT_H - 1 ) ty = DS_BOT_H - 1;
#endif
    in.pressed = h & ~in.held;
    in.released = in.held & ~h;
    in.held = h;
    in.rep = in.pressed;
    if ( in.pressed & K_DIRS ) rep_time = now + 300;
    else if ( ( h & K_DIRS ) && now >= rep_time ) { in.rep |= h & K_DIRS; rep_time = now + 75; }

    in.down = t && !prev_touch;
    in.up = !t && prev_touch;
    in.tap = 0; in.long_press = 0; in.drag_dx = in.drag_dy = 0;
    if ( in.down ) {
        in.down_x = tx; in.down_y = ty; in.down_time = now;
        in.drag = 0; lp_fired = 0; last_tx = tx; last_ty = ty;
    }
    else if ( t ) {
        if ( !in.drag && ( abs( tx - in.down_x ) > 6 || abs( ty - in.down_y ) > 6 ) ) in.drag = 1;
        if ( in.drag ) { in.drag_dx = tx - last_tx; in.drag_dy = ty - last_ty; }
        else if ( !lp_fired && now - in.down_time >= 450 ) { in.long_press = 1; lp_fired = 1; }
        last_tx = tx; last_ty = ty;
    }
    if ( in.up && !in.drag && !lp_fired ) { in.tap = 1; in.tap_x = in.down_x; in.tap_y = in.down_y; }
    in.touch = t;
    if ( t ) { in.tx = tx; in.ty = ty; }
    prev_touch = t;
}

/* ======================================================================
   Canvas <-> bottom screen geometry
   ====================================================================== */
static void sync_view( void )
{
    int vw, vh;
    view_size( &vw, &vh );
    if ( map_x != seen_map_x || map_y != seen_map_y ) {
        /* engine scrolled (focus on unit, AI move...): recentre the view */
        vx = ( DS_CANVAS_W - vw ) / 2;
        vy = ( DS_CANVAS_H - vh ) / 2;
        seen_map_x = map_x; seen_map_y = map_y;
        ds_mark_all();
    }
}

/* extra room past the map edges so units under the side strips can be reached */
#define VIEW_LM 56
#define VIEW_RM 44
/* move view by canvas pixels; scroll the engine map when leaving the canvas */
static void pan_view( int dx, int dy )
{
    int vw, vh, guard = 8, lm = VIEW_LM * zoom, rm = VIEW_RM * zoom;
    int at_left = 0, at_right = 0;
    view_size( &vw, &vh );
    vx += dx; vy += dy;
    while ( vx < 0 && guard-- ) {
        int old = map_x; dsb_goto( map_x - 2, map_y );
        if ( map_x == old ) { at_left = 1; break; }
        vx += ( old - map_x ) * hex_x_offset;
    }
    while ( vx > DS_CANVAS_W - vw && guard-- ) {
        int old = map_x; dsb_goto( map_x + 2, map_y );
        if ( map_x == old ) { at_right = 1; break; }
        vx -= ( map_x - old ) * hex_x_offset;
    }
    while ( vy < 0 && guard-- ) {
        int old = map_y; dsb_goto( map_x, map_y - 1 );
        if ( map_y == old ) { vy = 0; break; }
        vy += ( old - map_y ) * hex_h;
    }
    while ( vy > DS_CANVAS_H - vh && guard-- ) {
        int old = map_y; dsb_goto( map_x, map_y + 1 );
        if ( map_y == old ) { vy = DS_CANVAS_H - vh; break; }
        vy -= ( map_y - old ) * hex_h;
    }
    if ( vx < ( at_left || map_x == 0 ? -lm : 0 ) ) vx = ( at_left || map_x == 0 ) ? -lm : 0;
    if ( vx > DS_CANVAS_W - vw + ( at_right ? rm : 0 ) ) vx = DS_CANVAS_W - vw + ( at_right ? rm : 0 );
    if ( vy < 0 ) vy = 0;
    if ( vy > DS_CANVAS_H - vh ) vy = DS_CANVAS_H - vh;
    seen_map_x = map_x; seen_map_y = map_y;
    ds_mark_all();
}

static void set_zoom( int z )
{
    int vw, vh, cx, cy;
    if ( z == zoom ) return;
    view_size( &vw, &vh );
    cx = vx + vw / 2; cy = vy + vh / 2;
    zoom = z;
    view_size( &vw, &vh );
    vx = cx - vw / 2; vy = cy - vh / 2;
    pan_view( 0, 0 );
}

/* bottom-screen (0..319) to canvas */
static void bot_to_canvas( int bx, int by, int *cx, int *cy )
{
    *cx = vx + bx * zoom; *cy = vy + by * zoom;
}

/* hex -> bottom-screen top-left (absolute screen coordinates); 0 if not on canvas */
static int hex_to_bot( int mx, int my, int *bx, int *by )
{
    int sx, sy;
    if ( mx < 0 || my < 0 || mx >= map_w || my >= map_h ) return 0;
    if ( !dsb_screen_pos( mx, my, &sx, &sy ) ) return 0;
    *bx = DS_BOT_X + ( sx - vx ) / zoom;
    *by = DS_BOT_Y + ( sy - vy ) / zoom;
    return 1;
}

static int hex_center_canvas( int mx, int my, int ground, int *cx, int *cy )
{
    int sx, sy;
    if ( !dsb_screen_pos( mx, my, &sx, &sy ) ) return 0;
    *cx = sx + hex_w / 2;
    *cy = sy + ( ground ? hex_h * 3 / 4 : hex_h / 4 );
    return 1;
}

void ds_center_on( int mx, int my )
{
    dsb_focus( mx, my );
    sync_view();
}

/* keep the cursor hex inside the view */
static void ensure_visible( int mx, int my )
{
    int bx, by, hw = hex_w / zoom, hh = hex_h / zoom, guard = 6;
    while ( guard-- ) {
        int dx = 0, dy = 0;
        if ( !hex_to_bot( mx, my, &bx, &by ) ) { ds_center_on( mx, my ); return; }
        bx -= DS_BOT_X; by -= DS_BOT_Y;
        int lm = ( dsb_status() == DSB_STATUS_DEPLOY ) ? VIEW_LM + 4 : 4;
        if ( bx < lm ) dx = ( bx - lm - 26 ) * zoom;
        else if ( bx + hw > DS_BOT_W - 44 ) dx = ( bx + hw - ( DS_BOT_W - 74 ) ) * zoom;
        if ( by < 4 ) dy = ( by - 30 ) * zoom;
        else if ( by + hh > DS_BOT_H - 4 ) dy = ( by + hh - ( DS_BOT_H - 34 ) ) * zoom;
        if ( !dx && !dy ) return;
        pan_view( dx, dy );
    }
}

/* ======================================================================
   Messages and notifications from the engine
   ====================================================================== */
static void push_msg( const char *t )
{
    int i;
    if ( !t || !*t ) return;
    if ( msg_count > 0 && strcmp( msgs[0], t ) == 0 ) return;
    for ( i = MSG_N - 1; i > 0; i-- ) strcpy( msgs[i], msgs[i - 1] );
    snprintf( msgs[0], sizeof msgs[0], "%s", t );
    if ( msg_count < MSG_N ) msg_count++;
    ds_top_dirty = 1;
}

void dsui_label( void *label, const char *text )
{
    int st = dsb_status();
    if ( !gui || label != (void*)gui->label ) return;
    if ( st == DSB_STATUS_ATTACK || st == DSB_STATUS_STRAT_ATTACK || st == DSB_STATUS_MOVE )
        push_msg( text );
}

void dsui_confirm( const char *text )
{
    snprintf( confirm_text, sizeof confirm_text, "%s", text ? text : "" );
}

static void clear_selection_state( void )
{
    fc_target = 0; fc_strat = 0; fc_mx = fc_my = -1; fc_attacker = 0;
    detail_unit = 0; inspect_unit = 0;
    reach_on = 0; threat_all = 0;
    drag_unit = 0;
}
void ds_clear_selection( void ) { clear_selection_state(); }

int ds_engine_ready( void ) { return engine_ready; }

void dsui_engine_reset( void )
{
    engine_ready = 0;
}

#if !defined(__3DS__) && defined(__GLIBC__)
#include <malloc.h>
#endif
void dsui_scenario_loaded( void )
{
#if !defined(__3DS__) && defined(__GLIBC__)
    { struct mallinfo2 mi = mallinfo2(); printf( "dsui: heap in use %.1f MB\n", ( mi.uordblks + mi.hblkhd ) / 1048576.0 ); }
#endif
    engine_ready = ( setup.type != SETUP_RUN_TITLE && setup.type != SETUP_CAMP_BRIEFING && map && hex_w > 0 );
    clear_selection_state();
    seen_sel = 0;
    cursor_on = 0; cur_mx = cur_my = -1;
    last_sel = 0;
    msg_count = 0;
    free( reach ); reach = 0; reach_w = reach_h = 0;
    free( mm_col ); mm_col = 0; mm_weather = -1;
    seen_map_x = seen_map_y = -9999;
    ds_mark_all();
}

static int any_human_player( void )
{
    Player *p;
    if ( !players ) return 0;
    list_reset( players );
    while ( ( p = list_next( players ) ) )
        if ( p->ctrl == PLAYER_CTRL_HUMAN ) return 1;
    return 0;
}

/* all sides are CPU: let the viewer keep watching, take a side, or leave */
static void spectator_menu( void )
{
    const char *opts[4];
    Player *p, *side[2] = { 0, 0 };
    char take[2][48];
    int n = 0, k = 0, r;
    opts[n++] = "Keep watching";
    list_reset( players );
    while ( ( p = list_next( players ) ) && k < 2 ) {
        snprintf( take[k], sizeof take[k], "Play as %.30s", p->name );
        side[k] = p; opts[n++] = take[k]; k++;
    }
    opts[n++] = "Quit to title";
    r = dsui_choice_screen( "All sides are CPU", "Every side is played by the computer. "
                            "You can take over a side - you play it from its next turn.", opts, n );
    if ( r >= 1 && r <= k ) {
        side[r - 1]->ctrl = PLAYER_CTRL_HUMAN;
        push_msg( "You take over from that side's next turn" );
    }
    else if ( r == n - 1 ) dsb_quit_to_title();
}

void dsui_turn_begins( int human )
{
    if ( menu_pending ) {
        if ( human ) { menu_pending = 0; scr_open_game_menu(); }
        else if ( !any_human_player() ) { menu_pending = 0; spectator_menu(); }
    }
    clear_selection_state();
    last_sel = 0;
    if ( human && engine_ready && units ) {
        /* start the turn looking at the first unit with orders */
        Unit *u;
        if ( dsb_deploy_turn() && avail_units ) {
            /* deployment turn: look at the re-deployable units */
            list_reset( avail_units );
            while ( ( u = list_next( avail_units ) ) )
                if ( u->player == cur_player && u->x >= 0 && u->fresh_deploy ) {
                    ds_center_on( u->x, u->y );
                    ensure_visible( u->x, u->y );
                    break;
                }
            msg_count = 0;
            ds_mark_all();
            return;
        }
        list_reset( units );
        while ( ( u = list_next( units ) ) )
            if ( u->player == cur_player && !u->killed && u->x >= 0 && ( u->cur_mov > 0 || u->cur_atk_count > 0 ) ) {
                ds_center_on( u->x, u->y );
                ensure_visible( u->x, u->y );
                break;
            }
    }
    if ( human ) {
        msg_count = 0;
        if ( !dsb_deploy_turn() && scen_info ) {
            char name[64];
            snprintf( name, sizeof name, "Autosave: %.30s, turn %d", scen_info->name, turn + 1 );
            slot_save( SLOT_COUNT - 1, name );
        }
    }
    ds_mark_all();
}

/* ======================================================================
   Reach / threat computation
   ====================================================================== */
static void reach_alloc( void )
{
    if ( !reach || reach_w != map_w || reach_h != map_h ) {
        free( reach );
        reach = calloc( map_w * map_h, 1 );
        if ( !reach ) { reach_w = reach_h = 0; return; }
        reach_w = map_w; reach_h = map_h;
    }
    memset( reach, 0, map_w * map_h );
}

/* mark move (1) and attack (2) reach of unit u (uses engine path finding) */
static void reach_add_unit( Unit *u )
{
    int x, y, i, j, rng, saved_mov = u->cur_mov, mov = u->sel_prop->mov;
    int *infl;
    if ( !reach ) return;
    /* next-turn reach: full movement points (limited by fuel), and ignore zones of control
       (the influence masks are built for the current player's enemies, not this unit's) */
    if ( u->sel_prop->fuel && u->cur_fuel < mov ) mov = u->cur_fuel;
    u->cur_mov = mov;
    infl = malloc( sizeof( int ) * 2 * map_w * map_h );
    if ( infl )
        for ( x = 0; x < map_w; x++ ) for ( y = 0; y < map_h; y++ ) {
            infl[2 * ( y * map_w + x )] = mask[x][y].vis_infl;
            infl[2 * ( y * map_w + x ) + 1] = mask[x][y].vis_air_infl;
            mask[x][y].vis_infl = mask[x][y].vis_air_infl = 0;
        }
    map_get_unit_move_mask( u );
    u->cur_mov = saved_mov;
    if ( infl ) {
        for ( x = 0; x < map_w; x++ ) for ( y = 0; y < map_h; y++ ) {
            mask[x][y].vis_infl = infl[2 * ( y * map_w + x )];
            mask[x][y].vis_air_infl = infl[2 * ( y * map_w + x ) + 1];
        }
        free( infl );
    }
    rng = u->sel_prop->rng;
    if ( rng < 1 ) rng = 1;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ ) {
            int movable = ( mask[x][y].in_range && !mask[x][y].blocked ) || ( x == u->x && y == u->y );
            if ( !movable ) continue;
            reach[y * map_w + x] |= 1;
            for ( i = x - rng; i <= x + rng; i++ )
                for ( j = y - rng; j <= y + rng; j++ )
                    if ( i >= 0 && j >= 0 && i < map_w && j < map_h && get_dist( x, y, i, j ) <= rng )
                        reach[j * map_w + i] |= 2;
        }
    map_clear_unit_move_mask();
}

static void compute_reach( Unit *only )
{
    Unit *u;
    Unit *sel = cur_unit;
    reach_alloc();
    if ( !reach ) return;
    if ( only ) reach_add_unit( only );
    else {
        list_reset( units );
        while ( ( u = list_next( units ) ) ) {
            if ( u->killed || u->x < 0 || player_is_ally( u->player, cur_player ) ) continue;
            if ( !mask[u->x][u->y].spot ) continue;
            reach_add_unit( u );
        }
    }
    /* restore the selected unit's own move mask */
    if ( sel ) dsb_select( sel );
    reach_on = 1;
    ds_mark_all();
}

/* ======================================================================
   Actions
   ====================================================================== */
static void inject_click( int button, int cx, int cy )
{
    event_inject_button( button, 1, cx, cy );
}

int ds_ready_units( void )
{
    Unit *u;
    int n = 0;
    if ( !units || !cur_player ) return 0;
    list_reset( units );
    while ( ( u = list_next( units ) ) )
        if ( u->player == cur_player && !u->killed && u->x >= 0 && !u->is_guarding &&
             ( u->cur_mov > 0 || u->cur_atk_count > 0 ) )
            n++;
    return n;
}

static void end_turn_yes( void ) { dsb_end_turn_now(); }

void ds_end_turn_request( void )
{
    int n;
    if ( dsb_status() == DSB_STATUS_DEPLOY ) { dsb_press( ID_APPLY_DEPLOY ); return; }
    if ( dsb_status() != DSB_STATUS_NONE || dsb_cpu_turn() ) return;
    n = ds_ready_units();
    clear_selection_state();
    if ( n > 0 ) {
        char t[128];
        snprintf( t, sizeof t, n == 1 ? "1 unit can still act.#End your turn anyway?"
                                      : "%d units can still act.#End your turn anyway?", n );
        scr_open_confirm( t, 0, end_turn_yes );
    }
    else
        dsb_end_turn_now();
}

static void set_forecast( Unit *t, int mx, int my, int strat )
{
    fc_target = t; fc_mx = mx; fc_my = my; fc_strat = strat; fc_attacker = cur_unit;
    fc_att = fc_def = 0;
    if ( t && cur_unit ) unit_get_expected_losses( cur_unit, t, &fc_att, &fc_def );
    cur_mx = mx; cur_my = my;
    ds_mark_all();
}

/* the tap / A action on hex mx,my; cx,cy is the canvas point (region matters) */
static void act_on_hex( int mx, int my, int cx, int cy, int region )
{
    int st = dsb_status();
    Unit *t, *u;
    int strat;

    cur_mx = mx; cur_my = my;
    detail_unit = 0;
    if ( st == DSB_STATUS_DEPLOY || st == DSB_STATUS_MERGE || st == DSB_STATUS_SPLIT || st == DSB_STATUS_DROP ) {
        inject_click( BUTTON_LEFT, cx, cy );
        ds_mark_all();
        return;
    }
    if ( st != DSB_STATUS_NONE ) return;

    if ( cur_unit ) {
        inspect_unit = 0;
        t = dsb_target( mx, my, region );
        strat = dsb_strat_target( mx, my ) && mx == cur_unit->x && my == cur_unit->y;
        if ( t || strat ) {
            if ( dsui_quick_attack || ( fc_target == t && fc_strat == strat && fc_mx == mx && fc_my == my ) ) {
                int gx = cx, gy = cy;
                if ( strat ) hex_center_canvas( mx, my, 1, &gx, &gy );
                fc_target = 0; fc_strat = 0; fc_mx = fc_my = -1;
                inject_click( BUTTON_LEFT, gx, gy );
            }
            else set_forecast( t, mx, my, strat );
            return;
        }
        fc_target = 0; fc_strat = 0; fc_mx = fc_my = -1;
        if ( mx == cur_unit->x && my == cur_unit->y ) {
            /* tap the selected unit again: cycle air/ground stack, else deselect */
            Unit *other = ( map[mx][my].a_unit == cur_unit ) ? map[mx][my].g_unit : map[mx][my].a_unit;
            if ( other && other->player == cur_player ) dsb_select( other );
            else dsb_select( 0 );
            ds_mark_all();
            return;
        }
        u = dsb_prim_unit( mx, my, region );
        if ( u && !player_is_ally( u->player, cur_player ) && mask[mx][my].spot &&
             !( mask[mx][my].in_range && !mask[mx][my].blocked ) ) {
            /* enemy out of reach: show its card instead of a useless click */
            inspect_unit = u;
            ds_mark_all();
            return;
        }
        if ( !u && cur_unit->cur_mov > 0 && !( mask[mx][my].in_range && !mask[mx][my].blocked ) )
            push_msg( "Out of reach - highlighted hexes only" );
        else if ( !u && cur_unit->cur_mov == 0 )
            push_msg( "This unit has no movement left" );
        inject_click( BUTTON_LEFT, cx, cy );
    }
    else {
        reach_on = 0;
        u = dsb_prim_unit( mx, my, region );
        if ( u && mask[mx][my].spot && !player_is_ally( u->player, cur_player ) ) {
            inspect_unit = u;
            compute_reach( u );
            return;
        }
        inspect_unit = 0;
        inject_click( BUTTON_LEFT, cx, cy );
    }
    ds_mark_all();
}

static void act_at_cursor( void )
{
    int cx, cy;
    if ( cur_mx < 0 ) return;
    if ( !hex_center_canvas( cur_mx, cur_my, 1, &cx, &cy ) ) return;
    act_on_hex( cur_mx, cur_my, cx, cy, DSB_REGION_GROUND );
}

static void cancel_action( void )
{
    int st = dsb_status();
    if ( st == DSB_STATUS_MERGE || st == DSB_STATUS_SPLIT || st == DSB_STATUS_DROP ) {
        int cx = ( vx < 0 ? 0 : vx ) + 10, cy = vy + 10;
        if ( cur_unit ) hex_center_canvas( cur_unit->x, cur_unit->y, 1, &cx, &cy );
        inject_click( BUTTON_RIGHT, cx, cy );
    }
    else if ( detail_unit ) detail_unit = 0;
    else if ( fc_target || fc_strat ) { fc_target = 0; fc_strat = 0; fc_mx = fc_my = -1; }
    else if ( inspect_unit || reach_on ) { inspect_unit = 0; reach_on = 0; }
    else if ( cur_unit ) dsb_select( 0 );
    else cursor_on = 0;
    ds_mark_all();
}

static void next_ready_unit( void )
{
    SDL_Event e;
    memset( &e, 0, sizeof e );
    e.type = SDL_KEYDOWN;
    e.key.type = SDL_KEYDOWN;
    e.key.state = SDL_PRESSED;
    e.key.keysym.sym = SDLK_n;
    e.key.keysym.unicode = 'n';
    SDL_PushEvent( &e );
    clear_selection_state();
    ds_mark_all();
}

/* action strip (right edge of the bottom screen while a unit is selected) */
enum { ACT_SUPPLY, ACT_UNDO, ACT_AIR, ACT_MERGE, ACT_SPLIT, ACT_INFO, ACT_COUNT };
static const char *act_names[ACT_COUNT] = { "Supply", "Undo", "Air", "Merge", "Split", "Info" };
#define STRIP_W 44
#define STRIP_X ( DS_BOT_W - STRIP_W )

static int strip_visible( void )
{
    return cur_unit && ui_mode == UM_NONE && dsb_status() == DSB_STATUS_NONE && !dsb_cpu_turn();
}
static int act_enabled( int a )
{
    if ( !cur_unit ) return 0;
    switch ( a ) {
        case ACT_SUPPLY: return unit_check_supply( cur_unit, UNIT_SUPPLY_ANYTHING, 0, 0 );
        case ACT_UNDO: return dsb_can_undo();
        case ACT_AIR: return map_check_unit_embark( cur_unit, cur_unit->x, cur_unit->y, EMBARK_AIR, 0 ) ||
                             map_check_unit_debark( cur_unit, cur_unit->x, cur_unit->y, EMBARK_AIR, 0 );
        case ACT_MERGE: return dsb_merge_count() > 0;
        case ACT_SPLIT: return unit_get_split_strength( cur_unit ) > 0;
        case ACT_INFO: return 1;
    }
    return 0;
}
static void do_act( int a )
{
    if ( !act_enabled( a ) ) return;
    clear_selection_state();
    switch ( a ) {
        case ACT_SUPPLY: dsb_press( ID_SUPPLY ); break;
        case ACT_UNDO: dsb_press( ID_UNDO ); break;
        case ACT_AIR: dsb_press( ID_EMBARK_AIR ); break;
        case ACT_MERGE: dsb_press( ID_MERGE ); break;
        case ACT_SPLIT: scr_open_split(); break;
        case ACT_INFO: detail_unit = cur_unit; break;
    }
    ds_mark_all();
}

/* deploy strip (left edge during deployment) */
#define DEP_W 56
#define DEP_ROW 40
static int dep_scroll = 0;
static int dep_rows( void ) { return ( DS_BOT_H - 2 * 30 ) / DEP_ROW; }

static void deploy_input( void )
{
    int n = left_deploy_units ? left_deploy_units->count : 0;
    int rows = dep_rows(), i;
    if ( dep_scroll > ( n > rows ? n - rows : 0 ) ) dep_scroll = n > rows ? n - rows : 0;
    if ( in.pressed & ( K_L | K_R ) && n > 0 ) {
        int idx = list_check( left_deploy_units, deploy_unit );
        idx = ( idx < 0 ) ? 0 : ( idx + ( ( in.pressed & K_R ) ? 1 : n - 1 ) ) % n;
        dsb_deploy_select( list_get( left_deploy_units, idx ) );
        if ( idx < dep_scroll ) dep_scroll = idx;
        if ( idx >= dep_scroll + rows ) dep_scroll = idx - rows + 1;
        ds_mark_all();
    }
    if ( in.pressed & K_Y ) dsb_press( ID_APPLY_DEPLOY );
    if ( in.tap && in.tap_x < DEP_W ) {
        if ( in.tap_y >= DS_BOT_H - 60 && in.tap_y < DS_BOT_H - 30 ) dsb_press( ID_APPLY_DEPLOY );
        else if ( in.tap_y >= DS_BOT_H - 30 ) { if ( !dsb_deploy_turn() ) dsb_press( ID_CANCEL_DEPLOY ); }
        else {
            i = dep_scroll + in.tap_y / DEP_ROW;
            if ( in.tap_y / DEP_ROW < rows && i < n ) dsb_deploy_select( list_get( left_deploy_units, i ) );
        }
        in.tap = 0;
        ds_mark_all();
    }
    if ( in.drag && in.down_x < DEP_W && in.drag_dy ) {
        static int acc = 0;
        acc += in.drag_dy;
        while ( acc <= -DEP_ROW / 2 ) { acc += DEP_ROW / 2; if ( dep_scroll + rows < n ) dep_scroll++; }
        while ( acc >= DEP_ROW / 2 ) { acc -= DEP_ROW / 2; if ( dep_scroll > 0 ) dep_scroll--; }
        ds_mark_all();
    }
}

/* ======================================================================
   Map-mode input
   ====================================================================== */
static void move_cursor( int dx, int dy )
{
    if ( !cursor_on || cur_mx < 0 ) {
        cursor_on = 1;
        if ( cur_unit ) { cur_mx = cur_unit->x; cur_my = cur_unit->y; }
        else {
            int cx, cy, r;
            bot_to_canvas( DS_BOT_W / 2, DS_BOT_H / 2, &cx, &cy );
            if ( !dsb_map_pos( cx, cy, &cur_mx, &cur_my, &r ) ) { cur_mx = map_x + map_sw / 2; cur_my = map_y + map_sh / 2; }
        }
    }
    else { cur_mx += dx; cur_my += dy; }
    if ( cur_mx >= map_w ) cur_mx = map_w - 1;
    if ( cur_my >= map_h ) cur_my = map_h - 1;
    if ( cur_mx < 0 ) cur_mx = 0;
    if ( cur_my < 0 ) cur_my = 0;
    ensure_visible( cur_mx, cur_my );
    ds_mark_all();
}

static void map_input( void )
{
    int st = dsb_status();
    int human = cur_player && !dsb_cpu_turn() && cur_ctrl == PLAYER_CTRL_HUMAN;
    int deploy = ( st == DSB_STATUS_DEPLOY );
    int cx, cy, mx, my, region;

    if ( !human ) {
        /* the AI is moving: remember Start and open the menu when it is safe */
        if ( ( in.pressed & K_START ) && engine_ready && !menu_pending ) {
            menu_pending = 1;
            push_msg( any_human_player() ? "Menu opens after the enemy turn" : "Menu opens at the start of the next turn" );
            ds_mark_all();
        }
        return;
    }
    if ( st != DSB_STATUS_NONE && st != DSB_STATUS_DEPLOY && st != DSB_STATUS_MERGE &&
         st != DSB_STATUS_SPLIT && st != DSB_STATUS_DROP )
        return; /* animations etc. */

    /* --- buttons --- */
    if ( in.pressed & K_START ) { scr_open_game_menu(); return; }
    if ( ( in.pressed & K_SELECT ) && !deploy ) { scr_open_overview(); return; }
    if ( in.pressed & K_ZL ) set_zoom( 2 );
    if ( in.pressed & K_ZR ) set_zoom( 1 );
    if ( in.held & ( K_CUP | K_CDOWN | K_CLEFT | K_CRIGHT ) ) {
        int s = 6 * zoom;
        pan_view( ( in.held & K_CLEFT ) ? -s : ( in.held & K_CRIGHT ) ? s : 0,
                  ( in.held & K_CUP ) ? -s : ( in.held & K_CDOWN ) ? s : 0 );
    }
    if ( in.rep & K_DIRS )
        move_cursor( ( in.rep & K_LEFT ) ? -1 : ( in.rep & K_RIGHT ) ? 1 : 0,
                     ( in.rep & K_UP ) ? -1 : ( in.rep & K_DOWN ) ? 1 : 0 );
    if ( in.pressed & K_A ) {
        if ( !cursor_on ) move_cursor( 0, 0 );
        else act_at_cursor();
    }
    if ( in.pressed & K_B ) cancel_action();
    if ( deploy ) { deploy_input(); }
    else {
        if ( ( in.pressed & K_X ) && cur_unit && dsb_can_undo() ) do_act( ACT_UNDO );
        if ( ( in.pressed & K_Y ) && st == DSB_STATUS_NONE ) { ds_end_turn_request(); return; }
        if ( ( in.pressed & K_R ) && st == DSB_STATUS_NONE ) next_ready_unit();
        if ( ( in.pressed & K_L ) && st == DSB_STATUS_NONE ) { threat_all = 1; inspect_unit = 0; compute_reach( 0 ); }
        if ( ( in.released & K_L ) && threat_all ) { threat_all = 0; reach_on = 0; ds_mark_all(); }
    }

    /* --- touch --- */
    if ( strip_visible() && in.tap && in.tap_x >= STRIP_X ) {
        int a = in.tap_y / ( DS_BOT_H / ACT_COUNT );
        if ( a >= 0 && a < ACT_COUNT ) do_act( a );
        return;
    }
    if ( deploy && in.down_x < DEP_W && ( in.tap || in.drag ) ) return; /* handled by deploy strip */

    if ( in.down ) {
        bot_to_canvas( in.down_x, in.down_y, &cx, &cy );
        drag_unit = 0; panning = 0;
        if ( st == DSB_STATUS_NONE && dsb_map_pos( cx, cy, &mx, &my, &region ) ) {
            Unit *u = dsb_prim_unit( mx, my, region );
            if ( u && u->player == cur_player && !u->killed && u->cur_mov > 0 &&
                 !( strip_visible() && in.down_x >= STRIP_X ) )
                drag_unit = u;
        }
    }
    if ( in.drag ) {
        if ( drag_unit ) {
            bot_to_canvas( in.tx, in.ty, &cx, &cy );
            if ( dsb_map_pos( cx, cy, &mx, &my, &region ) && ( mx != cur_mx || my != cur_my ) ) {
                cur_mx = mx; cur_my = my; cursor_on = 1;
                if ( cur_unit != drag_unit ) { dsb_select( drag_unit ); clear_selection_state(); }
                ds_mark_all();
            }
        }
        else {
            panning = 1;
            if ( in.drag_dx || in.drag_dy ) pan_view( -in.drag_dx * zoom, -in.drag_dy * zoom );
        }
    }
    if ( in.up && in.drag && drag_unit ) {
        bot_to_canvas( in.tx, in.ty, &cx, &cy );
        if ( dsb_map_pos( cx, cy, &mx, &my, &region ) && cur_unit == drag_unit &&
             ( mx != drag_unit->x || my != drag_unit->y ) )
            act_on_hex( mx, my, cx, cy, region );
        drag_unit = 0;
    }
    if ( in.long_press && !( strip_visible() && in.down_x >= STRIP_X ) && !( deploy && in.down_x < DEP_W ) ) {
        bot_to_canvas( in.tx, in.ty, &cx, &cy );
        if ( dsb_map_pos( cx, cy, &mx, &my, &region ) ) {
            Unit *u = dsb_prim_unit( mx, my, region );
            if ( u && mask[mx][my].spot ) { detail_unit = u; cur_mx = mx; cur_my = my; ds_mark_all(); }
        }
        drag_unit = 0;
    }
    if ( in.tap ) {
        bot_to_canvas( in.tap_x, in.tap_y, &cx, &cy );
        if ( dsb_map_pos( cx, cy, &mx, &my, &region ) ) {
            cursor_on = 0;
            act_on_hex( mx, my, cx, cy, region );
        }
    }
}

/* select the next ready unit automatically once the current one is spent */
static void auto_advance( void )
{
    int spent;
    if ( !dsui_auto_advance || dsb_cpu_turn() || dsb_status() != DSB_STATUS_NONE || actions_count() > 0 ) return;
    if ( cur_unit != last_sel ) {
        last_sel = cur_unit;
        last_sel_spent = cur_unit ? ( cur_unit->cur_mov == 0 && cur_unit->cur_atk_count == 0 ) : 0;
        return;
    }
    if ( !cur_unit ) return;
    spent = ( cur_unit->cur_mov == 0 && cur_unit->cur_atk_count == 0 );
    if ( spent && !last_sel_spent && ds_ready_units() > 0 ) next_ready_unit();
    last_sel_spent = spent;
}

int dsui_tick( void )
{
    int st;
    ds_read_input();
    st = dsb_status();
    {
        static int last_st = -1, last_mode = -1;
        if ( st != last_st || ui_mode != last_mode ) {
            printf( "dsui: status %d mode %d\n", st, ui_mode );
            last_st = st; last_mode = ui_mode;
        }
    }
    if ( engine_ready ) sync_view();
    if ( ui_mode == UM_NONE ) {
        if ( st == DSB_STATUS_CONF ) scr_open_confirm( confirm_text, 1, 0 );
        else if ( st == DSB_STATUS_CAMP_BRIEFING ) scr_open_campbrief();
        else if ( setup.type == SETUP_RUN_TITLE && ( st == DSB_STATUS_TITLE || st == DSB_STATUS_TITLE_MENU ) )
            scr_open_title();
    }
    if ( ui_mode != UM_NONE ) {
        scr_input();
        if ( in.pressed || in.down || in.up || in.drag ) ds_mark_all();
        return ui_mode != UM_NONE;
    }
    if ( !engine_ready ) return 0;
    /* forecast becomes stale if the selection changed (checked before input so a stale one never attacks) */
    if ( ( fc_target || fc_strat ) && ( !cur_unit || cur_unit != fc_attacker || dsb_status() != DSB_STATUS_NONE ) ) { fc_target = 0; fc_strat = 0; fc_mx = fc_my = -1; }
    map_input();
    auto_advance();
    if ( cur_unit != seen_sel ) {
        seen_sel = cur_unit;
        if ( cur_unit && cur_unit->x >= 0 && dsb_status() == DSB_STATUS_NONE ) ensure_visible( cur_unit->x, cur_unit->y );
    }
    if ( fc_target && fc_target->killed ) fc_target = 0;
    if ( inspect_unit && inspect_unit->killed ) inspect_unit = 0;
    if ( detail_unit && detail_unit->killed ) detail_unit = 0;
    return 0;
}

/* ======================================================================
   Bottom screen drawing (map mode)
   ====================================================================== */
static void hex_overlay( int mx, int my, DSColor c, int alpha, DSColor oc, int outline )
{
    int bx, by;
    if ( !hex_mask || !hex_to_bot( mx, my, &bx, &by ) ) return;
    dg_mask_tint( bx, by, hex_w / zoom, hex_h / zoom, hex_mask, hex_w, hex_h, zoom, c, alpha, oc, outline );
}

static void draw_map_overlays( void )
{
    int st = dsb_status(), x, y, bx, by;
    Unit *u;
    int human = cur_player && !dsb_cpu_turn();
    int mx0, my0, mx1, my1, r;
    int cx, cy;

    /* visible hex range */
    bot_to_canvas( 0, 0, &cx, &cy );
    if ( !dsb_map_pos( cx, cy, &mx0, &my0, &r ) ) { mx0 = map_x; my0 = map_y; }
    bot_to_canvas( DS_BOT_W - 1, DS_BOT_H - 1, &cx, &cy );
    if ( !dsb_map_pos( cx, cy, &mx1, &my1, &r ) ) { mx1 = map_x + map_sw; my1 = map_y + map_sh; }
    mx0 -= 1; my0 -= 1; mx1 += 1; my1 += 1;
    if ( mx0 < 0 ) mx0 = 0; if ( my0 < 0 ) my0 = 0;
    if ( mx1 >= map_w ) mx1 = map_w - 1; if ( my1 >= map_h ) my1 = map_h - 1;

    /* enemy reach / threat */
    if ( reach_on && reach && reach_w == map_w ) {
        for ( x = mx0; x <= mx1; x++ )
            for ( y = my0; y <= my1; y++ ) {
                int v = reach[y * map_w + x];
                if ( v & 1 ) hex_overlay( x, y, DC_ENEMY, 70, 0, 0 );
                else if ( v & 2 ) hex_overlay( x, y, DC_WARN, 55, 0, 0 );
            }
    }
    /* ready dots */
    if ( human && st == DSB_STATUS_NONE && units ) {
        list_reset( units );
        while ( ( u = list_next( units ) ) ) {
            if ( u->player != cur_player || u->killed || u->x < 0 || u->is_guarding ) continue;
            if ( u->cur_mov <= 0 && u->cur_atk_count <= 0 ) continue;
            if ( u->x < mx0 || u->x > mx1 || u->y < my0 || u->y > my1 ) continue;
            if ( !hex_to_bot( u->x, u->y, &bx, &by ) ) continue;
            {
                int dx = bx + ( hex_w / zoom ) * 3 / 4, dy = by + ( hex_h / zoom ) / 5;
                dg_dot( dx, dy, zoom == 2 ? 2 : 3, 0x000000 );
                dg_dot( dx, dy, zoom == 2 ? 1 : 2, u->cur_mov > 0 ? DC_GOOD : DC_ACCENT );
            }
        }
    }
    /* forecast target */
    if ( fc_target || fc_strat ) hex_overlay( fc_mx, fc_my, DC_ENEMY, 60, DC_ENEMY, 1 );
    /* inspected enemy */
    if ( inspect_unit && !inspect_unit->killed ) hex_overlay( inspect_unit->x, inspect_unit->y, 0, 0, DC_ENEMY, 1 );
    /* cursor */
    if ( cursor_on && cur_mx >= 0 ) hex_overlay( cur_mx, cur_my, DC_ACCENT, 40, DC_ACCENT, 1 );
}

static void draw_banner( const char *t )
{
    int w = dg_text_w( &dsfont_body, t ) + 16;
    int x = DS_BOT_X + ( DS_BOT_W - w ) / 2, y = DS_BOT_Y + DS_BOT_H - 24;
    dg_fill_a( x, y, w, 20, DC_BG, 210 );
    dg_rect( x, y, w, 20, DC_BORDER );
    dg_text( &dsfont_body, x + 8, y + 3, DC_TEXT, t );
}

void ds_button( int x, int y, int w, int h, const char *label, int enabled, int highlight )
{
    dg_fill( x, y, w, h, highlight ? 0x34507a : DC_PANEL2 );
    dg_rect( x, y, w, h, highlight ? DC_ACCENT : DC_BORDER );
    dg_text_c( &dsfont_bold, x + w / 2, y + ( h - dsfont_bold.height ) / 2, enabled ? DC_TEXT : DC_DISABLED, label );
}

static void draw_strip( void )
{
    int i, bh = DS_BOT_H / ACT_COUNT;
    int x = DS_BOT_X + STRIP_X;
    dg_fill_a( x, DS_BOT_Y, STRIP_W, DS_BOT_H, DC_BG, 200 );
    for ( i = 0; i < ACT_COUNT; i++ ) {
        int en = act_enabled( i );
        int y = DS_BOT_Y + i * bh;
        dg_fill( x + 2, y + 2, STRIP_W - 3, bh - 3, en ? DC_PANEL2 : DC_PANEL );
        dg_rect( x + 2, y + 2, STRIP_W - 3, bh - 3, en ? DC_BORDER : DC_PANEL2 );
        dg_text_c( &dsfont_small, x + STRIP_W / 2 + 1, y + ( bh - dsfont_small.height ) / 2, en ? DC_TEXT : DC_DISABLED, act_names[i] );
    }
}

static void draw_deploy_strip( void )
{
    int n = left_deploy_units ? left_deploy_units->count : 0;
    int rows = dep_rows(), i;
    int x = DS_BOT_X;
    if ( dep_scroll > ( n > rows ? n - rows : 0 ) ) dep_scroll = n > rows ? n - rows : 0;
    dg_fill_a( x, DS_BOT_Y, DEP_W, DS_BOT_H, DC_BG, 220 );
    for ( i = 0; i < rows && dep_scroll + i < n; i++ ) {
        Unit *u = list_get( left_deploy_units, dep_scroll + i );
        int y = DS_BOT_Y + i * DEP_ROW;
        if ( u == deploy_unit ) dg_fill( x + 1, y + 1, DEP_W - 2, DEP_ROW - 2, 0x34507a );
        dg_rect( x + 1, y + 1, DEP_W - 2, DEP_ROW - 2, u == deploy_unit ? DC_ACCENT : DC_BORDER );
        dg_blit_scaled( u->sel_prop->icon, u->icon_offset, 0, u->sel_prop->icon_w, u->sel_prop->icon_h,
                        x + 4, y + 3, 48, 34 );
    }
    if ( n > rows ) {
        char t[24];
        snprintf( t, sizeof t, "%d/%d", dep_scroll + 1, n );
        dg_text_c( &dsfont_small, x + DEP_W / 2, DS_BOT_Y + rows * DEP_ROW + 2, DC_DIM, t );
    }
    ds_button( x + 2, DS_BOT_Y + DS_BOT_H - 58, DEP_W - 4, 26, "Done", 1, n == 0 );
    ds_button( x + 2, DS_BOT_Y + DS_BOT_H - 29, DEP_W - 4, 26, "Cancel", !dsb_deploy_turn(), 0 );
}

static void draw_bottom_map( void )
{
    int st = dsb_status();
    if ( !ds_canvas || !engine_ready || !scen_info || setup.type == SETUP_RUN_TITLE ) {
        dg_fill( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, DC_BG );
        if ( !engine_ready && setup.type != SETUP_RUN_TITLE )
            dg_text_c( &dsfont_bold, DS_BOT_X + DS_BOT_W / 2, DS_BOT_Y + 110, DC_DIM, "Loading..." );
        return;
    }
    {
        /* the view may extend past the canvas edge (map border margins) */
        int sx = vx, w = DS_BOT_W, dx = DS_BOT_X;
        if ( sx < 0 ) { int cut = ( -sx + zoom - 1 ) / zoom; dg_fill( dx, DS_BOT_Y, cut, DS_BOT_H, DC_BG ); dx += cut; w -= cut; sx += cut * zoom; }
        if ( sx + w * zoom > DS_CANVAS_W ) { int cut = ( sx + w * zoom - DS_CANVAS_W + zoom - 1 ) / zoom; w -= cut; dg_fill( dx + w, DS_BOT_Y, cut, DS_BOT_H, DC_BG ); }
        if ( w > 0 ) {
            if ( zoom == 2 ) dg_downscale2( ds_canvas, sx, vy, w, DS_BOT_H, dx, DS_BOT_Y );
            else dg_copy( ds_canvas, sx, vy, w, DS_BOT_H, dx, DS_BOT_Y );
        }
    }
    draw_map_overlays();
    if ( dsb_cpu_turn() ) return;
    if ( st == DSB_STATUS_DEPLOY ) draw_deploy_strip();
    else if ( strip_visible() ) draw_strip();
    if ( st == DSB_STATUS_MERGE ) draw_banner( "Tap a unit to merge with  -  B: cancel" );
    else if ( st == DSB_STATUS_SPLIT ) draw_banner( "Tap a hex or unit for the split  -  B: cancel" );
    else if ( st == DSB_STATUS_DROP ) draw_banner( "Tap a drop zone  -  B: cancel" );
}

/* ======================================================================
   Top screen
   ====================================================================== */
const char *ds_class_name( int cls )
{
    if ( cls < 0 || cls >= unit_class_count ) return "";
    return unit_classes[cls].name;
}

static void bar( int x, int y, int w, int h, int val, int max, DSColor c )
{
    int f;
    if ( max <= 0 ) max = 1;
    if ( val < 0 ) val = 0;
    if ( val > max ) val = max;
    f = w * val / max;
    dg_fill( x, y, w, h, 0x0c0f15 );
    dg_fill( x, y, f, h, c );
    dg_rect( x, y, w, h, DC_BORDER );
}

void ds_status_bar( void )
{
    char t[160], date[64];
    int x = 4;
    dg_fill( 0, 0, DS_W, 20, DC_PANEL );
    dg_hline( 0, 19, DS_W, DC_BORDER );
    if ( !scen_info || !cur_player ) {
        dg_text( &dsfont_bold, 6, 3, DC_TEXT, "LGeneral" );
        return;
    }
    scen_get_date( date );
    snprintf( t, sizeof t, "Turn %d/%d", turn + 1, scen_info->turn_limit );
    x += dg_text( &dsfont_bold, x, 3, DC_TEXT, t ) + 10;
    x += dg_text( &dsfont_body, x, 3, DC_DIM, weather_types[cur_weather].name ) + 10;
    snprintf( t, sizeof t, "%s", cur_player->name );
    x += dg_text( &dsfont_body, x, 3, dsb_cpu_turn() ? DC_ENEMY : DC_FRIEND, t ) + 10;
    snprintf( t, sizeof t, "Prestige %d", cur_player->cur_prestige );
    dg_text_r( &dsfont_body, DS_W - 6, 3, DC_ACCENT, t );
}

void ds_unit_card( int x, int y, int w, int h, Unit *u, const char *title, int full )
{
    char t[160];
    int tx = x + 70, ty = y + ( title ? 16 : 4 ), i;
    Unit_Lib_Entry *p;
    int friendly;
    dg_panel( x, y, w, h );
    if ( !u ) return;
    p = u->sel_prop;
    friendly = cur_player && player_is_ally( u->player, cur_player );
    dg_clip( x + 1, y + 1, w - 2, h - 2 );
    if ( p->icon ) dg_blit( p->icon, u->icon_offset, 0, p->icon_w, p->icon_h, x + 4, y + ( title ? 22 : 8 ) );
    if ( title ) dg_text_fit( &dsfont_small, x + 6, y + 3, w - 12, DC_DIM, title );
    dg_text_fit( &dsfont_bold, tx, ty, w - 74, friendly ? DC_TEXT : DC_ENEMY, u->name );
    ty += dsfont_bold.height + 1;
    snprintf( t, sizeof t, "%s  -  %s", ds_class_name( p->class ), u->player ? u->player->name : "" );
    dg_text_fit( &dsfont_small, tx, ty, w - 74, DC_DIM, t );
    ty += dsfont_small.height + 3;
    /* strength + experience */
    snprintf( t, sizeof t, "Str %d", u->str );
    dg_text( &dsfont_bold, tx, ty, DC_TEXT, t );
    bar( tx + 46, ty + 3, 70, 8, u->str, 10, u->str >= 7 ? DC_GOOD : u->str >= 4 ? DC_WARN : DC_ENEMY );
    for ( i = 0; i < 5; i++ )
        dg_dot( tx + 128 + i * 9, ty + 7, 3, i < u->exp_level ? DC_ACCENT : DC_PANEL2 );
    ty += dsfont_bold.height + 3;
    if ( friendly || full ) {
        int c = tx;
        if ( p->fuel > 0 ) {
            snprintf( t, sizeof t, "Fuel %d/%d", u->cur_fuel, p->fuel );
            c += dg_text( &dsfont_body, c, ty, unit_low_fuel( u ) ? DC_WARN : DC_TEXT, t ) + 10;
        }
        snprintf( t, sizeof t, "Ammo %d/%d", u->cur_ammo, p->ammo );
        c += dg_text( &dsfont_body, c, ty, unit_low_ammo( u ) ? DC_WARN : DC_TEXT, t ) + 10;
        snprintf( t, sizeof t, "Entr %d", u->entr );
        dg_text( &dsfont_body, c, ty, DC_DIM, t );
        ty += dsfont_body.height + 2;
        if ( friendly && cur_player && u->player == cur_player ) {
            snprintf( t, sizeof t, "%s  -  %s", u->cur_mov > 0 ? "Can move" : "Moved",
                      u->cur_atk_count > 0 ? "can attack" : "no attacks left" );
            dg_text( &dsfont_body, tx, ty, u->cur_mov > 0 || u->cur_atk_count > 0 ? DC_GOOD : DC_DIM, t );
            ty += dsfont_body.height + 2;
        }
    }
    if ( full ) {
        int col = x + 6, row = y + 76;
        if ( ty > row ) row = ty + 2;
        snprintf( t, sizeof t, "Ini %d   Mov %d   Spot %d   Range %d   Atk/turn %d",
                  p->ini, p->mov, p->spt, p->rng, p->atk_count );
        dg_text( &dsfont_body, col, row, DC_TEXT, t ); row += dsfont_body.height + 2;
        t[0] = 0;
        for ( i = 0; i < trgt_type_count && i < TARGET_TYPE_LIMIT; i++ ) {
            char s[40];
            snprintf( s, sizeof s, "%s%s %d", i ? "   " : "Attack: ", trgt_types[i].name, p->atks[i] );
            strncat( t, s, sizeof t - strlen( t ) - 1 );
        }
        dg_text_fit( &dsfont_body, col, row, w - 12, DC_TEXT, t ); row += dsfont_body.height + 2;
        snprintf( t, sizeof t, "Defense: ground %d   air %d   close %d", p->def_grnd, p->def_air, p->def_cls );
        dg_text( &dsfont_body, col, row, DC_TEXT, t ); row += dsfont_body.height + 2;
        if ( u->trsp_prop.id ) {
            snprintf( t, sizeof t, "Transport: %s%s", u->trsp_prop.name, u->sel_prop == &u->trsp_prop ? " (mounted)" : "" );
            dg_text_fit( &dsfont_body, col, row, w - 12, DC_DIM, t ); row += dsfont_body.height + 2;
        }
        if ( friendly ) {
            snprintf( t, sizeof t, "Supply here: %d%%   Experience: %d", u->supply_level, u->exp );
            dg_text( &dsfont_body, col, row, DC_DIM, t );
        }
    }
    dg_noclip();
}

void ds_entry_card( int x, int y, int w, int h, Unit_Lib_Entry *p, int cost, const char *note )
{
    char t[200];
    int i, row, tx = x + 70;
    dg_panel( x, y, w, h );
    if ( !p ) return;
    dg_clip( x + 1, y + 1, w - 2, h - 2 );
    if ( p->icon ) dg_blit( p->icon, 0, 0, p->icon_w, p->icon_h, x + 4, y + 6 );
    dg_text_fit( &dsfont_bold, tx, y + 4, w - 74, DC_TEXT, p->name );
    dg_text( &dsfont_small, tx, y + 4 + dsfont_bold.height + 1, DC_DIM, ds_class_name( p->class ) );
    snprintf( t, sizeof t, "Cost %d", cost );
    dg_text( &dsfont_bold, tx, y + 34, DC_ACCENT, t );
    if ( note ) dg_text_fit( &dsfont_small, tx + 70, y + 36, w - 144, DC_DIM, note );
    row = y + 60;
    snprintf( t, sizeof t, "Ini %d   Mov %d   Spot %d   Range %d   Ammo %d   Fuel %d",
              p->ini, p->mov, p->spt, p->rng, p->ammo, p->fuel );
    dg_text_fit( &dsfont_body, x + 6, row, w - 12, DC_TEXT, t ); row += dsfont_body.height + 2;
    t[0] = 0;
    for ( i = 0; i < trgt_type_count && i < TARGET_TYPE_LIMIT; i++ ) {
        char s[40];
        snprintf( s, sizeof s, "%s%s %d", i ? "   " : "Attack: ", trgt_types[i].name, p->atks[i] );
        strncat( t, s, sizeof t - strlen( t ) - 1 );
    }
    dg_text_fit( &dsfont_body, x + 6, row, w - 12, DC_TEXT, t ); row += dsfont_body.height + 2;
    snprintf( t, sizeof t, "Defense: ground %d   air %d   close %d", p->def_grnd, p->def_air, p->def_cls );
    dg_text( &dsfont_body, x + 6, row, DC_TEXT, t );
    dg_noclip();
}

static void forecast_card( int x, int y, int w, int h )
{
    char t[160];
    int row = y + 4;
    dg_panel( x, y, w, h );
    dg_rect( x, y, w, h, DC_ENEMY );
    if ( fc_strat && !fc_target ) {
        dg_text( &dsfont_bold, x + 6, row, DC_ENEMY, "Carpet bombing" ); row += 18;
        dg_text_wrap( &dsfont_body, x + 6, row, w - 12, h - 40, DC_TEXT,
                      "Damages the city or airfield under this bomber so it cannot supply or deploy for a while.", 0 );
        dg_text( &dsfont_small, x + 6, y + h - 16, DC_ACCENT, "Tap again or press A to bomb  -  B: cancel" );
        return;
    }
    if ( !fc_target || !cur_unit ) return;
    dg_text( &dsfont_small, x + 6, row, DC_DIM, "BATTLE FORECAST" ); row += dsfont_small.height + 1;
    snprintf( t, sizeof t, "%s  vs  %s", cur_unit->name, fc_target->name );
    dg_text_fit( &dsfont_bold, x + 6, row, w - 12, DC_TEXT, t ); row += dsfont_bold.height + 3;
    snprintf( t, sizeof t, "You lose ~%d", fc_att );
    dg_text( &dsfont_body, x + 6, row, DC_TEXT, t );
    bar( x + 96, row + 3, w - 104, 8, cur_unit->str - fc_att, cur_unit->str > 0 ? cur_unit->str : 1, DC_FRIEND );
    row += dsfont_body.height + 2;
    snprintf( t, sizeof t, "They lose ~%d", fc_def );
    dg_text( &dsfont_body, x + 6, row, DC_TEXT, t );
    bar( x + 96, row + 3, w - 104, 8, fc_target->str - fc_def, fc_target->str > 0 ? fc_target->str : 1, DC_ENEMY );
    row += dsfont_body.height + 2;
    if ( fc_def >= fc_target->str ) dg_text( &dsfont_bold, x + 6, row, DC_GOOD, "Likely destroys the target" );
    else if ( fc_att >= cur_unit->str ) dg_text( &dsfont_bold, x + 6, row, DC_ENEMY, "Your unit will likely be destroyed" );
    else if ( fc_att > fc_def ) dg_text( &dsfont_bold, x + 6, row, DC_WARN, "Unfavourable exchange" );
    else dg_text( &dsfont_bold, x + 6, row, DC_DIM, fc_def > fc_att ? "Favourable exchange" : "Even exchange" );
    dg_text( &dsfont_small, x + 6, y + h - 15, DC_ACCENT, "Tap again or A to attack  -  B: cancel" );
}

static void hex_card( int x, int y, int w, int h, int mx, int my )
{
    char t[160];
    int row = y + 4;
    Unit *u;
    Map_Tile *tile;
    dg_panel( x, y, w, h );
    if ( mx < 0 || my < 0 || mx >= map_w || my >= map_h ) return;
    tile = &map[mx][my];
    snprintf( t, sizeof t, "%s", tile->name ? tile->name : "" );
    dg_text_fit( &dsfont_bold, x + 6, row, w - 12, DC_TEXT, t ); row += dsfont_bold.height + 2;
    snprintf( t, sizeof t, "%s%s%s%s", tile->terrain ? tile->terrain->name : "",
              tile->obj ? "  -  objective" : "",
              tile->player ? "  -  held by " : "", tile->player ? tile->player->name : "" );
    dg_text_fit( &dsfont_small, x + 6, row, w - 12, tile->obj ? DC_ACCENT : DC_DIM, t ); row += dsfont_small.height + 4;
    if ( cur_unit && mask[mx][my].in_range && !mask[mx][my].blocked && mask[mx][my].moveCost > 0 ) {
        snprintf( t, sizeof t, "Move cost %d - tap to move here", mask[mx][my].moveCost );
        dg_text( &dsfont_body, x + 6, row, DC_GOOD, t ); row += dsfont_body.height + 2;
    }
    if ( mask[mx][my].spot ) {
        int k;
        for ( k = 0; k < 2; k++ ) {
            u = k == 0 ? tile->g_unit : tile->a_unit;
            if ( !u ) continue;
            snprintf( t, sizeof t, "%s (%s) - str %d", u->name, ds_class_name( u->sel_prop->class ), u->str );
            dg_text_fit( &dsfont_body, x + 6, row, w - 12,
                         cur_player && player_is_ally( u->player, cur_player ) ? DC_FRIEND : DC_ENEMY, t );
            row += dsfont_body.height + 2;
        }
    }
}

static void hints_card( int x, int y, int w, int h )
{
    int row = y + 6, i;
    dg_panel( x, y, w, h );
    if ( msg_count > 0 ) {
        for ( i = 0; i < msg_count && row < y + h - 14; i++ ) {
            dg_text_fit( &dsfont_body, x + 6, row, w - 12, i == 0 ? DC_WARN : DC_DIM, msgs[i] );
            row += dsfont_body.height + 2;
        }
        return;
    }
    if ( cur_unit )
        dg_text_wrap( &dsfont_small, x + 6, row, w - 12, h - 10, DC_DIM,
                      "Tap a hex to move. Tap an enemy for the forecast, tap again to attack. "
                      "Drag the unit to move it. Long-press any unit for full stats.", 0 );
    else
        dg_text_wrap( &dsfont_small, x + 6, row, w - 12, h - 10, DC_DIM,
                      "Tap a unit to select it, or press R for the next unit with orders. "
                      "Tap an enemy to see how far it can reach; hold L for all enemy threat. "
                      "Drag to pan, ZL/ZR to zoom, Y to end the turn, Start for the menu.", 0 );
}

/* ---- minimap ---- */
static void mm_cache( void )
{
    int x, y;
    if ( mm_col && mm_w == map_w && mm_h == map_h && mm_weather == cur_weather && mm_map == (void*)map ) return;
    free( mm_col );
    mm_col = malloc( sizeof( DSColor ) * map_w * map_h );
    mm_w = map_w; mm_h = map_h; mm_weather = cur_weather; mm_map = (void*)map;
    if ( !mm_col ) return;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ ) {
            Terrain_Type *tt = map[x][y].terrain;
            DSColor c = 0x406030;
            if ( tt && tt->images && tt->images[cur_weather] ) {
                SDL_Surface *s = tt->images[cur_weather];
                int ox = map[x][y].image_offset + hex_w / 2;
                c = dg_getpixel( s, ox, hex_h / 2 );
            }
            mm_col[y * map_w + x] = c;
        }
}

static void mm_geom( int w, int h, int *cell, int *ox, int *oy, int x, int y )
{
    int cw = w / ( map_w > 0 ? map_w : 1 ), ch = h * 2 / ( map_h * 2 + 1 > 0 ? map_h * 2 + 1 : 1 );
    *cell = cw < ch ? cw : ch;
    if ( *cell < 1 ) *cell = 1;
    *ox = x + ( w - *cell * map_w ) / 2;
    *oy = y + ( h - *cell * map_h - *cell / 2 ) / 2;
}

void ds_minimap( int x, int y, int w, int h, int interactive )
{
    int mx, my, cell, ox, oy;
    Unit *u;
    if ( !map || map_w <= 0 ) return;
    mm_cache();
    mm_geom( w, h, &cell, &ox, &oy, x, y );
    dg_fill( x, y, w, h, 0x05070a );
    for ( mx = 0; mx < map_w; mx++ )
        for ( my = 0; my < map_h; my++ ) {
            int px = ox + mx * cell, py = oy + my * cell + ( ( mx & 1 ) ? cell / 2 : 0 );
            DSColor c = mm_col ? mm_col[my * map_w + mx] : 0x406030;
            if ( mask && mask[mx][my].fog ) c = ( ( c >> 1 ) & 0x7f7f7f );
            dg_fill( px, py, cell, cell, c );
            if ( map[mx][my].obj ) {
                int ally = cur_player && map[mx][my].player && player_is_ally( map[mx][my].player, cur_player );
                dg_rect( px - 1, py - 1, cell + 2, cell + 2, ally ? DC_FRIEND : DC_ACCENT );
            }
        }
    if ( units ) {
        int pass;
        /* friends first, then spotted enemies drawn larger with an outline so they stand out */
        for ( pass = 0; pass < 2; pass++ ) {
            list_reset( units );
            while ( ( u = list_next( units ) ) ) {
                int px, py, ally;
                if ( u->killed || u->x < 0 ) continue;
                ally = cur_player && player_is_ally( u->player, cur_player );
                if ( ally != ( pass == 0 ) ) continue;
                if ( !ally && ( !mask || !mask[u->x][u->y].spot ) ) continue;
                px = ox + u->x * cell; py = oy + u->y * cell + ( ( u->x & 1 ) ? cell / 2 : 0 );
                if ( ally ) dg_fill( px, py, cell > 2 ? cell - 1 : cell, cell > 2 ? cell - 1 : cell, DC_FRIEND );
                else {
                    int sz = cell < 4 ? 4 : cell + 1, o = ( sz - cell ) / 2;
                    dg_fill( px - o - 1, py - o - 1, sz + 2, sz + 2, 0x000000 );
                    dg_fill( px - o, py - o, sz, sz, DC_ENEMY );
                }
            }
        }
    }
    /* view box */
    if ( scen_info ) {
        int cx, cy, x0, y0, x1, y1, r;
        bot_to_canvas( 0, 0, &cx, &cy );
        if ( !dsb_map_pos( cx, cy, &x0, &y0, &r ) ) { x0 = map_x; y0 = map_y; }
        bot_to_canvas( DS_BOT_W - 1, DS_BOT_H - 1, &cx, &cy );
        if ( !dsb_map_pos( cx, cy, &x1, &y1, &r ) ) { x1 = map_x + map_sw; y1 = map_y + map_sh; }
        if ( x1 >= map_w ) x1 = map_w - 1;
        if ( y1 >= map_h ) y1 = map_h - 1;
        dg_rect( ox + x0 * cell, oy + y0 * cell, ( x1 - x0 + 1 ) * cell, ( y1 - y0 + 1 ) * cell, DC_WHITE );
    }
}

int ds_minimap_hit( int x, int y, int w, int h, int px, int py, int *mx, int *my )
{
    int cell, ox, oy;
    if ( !map || map_w <= 0 ) return 0;
    mm_geom( w, h, &cell, &ox, &oy, x, y );
    *mx = ( px - ox ) / cell;
    if ( *mx < 0 || *mx >= map_w ) return 0;
    *my = ( py - oy - ( ( *mx & 1 ) ? cell / 2 : 0 ) ) / cell;
    if ( *my < 0 || *my >= map_h ) return 0;
    return 1;
}

static void objectives_box( int x, int y, int w, int h )
{
    int mx, my, own = 0, total = 0;
    char t[64];
    for ( mx = 0; mx < map_w; mx++ )
        for ( my = 0; my < map_h; my++ )
            if ( map[mx][my].obj ) {
                total++;
                if ( cur_player && map[mx][my].player && player_is_ally( map[mx][my].player, cur_player ) ) own++;
            }
    snprintf( t, sizeof t, "Objectives %d/%d", own, total );
    dg_text( &dsfont_bold, x, y, DC_ACCENT, t );
    snprintf( t, sizeof t, "Ready units: %d", cur_player && !dsb_cpu_turn() ? ds_ready_units() : 0 );
    dg_text( &dsfont_small, x, y + 16, DC_DIM, t );
    snprintf( t, sizeof t, "Zoom %d%%", zoom == 2 ? 50 : 100 );
    dg_text( &dsfont_small, x, y + 28, DC_DIM, t );
}

static void draw_top( void )
{
    int st = dsb_status();
    dg_noclip();
    dg_fill( 0, 0, DS_W, DS_TOP_H, DC_BG );
    if ( ui_mode != UM_NONE ) { scr_draw_top(); return; }
    if ( !engine_ready || !scen_info || setup.type == SETUP_RUN_TITLE ) { ds_status_bar(); return; }
    ds_status_bar();
    /* right column: minimap + objectives */
    ds_minimap( 270, 24, 126, 150, 0 );
    objectives_box( 272, 180, 124, 56 );

    if ( dsb_cpu_turn() ) {
        int row = 30, i;
        dg_panel( 4, 24, 262, 212 );
        dg_text( &dsfont_title, 12, row, DC_ENEMY, "Enemy turn" ); row += 22;
        dg_text( &dsfont_body, 12, row, DC_TEXT, cur_player ? cur_player->name : "" ); row += 24;
        for ( i = 0; i < msg_count; i++ ) {
            dg_text_fit( &dsfont_body, 12, row, 246, i == 0 ? DC_WARN : DC_DIM, msgs[i] );
            row += 16;
        }
        return;
    }
    if ( st == DSB_STATUS_DEPLOY ) {
        dg_panel( 4, 24, 262, 212 );
        if ( deploy_unit ) ds_unit_card( 4, 24, 262, 104, deploy_unit, "DEPLOY", 0 );
        dg_text_wrap( &dsfont_body, 12, 134, 246, 98, DC_TEXT,
                      "Tap a unit on the left, then tap a highlighted hex to place it. "
                      "Tap a placed unit to take it back. L/R pick the next unit. "
                      "Done (or Y) when finished.", 0 );
        return;
    }
    if ( detail_unit ) {
        ds_unit_card( 4, 24, 262, 212, detail_unit, "UNIT DETAILS  (B to close)", 1 );
        return;
    }
    /* upper card: selected unit */
    if ( cur_unit ) ds_unit_card( 4, 24, 262, 104, cur_unit, "SELECTED", 0 );
    else if ( inspect_unit ) ds_unit_card( 4, 24, 262, 104, inspect_unit, "ENEMY  (red: move, orange: attack reach)", 0 );
    else {
        dg_panel( 4, 24, 262, 104 );
        dg_text( &dsfont_bold, 12, 30, DC_TEXT, "No unit selected" );
        dg_text_wrap( &dsfont_small, 12, 50, 246, 70, DC_DIM,
                      "R: next unit with orders.  Green dot: can move.  Amber dot: can still attack.", 0 );
    }
    /* lower card */
    if ( fc_target || fc_strat ) forecast_card( 4, 132, 262, 104 );
    else if ( cur_unit && inspect_unit ) ds_unit_card( 4, 132, 262, 104, inspect_unit, "ENEMY - not in reach of the selected unit", 0 );
    else if ( cursor_on && cur_mx >= 0 ) hex_card( 4, 132, 262, 104, cur_mx, cur_my );
    else hints_card( 4, 132, 262, 104 );
}

/* signature of everything the panels show; redraw when it changes */
static unsigned state_sig( void )
{
    unsigned s = 2166136261u;
#define MIX( v ) s = ( s ^ (unsigned)( v ) ) * 16777619u
    MIX( (size_t)cur_unit ); MIX( (size_t)cur_player ); MIX( dsb_status() ); MIX( turn );
    MIX( ui_mode ); MIX( cur_mx ); MIX( cur_my ); MIX( cursor_on ); MIX( (size_t)fc_target ); MIX( fc_strat );
    MIX( (size_t)detail_unit ); MIX( (size_t)inspect_unit ); MIX( reach_on ); MIX( zoom ); MIX( map_x ); MIX( map_y );
    MIX( vx / 16 ); MIX( vy / 16 ); MIX( msg_count ); MIX( (size_t)deploy_unit ); MIX( cur_weather );
    if ( cur_unit ) { MIX( cur_unit->str ); MIX( cur_unit->cur_mov ); MIX( cur_unit->cur_atk_count );
                      MIX( cur_unit->cur_fuel ); MIX( cur_unit->cur_ammo ); MIX( cur_unit->x ); MIX( cur_unit->y ); }
    if ( cur_player ) MIX( cur_player->cur_prestige );
    if ( msg_count ) MIX( msgs[0][0] + msgs[0][1] * 7 + msgs[0][2] * 13 );
#undef MIX
    return s;
}

void ds_present( void )
{
    unsigned sig;
    if ( !ds_screen ) return;
    dg_target( ds_screen );
    sig = state_sig();
    if ( sig != last_sig ) { last_sig = sig; ds_top_dirty = 1; ds_bot_dirty = 1; }
    if ( !ds_top_dirty && !ds_bot_dirty ) return;
    if ( ds_bot_dirty ) {
        dg_clip( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H );
        if ( ui_mode != UM_NONE ) scr_draw_bottom();
        else draw_bottom_map();
        dg_noclip();
#ifndef __3DS__
        dg_fill( 0, DS_BOT_Y, DS_BOT_X, DS_BOT_H, 0x000000 );
        dg_fill( DS_BOT_X + DS_BOT_W, DS_BOT_Y, DS_W - DS_BOT_X - DS_BOT_W, DS_BOT_H, 0x000000 );
#endif
    }
    if ( ds_top_dirty ) {
        dg_clip( 0, 0, DS_W, DS_TOP_H );
        draw_top();
        dg_noclip();
    }
    SDL_UpdateRect( ds_screen, 0, 0, 0, 0 );
    ds_top_dirty = ds_bot_dirty = 0;
}

/* called by the screens module to draw the live map under an overlay */
void ds_draw_bottom_map( void ) { draw_bottom_map(); }

void dsui_canvas_updated( int count, SDL_Rect *rects )
{
    (void)count; (void)rects;
    ds_bot_dirty = 1;
    ds_present();
}

void dsui_frame( void )
{
    ds_present();
}

/* ======================================================================
   Blocking status screen (turn info, scenario end)
   ====================================================================== */
static const char *status_title = 0, *status_text = 0;
static const char **status_opts = 0;
static int status_nopts = 0;
#define CH_BTN_H 34
#define CH_BTN_GAP 8
static int choice_y0( void ) { return ( DS_BOT_H - status_nopts * ( CH_BTN_H + CH_BTN_GAP ) + CH_BTN_GAP ) / 2; }

/* blocking choice: returns the index of the chosen option (B = 0) */
int dsui_choice_screen( const char *title, const char *text, const char **opts, int n )
{
    Uint32 start = SDL_GetTicks();
    int old_mode = ui_mode, r = -1, i;
    extern int sdl_quit;
    status_title = title; status_text = text; status_opts = opts; status_nopts = n;
    ui_mode = UM_STATUS;
    ds_mark_all(); ds_present();
    while ( r < 0 ) {
        SDL_PumpEvents();
        ds_read_input();
        if ( sdl_quit ) { r = 0; break; }
        if ( SDL_GetTicks() - start > 250 ) {
            if ( in.pressed & K_B ) r = 0;
            if ( in.tap )
                for ( i = 0; i < n; i++ )
                    if ( ds_hit( in.tap_x, in.tap_y, 40, choice_y0() + i * ( CH_BTN_H + CH_BTN_GAP ), DS_BOT_W - 80, CH_BTN_H ) ) r = i;
        }
        SDL_Delay( 16 );
    }
    ui_mode = old_mode == UM_STATUS ? UM_NONE : old_mode;
    status_title = status_text = 0; status_opts = 0; status_nopts = 0;
    ds_mark_all();
    return r;
}

void dsui_status_screen( const char *title, const char *text, int timeout_ms )
{
    Uint32 start = SDL_GetTicks();
    int old_mode = ui_mode;
    extern int sdl_quit;
    status_title = title; status_text = text;
    ui_mode = UM_STATUS;
    ds_mark_all(); ds_present();
    /* wait until all input is released, then for a tap / A / B / Start */
    for ( ;; ) {
        SDL_PumpEvents();
        ds_read_input();
        if ( sdl_quit ) break;
        if ( timeout_ms > 0 && SDL_GetTicks() - start >= (Uint32)timeout_ms ) break;
        if ( SDL_GetTicks() - start > 250 && ( in.tap || ( in.pressed & ( K_A | K_B | K_START ) ) ) ) break;
        SDL_Delay( 16 );
    }
    ui_mode = old_mode == UM_STATUS ? UM_NONE : old_mode;
    status_title = status_text = 0;
    ds_mark_all();
}

void ds_status_draw_top( void )
{
    dg_fill( 0, 0, DS_W, DS_TOP_H, DC_BG );
    dg_panel( 10, 14, DS_W - 20, DS_TOP_H - 28 );
    if ( status_title ) dg_text_c( &dsfont_title, DS_W / 2, 26, DC_ACCENT, status_title );
    if ( status_text ) dg_text_wrap( &dsfont_body, 26, 54, DS_W - 52, DS_TOP_H - 80, DC_TEXT, status_text, 0 );
}
void ds_status_draw_bottom( void )
{
    dg_fill( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, DC_BG );
    if ( status_nopts > 0 ) {
        int i;
        for ( i = 0; i < status_nopts; i++ )
            ds_button( DS_BOT_X + 40, DS_BOT_Y + choice_y0() + i * ( CH_BTN_H + CH_BTN_GAP ), DS_BOT_W - 80, CH_BTN_H,
                       status_opts[i], 1, i == 0 );
        return;
    }
    ds_button( DS_BOT_X + 60, DS_BOT_Y + 90, 200, 50, "Continue", 1, 1 );
    dg_text_c( &dsfont_small, DS_BOT_X + DS_BOT_W / 2, DS_BOT_Y + 150, DC_DIM, "Tap or press A" );
}

#endif /* DSUI */
