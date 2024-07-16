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

// #include "../../system_sdl.h"
// #include "../../system.h"
// #include "../../machine.h"


#include "../../system.h"
#include "../../6502.h"
#include "../../via.h"
#include "../../8912.h"
#include "../../gui.h"
#include "../../disk.h"
#include "../../monitor.h"
#include "../../6551.h"


#include "../../machine.h"

/*
#include "avi.h"
#include "filereq.h"
#include "main.h"
#include "ula.h"
#include "joystick.h"
#include "tape.h"
#include "keyboard.h"
*/
#include "periph.h"

#include <dlfcn.h>

// dbg_printf est une fonction déclarée dans monitor.h mais est spécifique au moniteur
#define dbg_printf(x...) { printf(x); }
//#define dbg_printf(x...)

/*
jasmin:
	addr_start: 0x3f4
	add_end: 0x3ff+1
	enable:

microdisc:
	([0x310, 0x314], 0x318)
	addr_start: 0x310
	addr_end: 0x31b+1
	enable:

bd500:
	addr_start: 310
	addr_stop: 0x323+1
	enable:

pravetz:
	addr_start: 0x
	addr_stop:
	enable

ch376:
	addr_start: 0x340
	addr_end: 0x341+1
	enable:

acia:
	addr_start: acia_offset
	addr_end: acia_offset+3+1
	enable:

via:
	addr_start: 0x300
	addr_end: 0x3ff+1
	enable: true

via2:
	addr_start: 0x320
	addr_end: 0x32f+1
	enable:
*/

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
extern struct textzone *tz[];
extern struct osdmenu menus[];

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#define MAX_PERIPH 10

struct PERIPH {
    char osditem;
    char name[PERIPH_NAME_LEN+1];
    unsigned short addr_start;
    unsigned short addr_end;
    unsigned int instance;
    SDL_bool enable;
    struct PLUGIN *periph;
};

struct PERIPH periph_table[MAX_PERIPH];
unsigned int nb_periph = 0;
SDL_bool periph_oldvalid = SDL_FALSE;

void *library[MAX_PERIPH];
unsigned int nb_library = 0;
// void *handle;

/*
struct osdmenuitem periphitems[] = { { " Stack",            NULL,    0,      toggleperiph, 0, 0 },
                                     { " Reg 0",            NULL,    0,      toggleperiph, 1, 0 },
                                     { " Reg 1",            NULL,    0,      toggleperiph, 2, 0 },
                                     { OSDMENUBAR,          NULL,    0,      NULL,         0, 0 },
                                     { "Back",              "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                     { NULL, } };
*/
struct osdmenuitem *periphitems;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool periph_add(struct machine *oric, struct PLUGIN *plugin, char *name, unsigned short addr_start, SDL_bool enable)
{
    if (plugin == NULL)
        return SDL_FALSE;

    if (nb_periph == MAX_PERIPH)
        return SDL_FALSE;

    if (name == NULL)
        name = plugin->name;

    int i = periph_find_by_name(name);
    if (i != nb_periph) return SDL_FALSE;

    if (addr_start == 0)
        addr_start = plugin->default_addr;

    int instance = plugin->create(oric);

    if (!instance)
        return SDL_FALSE;

    memset(&periph_table[i], 0x00, sizeof(struct PERIPH));

    periph_table[i].osditem = (enable ? 14 : 32);

    strncpy(periph_table[i].name, name, PERIPH_NAME_LEN);

    periph_table[i].instance = instance;

    periph_table[i].periph = plugin;

//    if (addr_start == 0)
//        periph_table[i].addr_start = periph_table[i].periph->default_addr;
//    else
//        periph_table[i].addr_start = addr_start;
    periph_table[i].addr_start = addr_start;

    periph_table[i].addr_end = periph_table[i].addr_start + periph_table[i].periph->size - 1;

    periph_table[i].enable = enable;


    nb_periph++;

