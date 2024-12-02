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
#include "../../main.h"

// #include "../../plugin.h"
#include "periph.h"

#include <dlfcn.h>

// #define DEBUG_PLUGIN
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
extern struct textzone *tz[];
extern struct osdmenu menus[];

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#define MAX_PERIPH 20

struct DEVICE {
    char osditem;
    char name[PERIPH_NAME_LEN+1];
    Uint16 addr_start;
    Uint16 addr_end;
    Uint16 type;
    unsigned int instance;
    SDL_bool enable;
    struct PLUGIN *periph;
};

struct DEVICE devices_table[MAX_PERIPH];
unsigned int nb_periph = 0;
SDL_bool periph_oldvalid = SDL_FALSE;

void *library[MAX_PERIPH];
unsigned int nb_library = 0;
// void *handle;

struct osdmenuitem *periphitems;

struct plugin_opts
{
  char     lctmp[2048];
  char     plugin[1024];
  char     device[10+1];
  SDL_bool enable;
  SDL_bool load;
  int      base_address;
};



// -----------------------------------------------------------------------------
// INTERNAL
// -----------------------------------------------------------------------------
SDL_bool periph_add(struct machine *oric, struct PLUGIN *plugin, char *name, Uint16 addr_start, SDL_bool enable)
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

    memset(&devices_table[i], 0x00, sizeof(struct DEVICE));

    devices_table[i].osditem = (enable ? 14 : 32);

    strncpy(devices_table[i].name, name, PERIPH_NAME_LEN);

    devices_table[i].instance = instance;

    devices_table[i].periph = plugin;

    devices_table[i].addr_start = addr_start;

    devices_table[i].addr_end = devices_table[i].addr_start + devices_table[i].periph->size - 1;

    devices_table[i].enable = enable;

    devices_table[i].type = devices_table[i].periph->type;

    nb_periph++;

    dbg_printf("Instance %d of %s created\n", instance, plugin->name);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_del(char *name)
{
    return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_enable(char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    devices_table[i].enable = SDL_TRUE;

    return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
// INTERNAL (menu)
// -------------------------------------------------------------------------
SDL_bool periph_enable_by_id(int id, SDL_bool enable)
{
    dbg_printf("periph_enable_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    devices_table[id].enable = enable;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_disable(char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    devices_table[i].enable = SDL_FALSE;

    return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_reset_by_name(struct machine *oric, char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    if (devices_table[i].periph->reset != NULL)
        return (devices_table[i].periph->reset(oric, devices_table[i].instance));

    else
        return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
SDL_bool periph_reset_by_id(struct expansion_bus *oric, int id)
{
    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    if (devices_table[id].periph->reset != NULL)
        return (devices_table[id].periph->reset(oric, devices_table[id].instance));

    else
        return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool periph_reset_all(struct machine *oric)
{
    int i=0;

    struct expansion_bus myoric;
    myoric.cpu = &oric->cpu;
    myoric.romdis =  &oric->romdis;

    while (i != nb_periph)
    {
        if (devices_table[i].enable && (devices_table[i].periph->reset != NULL))
        {
            dbg_printf("PERIPH init: %s\n", devices_table[i].name);

            devices_table[i].periph->reset(&myoric, devices_table[i].instance);
        }
        i++;
    }
    // oric->romdis = SDL_TRUE;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool periph_ticktock_all(struct machine *oric, int cycles)
{
    int i=0;
    while (i != nb_periph)
    {
        if (devices_table[i].enable && (devices_table[i].periph->ticktock != NULL))
        {
            // dbg_printf("PERIPH ticktock: %s\n", devices_table[i].name);

            devices_table[i].periph->ticktock(oric, devices_table[i].instance, cycles);
        }
        i++;
    }
    return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
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
// INTERNAL
// -------------------------------------------------------------------------
SDL_bool periph_shut_by_id(struct machine *oric, int id)
{
    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    dbg_printf("Shutdown %s\n", devices_table[id].name);

    if (devices_table[id].periph->shutdown != NULL)
        return (devices_table[id].periph->shutdown(oric, devices_table[id].instance));

    else
        return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
Uint8 periph_read(struct machine *oric, Uint16 addr)
{
    SDL_bool fBank = SDL_FALSE;

    int i=periph_find_by_addr(oric, addr);
    Uint8 data = 0;

    if (i < nb_periph)
    {
        // dbg_printf("PERIPH READ: %s ($%04x): (from: $%04x)", devices_table[i].name, addr, oric->cpu.lastpc);

        // return devices_table[i].periph->read(oric, devices_table[i].instance, addr - devices_table[i].addr_start);

        if (addr >= 0xc000)
        {
            fBank = SDL_TRUE;
            addr = addr - 0xc000;
        }
        else
            addr = addr - devices_table[i].addr_start;

        if (devices_table[i].periph->read != NULL)
            data = devices_table[i].periph->read(oric, fBank, devices_table[i].instance, addr, SDL_TRUE);

        // dbg_printf(" -> $%02x\n", data);
        return data;
    }
    // ERREUR: pas de périphérique pour l'adresse demandée
    return (Uint8) 0;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool periph_write(struct machine *oric, Uint16 addr, Uint8 data)
{
    int i=periph_find_by_addr(oric, addr);
    SDL_bool fbank = (addr >= 0xc000);

    if (i < nb_periph)
    {
        // dbg_printf("PERIPH WRITE: %s ($%04x): $%02x (from $%04x)\n", devices_table[i].name, addr, data, oric->cpu.lastpc);

        if (fbank)
            addr = addr - 0xc000;
        else
            addr = addr - devices_table[i].addr_start;

        if (devices_table[i].periph->write != NULL)
            return devices_table[i].periph->write(oric, fbank, devices_table[i].instance, addr, data);
    }

    // Pas de périphérique pour l'adresse demandée
    return SDL_FALSE;
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
int periph_find_by_name(char *name)
{
    int i = 0;

    while ((i < nb_periph) && (strncasecmp(name, devices_table[i].name, PERIPH_NAME_LEN))) i++;

    return i;
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
int periph_find_by_addr(struct machine *oric, Uint16 addr)
{
    int i = 0;

    // Si accès à la rom interne -> fin
    if ((addr >= 0xc000) && (!oric->romdis))
        return nb_periph;

    dbg_printf("periph_find_by_addr(0x%04x)... ", addr);

    // while ((i < nb_periph) && ((devices_table[i].enable == SDL_FALSE) || (addr < devices_table[i].addr_start) || (addr > devices_table[i].addr_end))) i++;

    if (addr >= 0xc000)
        while (
            (i < nb_periph) &&
                (
                    ( devices_table[i].enable == SDL_FALSE ) ||
                    ( !(devices_table[i].type & PLG_BANK) )
                )
            ) i++;
/*
        while (
            (i < nb_periph) &&
                (
                    ( devices_table[i].enable == SDL_FALSE ) ||
                    ( !(devices_table[i].type & PLG_BANK) ) ||
                    ( (devices_table[i].periph->addresses == NULL) && ((addr < devices_table[i].addr_start) || (addr > devices_table[i].addr_end)) ) ||
                    ( (devices_table[i].periph->addresses != NULL) && !devices_table[i].periph->addresses(devices_table[i].instance, addr - devices_table[i].addr_start) )
                )
            ) i++;
*/
    else
        while (
            (i < nb_periph) &&
                (
                    ( devices_table[i].enable == SDL_FALSE) ||
                    ( !(devices_table[i].type & PLG_DEVICE) ) ||
                    ( (devices_table[i].periph->addresses == NULL) && ((addr < devices_table[i].addr_start) || (addr > devices_table[i].addr_end)) ) ||
                    ( (devices_table[i].periph->addresses != NULL) && !devices_table[i].periph->addresses(devices_table[i].instance, addr - devices_table[i].addr_start) )
                )
            ) i++;

/*
        while (
            (i < nb_periph) &&
                (
                    (devices_table[i].enable == SDL_FALSE) ||
                    ( !(devices_table[i].type & PLG_DEVICE) ) ||
                    ( addr < devices_table[i].addr_start ) ||
                    ( (devices_table[i].periph->addresses == NULL) && (addr > devices_table[i].addr_end) ) ||
                    ( (devices_table[i].periph->addresses != NULL) && !devices_table[i].periph->addresses(devices_table[i].instance, addr) )
                )
            ) i++;
*/

    dbg_printf("%d\n", i);

    return i;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool periph_present(struct machine *oric, Uint16 addr)
{
    return (periph_find_by_addr(oric, addr) != nb_periph);
}

// -------------------------------------------------------------------------
// INTERNAL (menu)
// -------------------------------------------------------------------------
SDL_bool periph_enabled_by_id(int id)
{
    dbg_printf("periph_enabled_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    return (devices_table[id].enable);
}

// -------------------------------------------------------------------------
// USED
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
// INTERNAL
// -------------------------------------------------------------------------
void periph_display(int i)
{
    if ( (i < 0) || (i >= nb_periph) )
    {
        error_printf("Periph out of range: %d", i);
    }
    else
    {
        error_printf("Periph name     : %s", devices_table[i].name);
        error_printf("Periph instance : %d", devices_table[i].instance);
        error_printf("Periph addresses: [%04X, %04X]", devices_table[i].addr_start, devices_table[i].addr_end);
        error_printf("Periph enable   : %s", (devices_table[i].enable ? "yes" : "no"));
        error_printf("Periph type     : 0x%02X", devices_table[i].type);
        error_printf("");
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
// INTERNAL (plugin)
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
// USED
// -------------------------------------------------------------------------
SDL_bool mon_periph_enabled_by_id(int id)
{
    dbg_printf("mon_periph_enabled_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    // Si on veut ne prendre en compte que les extensions qui ont une page pour
    // le moniteur.
    // return ( devices_table[id].enable && (devices_table[id].mon_update != NULL) );

    // Sinon
    return ( devices_table[id].enable );
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
Uint8 periph_mon_read(struct machine *oric, Uint16 addr)
{
    int i=periph_find_by_addr(oric, addr);

    if (i < nb_periph)
    {
        Uint8 data = 0;

        dbg_printf("PERIPH MON READ: %s ($%04x): (from: $%04x)", devices_table[i].name, addr, oric->cpu.lastpc);
        // return devices_table[i].periph->read(oric, devices_table[i].instance, addr - devices_table[i].addr_start);

        if (devices_table[i].periph->read != NULL)
        {
            if (addr >= 0xc000)
                addr = addr - 0xc000;
            else
                addr = addr - devices_table[i].addr_start;

            data = devices_table[i].periph->read(oric, (addr >= 0xc000), devices_table[i].instance, addr, SDL_FALSE);
            dbg_printf(" -> $%02x\n", data);
        }
        else
            dbg_printf("WRITE ONLY\n");

        return data;
    }
    // ERREUR: pas de périphérique pour l'adresse demandée
    return (Uint8) 0;
}

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
// void clear_textzone( struct machine *oric, int i );

void mon_update_periph( struct machine *oric, int id )
{
    struct textzone *ptz = tz[TZ_PERIPH];

    dbg_printf("*** MON_UPDATE_PERIPH: view = %d\n", id);

    // dbg_printf("W=%d, H=%d, X=%d, Y=%d\n", ptz->w, ptz->h, ptz->x, ptz->y);
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
            tzprintfpos(ptz, 2, i*4+1, "Name     : %s\n", devices_table[i].name);
            tzprintfpos(ptz, 2, i*4+2, "Addresses: %04X...%04X\n", devices_table[i].addr_start, devices_table[i].addr_end);
            tzprintfpos(ptz, 2, i*4+3, "Enable   : %s\n", (devices_table[i].enable ? "yes" : "no"));
            i++;
        }
    }

    return;
    }
    */
    // int view = periph_find_by_name("STACK");

    if (id == nb_periph) return;

    if (!devices_table[id].enable) return;


    my_tzsettitle(ptz, devices_table[id].name);
    clear_textzone(oric, TZ_PERIPH);

    if (devices_table[id].periph->mon_update == NULL)
    {
        dbg_printf("PERIPH: mon_update(%d) == NULL", id);

        tzprintfpos(ptz, 2, 2, "Name     : %s\n", devices_table[id].name);
        tzprintfpos(ptz, 2, 3, "Addresses: $%04X -> $%04X\n", devices_table[id].addr_start, devices_table[id].addr_end);
        tzprintfpos(ptz, 2, 4, "Enable   : %s\n", (devices_table[id].enable ? "yes" : "no"));
        tzprintfpos(ptz, 2, 5, "Type     : $%02X\n", devices_table[id].type);

        // Trait de séparation en ligne 6
        ptz->px = 0;
        ptz->py = 6;
        tzputc( ptz, 6 );

        for (int i=0; i<ptz->w-2; i++)
            tzputc( ptz, 2 );

        tzputc( ptz, 8 );

        return;
    }

    // my_tzsettitle(ptz, devices_table[id].name);
    // clear_textzone(oric, TZ_PERIPH);

    devices_table[id].periph->mon_update(ptz, devices_table[id].instance, devices_table[id].addr_start, periph_oldvalid);


}

// -------------------------------------------------------------------------
// USED
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
        if (devices_table[id].periph->mon_store_state != NULL)
            devices_table[id].periph->mon_store_state(oric, devices_table[id].instance);
    }
    periph_oldvalid = oldvalid;
}


// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
void mon_periph_oldvalid(SDL_bool oldvalid)
{
    dbg_printf("mon_periph_oldvalid(%d)\n", oldvalid);

    periph_oldvalid = oldvalid;
}

// -------------------------------------------------------------------------
// INTERNAL (menu)
// -------------------------------------------------------------------------
// Toggle extension on/off
void toggleperiph( struct machine *oric, struct osdmenuitem *mitem, int id )
{
    struct expansion_bus myoric;
    myoric.cpu = &oric->cpu;
    myoric.romdis =  &oric->romdis;

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
    periph_reset_by_id(&myoric, id);

    // Mise à jour du menu OSD
    mitem->name[0] = 14;
}


// *****************************************************************************
//                      Déclaratoin des extensions
// INTERNAL
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
        error_printf("=== ERREUR DE CHARGEMENT DE LA DLL: %s\n", dlerror());
        return NULL;
    }

    struct PLUGIN *plugin = dlsym(handle, "plugin");
    if (dlerror() != NULL)
    {
        error_printf("=== ERREUR DE CHARGEMENT DE LA DLL: symbol 'plugin'\n", dlerror());
        dlclose(handle);
        return NULL;
    }

    plugin_init = dlsym(handle, "plugin_init");
    if (dlerror() != NULL)
    {
        error_printf("=== ERREUR DE CHARGEMENT DE LA DLL: symbol 'plugin_init'%s\n", dlerror());
        dlclose(handle);
        return NULL;
    }

    library[nb_library++] = handle;

    if (!plugin_init(tzprintfpos, tzputc, mon_periphmod))
        return NULL;

    return plugin;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool periph_test(struct machine *oric)
{
    // struct PLUGIN *plugin;

    if (!nb_periph)
    {
        load_devices_config(oric);

    /*
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

        plugin=load_plugin("libds1501.so");
        if (plugin != NULL)
            if (!periph_add(oric, plugin, NULL, 0, oric->ds1501_activated))
                dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");

        plugin=load_plugin("libdebug.so");
        if (plugin != NULL)
            if (!periph_add(oric, plugin, NULL, 0, SDL_FALSE))
                dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");
    */

        // Création du menu OSD
        periphitems = calloc(nb_periph+3, sizeof(struct osdmenuitem));

        if (periphitems == NULL)
            dbg_printf("*** ERROR CALLOC\n");

        for (int i=0; i<nb_periph; i++)
        {
            dbg_printf("Initialisation %d\n", i);

            memset(&periphitems[i], 0x00, sizeof(struct osdmenuitem));

            // periphitems[i].name = strndup(&devices_table[i].osditem, PERIPH_NAME_LEN+1);
            periphitems[i].name = malloc(PERIPH_NAME_LEN+11);
            if (periphitems[i].name)
                sprintf(periphitems[i].name, "%c%-*s    $%04X", (devices_table[i].enable ? 14 : 32), PERIPH_NAME_LEN, devices_table[i].name, devices_table[i].addr_start);

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


// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
SDL_bool load_devices_config(struct machine *oric)
{
    struct PLUGIN *plugin;

    struct plugin_opts *sto;
    sto = malloc(sizeof(struct plugin_opts));

    if (!sto) return SDL_FALSE;

    FILE *f;
    Sint32 i;
    char config_path[4096];

    char *device;

    strcpy(config_path, "plugins.cfg");
    add_fileprefix(config_path, 4096);
    dbg_printf("Open plugins file: %s\n", config_path);

    f = fopen(config_path, "r");
    if (!f) return SDL_FALSE;

    while ( !feof(f) )
    {
        if (!fgets(sto->lctmp, 2048, f)) break;

        for (i=0; isws(sto->lctmp[i]); i++);

        dbg_printf("[1]: %s", sto->lctmp);

        while (sto->lctmp[i] == '[')
        {
            // On a un début de bloc

            i++;
            for (; isws(sto->lctmp[i]); i++);

            dbg_printf("Found paragraph: %s", sto->lctmp+i);

            sto->enable = SDL_FALSE;
            sto->load = SDL_FALSE;
            sto->plugin[0] = '\0';
            sto->device[0] = '\0';
            sto->base_address = 0;

            do
            {
                if (fgets(sto->lctmp, 2048, f))
                {
                    for (i=0; isws(sto->lctmp[i]); i++);

                    if ((sto->lctmp[i] != '\n') && (sto->lctmp[i] != ';'))
                    {
                        for (i=0; isws(sto->lctmp[i]); i++);

                        if (read_config_bool(&sto->lctmp[i]  , "enable"      , &sto->enable)) continue;
                        if (read_config_bool(&sto->lctmp[i]  , "load"        , &sto->load)) continue;
                        if (read_config_path(&sto->lctmp[i]  , "plugin"      , sto->plugin, 1024)) continue;
                        if (read_config_string(&sto->lctmp[i], "device"      , sto->device, 10+1)) continue;
                        if (read_config_int(&sto->lctmp[i]   , "base_address", &sto->base_address, 0x0300-1, 0x03ff+1)) continue;

                        // Si on arrive ici, on est soit au début d'un nouveau paragraphe, soit avec une option inconnue
                        if (sto->lctmp[0] != '[')
                            dbg_printf("\t\t[3]%s", sto->lctmp+i);
                    }
                }
            } while (!feof(f) && sto->lctmp[i] != '[');

            dbg_printf("\t[2]: loop\n");

            dbg_printf("device: %s\n", sto->device);
            dbg_printf("plugin: %s\n", sto->plugin);
            dbg_printf("load: %s\n", (sto->load ? "yes" : "no"));
            dbg_printf("enable: %s\n", (sto->enable ? "yes" : "no"));
            dbg_printf("base address: 0x%x\n", sto->base_address);

            if (sto->load && (sto->plugin[0] != '\0') && (((sto->base_address >= 0x300) && (sto->base_address <= 0x3ff)) || sto->base_address == 0))
            {
                plugin=load_plugin(sto->plugin);
                if (plugin != NULL)
                {
                    device =  (sto->device[0] != '\0' ? sto->device : NULL);

                    if (!periph_add(oric, plugin, device, sto->base_address, sto->enable))
                        dbg_printf("periph_test: erreur lors de l'ajout du périphérique\n");
                }
            }
        }
        dbg_printf("[1]: loop\n");
    }
    free(sto);
    fclose(f);

    return SDL_TRUE;
}


