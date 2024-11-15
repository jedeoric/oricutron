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
#ifndef NORDYNAMIC
void (*my_tzprintfpos)( struct textzone *ptz, int x, int y, char *fmt, ... );
void (*my_tzputc)( struct textzone *ptz, char c );
void (*mon_periphmod)( int x, int y, int w, struct textzone *vtz );
#endif

// *****************************************************************************
//                    Extension Registre avec auto-incrément
// *****************************************************************************
// Registre 16 bits avec post incrément
// 0000-0001: registre
// 0003     : incrément (signé)
// -----------------------------------------------------------------------------
#define INSTANCE_MAX 2

struct REG {
  Uint16 data;
  char incr;
  Uint16 old_data;
  char old_incr;
};

struct REG register_data[INSTANCE_MAX];
int register_instances = 0;

static char *description = "Hardware register";

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---plugin init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
unsigned int register_create(struct machine *oric)
{
    if (register_instances >= INSTANCE_MAX)
        return 0;

    return ++register_instances;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
SDL_bool register_shutdown(struct machine *oric, unsigned int instance)
{

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool register_reset(struct machine *oric, unsigned int instance )
{
    if ( (!instance) || (instance > register_instances) )
        return SDL_FALSE;

    instance--;

    register_data[instance].incr = 0;
    register_data[instance].data = 0;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture du registre
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
Uint8 register_read(struct machine *oric, unsigned int instance, Uint16 addr, SDL_bool run)
{
    // On inccrémente après la lecture du MSB
    //
    // ATTENTION:
    //     - DEEK lit d'abord le MSB puis le LSB
    //     - Oricutron lit d'abord le MSB puis le LSB pour un adressage indirect
    //       contrairement à ce que fait le 6502

    if ( (!instance) || (instance > register_instances) )
        return (Uint8) 0;

    instance--;

    switch (addr & 0x0003)
    {
        case 0:
            return register_data[instance].data & 0x00ff;

        case 1:
        {
            Uint8 data = register_data[instance].data >> 8;
            if (run)
                register_data[instance].data += register_data[instance].incr;
            return data;
        }
        case 2:
            return register_data[instance].incr;

        default:
            dbg_printf("register_READ: bad address $%04x\n", addr);
            return (Uint8) 0;
    }
}

    // -------------------------------------------------------------------------
    //                      Ecriture dans le registre
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
SDL_bool register_write(struct machine *oric, unsigned int instance, Uint16 addr, Uint8 data)
{
    // ATTENTION:
    //     - DOKE écrit d'abord le MSB puis le LSB
    //     - Oricutron lit d'abord le MSB puis le LSB pour un adressage indirect
    //       contrairement à ce que fait le 6502

     if ( (!instance) || (instance > register_instances) )
        return SDL_FALSE;

    instance--;

   switch (addr & 0x0003)
    {
        case 0:
            register_data[instance].data = (register_data[instance].data & 0xff00) | data;
            return SDL_TRUE;

        case 1:
            register_data[instance].data = (register_data[instance].data & 0x00ff) | (data << 8);
            return SDL_TRUE;

        case 2:
            register_data[instance].incr = data;
            return SDL_TRUE;

        default:
            dbg_printf("register_READ: address address $%04x\n", addr);
            return SDL_FALSE;
    }
}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
void mon_register_update(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
     if ( (!instance) || (instance > register_instances) )
        return;

    instance--;

    dbg_printf("REG: mon update\n");

    my_tzprintfpos( tz, 2, 2,  "Base address  : %04X", base_addr);
    my_tzprintfpos( tz, 2, 3,  "Register value: %04X", register_data[instance].data);
    my_tzprintfpos( tz, 2, 4,  "Register incr.:   %02X", (Uint8) register_data[instance].incr);


    if (oldvalid)
    {
        if (register_data[instance].data != register_data[instance].old_data)
            mon_periphmod( 18, 3, 4, tz );

        if (register_data[instance].incr != register_data[instance].old_incr)
            mon_periphmod( 20, 4, 2, tz );
    }
}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
void mon_register_store(struct machine *oric, unsigned int instance)
{
     if ( (!instance) || (instance > register_instances) )
        return;

    instance--;

    register_data[instance].old_data = register_data[instance].data;
    register_data[instance].old_incr = register_data[instance].incr;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "REGISTER",
                0x0362, 3,
                register_create,
                register_shutdown,
                register_reset,
                register_read,
                register_write,
		NULL,
                mon_register_update,
                mon_register_store,
    };

