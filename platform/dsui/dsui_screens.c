/*
 * dsui_screens.c - native menus and dialogs for the 3DS front end:
 * title, game menu, confirm, scenario/campaign pickers, save/load, options,
 * purchase, split, unit list, overview map, campaign briefing, text pages.
 *
 * Bottom screen = the touch list/buttons, top screen = details for the selection.
 */
#ifdef DSUI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsui_int.h"
#include <strings.h>
#ifdef WITH_SOUND
#include "audio.h"
extern int audio_ok;
#endif
#include "file.h"

extern void ds_draw_bottom_map( void );
extern void ds_status_draw_top( void );
extern void ds_status_draw_bottom( void );
extern char *gui_scen_info_text( void );
extern char *gui_conds_text( void );
extern const char *get_gamedir( void );

/* ======================================================================
   Generic touch list
   ====================================================================== */
#define MAX_ITEMS 160
#define HEAD_H 24
#define ROW_H 26
#define FOOT_H 36

typedef struct { char label[80]; char right[40]; int id; int enabled; } Item;
static Item items[MAX_ITEMS];
static int n_items = 0, sel = 0, scroll_top = 0;
static char list_title[64];
static int two_tap = 0;            /* first tap selects (details on top), second activates */
static const char *btns[3];
static int btn_on[3] = { 1, 1, 1 };
static int n_btns = 0;
static int list_x = 0, list_w = DS_BOT_W;   /* list column inside the bottom screen */

enum { L_NONE = -1, L_BACK = -2, L_BTN0 = -10 };

static void lst_init( const char *title, int twotap )
{
    n_items = 0; sel = 0; scroll_top = 0; n_btns = 0;
    two_tap = twotap; list_x = 0; list_w = DS_BOT_W;
    snprintf( list_title, sizeof list_title, "%s", title );
}
static void lst_add( const char *label, const char *right, int id, int enabled )
{
    if ( n_items >= MAX_ITEMS ) return;
    snprintf( items[n_items].label, sizeof items[0].label, "%s", label ? label : "" );
    snprintf( items[n_items].right, sizeof items[0].right, "%s", right ? right : "" );
    items[n_items].id = id; items[n_items].enabled = enabled;
    n_items++;
}
static void lst_buttons( const char *a, const char *b, const char *c )
{
    n_btns = 0;
    btn_on[0] = btn_on[1] = btn_on[2] = 1;
    if ( a ) btns[n_btns++] = a;
    if ( b ) btns[n_btns++] = b;
    if ( c ) btns[n_btns++] = c;
}
static int list_rows( void ) { return ( DS_BOT_H - HEAD_H - ( n_btns ? FOOT_H : 0 ) ) / ROW_H; }
static void list_clamp( void )
{
    int rows = list_rows();
    if ( sel >= n_items ) sel = n_items - 1;
    if ( sel < 0 ) sel = 0;
    if ( sel < scroll_top ) scroll_top = sel;
    if ( sel >= scroll_top + rows ) scroll_top = sel - rows + 1;
    if ( scroll_top > n_items - rows ) scroll_top = n_items - rows;
    if ( scroll_top < 0 ) scroll_top = 0;
}

static void draw_list( void )
{
    int i, rows = list_rows();
    int x = DS_BOT_X + list_x, y = DS_BOT_Y;
    dg_fill( x, y, list_w, DS_BOT_H, DC_BG );
    dg_fill( x, y, list_w, HEAD_H - 2, DC_PANEL );
    dg_text_fit( &dsfont_bold, x + 8, y + 4, list_w - 16, DC_ACCENT, list_title );
    if ( n_items > rows ) {
        char t[24];
        snprintf( t, sizeof t, "%d/%d", sel + 1, n_items );
        dg_text_r( &dsfont_small, x + list_w - 6, y + 6, DC_DIM, t );
    }
    for ( i = 0; i < rows && scroll_top + i < n_items; i++ ) {
        Item *it = &items[scroll_top + i];
        int ry = y + HEAD_H + i * ROW_H;
        int is_sel = ( scroll_top + i == sel );
        dg_fill( x + 2, ry, list_w - 4, ROW_H - 2, is_sel ? 0x34507a : DC_PANEL );
        if ( is_sel ) dg_rect( x + 2, ry, list_w - 4, ROW_H - 2, DC_ACCENT );
        dg_text_fit( &dsfont_body, x + 10, ry + 5, list_w - 20 - dg_text_w( &dsfont_body, it->right ),
                     it->enabled ? DC_TEXT : DC_DISABLED, it->label );
        if ( it->right[0] ) dg_text_r( &dsfont_body, x + list_w - 10, ry + 5, it->enabled ? DC_DIM : DC_DISABLED, it->right );
    }
    if ( n_items > rows ) {
        /* scroll bar */
        int bh = ( DS_BOT_H - HEAD_H - ( n_btns ? FOOT_H : 0 ) );
        int th = bh * rows / n_items, ty = bh * scroll_top / n_items;
        dg_fill( x + list_w - 3, y + HEAD_H + ty, 2, th, DC_BORDER );
    }
    if ( n_btns ) {
        int bw = ( list_w - 8 - ( n_btns - 1 ) * 6 ) / n_btns;
        for ( i = 0; i < n_btns; i++ )
            ds_button( x + 4 + i * ( bw + 6 ), y + DS_BOT_H - FOOT_H + 4, bw, FOOT_H - 8, btns[i], btn_on[i], i == n_btns - 1 && btn_on[i] );
    }
}

/* returns item id when activated, L_BACK, L_BTN0+i (negative), or L_NONE */
static int list_input( void )
{
    int rows = list_rows();
    static int acc = 0;
    if ( in.rep & K_UP ) { sel--; list_clamp(); }
    if ( in.rep & K_DOWN ) { sel++; list_clamp(); }
    if ( in.rep & K_LEFT ) { sel -= rows; list_clamp(); }
    if ( in.rep & K_RIGHT ) { sel += rows; list_clamp(); }
    if ( in.pressed & K_B ) return L_BACK;
    if ( ( in.pressed & K_A ) && n_items > 0 && items[sel].enabled ) return items[sel].id;
    if ( ( in.pressed & K_Y ) && n_btns > 0 && btn_on[n_btns - 1] ) return L_BTN0 - ( n_btns - 1 );
    if ( in.drag && in.down_x >= list_x && in.down_x < list_x + list_w && in.down_y >= HEAD_H ) {
        acc += in.drag_dy;
        while ( acc <= -ROW_H / 2 ) { acc += ROW_H / 2; if ( scroll_top + rows < n_items ) scroll_top++; }
        while ( acc >= ROW_H / 2 ) { acc -= ROW_H / 2; if ( scroll_top > 0 ) scroll_top--; }
        if ( sel < scroll_top ) sel = scroll_top;
        if ( sel >= scroll_top + rows ) sel = scroll_top + rows - 1;
    }
    if ( in.down ) acc = 0;
    if ( in.tap && in.tap_x >= list_x && in.tap_x < list_x + list_w ) {
        int ty = in.tap_y;
        if ( n_btns && ty >= DS_BOT_H - FOOT_H ) {
            int bw = ( list_w - 8 - ( n_btns - 1 ) * 6 ) / n_btns;
            int i = ( in.tap_x - list_x - 4 ) / ( bw + 6 );
            if ( i >= 0 && i < n_btns && btn_on[i] ) return L_BTN0 - i;
            return L_NONE;
        }
        if ( ty >= HEAD_H ) {
            int i = scroll_top + ( ty - HEAD_H ) / ROW_H;
            if ( i < n_items && ( ty - HEAD_H ) / ROW_H < rows ) {
                if ( two_tap != 2 && ( !two_tap || i == sel ) ) { sel = i; if ( items[i].enabled ) return items[i].id; }
                sel = i;
            }
        }
    }
    return L_NONE;
}

