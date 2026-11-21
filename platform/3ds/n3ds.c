/*
 * n3ds.c - Nintendo 3DS platform layer for LGeneral
 *
 * - New 3DS 804 MHz + L2 cache
 * - stdout/stderr go unbuffered to sdmc:/3ds/lgeneral/lgeneral.log so the
 *   last line before any crash is on the SD card (OpenXcom lesson)
 * - CPU exception handler appends PC/LR/fault address to crash.txt;
 *   resolve with tools/addr2line.sh against build/lgeneral.elf
 * - 3DS buttons opened as an SDL joystick (mapped in event.c)
 */
#ifdef __3DS__

#include <3ds.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>

extern u32 __ctru_heap_size; /* libctru: total newlib heap reserved at startup */

#include "n3ds.h"
#ifndef PORT_VERSION
#define PORT_VERSION "unknown"
#endif

/* main thread stack: the libctru default (32 KB) is too thin for the engine + AI call chains */
u32 __stacksize__ = 256 * 1024;

static SDL_Joystick *pad = 0;

/* LG-03: HOME / lid. The hook runs inside aptMainLoop() (called from SDL_PumpEvents on the main
   thread), which blocks while the game is suspended, so the game is paused by itself. The hook only
   sets a flag; the main loop clears stale input and redraws when it sees it. Nothing is saved here. */
static aptHookCookie apt_cookie;
static volatile int apt_resumed = 0;
static void apt_hook( APT_HookType type, void *param )
{
    (void)param;
    if ( type == APTHOOK_ONRESTORE || type == APTHOOK_ONWAKEUP ) apt_resumed = 1;
}
int n3ds_take_resumed( void )
{
    if ( !apt_resumed ) return 0;
    apt_resumed = 0;
    return 1;
}
static u8 exc_stack[0x4000] __attribute__((aligned(8)));

static const char *exc_name( ERRF_ExceptionType t )
{
    switch ( t ) {
        case ERRF_EXCEPTION_PREFETCH_ABORT: return "prefetch abort";
        case ERRF_EXCEPTION_DATA_ABORT: return "data abort";
        case ERRF_EXCEPTION_UNDEFINED: return "undefined instruction";
        case ERRF_EXCEPTION_VFP: return "VFP (floating point)";
        default: return "unknown";
    }
}

static void crash_handler( ERRF_ExceptionInfo *excep, CpuRegisters *regs )
{
    FILE *f = fopen( LG_3DS_ROOT "/crash.txt", "a" );
    int i;
    if ( f ) {
        fprintf( f, "=== LGeneral %s crash ===\n", "1.4.4-3ds" );
        fprintf( f, "type: %s  fsr=%08lx  far=%08lx\n", exc_name( excep->type ),
                 (unsigned long)excep->fsr, (unsigned long)excep->far );
        fprintf( f, "pc=%08lx lr=%08lx sp=%08lx cpsr=%08lx\n",
                 (unsigned long)regs->pc, (unsigned long)regs->lr,
                 (unsigned long)regs->sp, (unsigned long)regs->cpsr );
        for ( i = 0; i < 13; i++ )
            fprintf( f, "r%-2d=%08lx%s", i, (unsigned long)regs->r[i], ( i % 4 == 3 ) ? "\n" : "  " );
        fprintf( f, "\nresolve: tools/addr2line.sh %08lx %08lx\n\n",
                 (unsigned long)regs->pc, (unsigned long)regs->lr );
        fclose( f );
    }
    printf( "CRASH: %s at pc=%08lx lr=%08lx (see crash.txt)\n", exc_name( excep->type ),
            (unsigned long)regs->pc, (unsigned long)regs->lr );
    // Restart the console instead of leaving a stuck process (and maybe a hung GPU) behind:
    // cold restart through the MCU (Luma treats it as a power-on), APT restart as the fallback.
    // Same as the other ports and the selftest watchdog (Nov 16 2026).
    if ( R_SUCCEEDED( mcuHwcInit() ) ) {
        u8 v = 1 << 2;
        MCUHWC_WriteRegister( 0x20, &v, 1 );
        mcuHwcExit();
        svcSleepThread( 3000000000LL );
    }
    APT_HardwareResetAsync();
    svcSleepThread( 2000000000LL );
    svcExitProcess();
}

static int dbg_log = 0;
int n3ds_debug_log( void ) { return dbg_log; }