    dbg_printf("Instance %d of %s created\n", instance, name);

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_del(char *name)
{
    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_enable(char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    periph_table[i].enable = SDL_TRUE;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_enable_by_id(int id, SDL_bool enable)
{
    dbg_printf("periph_enable_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    periph_table[id].enable = enable;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_disable(char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    periph_table[i].enable = SDL_FALSE;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_reset_by_name(struct machine *oric, char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    if (periph_table[i].periph->reset != NULL)
        return (periph_table[i].periph->reset(oric, periph_table[i].instance));

    else
        return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_reset_by_id(struct machine *oric, int id)
{
    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    if (periph_table[id].periph->reset != NULL)
        return (periph_table[id].periph->reset(oric, periph_table[id].instance));

    else
        return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_reset_all(struct machine *oric)
{
    int i=0;
    while (i != nb_periph)
    {
        if (periph_table[i].enable && (periph_table[i].periph->reset != NULL))
        {
            dbg_printf("PERIPH init: %s\n", periph_table[i].name);

            periph_table[i].periph->reset(oric, periph_table[i].instance);
        }
        i++;
    }
    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
void shut_periph(struct machine *oric)
{
    dbg_printf("*** Shutdown periph\n");

    if (nb_periph)
    {
        for (int id=0; id<nb_periph; id++)
        {
            free(periphitems[id].name);

            periph_shut_by_id(oric, id);
        }

        free(periphitems);
    }

    for (int i=0; i<nb_library; i++)
        dlclose(library[i]);
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_shut_by_id(struct machine *oric, int id)
{
    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    dbg_printf("Shutdown %s\n", periph_table[id].name);

    if (periph_table[id].periph->shutdown != NULL)
        return (periph_table[id].periph->shutdown(oric, periph_table[id].instance));

    else
        return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
unsigned char periph_read(struct machine *oric, unsigned short addr)
{
    int i=periph_find_by_addr(addr);

    if (i < nb_periph)
    {
        dbg_printf("PERIPH READ: %s ($%04x): (from: $%04x)", periph_table[i].name, addr, oric->cpu.lastpc);
        // return periph_table[i].periph->read(oric, periph_table[i].instance, addr - periph_table[i].addr_start);
        int data = periph_table[i].periph->read(oric, periph_table[i].instance, addr - periph_table[i].addr_start, SDL_TRUE);
        dbg_printf(" -> $%02x\n", data);
        return data;
    }
    // ERREUR: pas de périphérique pour l'adresse demandée
    return (unsigned char) 0;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_write(struct machine *oric, unsigned short addr, unsigned char data)
{
    int i=periph_find_by_addr(addr);

    if (i < nb_periph)
    {
        dbg_printf("PERIPH WRITE: %s ($%04x): $%02x (from $%04x)\n", periph_table[i].name, addr, data, oric->cpu.lastpc);
        return periph_table[i].periph->write(oric, periph_table[i].instance, addr - periph_table[i].addr_start, data);
    }

    // Pas de périphérique pour l'adresse demandée
    return SDL_FALSE;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
int periph_find_by_name(char *name)
{
    int i = 0;

    while ((i < nb_periph) && (strncasecmp(name, periph_table[i].name, PERIPH_NAME_LEN))) i++;

    return i;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
int periph_find_by_addr(unsigned short addr)
{
    int i = 0;

    while ((i < nb_periph) && ((periph_table[i].enable == SDL_FALSE) || (addr < periph_table[i].addr_start) || (addr > periph_table[i].addr_end))) i++;

    return i;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_present(unsigned short addr)
{
    return (periph_find_by_addr(addr) != nb_periph);
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_enabled_by_id(int id)
{
    dbg_printf("periph_enabled_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    return (periph_table[id].enable);
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
void periph_list()
{
    if (nb_periph == 0)
    {
        dbg_printf("Periph: empty list\n");
    }
    else
    {
        int i=0;

        while(i < nb_periph)
            periph_display(i++);
    }
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
void periph_display(int i)
{
    if ( (i < 0) || (i >= nb_periph) )
    {
        dbg_printf("Periph out of range: %d\n", i);
    }
    else
    {
        dbg_printf("Periph name: %s\n", periph_table[i].name);
        dbg_printf("Periph addresses: [%04X, %04X]\n", periph_table[i].addr_start, periph_table[i].addr_end);
        dbg_printf("Periph enable: %s\n", (periph_table[i].enable ? "yes" : "no"));
        dbg_printf("\n");
    }
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
// Set the title of a textzone
void my_tzsettitle( struct textzone *ptz, char *title )
{
  int ox, oy;
  // makebox( ptz, 0, 0, ptz->w, ptz->h, menufc(SDL_FALSE), menubc(SDL_FALSE));
  makebox( ptz, 0, 0, ptz->w, ptz->h, 2, 3);
  if( !title ) return;

  // tzsetcol( ptz, menufc(SDL_FALSE), menubc(SDL_FALSE));
  tzsetcol( ptz, 2, 3);
  ox = ptz->px;
  oy = ptz->py;
  ptz->px = 3;
  ptz->py = 0;
  tzstr( ptz, "[ " );
  tzstr( ptz, title );
  tzstr( ptz, " ]" );
  ptz->px = ox;
  ptz->py = oy;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
// Copie de mon_viamod
void mon_periphmod( int x, int y, int w, struct textzone *vtz )
{
  int offs, i;

  offs = y*vtz->w+x;
  for( i=0; i<w; i++, offs++ )
  {
    vtz->fc[offs] = 1;
    vtz->bc[offs] = 8;
  }
}
    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool mon_periph_enabled_by_id(int id)
{
    dbg_printf("mon_periph_enabled_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    // Si on veut ne prendre en compte que les extensions qui ont une page pour
    // le moniteur.
    // return ( periph_table[id].enable && (periph_table[id].mon_update != NULL) );

    // Sinon
    return ( periph_table[id].enable );
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
unsigned char periph_mon_read(struct machine *oric, unsigned short addr)
{
    int i=periph_find_by_addr(addr);

    if (i < nb_periph)
    {
        dbg_printf("PERIPH MON READ: %s ($%04x): (from: $%04x)", periph_table[i].name, addr, oric->cpu.lastpc);
        // return periph_table[i].periph->read(oric, periph_table[i].instance, addr - periph_table[i].addr_start);
        int data = periph_table[i].periph->read(oric, periph_table[i].instance, addr - periph_table[i].addr_start, SDL_FALSE);
        dbg_printf(" -> $%02x\n", data);
        return data;
    }
    // ERREUR: pas de périphérique pour l'adresse demandée
    return (unsigned char) 0;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
// void clear_textzone( struct machine *oric, int i );

void mon_update_periph( struct machine *oric, int id )
{
    struct textzone *ptz = tz[TZ_PERIPH];

    dbg_printf("*** MON_UPDATE_PERIPH: view = %d\n", id);

//    dbg_printf("W=%d, H=%d, X=%d, Y=%d\n", ptz->w, ptz->h, ptz->x, ptz->y);
/*
    if (view == 0)
    {
        my_tzsettitle(ptz, "Periph List");
        clear_textzone(oric, TZ_PERIPH);

        // Ligne: 0 -> titre
        //        1->19 texte
        //        20: cadre bas
        // Colonne: 1 -> 28

        for (int i=0; i<16; i++)
        {
            tzsetcol(ptz, i, 3);
            tzprintfpos(ptz, 2, i+1, "Couleur: %d", i);
    }
    tzstrpos(ptz, 1, 18, "123456789.123456789.12345678");

    return;
*/


    if (nb_periph == 0)
    {
        tzprintfpos( ptz, 2, 2,  "No extensions");
        return;

    }
/*
    else
    {
        int i=0;

        while(i < nb_periph)
        {
            tzprintfpos(ptz, 2, i*4+1, "Name     : %s\n", periph_table[i].name);
            tzprintfpos(ptz, 2, i*4+2, "Addresses: %04X...%04X\n", periph_table[i].addr_start, periph_table[i].addr_end);
            tzprintfpos(ptz, 2, i*4+3, "Enable   : %s\n", (periph_table[i].enable ? "yes" : "no"));
            i++;
        }
    }

    return;
    }
*/
    // int view = periph_find_by_name("STACK");

    if (id == nb_periph) return;

    if (!periph_table[id].enable) return;


    my_tzsettitle(ptz, periph_table[id].name);
    clear_textzone(oric, TZ_PERIPH);

    if (periph_table[id].periph->mon_update == NULL)
    {
        tzprintfpos(ptz, 2, 2, "Name     : %s\n", periph_table[id].name);
        tzprintfpos(ptz, 2, 3, "Addresses: %04X -> %04X\n", periph_table[id].addr_start, periph_table[id].addr_end);
        tzprintfpos(ptz, 2, 4, "Enable   : %s\n", (periph_table[id].enable ? "yes" : "no"));

        // Trait de séparation en ligne 6
        ptz->px = 0;
        ptz->py = 6;
        tzputc( ptz, 6 );

        for (int i=0; ptz->w-2; i++)
            tzputc( ptz, 2 );

        tzputc( ptz, 8 );

        return;
    }

//    my_tzsettitle(ptz, periph_table[id].name);
//    clear_textzone(oric, TZ_PERIPH);

    periph_table[id].periph->mon_update(ptz, periph_table[id].instance, periph_table[id].addr_start, periph_oldvalid);

    // 1: coin supérieur gauche
    // 2: - trait horizontal milieu épais
    // 3: T
    // 4: coin supérieur droit
    // 5: |
    // 6: |-
    // 7: -|-
    // 8: -|
    // 9: coin inférieur gauche |_
    // 10:
    // 11: coin inférieur droit _|
    // 12: trait horizonral milieu fin (pointillés)
    // 14: check mark
    // 15: moitié gauche K7
    // 16: moitié droite k7
    // 17: bouton magnéto stop (carré plein)
    // 18: bouton magnéto play
    // 19: bouton magnéto eject
    // 20: mpitié gauche D7
    // 21: moitié droite D7
    // 22: ...
    // 23: <-

}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
int mon_periph_count()
{
    return nb_periph;
}


    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
void mon_store_state_periph(struct machine *oric, SDL_bool oldvalid)
{
    dbg_printf("mon_store_state_periph(%d)\n", oldvalid);

    for (int id=0; id < nb_periph; id++)
    {
        if (periph_table[id].periph->mon_store_state != NULL)
            periph_table[id].periph->mon_store_state(oric, periph_table[id].instance);
    }
    periph_oldvalid = oldvalid;
}


    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
void mon_periph_oldvalid(SDL_bool oldvalid)
{
    dbg_printf("mon_periph_oldvalid(%d)\n", oldvalid);

    periph_oldvalid = oldvalid;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
// Toggle extension on/off
void toggleperiph( struct machine *oric, struct osdmenuitem *mitem, int id )
{
    if( periph_enabled_by_id(id) )
    {
        periph_enable_by_id(id, SDL_FALSE);

        // Mise à jour du menu OSD
        mitem->name[0] = 32;

        return;
    }

    // Le périphérique était désactivé, on l'active...
    periph_enable_by_id(id, SDL_TRUE);

    // .. et on l'initialise
    // À voir si on conserve l'initialisation dans ce cas
    periph_reset_by_id(oric, id);

    // Mise à jour du menu OSD
    mitem->name[0] = 14;
}



// *****************************************************************************
//                      Extension Stack hardware
// *****************************************************************************
// PIle hardware 16 niveaux
// 0000: pile
// 0001: pointeur de pile
// -----------------------------------------------------------------------------
/*
#define STACK_SIZE 16

struct STACK {
  unsigned char data[STACK_SIZE];
  unsigned char ptr;
  unsigned char old_data[STACK_SIZE];
  unsigned char old_ptr;
};

struct STACK stack_data;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool stack_reset(struct machine *oric, void *userdata )
{
    struct STACK *stack = (struct STACK *) userdata;

    stack->ptr = 0;

    // À voir si on initialise avec des données aléatoires au lieu de 0x00
    memset(stack->data, 0x00, STACK_SIZE);

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture de la pile (POP)
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
unsigned char stack_read(struct machine *oric, void *userdata, unsigned short addr, SDL_bool run)
{
    struct STACK *stack = (struct STACK *) userdata;

    switch (addr)
    {
        case 0:
            if (run)
            {
                // return stack->data[--stack->ptr];
                stack->ptr = (unsigned char)(stack->ptr -1) % STACK_SIZE;
                return stack->data[stack->ptr];
            }
            else
                return stack->data[stack->ptr];
        case 1:
            return stack->ptr;

        default:
            dbg_printf("STACK READ: bad address $%04x\n", addr);
            return (unsigned char) 0;
    }
}

    // -------------------------------------------------------------------------
    //                      Ecriture dasn la pile (PUSH)
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
SDL_bool stack_write(struct machine *oric, void *userdata, unsigned short addr, unsigned char data)
{
    struct STACK *stack = (struct STACK *) userdata;

    switch (addr)
    {
        case 0:
            // stack->data[stack->ptr++] = data;
            stack->data[stack->ptr] = data;
            stack->ptr = (stack->ptr +1) % STACK_SIZE;
            break;

        case 1:
            stack->ptr = data % 16;
            break;

        default:
            dbg_printf("STACK WRITE: bad address $%04x\n", addr);
            return (unsigned char) 0;
    }
    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
void mon_stack_update(struct textzone *tz, void *userdata, unsigned short base_addr, SDL_bool oldvalid)
{
    struct STACK *stack = (struct STACK *) userdata;

    int i;

    dbg_printf("STACK: mon update\n");

    tzprintfpos( tz, 2, 2,  "Base address : %04X", base_addr);
    tzprintfpos( tz, 2, 3,  "Stack pointer:   %02X", stack->ptr);
    tzprintfpos( tz, 2, 4,  "Stack size   :   %02X", STACK_SIZE);

    // Trait de séparation en ligne 6
    tz->px = 0;
    tz->py = 6;
    tzputc( tz, 6 );

    for (int i=0; i < tz->w-2; i++)
//        tzputc( tz, 2 );
        tzputc( tz, 12 );

    tzputc( tz, 8 );

    for (i=0; i<8; i++)
    {
        tzprintfpos(tz, 4, i+7, "%c %02X: %02X", (i==stack->ptr ? '>' : ' '), i, stack->data[i]);
        tzprintfpos(tz, 4+12, i+7, "%c %02X: %02X", (i+8==stack->ptr ? '>' : ' '), i+8, stack->data[i+8]);
    }


    if (oldvalid)
    {
        if (stack->ptr != stack->old_ptr)
            mon_periphmod( 19, 3, 2, tz );

        for (i=0; i<8; i++)
        {
            if (stack->data[i] != stack->old_data[i])
                mon_periphmod( 10, i+7, 2, tz );

            if (stack->data[i+8] != stack->old_data[i+8])
                mon_periphmod( 10+12, i+7, 2, tz );
        }
    }

//    tzstrpos(tz, 1, 15, "....+....|....+....|....+...");
}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
void mon_stack_store(struct machine *oric, void *userdata)
{
    struct STACK *stack = (struct STACK *) userdata;

    // Copy data+ptr
    memcpy(stack->old_data, stack->data, STACK_SIZE+1);
}
*/

/*
// *****************************************************************************
//                    Extension Registre avec auto-incrément
// *****************************************************************************
// Registre 16 bits avec post incrément
// 0000-0001: registre
// 0003     : incrément (signé)
// -----------------------------------------------------------------------------

struct REG {
  unsigned short data;
  char incr;
  unsigned short old_data;
  char old_incr;
};

struct REG reg_data[2];


SDL_bool reg_reset(struct machine *oric, void *userdata )
{
    struct REG *reg = (struct REG *) userdata;

    reg->incr = 0;
    reg->data = 0;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture du registre
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
unsigned char reg_read(struct machine *oric, void *userdata, unsigned short addr, SDL_bool run)
{
    struct REG *reg = (struct REG *) userdata;

    // On inccrémente après la lecture du MSB
    //
    // ATTENTION:
    //     - DEEK lit d'abord le MSB puis le LSB
    //     - Oricutron lit d'abord le MSB puis le LSB pour un adressage indirect
    //       contrairement à ce que fait le 6502
    switch (addr & 0x0003)
    {
        case 0:
            return reg->data & 0x00ff;

        case 1:
        {
            unsigned char data = reg->data >> 8;
            if (run)
                reg->data += reg->incr;
            return data;
        }
        case 2:
            return reg->incr;

        default:
            dbg_printf("REG_READ: bad address $%04x\n", addr);
            return (unsigned char) 0;
    }
}

    // -------------------------------------------------------------------------
    //                      Ecriture dans le registre
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
SDL_bool reg_write(struct machine *oric, void *userdata, unsigned short addr, unsigned char data)
{
    struct REG *reg = (struct REG *) userdata;

    // ATTENTION:
    //     - DOKE écrit d'abord le MSB puis le LSB
    //     - Oricutron lit d'abord le MSB puis le LSB pour un adressage indirect
    //       contrairement à ce que fait le 6502

    switch (addr & 0x0003)
    {
        case 0:
            reg->data = (reg->data & 0xff00) | data;
            return SDL_TRUE;

        case 1:
            reg->data = (reg->data & 0x00ff) | (data << 8);
            return SDL_TRUE;

        case 2:
            reg->incr = data;
            return SDL_TRUE;

        default:
            dbg_printf("REG_READ: address address $%04x\n", addr);
            return SDL_FALSE;
    }
}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
void mon_reg_update(struct textzone *tz, void *userdata, unsigned short base_addr, SDL_bool oldvalid)
{
    struct REG *reg = (struct REG *) userdata;

    dbg_printf("REG: mon update\n");

    tzprintfpos( tz, 2, 2,  "Base address  : %04X", base_addr);
    tzprintfpos( tz, 2, 3,  "Register value: %04X", reg->data);
    tzprintfpos( tz, 2, 4,  "Register incr.:   %02X", (unsigned char) reg->incr);


    if (oldvalid)
    {
        if (reg->data != reg->old_data)
            mon_periphmod( 18, 3, 4, tz );

        if (reg->incr != reg->old_incr)
            mon_periphmod( 20, 4, 2, tz );
    }
}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
void mon_reg_store(struct machine *oric, void *userdata)
{
    struct REG *reg = (struct REG *) userdata;

    reg->old_data = reg->data;
    reg->old_incr = reg->incr;
}
*/

// *****************************************************************************
//                      Déclaratoin des extensions
// *****************************************************************************
struct PLUGIN * load_plugin(char *library_name)
{
    SDL_bool (*plugin_init)(void *tzprintfpos, void *tzputc, void *mon_periphmod);
    void *handle;

    if (library == NULL)
        return NULL;

    dbg_printf("load_plugin: %s\n", library_name);

    // if ( (handle = dlopen("/home/hcl/devel/GIT-WC/oricutron-hcl/plugins/stack/libstack.so", RTLD_NOW)) == NULL)
    if ( (handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL)) == NULL)
    {
        dbg_printf("=== ERREUR DE CHARGEMENT DE LA DLL: %s\n", dlerror());
        return NULL;
    }

    struct PLUGIN *plugin = dlsym(handle, "plugin");
    if (dlerror() != NULL)
    {
        dlclose(handle);
        return NULL;
    }

    plugin_init = dlsym(handle, "plugin_init");
    if (dlerror() != NULL)
    {
        dlclose(handle);
        return NULL;
    }

    library[nb_library++] = handle;

    if (!plugin_init(tzprintfpos, tzputc, mon_periphmod))
        return NULL;

    return plugin;
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
SDL_bool periph_test(struct machine *oric)
{
    struct PLUGIN *plugin;

    if (!nb_periph)
    {
        // Déclaration des périphériques
        plugin=load_plugin("libstack.so");
        if (plugin != NULL)
            if (!periph_add(oric, plugin, NULL, 0x360, SDL_FALSE))
                dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");

        plugin = load_plugin("libregister.so");
        if (plugin != NULL)
        {
            if (!periph_add(oric, plugin, "Reg 0", 0x362, SDL_FALSE))
                dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");

            if (!periph_add(oric, plugin, "Reg 1", 0x366, SDL_FALSE))
                dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");
        }

        // On suppose que le fichier de configuration a déjà été lu
        plugin=load_plugin("libch376.so");
        if (plugin != NULL)
            if (!periph_add(oric, plugin, NULL, 0x340, oric->ch376_activated))
                dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");

        // Création du menu OSD
        periphitems = calloc(nb_periph+3, sizeof(struct osdmenuitem));

        if (periphitems == NULL)
            dbg_printf("*** ERROR CALLOC\n");

        for (int i=0; i<nb_periph; i++)
        {
            dbg_printf("Initialisation %d\n", i);

            memset(&periphitems[i], 0x00, sizeof(struct osdmenuitem));

            // periphitems[i].name = strndup(&periph_table[i].osditem, PERIPH_NAME_LEN+1);
            periphitems[i].name = malloc(PERIPH_NAME_LEN+10);
            if (periphitems[i].name)
                sprintf(periphitems[i].name, "%c%-*s    $%04X", (periph_table[i].enable ? 14 : 32), PERIPH_NAME_LEN, periph_table[i].name, periph_table[i].addr_start);

            else
                dbg_printf("*** MALLOC ERROR ***\n");

            periphitems[i].func = toggleperiph;
            periphitems[i].arg = i;
        }

        // AJout de l'option pour retour vers le menu principal
        periphitems[nb_periph].name = OSDMENUBAR;

        periphitems[nb_periph+1].name   = "Back";
        periphitems[nb_periph+1].key    = "\x17";
        periphitems[nb_periph+1].sdlkey = SDLK_BACKSPACE;
        periphitems[nb_periph+1].func   = gotomenu;
        periphitems[nb_periph+1].arg    = 1;            // Menu Hardware

        // Intégration du menu dans le menu principal
        menus[8].items = periphitems;
    }

    return SDL_TRUE;
}