/* ======================================================================
   Mode bookkeeping
   ====================================================================== */
static int return_mode = UM_NONE;    /* where Back goes for nested lists */

void scr_close( void )
{
    ui_mode = UM_NONE;
    ds_mark_all();
}

static void top_text_panel( const char *title, const char *text, int skip )
{
    dg_fill( 0, 0, DS_W, DS_TOP_H, DC_BG );
    dg_panel( 6, 6, DS_W - 12, DS_TOP_H - 12 );
    if ( title ) dg_text_fit( &dsfont_title, 16, 12, DS_W - 32, DC_ACCENT, title );
    if ( text ) dg_text_wrap( &dsfont_body, 16, title ? 36 : 14, DS_W - 32, DS_TOP_H - ( title ? 48 : 26 ), DC_TEXT, text, skip );
}

/* ======================================================================
   Options (persisted in the config dir)
   ====================================================================== */
void dsui_options_load( void )
{
    char path[600];
    FILE *f;
    snprintf( path, sizeof path, "%s/dsui.conf", config.dir_name );
    if ( ( f = fopen( path, "r" ) ) ) {
        int a = 0, b = 1;
        if ( fscanf( f, "quick_attack=%d auto_advance=%d", &a, &b ) == 2 ) {
            dsui_quick_attack = a; dsui_auto_advance = b;
        }
        fclose( f );
    }
}
void dsui_options_save( void )
{
    char path[600];
    FILE *f;
    snprintf( path, sizeof path, "%s/dsui.conf", config.dir_name );
    if ( ( f = fopen( path, "w" ) ) ) {
        fprintf( f, "quick_attack=%d auto_advance=%d\n", dsui_quick_attack, dsui_auto_advance );
        fclose( f );
    }
}

static void build_options( void )
{
    int keep = sel;
    lst_init( "Options", 0 );
    lst_add( "Attack needs a forecast tap first", dsui_quick_attack ? "Off" : "On", 1, 1 );
    lst_add( "Auto-select the next ready unit", dsui_auto_advance ? "On" : "Off", 2, 1 );
    lst_add( "Strength numbers on units", config.show_bar ? "On" : "Off", 3, 1 );
    lst_add( "Hex grid", config.grid ? "On" : "Off", 4, 1 );
    lst_add( "Watch the enemy turn", config.show_cpu_turn ? "On" : "Off", 5, 1 );
    {
        char t[16]; snprintf( t, sizeof t, "x%d", config.anim_speed );
        lst_add( "Animation speed", t, 6, 1 );
    }
#ifdef WITH_SOUND
    lst_add( "Sound effects", !audio_ok ? "Unavailable" : config.sound_on ? "On" : "Off", 7, audio_ok );
    {
        char t[16]; snprintf( t, sizeof t, "%d%%", config.sound_volume * 100 / 128 );
        lst_add( "Volume", t, 8, audio_ok && config.sound_on );
    }
#endif
    lst_buttons( "Back", 0, 0 );
    sel = keep; list_clamp();
}

void scr_open_options( void )
{
    return_mode = ui_mode;
    ui_mode = UM_OPTIONS;
    build_options();
    ds_mark_all();
}

static void options_input( void )
{
    int r = list_input();
    switch ( r ) {
        case 1: dsui_quick_attack = !dsui_quick_attack; break;
        case 2: dsui_auto_advance = !dsui_auto_advance; break;
        case 3: config.show_bar = !config.show_bar; dsb_redraw(); break;
        case 4: config.grid = !config.grid; dsb_redraw(); break;
        case 5: config.show_cpu_turn = !config.show_cpu_turn; break;
        case 6: config.anim_speed = config.anim_speed >= 4 ? 1 : config.anim_speed + 1; break;
#ifdef WITH_SOUND
        case 7: config.sound_on = !config.sound_on; audio_enable( config.sound_on ); break;
        case 8:
            config.sound_volume = config.sound_volume >= 128 ? 32 : config.sound_volume + 32;
            if ( config.sound_volume > 128 ) config.sound_volume = 128;
            audio_set_volume( config.sound_volume );
            break;
#endif
        case L_BACK: case L_BTN0:
            dsui_options_save();
            save_config();
            if ( return_mode == UM_TITLE ) scr_open_title();
            else if ( return_mode == UM_MENU ) scr_open_game_menu();
            else scr_close();
            return;
        default: return;
    }
    build_options();
}

/* ======================================================================
   Text pages (scenario info, objectives, help)
   ====================================================================== */
static char *page_text = 0;
static char page_title[64];
static int page_skip = 0;
static int page_lines_per = 10;
#define MAX_PAGES 32
static int page_start[MAX_PAGES], page_n = 1, page_i = 0;

/* paginate at paragraph breaks ("##") where possible */
static void pages_layout( void )
{
    char *copy, *p;
    int line = 0, used = 0;
    page_n = 1; page_start[0] = 0; page_i = 0;
    if ( !page_text ) return;
    copy = strdup( page_text );
    p = copy;
    while ( p && *p ) {
        char *brk = strstr( p, "##" ), save = 0;
        int n;
        if ( brk ) { save = *brk; *brk = 0; }
        n = dg_text_lines( &dsfont_body, DS_W - 32, p );
        if ( n < 1 ) n = 1;
        if ( used > 0 && used + 1 + n > page_lines_per && page_n < MAX_PAGES ) {
            page_start[page_n++] = line; used = 0;     /* new page starts with this paragraph */
        }
        else if ( used > 0 ) used++;                   /* blank separator line */
        used += n;
        while ( used > page_lines_per && page_n < MAX_PAGES ) {
            /* paragraph longer than a page: hard split */
            page_start[page_n] = page_start[page_n - 1] + page_lines_per;
            page_n++;
            used -= page_lines_per;
        }
        line += n + 1;
        if ( brk ) { *brk = save; p = brk + 2; } else p = 0;
    }
    free( copy );
}

void scr_open_pages( const char *title, char *text_owned )
{
    free( page_text );
    page_text = text_owned;
    snprintf( page_title, sizeof page_title, "%s", title );
    page_lines_per = ( DS_TOP_H - 48 ) / ( dsfont_body.height + 1 );
    pages_layout();
    page_skip = 0;
    if ( ui_mode != UM_PAGES ) return_mode = ui_mode;
    ui_mode = UM_PAGES;
    ds_mark_all();
}

static void pages_draw_top( void )
{
    int lines = ( page_i + 1 < page_n ) ? page_start[page_i + 1] - page_skip : page_lines_per;
    if ( lines > page_lines_per ) lines = page_lines_per;
    dg_fill( 0, 0, DS_W, DS_TOP_H, DC_BG );
    dg_panel( 6, 6, DS_W - 12, DS_TOP_H - 12 );
    dg_text_fit( &dsfont_title, 16, 12, DS_W - 32, DC_ACCENT, page_title );
    if ( page_text )
        dg_text_wrap( &dsfont_body, 16, 36, DS_W - 32, lines * ( dsfont_body.height + 1 ), DC_TEXT, page_text, page_skip );
}