/* every main-loop tick: write the buffered log out every ~10 s; past 1 MB start a new one */
void n3ds_log_tick( void )
{
    static u64 last = 0;
    u64 now = osGetTime();
    if ( now - last < 10000 ) return;
    last = now;
    fflush( stdout );
    if ( ftell( stdout ) > 1024 * 1024 ) {
        remove( LG_3DS_ROOT "/lgeneral.old.log" );
        rename( LG_3DS_ROOT "/lgeneral.log", LG_3DS_ROOT "/lgeneral.old.log" );
        if ( freopen( LG_3DS_ROOT "/lgeneral.log", "w", stdout ) && freopen( LG_3DS_ROOT "/lgeneral.log", "a", stdout ) )
            setvbuf( stdout, NULL, _IOFBF, 16384 );
        freopen( LG_3DS_ROOT "/lgeneral.log", "a", stderr );
        setvbuf( stderr, NULL, _IONBF, 0 );
        printf( "(log rolled over at 1 MB: earlier lines in lgeneral.old.log)\n" );
    }
}

void n3ds_init( void )
{
    bool is_new = false;

    osSetSpeedupEnable( true );

    mkdir( "sdmc:/3ds", 0777 );
    mkdir( LG_3DS_ROOT, 0777 );
    mkdir( LG_3DS_ROOT "/config", 0777 );
    chdir( LG_3DS_ROOT );

    /* LG-07: lgeneral.log was written unbuffered, one SD write per line: the AI's turn
       trace, a status line per UI change, every refused move. Now stdout is buffered
       (16 KB, written out every ~10 s by n3ds_log_tick, rolled over at 1 MB); without
       sdmc:/3ds/lgeneral/debug.txt (checked once, here) the AI trace and the per-tick
       status lines are off. Start, exit, HOME-close and save lines stay. Both streams
       append, so neither overwrites the other's lines. */
    {
        FILE *d = fopen( LG_3DS_ROOT "/debug.txt", "r" );
        dbg_log = d != NULL;
        if ( d ) fclose( d );
    }
    if ( freopen( LG_3DS_ROOT "/lgeneral.log", "w", stdout ) && freopen( LG_3DS_ROOT "/lgeneral.log", "a", stdout ) )
        setvbuf( stdout, NULL, _IOFBF, 16384 );
    if ( freopen( LG_3DS_ROOT "/lgeneral.log", "a", stderr ) )
        setvbuf( stderr, NULL, _IONBF, 0 );

    threadOnException( crash_handler, exc_stack + sizeof( exc_stack ), WRITE_DATA_TO_HANDLER_STACK );
    aptHook( &apt_cookie, apt_hook, 0 );

    APT_CheckNew3DS( &is_new );
    printf( "LGeneral 3DS build %s (%s %s)%s\n", PORT_VERSION, __DATE__, __TIME__,
            dbg_log ? ", debug log on (debug.txt)" : "" );
    printf( "console: %s 3DS, heap %lu KB, linear free %lu KB\n", is_new ? "New" : "Old",
            (unsigned long)( __ctru_heap_size / 1024 ), (unsigned long)( linearSpaceFree() / 1024 ) );
    // Seen Nov 18 2026: started right after another big game, the 3DS gave LGeneral a 992 KB heap;
    // the video set-up then failed and the game crashed. Say so and go back instead of crashing.
    if ( __ctru_heap_size < 16u * 1024 * 1024 ) {
        // No error applet here: with ~1 MB it crashed too (Nov 18). Log and leave quietly.
        printf( "not enough memory to start (heap %lu KB): exiting\n",
                (unsigned long)( __ctru_heap_size / 1024 ) );
        fflush( stdout );
        // Back to the Homebrew Launcher (as the selftest does), not the HOME Menu: a bare
        // svcExitProcess left the 3DS on the HOME Menu with the test agent stopped (Nov 18).
        aptSetChainloaderToSelf();
        exit( 0 );
    }
}

void n3ds_post_sdl_init( void )
{
    if ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) < 0 ) {
        printf( "joystick init failed: %s\n", SDL_GetError() );
        return;
    }
    if ( SDL_NumJoysticks() > 0 )
        pad = SDL_JoystickOpen( 0 );
    SDL_JoystickEventState( SDL_ENABLE );
    printf( "buttons: %s\n", pad ? "ok" : "not found" );
}

void n3ds_log_mem( const char *tag )
{
    struct mallinfo mi = mallinfo();
    printf( "mem [%s]: heap used %lu KB of %lu KB (%lu KB left), linear free %lu KB\n", tag,
            (unsigned long)( mi.uordblks / 1024 ),
            (unsigned long)( __ctru_heap_size / 1024 ),
            (unsigned long)( ( __ctru_heap_size - mi.uordblks ) / 1024 ),
            (unsigned long)( linearSpaceFree() / 1024 ) );
}

void n3ds_exit( void )
{
    aptUnhook( &apt_cookie );
    n3ds_log_mem( "exit" );
    fflush( stdout );
    if ( pad ) SDL_JoystickClose( pad );
    pad = 0;
}

#endif /* __3DS__ */
