/*
 * dsui_bridge.h - access to engine internals for the native front end.
 * The functions are implemented at the end of lgeneral/src/engine.c (#ifdef DSUI).
 */
#ifndef DSUI_BRIDGE_H
#define DSUI_BRIDGE_H

#include "lgeneral.h"
#include "date.h"
#include "event.h"
#include "windows.h"
#include "nation.h"
#include "unit.h"
#include "purchase_dlg.h"
#include "gui.h"
#include "map.h"
#include "scenario.h"
#include "slot.h"
#include "action.h"
#include "campaign.h"
#include "unit_lib.h"
#include "misc.h"

/* engine status values (mirror of the enum in engine.c) */
enum {
    DSB_STATUS_NONE = 0, DSB_STATUS_MOVE, DSB_STATUS_ATTACK, DSB_STATUS_STRAT_ATTACK,
    DSB_STATUS_MERGE, DSB_STATUS_SPLIT, DSB_STATUS_DEPLOY, DSB_STATUS_UNIT_LIST,
    DSB_STATUS_DROP, DSB_STATUS_INFO, DSB_STATUS_SCEN_INFO, DSB_STATUS_CONF,
    DSB_STATUS_UNIT_MENU, DSB_STATUS_GAME_MENU, DSB_STATUS_DEPLOY_INFO, DSB_STATUS_STRAT_MAP,
    DSB_STATUS_RENAME, DSB_STATUS_SAVE, DSB_STATUS_TITLE, DSB_STATUS_TITLE_MENU,
    DSB_STATUS_RUN_SCEN_DLG, DSB_STATUS_RUN_CAMP_DLG, DSB_STATUS_RUN_SETUP,
    DSB_STATUS_RUN_MODULE_DLG, DSB_STATUS_CAMP_BRIEFING, DSB_STATUS_PURCHASE, DSB_STATUS_VMODE_DLG
};
enum { DSB_REGION_GROUND = 0, DSB_REGION_AIR };

/* engine globals */
extern Unit *cur_unit, *deploy_unit;
extern Player *cur_player;
extern int cur_ctrl, map_x, map_y, map_sw, map_sh, air_mode;
extern int hex_w, hex_h, hex_x_offset, hex_y_offset;
extern int map_w, map_h, turn, cur_weather;
extern Map_Tile **map;
extern Mask_Tile **mask;
extern List *units, *reinf, *avail_units, *players, *left_deploy_units, *unit_lib;
extern Scen_Info *scen_info;
extern Setup setup;
extern Config config;
extern Weather_Type *weather_types;
extern Trgt_Type *trgt_types;
extern int trgt_type_count;
extern Unit_Class *unit_classes;
extern int unit_class_count;
extern Nation *nations;
extern int camp_loaded;
extern Camp_Entry *camp_cur_scen;
extern GUI *gui;
extern int term_game;

int   dsb_status( void );
void  dsb_set_status( int s );
void  dsb_press( int button_id );            /* run an engine button handler (ID_*) */
int   dsb_map_pos( int cx, int cy, int *mx, int *my, int *region );
int   dsb_screen_pos( int mx, int my, int *sx, int *sy );
Unit *dsb_target( int mx, int my, int region );
Unit *dsb_prim_unit( int mx, int my, int region );
Unit *dsb_select_unit_at( int mx, int my, int region );
int   dsb_strat_target( int mx, int my );
void  dsb_select( Unit *u );
void  dsb_focus( int mx, int my );
void  dsb_goto( int x, int y );
void  dsb_redraw( void );
int   dsb_can_undo( void );
int   dsb_merge_count( void );
int   dsb_deploy_turn( void );
void  dsb_deploy_select( Unit *u );
void  dsb_camp_choose( const char *id );      /* campaign briefing: continue with result id (0 = default) */
void *dsb_camp_pane( void );
void  dsb_quit_to_title( void );
int   dsb_purchase_allowed( void );
int   dsb_deploy_allowed( void );
int   dsb_cpu_turn( void );
void  dsb_end_turn_now( void );              /* queue end turn without the engine's confirm */

/* gui.c message pane accessors */
const char *gui_message_pane_text( void *pane );
int   gui_message_pane_option_count( void *pane );
const char *gui_message_pane_option_desc( void *pane, int i );
const char *gui_message_pane_option_id( void *pane, int i );
const char *gui_message_pane_default( void *pane );

/* purchase_dlg.c */
List *get_purchase_nations( void );
List *get_purchasable_unit_lib_entries( const char *nationid, const char *uclassid, const Date *date );
int   player_get_purchase_unit_limit( Player *player, int *core_limit, int *aux_limit );
int   player_can_purchase_unit( Player *p, Unit_Lib_Entry *unit, Unit_Lib_Entry *trsp );
void  player_purchase_unit( Player *player, Nation *nation, int core, Unit_Lib_Entry *unit_prop, Unit_Lib_Entry *trsp_prop );

/* event.c injection */
void event_inject_motion( int x, int y );
void event_inject_button( int button, int down, int x, int y );

#endif