static void pages_draw_bottom( void )
{
    char t[32];
    dg_fill( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, DC_BG );
    snprintf( t, sizeof t, "Page %d of %d", page_i + 1, page_n );
    dg_text_c( &dsfont_bold, DS_BOT_X + DS_BOT_W / 2, DS_BOT_Y + 40, DC_TEXT, t );
    ds_button( DS_BOT_X + 10, DS_BOT_Y + 80, 140, 56, "Previous", page_i > 0, 0 );
    ds_button( DS_BOT_X + 170, DS_BOT_Y + 80, 140, 56, "Next", page_i + 1 < page_n, 0 );
    ds_button( DS_BOT_X + 60, DS_BOT_Y + 170, 200, 44, "Close", 1, 1 );
}

static void pages_input( void )
{
    int prev = ( in.rep & ( K_UP | K_LEFT | K_L ) ) || ( in.tap && ds_hit( in.tap_x, in.tap_y, 10, 80, 140, 56 ) );
    int next = ( in.rep & ( K_DOWN | K_RIGHT | K_R ) ) || ( in.tap && ds_hit( in.tap_x, in.tap_y, 170, 80, 140, 56 ) );
    int close = ( in.pressed & ( K_B | K_A | K_START ) ) || ( in.tap && ds_hit( in.tap_x, in.tap_y, 60, 170, 200, 44 ) );
    if ( prev && page_i > 0 ) page_i--;
    if ( next && page_i + 1 < page_n ) page_i++;
    page_skip = page_start[page_i];
    if ( close ) {
        free( page_text ); page_text = 0;
        if ( return_mode == UM_TITLE ) scr_open_title();
        else if ( return_mode == UM_MENU ) scr_open_game_menu();
        else scr_close();
    }
}

static const char *help_text =
    "THE TWO SCREENS#"
    "The map is on the touch screen. The top screen shows the selected unit, the battle forecast or "
    "the hex under the cursor, a minimap and your objectives.##"
    "TOUCH#"
    "- Tap your unit to select it. Tap a hex to move there.#"
    "- Tap an enemy to see the battle forecast, tap it again to attack.#"
    "- Drag a unit to a hex to move it in one motion. Drag empty map to pan.#"
    "- Long-press any unit for its full statistics.#"
    "- Tap the selected unit again to switch between the air and ground unit on that hex, "
    "or to deselect it.#"
    "- With nothing selected, tap an enemy to see where it can move (red) and attack (orange).#"
    "- The strip on the right edge holds Supply, Undo, Air (embark/debark), Merge, Split and Info.##"
    "BUTTONS#"
    "Circle Pad / D-pad: move the hex cursor.  A: act on the cursor hex.  B: cancel / deselect.#"
    "X: undo the last move.  Y: end turn.  R: next unit with orders.  Hold L: all enemy threat.#"
    "ZL / ZR: zoom out / in.  C-stick: pan.  Start: menu.  Select: overview map.##"
    "MARKERS#"
    "Green dot: unit can still move.  Amber dot: unit can still attack.#"
    "Hexes outside a selected unit's reach are darkened.##"
    "BUYING AND DEPLOYING#"
    "Start > Purchase units: pick a class on the left, a unit, then Buy. X shows your orders (Refund).#"
    "New units arrive next turn: Start > Deploy units, tap one in the left strip, then a highlighted hex.#"
    "On a deployment turn you can also tap placed units to pick them up and move them.##"
    "TIPS#"
    "- Supply units next to cities, airfields or ports for full refit.#"
    "- Artillery and anti-air give defensive fire to adjacent friends.#"
    "- Entrenched infantry in rough terrain is very hard to dislodge.#"
    "- The game autosaves at the start of each of your turns (Continue on the title screen).";

void scr_open_help( void )
{
    scr_open_pages( "How to play", strdup( help_text ) );
}

/* ======================================================================
   Confirm
   ====================================================================== */
static char cf_text[256];
static int cf_engine = 0;
static void (*cf_yes)( void ) = 0;

void scr_open_confirm( const char *text, int engine_owned, void (*yes)( void ) )
{
    snprintf( cf_text, sizeof cf_text, "%s", text && *text ? text : "Are you sure?" );
    cf_engine = engine_owned; cf_yes = yes;
    ui_mode = UM_CONFIRM;
    ds_mark_all();
}

static void confirm_draw_bottom( void )
{
    ds_draw_bottom_map();
    dg_fill_a( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, DC_BG, 170 );
    dg_panel( DS_BOT_X + 16, DS_BOT_Y + 36, DS_BOT_W - 32, 150 );
    dg_text_wrap( &dsfont_bold, DS_BOT_X + 26, DS_BOT_Y + 46, DS_BOT_W - 52, 80, DC_TEXT, cf_text, 0 );
    ds_button( DS_BOT_X + 26, DS_BOT_Y + 132, 124, 44, "No", 1, 0 );
    ds_button( DS_BOT_X + 170, DS_BOT_Y + 132, 124, 44, "Yes", 1, 1 );
}

static void confirm_input( void )
{
    int yes = ( in.pressed & K_A ) || ( in.tap && ds_hit( in.tap_x, in.tap_y, 170, 132, 124, 44 ) );
    int no = ( in.pressed & K_B ) || ( in.tap && ds_hit( in.tap_x, in.tap_y, 26, 132, 124, 44 ) );
    if ( !yes && !no ) return;
    ui_mode = UM_NONE;
    if ( cf_engine ) dsb_press( yes ? ID_OK : ID_CANCEL );
    else if ( yes && cf_yes ) cf_yes();
    ds_mark_all();
}

/* ======================================================================
   Split
   ====================================================================== */
void scr_open_split( void )
{
    int i, max;
    char t[64];
    if ( !cur_unit ) return;
    max = unit_get_split_strength( cur_unit );
    if ( max > 10 ) max = 10;
    lst_init( "Split off how many strength points?", 0 );
    for ( i = 1; i <= max; i++ ) {
        snprintf( t, sizeof t, "%d strength", i );
        lst_add( t, 0, i, 1 );
    }
    lst_buttons( "Cancel", 0, 0 );
    ui_mode = UM_SPLIT;
    ds_mark_all();
}
static void split_input( void )
{
    int r = list_input();
    if ( r > 0 ) { ui_mode = UM_NONE; dsb_press( ID_SPLIT_1 + r - 1 ); ds_mark_all(); }
    else if ( r == L_BACK || r == L_BTN0 ) scr_close();
}

/* ======================================================================
   Save / load slots
   ====================================================================== */
static int slots_save = 0;
static int slots_from = UM_NONE;
static int pending_slot = -1;

static void build_slots( void )
{
    int i;
    lst_init( slots_save ? "Save game - pick a slot" : "Load game", 0 );
    for ( i = 0; i < SLOT_COUNT; i++ ) {
        char lab[80];
        int valid = slot_is_valid( i );
        if ( i == SLOT_COUNT - 1 ) {
            if ( slots_save ) continue;            /* autosave slot is not for manual saves */
            snprintf( lab, sizeof lab, "%s", valid ? slot_get_name( i ) : "Autosave (empty)" );
        }
        else snprintf( lab, sizeof lab, "%d. %s", i + 1, valid ? slot_get_name( i ) : "(empty)" );
        lst_add( lab, 0, i + 1, slots_save ? 1 : valid );
    }
    lst_buttons( "Back", 0, 0 );
}
void scr_open_slots( int save )
{
    slots_save = save;
    slots_from = ui_mode;
    build_slots();
    ui_mode = UM_SLOTS;
    ds_mark_all();
}
static void do_save( void )
{
    char name[64];
    if ( pending_slot < 0 || !scen_info ) return;
    snprintf( name, sizeof name, "%.36s, turn %d", scen_info->name, turn + 1 );
    slot_save( pending_slot, name );
    pending_slot = -1;
}
static void slots_input( void )
{
    int r = list_input();
    if ( r == L_BACK || r == L_BTN0 ) {
        if ( slots_from == UM_TITLE ) scr_open_title(); else scr_open_game_menu();
        return;
    }
    if ( r <= 0 ) return;
    r--;
    if ( slots_save ) {
        pending_slot = r;
        if ( slot_is_valid( r ) ) scr_open_confirm( "Overwrite this saved game?", 0, do_save );
        else { do_save(); scr_close(); }
    }
    else {
        scr_close();
        dsb_set_status( setup.type == SETUP_RUN_TITLE ? DSB_STATUS_TITLE : DSB_STATUS_NONE );
        action_queue_load( r );
    }
}

