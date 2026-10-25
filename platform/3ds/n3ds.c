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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>

extern u32 __ctru_heap_size; /* libctru: total newlib heap reserved at startup */

#include "n3ds.h"

/* main thread stack: the libctru default (32 KB) is too thin for the engine + AI call chains */
u32 __stacksize__ = 256 * 1024;

static SDL_Joystick *pad = 0;
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
    svcExitProcess();
}

void n3ds_init( void )
{
    bool is_new = false;

    osSetSpeedupEnable( true );

    mkdir( "sdmc:/3ds", 0777 );
    mkdir( LG_3DS_ROOT, 0777 );
    mkdir( LG_3DS_ROOT "/config", 0777 );
    chdir( LG_3DS_ROOT );

    if ( freopen( LG_3DS_ROOT "/lgeneral.log", "w", stdout ) )
        setvbuf( stdout, NULL, _IONBF, 0 );
    if ( freopen( LG_3DS_ROOT "/lgeneral.log", "a", stderr ) )
        setvbuf( stderr, NULL, _IONBF, 0 );

    threadOnException( crash_handler, exc_stack + sizeof( exc_stack ), WRITE_DATA_TO_HANDLER_STACK );

    APT_CheckNew3DS( &is_new );
    printf( "LGeneral 3DS build %s %s\n", __DATE__, __TIME__ );
    printf( "console: %s 3DS, heap %lu KB, linear free %lu KB\n", is_new ? "New" : "Old",
            (unsigned long)( __ctru_heap_size / 1024 ), (unsigned long)( linearSpaceFree() / 1024 ) );
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
    n3ds_log_mem( "exit" );
    if ( pad ) SDL_JoystickClose( pad );
    pad = 0;
}

#endif /* __3DS__ */
