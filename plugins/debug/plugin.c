// vim: tabstop=4 expandtab

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(__amigaos4__) || defined(__MORPHOS__)
#include <proto/dos.h>
#include <dos/dostags.h>
#include <proto/amigaguide.h>
#endif


#include "../../system.h"
#include "../../6502.h"
#include "../../via.h"
#include "../../8912.h"
#include "../../gui.h"
#include "../../disk.h"
#include "../../monitor.h"
#include "../../6551.h"


#include "../../machine.h"

#include "plugin.h"

#ifdef DEBUG_PLUGIN
    // dbg_printf est une fonction déclarée dans monitor.h mais est spécifique au moniteur
    // #define dbg_printf(x...) { printf(x); }
    #define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
    #define dbg_printf(...)
#endif

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
// extern struct textzone *tz[];
// extern struct osdmenu menus[];
// Si Oricutron est compilé sans l'option -rdynamic alors il faut passer la
// référence des fonctions tzprintfpos et tzpoutc au plugin.
#ifndef RDYNAMIC
void (*my_tzprintfpos)( struct textzone *ptz, int x, int y, char *fmt, ... );
void (*my_tzputc)( struct textzone *ptz, char c );
void (*mon_periphmod)( int x, int y, int w, struct textzone *vtz );
#endif

// *****************************************************************************
//                                  Datas
// *****************************************************************************
//
// -----------------------------------------------------------------------------
#define BASE_ADDR 0x360
#define END_ADDR 0x361

#define DATA_SIZE 16
#define INSTANCE_MAX 1

struct DATA {
  unsigned char data[DATA_SIZE];
  unsigned char ptr;
};

struct DATA userdata[INSTANCE_MAX];
struct DATA userdata_old[INSTANCE_MAX];

int plugin_instances = 0;
SDL_bool active = SDL_FALSE;

static char *description = "Debug";

// -----------------------------------------------------------------------------
//                              plugin_init
// -----------------------------------------------------------------------------
// Run once right after thz library load
//
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---plugin init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

    // -----------------------------------------------------------------------------
    //                              plugin_create
    // -----------------------------------------------------------------------------
    // Called to create a new instance of the extension
unsigned int plugin_create(struct machine *oric)
{
    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    return ++plugin_instances;
}

    // -----------------------------------------------------------------------------
    //                              plugin_shutdown
    // -----------------------------------------------------------------------------
    // Called on exit
SDL_bool plugin_shutdown(struct machine *oric, unsigned int instance)
{

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                                  plugin_reset
// -----------------------------------------------------------------------------
// Called by init_machine and [F4]
SDL_bool plugin_reset(struct machine *oric, unsigned int instance)
{
    dbg_printf("stack_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance].ptr = 0;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture de la pile (POP)
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    // Read access
unsigned char plugin_read(struct machine *oric, SDL_bool fBank, unsigned int instance, unsigned short addr, SDL_bool run)
{
    if ( (!instance) || (instance > plugin_instances) )
        return (unsigned char) 0;

    instance--;

    return (unsigned char) 0;
}

    // -------------------------------------------------------------------------
    //                      Ecriture dasn la pile (PUSH)
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    // Write access
SDL_bool plugin_write(struct machine *oric, SDL_bool fBank, unsigned int instance, unsigned short addr, unsigned char data)
{
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    active = data;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                              Horloge
    // -------------------------------------------------------------------------
    // Called
void plugin_ticktock(struct machine *oric, unsigned int instance, int cycles)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    if ( !cycles )
        return;

    if (active)
        dbg_printf("PC=%04X, A=%02X, X=%02X, Y=%02X, SP=1%02X, CALCOP=%02X LPC=%04X, CALCPC=%04X, BADDR=%04X\n", oric->cpu.pc, oric->cpu.a, oric->cpu.x, oric->cpu.y, oric->cpu.sp, oric->cpu.calcop, oric->cpu.lastpc, oric->cpu.calcpc, oric->cpu.baddr);

}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
    // Monitor page
    // Rows: 19 (1-19)
    // Columns: 28 (1-28)
void mon_plugin_update(struct textzone *tz, unsigned int instance, unsigned short base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
    // Called by monitor
void mon_plugin_store(struct machine *oric, unsigned int instance)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "DEBUG",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                plugin_create,
                plugin_shutdown,
                plugin_reset,
                plugin_read,
                plugin_write,
                plugin_ticktock,
                NULL,
                NULL,
    };

