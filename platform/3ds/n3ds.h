/*
 * n3ds.h - Nintendo 3DS platform layer for LGeneral
 *
 * Everything 3DS-specific lives behind these calls so the engine
 * sources only gain small #ifdef __3DS__ hooks.
 */
#ifndef LG_N3DS_H
#define LG_N3DS_H

#ifdef __3DS__

/* game data, config and logs all live here on the SD card */
#define LG_3DS_ROOT "sdmc:/3ds/lgeneral"

/* first-boot video mode: engine renders 640x480, SDL scales it to the top screen.
 * Phase 3 replaces this with the dual-screen 400x480 layout. */
#define LG_3DS_WIDTH  640
#define LG_3DS_HEIGHT 480

/* call first thing in main(): CPU speedup, log file, crash handler, folders */
void n3ds_init( void );

/* call right after SDL_Init(): opens the 3DS buttons as an SDL joystick */
void n3ds_post_sdl_init( void );

/* log free app memory with a tag (memory headroom shows up in lgeneral.log) */
int n3ds_debug_log( void );   /* sdmc:/3ds/lgeneral/debug.txt existed at start-up */
void n3ds_log_tick( void );   /* main loop: write the buffered log out, cap it */
void n3ds_log_mem( const char *tag );

/* 1 once after the game comes back from the HOME Menu or sleep (then 0 until the next time) */
int n3ds_take_resumed( void );

/* call before exit */
void n3ds_exit( void );

#endif /* __3DS__ */
#endif