/* ======================================================================
   Scenario / campaign pickers
   ====================================================================== */
#define MAX_PATHS 160
static char *paths[MAX_PATHS];
static int n_paths = 0;
static char scen_group[128];
static int scen_level = 0;           /* 0 = groups, 1 = scenarios in group */
static char *sel_info = 0;
static int sel_info_for = -1;
static int picking_camp = 0;

static void clear_paths( void )
{
    int i;
    for ( i = 0; i < n_paths; i++ ) free( paths[i] );
    n_paths = 0;
    free( sel_info ); sel_info = 0; sel_info_for = -1;
}

static void build_dir_list( const char *base, const char *sub, int dirs_only_level )
{
    char path[600];
    List *entries;
    char *e;
    clear_paths();
    snprintf( path, sizeof path, "%s/%s%s%s", get_gamedir(), base, sub[0] ? "/" : "", sub );
    entries = dir_get_entries( path, path, 0 );
    if ( !entries ) return;
    list_reset( entries );
    while ( ( e = list_next( entries ) ) && n_paths < MAX_PATHS ) {
        int is_dir = ( e[0] == '*' );
        const char *name = is_dir ? e + 1 : e;
        char rel[300];
        if ( strcmp( name, ".." ) == 0 || strcmp( name, "." ) == 0 ) continue;
        if ( dirs_only_level && !is_dir ) continue;
        if ( !dirs_only_level && is_dir ) continue;
        snprintf( rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", name );
        paths[n_paths++] = strdup( rel );
        if ( strcasecmp( name, "pg" ) == 0 )
            lst_add( is_dir ? "Panzer General (1939-1945)" : "Panzer General: World War II", is_dir ? ">" : 0, n_paths, 1 );
        else if ( strcasecmp( name, "kukgen" ) == 0 )
            lst_add( is_dir ? "K.u.k General (World War I)" : "K.u.k General: World War I", is_dir ? ">" : 0, n_paths, 1 );
        else lst_add( name, is_dir ? ">" : 0, n_paths, 1 );
    }
    list_delete( entries );
}

static void scen_build( void )
{
    if ( picking_camp ) {
        lst_init( "New campaign", 1 );
        build_dir_list( "campaigns", "", 0 );
        lst_buttons( "Back", "Start", 0 );
    }
    else if ( scen_level == 0 ) {
        lst_init( "New scenario - choose a set", 0 );
        build_dir_list( "scenarios", "", 1 );
        {   /* Panzer General first */
            int k;
            for ( k = 1; k < n_items; k++ )
                if ( strncmp( items[k].label, "Panzer", 6 ) == 0 ) {
                    Item tmp = items[k]; memmove( &items[1], &items[0], sizeof( Item ) * k ); items[0] = tmp;
                    break;
                }
        }
        lst_buttons( "Back", 0, 0 );
    }
    else {
        char t[64];
        snprintf( t, sizeof t, "Scenarios: %s", scen_group );
        lst_init( t, 1 );
        build_dir_list( "scenarios", scen_group, 0 );
        lst_buttons( "Back", "Next", 0 );
    }
}

static void open_picker( int camp )
{
    picking_camp = camp;
    scen_level = 0;
    scen_build();
    ui_mode = camp ? UM_CAMPLIST : UM_SCENLIST;
    ds_mark_all();
}

static void ensure_sel_info( void )
{
    if ( n_items <= 0 || sel_info_for == sel ) return;
    free( sel_info ); sel_info = 0;
    sel_info_for = sel;
    if ( picking_camp ) sel_info = camp_load_info( paths[items[sel].id - 1] );
    else if ( scen_level == 1 ) sel_info = scen_load_info( paths[items[sel].id - 1] );
}

static void open_scen_setup( void );

static void scenlist_input( void )
{
    int r = list_input();
    if ( r == L_BACK || r == L_BTN0 ) {
        if ( !picking_camp && scen_level == 1 ) { scen_level = 0; scen_build(); }
        else { clear_paths(); scr_open_title(); }
        return;
    }
    if ( r == L_BTN0 - 1 ) r = n_items > 0 ? items[sel].id : L_NONE;   /* Start / Next button */
    if ( r <= 0 ) return;
    if ( picking_camp ) {
        free( sel_info ); sel_info = camp_load_info( paths[r - 1] ); sel_info_for = sel;
        if ( !sel_info ) return;
        clear_paths();
        scr_close();
        setup.type = SETUP_CAMP_BRIEFING;
        dsb_set_status( DSB_STATUS_NONE );
        action_queue_start_camp();
        return;
    }
    if ( scen_level == 0 ) {
        snprintf( scen_group, sizeof scen_group, "%s", paths[r - 1] );
        scen_level = 1;
        scen_build();
        return;
    }
    free( sel_info ); sel_info = scen_load_info( paths[r - 1] ); sel_info_for = sel;
    if ( sel_info ) open_scen_setup();
}

static void scenlist_draw_top( void )
{
    ensure_sel_info();
    if ( sel_info ) top_text_panel( 0, sel_info, 0 );
    else if ( !picking_camp && scen_level == 0 )
        top_text_panel( "New scenario", "Choose a scenario set. Panzer General covers 1939-1945 from Poland to Berlin; "
                        "K.u.k General is a World War I set.", 0 );
    else top_text_panel( picking_camp ? "New campaign" : "New scenario", "Select an entry to see its briefing.", 0 );
}

/* scenario setup (player control and rules) */
static void build_setup( void )
{
    int i, keep = sel;
    char t[96];
    lst_init( "Scenario setup", 0 );
    for ( i = 0; i < setup.player_count; i++ ) {
        snprintf( t, sizeof t, "%s", setup.names[i] );
        lst_add( t, setup.ctrl[i] == PLAYER_CTRL_HUMAN ? "Human" : "CPU", 100 + i, 1 );
    }
    lst_add( "Fog of war", config.fog_of_war ? "On" : "Off", 1, 1 );
    lst_add( "Supply", config.supply ? "On" : "Off", 2, 1 );
    lst_add( "Weather", config.weather ? "On" : "Off", 3, 1 );
    lst_add( "Deployment turn", config.deploy_turn ? "On" : "Off", 4, 1 );
    lst_add( "Purchase units with prestige", config.purchase ? "On" : "Off", 5, 1 );
    lst_buttons( "Back", "Start", 0 );
    sel = keep; list_clamp();
}
static void open_scen_setup( void )
{
    sel = 0;
    build_setup();
    ui_mode = UM_SCENSETUP;
    ds_mark_all();
}
static void scensetup_input( void )
{
    int r = list_input();
    if ( r == L_BACK || r == L_BTN0 ) { scen_level = 1; scen_build(); ui_mode = UM_SCENLIST; return; }
    if ( r == L_BTN0 - 1 ) {
        int i, humans = 0;
        for ( i = 0; i < setup.player_count; i++ ) if ( setup.ctrl[i] == PLAYER_CTRL_HUMAN ) humans++;
        if ( humans == 0 ) {
            const char *opts[2] = { "Back to setup", "Watch the computer play" };
            if ( dsui_choice_screen( "No human player", "Every side is set to CPU, so you would only watch. "
                                     "Tap a side in the setup list to switch it to Human.", opts, 2 ) == 0 ) {
                ui_mode = UM_SCENSETUP; ds_mark_all(); return;
            }
        }
        clear_paths();
        scr_close();
        dsb_set_status( DSB_STATUS_NONE );
        action_queue_start_scen();
        return;
    }
    if ( r >= 100 && r - 100 < setup.player_count ) {
        int i = r - 100;
        setup.ctrl[i] = setup.ctrl[i] == PLAYER_CTRL_HUMAN ? PLAYER_CTRL_CPU : PLAYER_CTRL_HUMAN;
    }
    else switch ( r ) {
        case 1: config.fog_of_war = !config.fog_of_war; break;
        case 2: config.supply = !config.supply; break;
        case 3: config.weather = !config.weather; break;
        case 4: config.deploy_turn = !config.deploy_turn; break;
        case 5: config.purchase = !config.purchase; break;
        default: return;
    }
    build_setup();
}

/* ======================================================================
   Title
   ====================================================================== */
enum { T_CONTINUE = 1, T_SCEN, T_CAMP, T_LOAD, T_OPTIONS, T_HELP, T_QUIT };

void scr_open_title( void )
{
    lst_init( "LGeneral", 0 );
    if ( slot_is_valid( SLOT_COUNT - 1 ) ) lst_add( "Continue", slot_get_name( SLOT_COUNT - 1 ), T_CONTINUE, 1 );
    lst_add( "New scenario", 0, T_SCEN, 1 );
    lst_add( "New campaign", 0, T_CAMP, 1 );
    lst_add( "Load game", 0, T_LOAD, 1 );
    lst_add( "Options", 0, T_OPTIONS, 1 );
    lst_add( "How to play", 0, T_HELP, 1 );
    lst_add( "Quit", 0, T_QUIT, 1 );
    ui_mode = UM_TITLE;
    ds_mark_all();
}

static void title_draw_top( void )
{
    dg_fill( 0, 0, DS_W, DS_TOP_H, DC_BG );
    if ( gui && gui->bkgnd ) {
        int sw = gui->bkgnd->w, sh = gui->bkgnd->h;
        int dh = DS_TOP_H, dw = sw * dh / ( sh ? sh : 1 );
        if ( dw > DS_W ) { dw = DS_W; dh = sh * dw / sw; }
        dg_blit_scaled( gui->bkgnd, 0, 0, sw, sh, ( DS_W - dw ) / 2, ( DS_TOP_H - dh ) / 2, dw, dh );
    }
    dg_fill_a( 0, DS_TOP_H - 22, DS_W, 22, DC_BG, 190 );
    dg_text( &dsfont_small, 6, DS_TOP_H - 17, DC_DIM, "LGeneral 1.4.4 (c) Michael Speck - GPL. 3DS edition." );
}

static void title_input( void )
{
    int r = list_input();
    switch ( r ) {
        case T_CONTINUE:
            scr_close();
            action_queue_load( SLOT_COUNT - 1 );
            break;
        case T_SCEN: open_picker( 0 ); break;
        case T_CAMP: open_picker( 1 ); break;
        case T_LOAD: scr_open_slots( 0 ); break;
        case T_OPTIONS: scr_open_options(); break;
        case T_HELP: scr_open_help(); break;
        case T_QUIT:
            scr_close();
            action_queue_quit();
            break;
    }
}

/* ======================================================================
   Game menu (Start)
   ====================================================================== */
enum { G_RESUME = 1, G_UNITS, G_PURCHASE, G_DEPLOY, G_INFO, G_CONDS, G_SAVE, G_LOAD,
       G_OPTIONS, G_HELP, G_DISBAND, G_ENDTURN, G_RESTART, G_QUIT };

void scr_open_game_menu( void )
{
    char t[32];
    lst_init( "Menu", 0 );
    lst_add( "Resume", 0, G_RESUME, 1 );
    snprintf( t, sizeof t, "%d ready", ds_ready_units() );
    lst_add( "Unit list", t, G_UNITS, 1 );
    lst_add( "Purchase units", 0, G_PURCHASE, dsb_purchase_allowed() );
    if ( dsb_deploy_turn() ) t[0] = 0;
    else snprintf( t, sizeof t, "%d waiting", avail_units ? avail_units->count : 0 );
    lst_add( "Deploy units", t, G_DEPLOY, dsb_deploy_allowed() );
    lst_add( "Scenario info", 0, G_INFO, 1 );
    lst_add( "Victory conditions", 0, G_CONDS, 1 );
    lst_add( "Save game", 0, G_SAVE, dsb_status() == DSB_STATUS_NONE && !dsb_deploy_turn() );
    lst_add( "Load game", 0, G_LOAD, 1 );
    lst_add( "Options", 0, G_OPTIONS, 1 );
    lst_add( "How to play", 0, G_HELP, 1 );
    lst_add( "Disband selected unit", 0, G_DISBAND, cur_unit && cur_unit->unused );
    if ( dsb_status() == DSB_STATUS_DEPLOY ) lst_add( "Finish deployment", 0, G_ENDTURN, 1 );
    else lst_add( "End turn", 0, G_ENDTURN, dsb_status() == DSB_STATUS_NONE );
    lst_add( "Restart scenario", 0, G_RESTART, 1 );
    lst_add( "Quit to title", 0, G_QUIT, 1 );
    ui_mode = UM_MENU;
    ds_mark_all();
}

static void quit_yes( void ) { dsb_quit_to_title(); }
static void restart_yes( void ) { action_queue_restart(); }

static void menu_input( void )
{
    int r = list_input();
    switch ( r ) {
        case L_BACK: case G_RESUME: scr_close(); break;
        case G_UNITS: scr_open_unitlist(); break;
        case G_PURCHASE: scr_open_purchase(); break;
        case G_DEPLOY: scr_close(); dsb_press( ID_DEPLOY ); break;
        case G_INFO: scr_open_pages( scen_info ? scen_info->name : "Scenario", gui_scen_info_text() ); break;
        case G_CONDS: scr_open_pages( "Victory conditions", gui_conds_text() ); break;
        case G_SAVE: scr_open_slots( 1 ); break;
        case G_LOAD: scr_open_slots( 0 ); break;
        case G_OPTIONS: scr_open_options(); break;
        case G_HELP: scr_open_help(); break;
        case G_DISBAND: scr_close(); dsb_press( ID_DISBAND ); break;
        case G_ENDTURN: scr_close(); ds_end_turn_request(); break;
        case G_RESTART: scr_open_confirm( "Restart this scenario from the beginning?", 0, restart_yes ); break;
        case G_QUIT: scr_open_confirm( "Quit to the title screen?#Unsaved progress since the last autosave is lost.", 0, quit_yes ); break;
    }
}

/* ======================================================================
   Unit list
   ====================================================================== */
static Unit *ulist[MAX_ITEMS];

void scr_open_unitlist( void )
{
    Unit *u;
    int pass;
    char r[40];
    lst_init( "Your units (ready first)", 0 );
    if ( units && cur_player )
        for ( pass = 0; pass < 2; pass++ ) {
            list_reset( units );
            while ( ( u = list_next( units ) ) ) {
                int ready;
                if ( u->player != cur_player || u->killed || u->x < 0 ) continue;
                ready = ( u->cur_mov > 0 || u->cur_atk_count > 0 ) && !u->is_guarding;
                if ( ( pass == 0 ) != ready ) continue;
                if ( n_items >= MAX_ITEMS ) break;
                snprintf( r, sizeof r, "str %d%s", u->str, ready ? "  ready" : "" );
                ulist[n_items] = u;
                lst_add( u->name, r, n_items + 1, 1 );
            }
        }
    lst_buttons( "Back", 0, 0 );
    ui_mode = UM_UNITLIST;
    ds_mark_all();
}

static void unitlist_input( void )
{
    int r = list_input();
    if ( r == L_BACK || r == L_BTN0 ) { scr_open_game_menu(); return; }
    if ( r > 0 && r <= n_items ) {
        Unit *u = ulist[r - 1];
        scr_close();
        ds_clear_selection();
        ds_center_on( u->x, u->y );
        dsb_select( u );
    }
}

/* ======================================================================
   Overview (tap the strategic map to jump)
   ====================================================================== */
void scr_open_overview( void )
{
    ui_mode = UM_OVERVIEW;
    ds_mark_all();
}
static void overview_input( void )
{
    int mx, my;
    if ( in.pressed & ( K_B | K_SELECT | K_START ) ) { scr_close(); return; }
    if ( in.tap && ds_engine_ready() && ds_minimap_hit( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, DS_BOT_X + in.tap_x, DS_BOT_Y + in.tap_y, &mx, &my ) ) {
        scr_close();
        ds_center_on( mx, my );
    }
}

/* ======================================================================
   Campaign briefing
   ====================================================================== */
static int brief_skip = 0;

void scr_open_campbrief( void )
{
    void *pane = dsb_camp_pane();
    int i, n = gui_message_pane_option_count( pane );
    lst_init( camp_cur_scen && camp_cur_scen->title ? camp_cur_scen->title : "Campaign", 0 );
    if ( n > 0 )
        for ( i = 0; i < n; i++ ) lst_add( gui_message_pane_option_desc( pane, i ), 0, i + 1, 1 );
    else
        lst_add( camp_cur_scen && camp_cur_scen->scen ? "Begin the battle" : "Continue", 0, 1000, 1 );
    lst_add( "Back to title", 0, 2000, 1 );
    brief_skip = 0;
    ui_mode = UM_CAMPBRIEF;
    ds_mark_all();
}

static void campbrief_input( void )
{
    void *pane = dsb_camp_pane();
    int r;
    int lines = ( DS_TOP_H - 48 ) / ( dsfont_body.height + 1 );
    if ( in.rep & ( K_L ) ) { brief_skip -= lines; if ( brief_skip < 0 ) brief_skip = 0; }
    if ( in.rep & ( K_R ) ) brief_skip += lines;
    r = list_input();
    if ( r == L_BACK ) return;
    if ( r == 2000 ) { scr_close(); dsb_quit_to_title(); return; }
    if ( r == 1000 ) { scr_close(); dsb_camp_choose( 0 ); return; }
    if ( r > 0 && r <= gui_message_pane_option_count( pane ) ) {
        scr_close();
        dsb_camp_choose( gui_message_pane_option_id( pane, r - 1 ) );
    }
}

static void campbrief_draw_top( void )
{
    void *pane = dsb_camp_pane();
    const char *t = gui_message_pane_text( pane );
    int total = t ? dg_text_lines( &dsfont_body, DS_W - 32, t ) : 0;
    int lines = ( DS_TOP_H - 48 ) / ( dsfont_body.height + 1 );
    if ( brief_skip > total - 1 ) brief_skip = total > 0 ? total - 1 : 0;
    top_text_panel( "Briefing", t, brief_skip );
    if ( total > lines ) dg_text_r( &dsfont_small, DS_W - 14, DS_TOP_H - 18, DC_DIM, "L / R: scroll" );
}

/* ======================================================================
   Purchase
   ====================================================================== */
static List *pu_nations = 0;
static int pu_nation = 0;
static int pu_classes[64], pu_nclasses = 0, pu_class = 0, pu_class_top = 0;
static List *pu_units = 0;
static List *pu_trsp = 0;
static int pu_trsp_sel = -1;            /* -1 = no transporter */
static int pu_ordered_view = 0;
static Unit *pu_reinf[MAX_ITEMS];
static int pu_affordable_only = 0;
static Unit_Lib_Entry *pu_last = 0;      /* last purchase (repeat) */
#define PU_CLASS_W 92
#define PU_CLASS_ROW 22

static Nation *pu_cur_nation( void ) { return pu_nations ? list_get( pu_nations, pu_nation ) : 0; }

static void pu_free( void )
{
    if ( pu_units ) list_delete( pu_units ); pu_units = 0;
    if ( pu_trsp ) list_delete( pu_trsp ); pu_trsp = 0;
}

static Unit_Lib_Entry *pu_sel_entry( void )
{
    if ( pu_ordered_view || !pu_units || n_items <= 0 || items[sel].id <= 0 ) return 0;
    return list_get( pu_units, items[sel].id - 1 );
}
static Unit_Lib_Entry *pu_sel_trsp( void )
{
    Unit_Lib_Entry *e = pu_sel_entry();
    if ( !e || !( e->flags & GROUND_TRSP_OK ) || pu_trsp_sel < 0 || !pu_trsp ) return 0;
    return list_get( pu_trsp, pu_trsp_sel );
}

static void pu_build_list( void )
{
    Nation *n = pu_cur_nation();
    Unit_Lib_Entry *e;
    int keep = sel, i = 0;
    char cost[16];
    lst_init( "", 2 );   /* taps only select; Buy/Refund buttons act */
    list_x = PU_CLASS_W; list_w = DS_BOT_W - PU_CLASS_W;
    if ( pu_ordered_view ) {
        Unit *u;
        int k = 0;
        snprintf( list_title, sizeof list_title, "Ordered (arrive next turn)" );
        list_reset( reinf );
        while ( ( u = list_next( reinf ) ) )
            if ( u->player == cur_player && k < MAX_ITEMS ) {
                snprintf( cost, sizeof cost, "%d", u->prop.cost + ( u->trsp_prop.id ? u->trsp_prop.cost : 0 ) );
                pu_reinf[k] = u;
                lst_add( u->name, cost, ++k, 1 );
            }
        lst_buttons( "Done", "Shop", "Refund" );
        sel = keep; list_clamp();
        return;
    }
    snprintf( list_title, sizeof list_title, "%s", pu_nclasses ? unit_classes[pu_classes[pu_class]].name : "" );
    pu_free();
    if ( n && pu_nclasses )
        pu_units = get_purchasable_unit_lib_entries( n->id, unit_classes[pu_classes[pu_class]].id,
                                                     (const Date*)&scen_info->start_date );
    if ( pu_units ) {
        list_reset( pu_units );
        while ( ( e = list_next( pu_units ) ) ) {
            int ok = player_can_purchase_unit( cur_player, e, 0 );
            i++;
            if ( pu_affordable_only && !ok ) continue;
            snprintf( cost, sizeof cost, "%d", e->cost );
            lst_add( e->name, cost, i, ok );
            if ( e == pu_last ) keep = n_items - 1;
        }
    }
    /* transporters for this nation */
    {
        int k;
        for ( k = 0; k < unit_class_count; k++ )
            if ( unit_classes[k].purchase == UC_PT_TRSP && n ) {
                pu_trsp = get_purchasable_unit_lib_entries( n->id, unit_classes[k].id, (const Date*)&scen_info->start_date );
                break;
            }
    }
    if ( pu_trsp_sel >= ( pu_trsp ? pu_trsp->count : 0 ) ) pu_trsp_sel = -1;
    lst_buttons( "Done", "Transport", "Buy" );
    sel = keep; list_clamp();
}

void scr_open_purchase( void )
{
    int k;
    if ( !dsb_purchase_allowed() ) return;
    if ( pu_nations ) list_delete( pu_nations );
    pu_nations = get_purchase_nations();
    pu_nation = 0;
    pu_nclasses = 0;
    for ( k = 0; k < unit_class_count && pu_nclasses < 64; k++ )
        if ( unit_classes[k].purchase == UC_PT_NORMAL ) pu_classes[pu_nclasses++] = k;
    if ( pu_class >= pu_nclasses ) pu_class = 0;
    pu_ordered_view = 0;
    pu_trsp_sel = -1;
    sel = 0;
    pu_build_list();
    ui_mode = UM_PURCHASE;
    ds_mark_all();
}

static void pu_refund( Unit *u )
{
    Unit *e;
    list_reset( reinf );
    while ( ( e = list_next( reinf ) ) )
        if ( e == u ) {
            cur_player->cur_prestige += u->prop.cost;
            if ( u->trsp_prop.id ) cur_player->cur_prestige += u->trsp_prop.cost;
            list_delete_current( reinf );
            return;
        }
}

static void purchase_input( void )
{
    int r, i;
    /* class column */
    if ( !pu_ordered_view ) {
        int rows = ( DS_BOT_H - HEAD_H - FOOT_H ) / PU_CLASS_ROW;
        if ( in.pressed & ( K_L | K_R ) ) {
            pu_class = ( pu_class + ( ( in.pressed & K_R ) ? 1 : pu_nclasses - 1 ) ) % ( pu_nclasses ? pu_nclasses : 1 );
            sel = 0; pu_build_list(); return;
        }
        if ( in.tap && in.tap_x < PU_CLASS_W ) {
            if ( in.tap_y < HEAD_H ) {
                if ( pu_nations && pu_nations->count > 1 ) {
                    pu_nation = ( pu_nation + 1 ) % pu_nations->count;
                    sel = 0; pu_build_list();
                }
                return;
            }
            if ( in.tap_y >= DS_BOT_H - FOOT_H ) { pu_affordable_only = !pu_affordable_only; sel = 0; pu_build_list(); return; }
            i = pu_class_top + ( in.tap_y - HEAD_H ) / PU_CLASS_ROW;
            if ( ( in.tap_y - HEAD_H ) / PU_CLASS_ROW < rows && i >= 0 && i < pu_nclasses ) { pu_class = i; sel = 0; pu_build_list(); }
            return;
        }
        if ( in.drag && in.down_x < PU_CLASS_W ) {
            static int acc = 0;
            acc += in.drag_dy;
            while ( acc <= -PU_CLASS_ROW ) { acc += PU_CLASS_ROW; if ( pu_class_top + rows < pu_nclasses ) pu_class_top++; }
            while ( acc >= PU_CLASS_ROW ) { acc -= PU_CLASS_ROW; if ( pu_class_top > 0 ) pu_class_top--; }
            return;
        }
        if ( pu_class < pu_class_top ) pu_class_top = pu_class;
        if ( pu_class >= pu_class_top + rows ) pu_class_top = pu_class - rows + 1;
    }
    r = list_input();
    if ( r == L_BACK || r == L_BTN0 ) {
        pu_free();
        if ( pu_nations ) { list_delete( pu_nations ); pu_nations = 0; }
        scr_close();
        return;
    }
    if ( pu_ordered_view ) {
        if ( r == L_BTN0 - 1 ) { pu_ordered_view = 0; sel = 0; pu_build_list(); }
        else if ( r == L_BTN0 - 2 || r > 0 ) {
            int idx = ( r > 0 ) ? r - 1 : ( n_items > 0 ? items[sel].id - 1 : -1 );
            if ( r > 0 && !( in.pressed & K_A ) ) return;   /* tap selects; Refund button refunds */
            if ( idx >= 0 ) { pu_refund( pu_reinf[idx] ); pu_build_list(); }
        }
        return;
    }
    if ( in.pressed & K_X ) { pu_ordered_view = 1; sel = 0; pu_build_list(); return; }
    if ( r == L_BTN0 - 1 ) {
        Unit_Lib_Entry *e = pu_sel_entry();
        if ( e && ( e->flags & GROUND_TRSP_OK ) && pu_trsp && pu_trsp->count > 0 ) {
            pu_trsp_sel++;
            if ( pu_trsp_sel >= pu_trsp->count ) pu_trsp_sel = -1;
        }
        return;
    }
    if ( r == L_BTN0 - 2 || r > 0 ) {
        Unit_Lib_Entry *e = pu_sel_entry(), *t = pu_sel_trsp();
        int core_limit = 0;
        if ( !e || !player_can_purchase_unit( cur_player, e, t ) ) return;
        player_get_purchase_unit_limit( cur_player, &core_limit, 0 );
        player_purchase_unit( cur_player, pu_cur_nation(), core_limit > 0 ? 1 : 0, e, t );
        pu_last = e;
        pu_build_list();
    }
}

static void purchase_draw_bottom( void )
{
    int i, rows = ( DS_BOT_H - HEAD_H - FOOT_H ) / PU_CLASS_ROW;
    int x = DS_BOT_X, y = DS_BOT_Y;
    char t[48];
    Nation *n = pu_cur_nation();
    /* adjust Buy button label with cost */
    static char buy[24], trsp[32];
    if ( !pu_ordered_view ) {
        Unit_Lib_Entry *e = pu_sel_entry(), *tr = pu_sel_trsp();
        snprintf( buy, sizeof buy, "Buy %d", e ? e->cost + ( tr ? tr->cost : 0 ) : 0 );
        snprintf( trsp, sizeof trsp, "%s", ( e && ( e->flags & GROUND_TRSP_OK ) ) ? ( tr ? "Trsp: yes" : "Trsp: no" ) : "No trsp" );
        btns[1] = trsp; btns[2] = buy;
        btn_on[1] = e && ( e->flags & GROUND_TRSP_OK );
        btn_on[2] = e && cur_player && player_can_purchase_unit( cur_player, e, tr );
    }
    else btn_on[1] = btn_on[2] = 1;
    draw_list();
    /* class column */
    dg_fill( x, y, PU_CLASS_W, DS_BOT_H, DC_PANEL );
    dg_fill( x, y, PU_CLASS_W - 2, HEAD_H - 2, DC_PANEL2 );
    snprintf( t, sizeof t, "%s%s", n ? n->name : "", pu_nations && pu_nations->count > 1 ? " >" : "" );
    dg_text_fit( &dsfont_small, x + 4, y + 6, PU_CLASS_W - 8, DC_ACCENT, t );
    if ( !pu_ordered_view ) {
        for ( i = 0; i < rows && pu_class_top + i < pu_nclasses; i++ ) {
            int c = pu_class_top + i, ry = y + HEAD_H + i * PU_CLASS_ROW;
            if ( c == pu_class ) dg_fill( x + 2, ry, PU_CLASS_W - 4, PU_CLASS_ROW - 2, 0x34507a );
            dg_text_fit( &dsfont_small, x + 6, ry + 5, PU_CLASS_W - 10, c == pu_class ? DC_TEXT : DC_DIM,
                         unit_classes[pu_classes[c]].name );
        }
        ds_button( x + 2, y + DS_BOT_H - FOOT_H + 4, PU_CLASS_W - 6, FOOT_H - 8,
                   pu_affordable_only ? "Affordable" : "All units", 1, pu_affordable_only );
    }
    else dg_text_wrap( &dsfont_small, x + 6, y + HEAD_H + 6, PU_CLASS_W - 12, 120, DC_DIM,
                       "Tap a unit, then Refund to cancel the order.", 0 );
}

static void purchase_draw_top( void )
{
    char t[160];
    int core = 0, aux = 0, lim;
    dg_fill( 0, 0, DS_W, DS_TOP_H, DC_BG );
    ds_status_bar();
    lim = player_get_purchase_unit_limit( cur_player, &core, &aux );
    if ( lim < 0 ) snprintf( t, sizeof t, "Unit slots: unlimited" );
    else if ( camp_loaded ) snprintf( t, sizeof t, "Unit slots: %d core / %d auxiliary", core, aux );
    else snprintf( t, sizeof t, "Unit slots left: %d", aux );
    dg_text( &dsfont_body, 8, 24, lim == 0 ? DC_ENEMY : DC_DIM, t );
    if ( pu_ordered_view ) {
        if ( n_items > 0 ) {
            Unit *u = pu_reinf[items[sel].id - 1];
            ds_entry_card( 4, 44, DS_W - 8, 150, &u->prop, u->prop.cost + ( u->trsp_prop.id ? u->trsp_prop.cost : 0 ),
                           u->trsp_prop.id ? u->trsp_prop.name : 0 );
        }
        else top_text_panel( 0, "Nothing ordered this turn.", 0 );
    }
    else {
        Unit_Lib_Entry *e = pu_sel_entry(), *tr = pu_sel_trsp();
        if ( e ) {
            char note[80];
            snprintf( note, sizeof note, "%s", tr ? tr->name : ( e->flags & GROUND_TRSP_OK ) ? "no transporter" : "" );
            ds_entry_card( 4, 44, DS_W - 8, 150, e, e->cost + ( tr ? tr->cost : 0 ), note );
        }
        else dg_text( &dsfont_body, 12, 60, DC_DIM, "No units of this class available." );
    }
    dg_text_wrap( &dsfont_small, 8, 200, DS_W - 16, 36, DC_DIM,
                  "L/R: unit class.  X: ordered units.  Ordered units arrive next turn - deploy them from Start > Deploy units.", 0 );
}

/* ======================================================================
   Dispatch
   ====================================================================== */
void scr_input( void )
{
    switch ( ui_mode ) {
        case UM_TITLE: title_input(); break;
        case UM_MENU: menu_input(); break;
        case UM_CONFIRM: confirm_input(); break;
        case UM_PURCHASE: purchase_input(); break;
        case UM_SPLIT: split_input(); break;
        case UM_PAGES: pages_input(); break;
        case UM_SCENLIST: case UM_CAMPLIST: scenlist_input(); break;
        case UM_SCENSETUP: scensetup_input(); break;
        case UM_SLOTS: slots_input(); break;
        case UM_OPTIONS: options_input(); break;
        case UM_UNITLIST: unitlist_input(); break;
        case UM_OVERVIEW: overview_input(); break;
        case UM_CAMPBRIEF: campbrief_input(); break;
        default: break;
    }
}

void scr_draw_bottom( void )
{
    switch ( ui_mode ) {
        case UM_STATUS: ds_status_draw_bottom(); break;
        case UM_CONFIRM: confirm_draw_bottom(); break;
        case UM_PAGES: pages_draw_bottom(); break;
        case UM_PURCHASE: purchase_draw_bottom(); break;
        case UM_OVERVIEW:
            dg_fill( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, DC_BG );
            if ( ds_engine_ready() ) ds_minimap( DS_BOT_X, DS_BOT_Y, DS_BOT_W, DS_BOT_H, 1 );
            break;
        default: draw_list(); break;
    }
}

/* scenario summary for the top screen while a menu is open */
static void menu_summary_top( int y0, int h )
{
    static char *conds = 0;
    static void *conds_for = 0;
    static int conds_turn = -1;
    char date[64], t[96];
    int mine = 0, foes = 0, y = y0 + 4;
    Unit *u;
    if ( conds_for != (void*)scen_info || conds_turn != turn ) {
        free( conds ); conds = gui_conds_text();
        conds_for = scen_info; conds_turn = turn;
    }
    if ( units ) {
        list_reset( units );
        while ( ( u = list_next( units ) ) ) {
            if ( u->killed ) continue;
            if ( u->player == cur_player ) mine++;
            else if ( cur_player && !player_is_ally( cur_player, u->player ) && mask[u->x][u->y].spot ) foes++;
        }
    }
    if ( avail_units ) {
        list_reset( avail_units );
        while ( ( u = list_next( avail_units ) ) ) if ( u->player == cur_player ) mine++;
    }
    dg_panel( 4, y0, 262, h );
    dg_text_fit( &dsfont_bold, 10, y, 250, DC_ACCENT, scen_info->name ); y += 16;
    scen_get_date( date );
    if ( turn + 1 < scen_info->turn_limit )
        snprintf( t, sizeof t, "%s  -  %d turns left", date, scen_info->turn_limit - turn );
    else snprintf( t, sizeof t, "%s  -  last turn", date );
    dg_text_fit( &dsfont_small, 10, y, 250, DC_DIM, t ); y += 14;
    snprintf( t, sizeof t, "Your units: %d    Enemies in sight: %d", mine, foes );
    dg_text_fit( &dsfont_body, 10, y, 250, DC_TEXT, t ); y += 18;
    if ( conds && y0 + h - 4 - y > 12 ) dg_text_wrap( &dsfont_small, 10, y, 250, y0 + h - 4 - y, DC_TEXT, conds, 0 );
}

void scr_draw_top( void )
{
    switch ( ui_mode ) {
        case UM_STATUS: ds_status_draw_top(); break;
        case UM_TITLE: title_draw_top(); break;
        case UM_PAGES: pages_draw_top(); break;
        case UM_SCENLIST: case UM_CAMPLIST: scenlist_draw_top(); break;
        case UM_SCENSETUP: top_text_panel( 0, sel_info, 0 ); break;
        case UM_CAMPBRIEF: campbrief_draw_top(); break;
        case UM_PURCHASE: purchase_draw_top(); break;
        case UM_CONFIRM:
            top_text_panel( "Please confirm", cf_text, 0 );
            break;
        case UM_UNITLIST:
            ds_status_bar();
            if ( n_items > 0 ) ds_unit_card( 4, 24, DS_W - 8, 212, ulist[items[sel].id - 1], "UNIT", 1 );
            break;
        case UM_OVERVIEW:
            ds_status_bar();
            top_text_panel( "Overview", "Tap the map below to jump there.  B to return.##"
                            "Blue: your units.  Red: spotted enemies.  Boxes: objectives (blue = held).", 0 );
            break;
        case UM_SPLIT:
            ds_status_bar();
            if ( cur_unit ) ds_unit_card( 4, 24, DS_W - 8, 212, cur_unit, "SPLIT", 1 );
            break;
        case UM_OPTIONS:
            top_text_panel( "Options", "Forecast tap: when on, the first tap on an enemy shows the battle forecast "
                            "and the second tap attacks. Off attacks on the first tap.#"
                            "Auto-select: after a unit has moved and attacked, the next unit with orders is selected.#"
                            "Strength numbers: show unit strength on the map.#"
                            "Hex grid: outline every hex.#"
                            "Watch the enemy turn: follow CPU moves on the map (off is faster).#"
                            "Animation speed: how fast moves and battles play.", 0 );
            break;
        default:
            ds_status_bar();
            if ( scen_info && ds_engine_ready() ) {
                ds_minimap( 270, 24, 126, 150, 0 );
                if ( cur_unit ) { ds_unit_card( 4, 24, 262, 104, cur_unit, "SELECTED", 0 ); menu_summary_top( 132, 104 ); }
                else menu_summary_top( 24, 212 );
            }
            else title_draw_top();
            break;
    }
}

#endif /* DSUI */
