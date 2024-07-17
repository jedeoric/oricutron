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

// dbg_printf est une fonction déclarée dans monitor.h mais est spécifique au moniteur
#define dbg_printf(x...) { printf(x); }

#define DS1501_SECONDS_REGISTER 0x360
#define DS1501_MINUTES_REGISTER 0x361
#define DS1501_HOUR_REGISTER    0x362
#define DS1501_DAY_REGISTER     0x363
#define DS1501_DATE_REGISTER    0x364
#define DS1501_MONTH_REGISTER   0x365
#define DS1501_YEAR_REGISTER    0x366
#define DS1501_CENTURY_REGISTER 0x367

#define DS1501_ADDRESS_INTERNAL_RAM_REGISTER 0x370
#define DS1501_DATA_INTERNAL_RAM_REGISTER 0x373

#define DS1501_CTRLA_REGISTER   0x36E
#define DS1501_CTRLB_REGISTER   0x36F

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
#define END_ADDR 0x37F

#define INSTANCE_MAX 1

// Control A flags
#define BLF1_mask    0x80
#define BLF2_mask    0x40
#define PRS_mask     0x20
#define PAB_mask     0x10
#define TDF_mask     0x08
#define KSF_mask     0x04
#define WDF_mask     0x02
#define IRQF_mask    0x01

// Control B flags
#define TE_mask      0x80
#define CS_mask      0x40
#define BME_mask     0x20
#define TPE_mask     0x10
#define TIE_mask     0x08
#define KIE_mask     0x04
#define WDE_mask     0x02
#define WDS_mask     0x01

// Month register control bits
#define EOSC_mask   0x80
#define E32K_mask   0x40
#define BB32_maks   0x20

// Alarm control bits
#define AM1_mask    0x80
#define AM2_mask    0x80
#define AM3_mask    0x80
#define AM4_mask    0x80
#define DYDT_mask   0x40

// Utilitaire
#define bcd2bin(data)   (data & 0x0f) + (data >> 4)*10

struct DS1501_REGISTERS
{
    unsigned char seconds;
    unsigned char minutes;
    unsigned char hours;
    unsigned char day;
    unsigned char date;
    unsigned char month;
    unsigned char year;
    unsigned char century;
    unsigned char alarm_seconds;
    unsigned char alarm_minutes;
    unsigned char alarm_hours;
    unsigned char alarm_day_date;
    unsigned char watchdog_ms;
    unsigned char watchdog_s;
};

struct DS1501
{
    struct DS1501_REGISTERS internal;
    struct DS1501_REGISTERS external;

    unsigned char control_a;
    unsigned char control_b;
    unsigned char ram_address;
    unsigned char ram[256];

//    int clock;
    int clock_us;
    int clock_ms;
    unsigned char internal_watchdog_ms;
    unsigned char internal_watchdog_s;
};

struct DS1501 *userdata[INSTANCE_MAX];
struct DS1501 *userdata_old[INSTANCE_MAX];

int plugin_instances = 0;

static char *description = "DS1501";

unsigned char bin2bcd(unsigned char value);

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
unsigned int plugin_create(struct machine *oric)
{
    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    userdata[plugin_instances] = malloc(sizeof(struct DS1501));
    userdata_old[plugin_instances] = malloc(sizeof(struct DS1501));

    if (userdata[plugin_instances])
    {
        if (userdata_old[plugin_instances] == NULL)
        {
            free(userdata[plugin_instances]);
            return 0;
        }

        // EOSC = E32K = TIE = KIE = WDE = WDS = 0
        userdata[plugin_instances]->internal.month &= ~(EOSC_mask | E32K_mask);
        userdata[plugin_instances]->control_b &= ~(TIE_mask | KIE_mask |  WDE_mask | WDS_mask);

        userdata[plugin_instances]->clock_us = 10000;
        userdata[plugin_instances]->clock_ms = 100;

        // Temporaire pour tests
        userdata[plugin_instances]->internal.hours = 0;
        userdata[plugin_instances]->internal.minutes = 0;
        userdata[plugin_instances]->internal.seconds = 0;

      return ++plugin_instances;
    }

    return 0;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
SDL_bool plugin_shutdown(struct machine *oric, unsigned int instance)
{
    if (plugin_instances >= INSTANCE_MAX)
        return SDL_FALSE;

    free(userdata[plugin_instances]);
    free(userdata_old[plugin_instances]);

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool plugin_reset(struct machine *oric, unsigned int instance)
{
    dbg_printf("stack_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture du DS1501
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
unsigned char plugin_read(struct machine *oric, unsigned int instance, unsigned short addr, SDL_bool run)
{
    if ( (!instance) || (instance > plugin_instances) )
        return (unsigned char) 0;

    instance--;

    switch (addr)
    {
       // Seconds
        case 0x00:
            return userdata[instance]->external.seconds;
            break;

        // Minutes
        case 0x01:
            return userdata[instance]->external.minutes;
            break;

        // Hours
        case 0x02:
            return userdata[instance]->external.hours;
            break;

        // Day
        case 0x03:
            return userdata[instance]->external.day;
            break;

        // Date
        case 0x04:
            return userdata[instance]->external.date;
            break;

        // Month
        case 0x05:
            return userdata[instance]->external.month;
            break;

        // Year
        case 0x06:
            return userdata[instance]->external.year;
            break;

        // Century
        case 0x07:
            return userdata[instance]->external.century;
            break;

        // Alarm Seconds
        case 0x08:
            return userdata[instance]->external.alarm_seconds;
            break;

        // Alarm Minutes
        case 0x09:
            return userdata[instance]->external.alarm_minutes;
            break;

        // Alarm Hours
        case 0x0a:
            return userdata[instance]->external.alarm_hours;
            break;

        // Alarm Day/Date
        case 0x0b:
            return userdata[instance]->external.alarm_day_date;
            break;

        // Watchdog
        case 0x0c:
            return userdata[instance]->external.watchdog_ms;
            break;

        // Watchdog
        case 0x0d:
            return userdata[instance]->external.watchdog_s;
            break;

        // Control A
        // BLF1 | BLF2 | PRS | PAB | TDF | KSF | WDF | IRQF
        case 0x0e:
        {
            unsigned char data = userdata[instance]->control_a;
            userdata[instance]->control_a &= ~(TDF_mask|KSF_mask|WDF_mask|IRQF_mask);
            return data;
        }

        // Contorl B
        // TE | CS | BME | TPE | TIE | KIE | WDE | WDS
        case 0x0f:
            return userdata[instance]->control_b;
            break;

        // RAM Addres
        case 0x10:
            return userdata[instance]->ram_address;
            break;

        // Reserved
        case 0x11:
        case 0x12:
            return (unsigned char) 0;
            break;

        // RAM Data
        case 0x13:
            if (userdata[instance]->control_b & BME_mask)
                return userdata[instance]->ram[userdata[instance]->ram_address++];

            return userdata[instance]->ram[userdata[instance]->ram_address];
            break;

        // Reserved
        case 0x14 ... 0x1f:
            break;

        default:
            dbg_printf("DS1501 READ: bad address $%04x\n", addr);
            return (unsigned char) 0;
    }
}

    // -------------------------------------------------------------------------
    //                      Ecriture dans le DS1501
    // -------------------------------------------------------------------------
SDL_bool plugin_write(struct machine *oric, unsigned int instance, unsigned short addr, unsigned char data)
{
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    switch (addr)
    {
       // Seconds
        case 0x00:
            // data &= 0x7f;
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.seconds = data & 0x7f;
            break;

        // Minutes
        case 0x01:
            // data &= 0x7f;
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.minutes = data & 0x7f;
            break;

        // Hours
        case 0x02:
            // data &= 0x3f;
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.hours = data & 0x3f;
            break;

        // Day
        case 0x03:
            userdata[instance]->external.day = data & 0x07;
            break;

        // Date
        case 0x04:
            //data &= 0x3f;
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.date = data & 0x3f;
            break;

        // Month
        case 0x05:
            // data &= 0x1f;
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.month = data;
            break;

        // Year
        case 0x06:
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.year = data;
            break;

        // Century
        case 0x07:
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.century = data;
            break;

        // Alarm Seconds
        case 0x08:
            userdata[instance]->external.alarm_seconds = data;
            break;

        // Alarm Minutes
        case 0x09:
            userdata[instance]->external.alarm_minutes = data;
            break;

        // Alarm Hours
        case 0x0a:
            userdata[instance]->external.alarm_hours = data & 0xbf;
            break;

        // Alarm Day/Date
        case 0x0b:
           userdata[instance]->external.alarm_day_date = data;
            break;

        // Watchdog
        case 0x0c:
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.watchdog_ms = data;
            break;

        // Watchdog
        case 0x0d:
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.watchdog_s = data;
            break;

        // Control A
        case 0x0e:
            userdata[instance]->control_a = data;

            if (userdata[instance]->control_a & KSF_mask)
            {
                    userdata[instance]->control_a &= ~PAB_mask;

                    if ( userdata[instance]->control_b & KIE_mask)
                    {
                        userdata[instance]->control_a |= IRQF_mask;
                        oric->cpu.irq = 1;
                    }
            }
            break;

        // Contorl B
        case 0x0f:
            userdata[instance]->control_b = data;
            break;

        // RAM Addres
        case 0x10:
            userdata[instance]->ram_address = data;
            break;

        // Reserved
        case 0x11:
        case 0x12:
            break;

        // RAM Data
        case 0x13:
            if (userdata[instance]->control_b & BME_mask)
                userdata[instance]->ram[userdata[instance]->ram_address++] = data;

            else
                userdata[instance]->ram[userdata[instance]->ram_address] = data;

            break;

        // Reserved
        case 0x14 ... 0x1f:
            break;

        default:
            dbg_printf("DS1501 WRITE: bad address $%04x\n", addr);
            return SDL_FALSE;
    }

    // Transfert vers external
    if (userdata[instance]->control_b & TE_mask)
    {
        userdata[instance]->internal.seconds = bcd2bin(userdata[instance]->external.seconds);
        userdata[instance]->internal.minutes = bcd2bin(userdata[instance]->external.minutes);
        userdata[instance]->internal.hours = bcd2bin(userdata[instance]->external.hours);
        userdata[instance]->internal.day = bcd2bin(userdata[instance]->external.day);
        userdata[instance]->internal.date = bcd2bin(userdata[instance]->external.date);

        userdata[instance]->internal.month = (userdata[instance]->external.month & 0x0e) | bcd2bin(userdata[instance]->external.month & 0x1f);
        userdata[instance]->internal.year = bcd2bin(userdata[instance]->external.year);
        userdata[instance]->internal.century = bcd2bin(userdata[instance]->external.century);
    }

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                              Horloge
    // -------------------------------------------------------------------------
void plugin_ticktock(struct machine *oric, unsigned int instance, int cycles)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    if ( (cycles == 0) || (userdata[plugin_instances]->internal.month & EOSC_mask) )
        return;

    userdata[instance]->clock_us -= cycles;

    // 10ms
    if (userdata[instance]->clock_us <= 0)
    {
        // 10 ms
        userdata[instance]->clock_us += 10000;

        userdata[instance]->clock_ms--;

        // Décrémenter le Watchdog
        if (!userdata[instance]->internal.watchdog_ms)
        {
            userdata[instance]->internal.watchdog_ms = 99;

            // Si watchdog_s est à 0, on repart à 100 -1
            if (!userdata[instance]->internal.watchdog_s )
                userdata[instance]->internal.watchdog_s = 100;

            userdata[instance]->internal.watchdog_s--;
        }
        else
            userdata[instance]->internal.watchdog_ms--;

        // Time out?
        if (( userdata[instance]->internal.watchdog_ms == 0) && (userdata[instance]->internal.watchdog_s) )
        {
            userdata[instance]->control_a |= WDF_mask;

            if (userdata[instance]->control_b & WDE_mask)
            {
                if (userdata[instance]->control_b & WDS_mask)
                {
                    dbg_printf("DS1501: reset système\n");

                    userdata[instance]->control_b &= ~WDE_mask;
                    // oric->cpu-reset = 1;
                }
                else
                {
                    dbg_printf("DS1501: fire IRQ\n");

                    userdata[instance]->internal.watchdog_ms = userdata[instance]->internal_watchdog_ms;
                    userdata[instance]->internal.watchdog_s = userdata[instance]->internal_watchdog_s;

                    oric->cpu.irq = 1;
                }

                userdata[instance]->control_a |= IRQF_mask;
            }
        }

        // 1s
        if (userdata[instance]->clock_ms <= 0)
        {
            // 1s
            userdata[instance]->clock_ms += 100;

            // Mise à jour de l'horloge
            userdata[instance]->internal.seconds++;

            if (userdata[instance]->internal.seconds == 60)
            {
                userdata[instance]->internal.seconds = 0;
                userdata[instance]->internal.minutes++;

                if (userdata[instance]->internal.minutes == 60)
                {
                    userdata[instance]->internal.minutes = 0;
                    userdata[instance]->internal.hours++;

                    if (userdata[instance]->internal.hours == 24)
                    {
                        userdata[instance]->internal.hours = 0;

                        // Day of Week
                        userdata[instance]->internal.day = ((userdata[instance]->internal.day) % 7)+1;

                        // Date
                        userdata[instance]->internal.date += 1;

                        unsigned char month_flags = userdata[instance]->internal.month & 0xe0;
                        userdata[instance]->internal.month = userdata[instance]->internal.month & 0x1f;

                        switch  (userdata[instance]->internal.date)
                        {
                            case 29:
                                if (userdata[instance]->internal.month == 2)
                                    if ( (userdata[instance]->internal.century * 100 + userdata[instance]->internal.year) % 4 )
                                    {
                                        userdata[instance]->internal.date = 1;
                                        userdata[instance]->internal.month++;
                                    }
                                break;

                            case 30:
                                if (userdata[instance]->internal.month == 2)
                                {
                                    userdata[instance]->internal.date = 1;
                                    userdata[instance]->internal.month++;
                                }
                                break;

                            case 31:
                                switch(userdata[instance]->internal.month)
                                {
                                    case 2:
                                    case 4:
                                    case 6:
                                    case 9:
                                    case 11:
                                        userdata[instance]->internal.date = 1;
                                        userdata[instance]->internal.month++;
                                }
                                break;

                            case 32:
                                userdata[instance]->internal.date = 1;
                                userdata[instance]->internal.month++;
                                break;
                        }

                        if (userdata[instance]->internal.month == 13)
                        {
                            userdata[instance]->internal.month = 1;
                            userdata[instance]->internal.year++;

                            if (userdata[instance]->internal.year == 100)
                            {
                                userdata[instance]->internal.year = 0;
                                userdata[instance]->internal.century = (++userdata[instance]->internal.century) % 40;
                            }
                        }
                        userdata[instance]->internal.month |= month_flags;
                    }
                }
            }

            // Transfert vers external
            if (userdata[instance]->control_b & TE_mask)
            {
                userdata[instance]->external.seconds = bin2bcd(userdata[instance]->internal.seconds);
                userdata[instance]->external.minutes = bin2bcd(userdata[instance]->internal.minutes);
                userdata[instance]->external.hours = bin2bcd(userdata[instance]->internal.hours);
                userdata[instance]->external.day = bin2bcd(userdata[instance]->internal.day);
                userdata[instance]->external.date = bin2bcd(userdata[instance]->internal.date);

                userdata[instance]->external.month = (userdata[instance]->internal.month & 0x0e) | bin2bcd(userdata[instance]->internal.month & 0x1f);
                userdata[instance]->external.year = bin2bcd(userdata[instance]->internal.year);
                userdata[instance]->external.century = bin2bcd(userdata[instance]->internal.century);

            }
            // Gestion de l'alarme
            unsigned char AMx = ((userdata[instance]->internal.alarm_day_date & AM4_mask) >> 4) | ((userdata[instance]->internal.alarm_hours & AM3_mask) >> 5) | ((userdata[instance]->internal.alarm_minutes & AM2_mask) >> 6) | ((userdata[instance]->internal.alarm_seconds & AM1_mask) >> 7);

            SDL_bool alarm = SDL_TRUE;

            switch (AMx)
            {
                case 0x00:
                    // When date, hours, minutes and seconds match
                    if (userdata[instance]->internal.alarm_day_date & DYDT_mask)
                        alarm &= (userdata[instance]->internal.date == (userdata[instance]->internal.alarm_day_date & 0x3f));

                    // When day, hours, minutes and seconds match
                    else
                        alarm &= (userdata[instance]->internal.day == (userdata[instance]->internal.alarm_day_date & 0x3f));

                // When hours, minutes and seconds match
                case 0x08:
                    alarm &= (userdata[instance]->internal.hours == userdata[instance]->internal.alarm_hours);

                // When minutes and seconds match
                case 0x0c:
                    alarm &= (userdata[instance]->internal.minutes == userdata[instance]->internal.alarm_minutes);

                // When seconds match
                case 0x0e:
                    alarm &= (userdata[instance]->internal.seconds == userdata[instance]->internal.alarm_seconds);
                    break;

                 // Once per second
                case 0x0f:
                    break;

               default:
                    dbg_printf("DS1501: AMx invalid %02X\n", AMx);
            }

            if (alarm)
            {
                userdata[instance]->control_a |= TDF_mask;

                if (userdata[instance]->control_b & TIE_mask)
                {
                    userdata[instance]->control_a |= IRQF_mask;
                    oric->cpu.irq = 1;
                }

                if (userdata[instance]->control_b & TPE_mask)
                {
                    userdata[instance]->control_a &= ~PAB_mask;
                    dbg_printf("DS1501: TPE -> -RST", AMx);
                }
            }
        }
    }
}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
void mon_plugin_update(struct textzone *tz, unsigned int instance, unsigned short base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    int i, line;

    dbg_printf("DS1501: mon update\n");

    my_tzprintfpos( tz, 2, 2,  "Base address : %04X", base_addr);

    // Trait de séparation en ligne 4
    tz->px = 0;
    tz->py = 4;
    my_tzputc( tz, 6 );

    for (i=0; i < tz->w-2; i++)
//        my_tzputc( tz, 2 );
        my_tzputc( tz, 12 );

    my_tzputc( tz, 8 );

    line = 5;
    my_tzprintfpos( tz, 2, line++,  "Control A : $%02X %%", userdata[instance]->control_a);
    for( i=128; i; i>>=1 )
      my_tzputc(tz, (userdata[instance]->control_a & i)?'1':'0');

    my_tzprintfpos( tz, 2, line++,  "Control B : $%02X %%", userdata[instance]->control_b);
    for( i=128; i; i>>=1 )
      my_tzputc(tz, (userdata[instance]->control_b & i)?'1':'0');

    line++;
    my_tzprintfpos( tz, 2, line++,  "Date : %02X/%02X/%02X%02X", userdata[instance]->external.date, userdata[instance]->external.month & 0x1f, userdata[instance]->external.century, userdata[instance]->external.year);
    my_tzprintfpos( tz, 2, line++,  "Hour : %02X:%02X:%02X", userdata[instance]->external.hours, userdata[instance]->external.minutes, userdata[instance]->external.seconds);

    line++;
    my_tzprintfpos( tz, 2, line++,  "Watchdog : %02X.%02X", userdata[instance]->external.watchdog_s, userdata[instance]->external.watchdog_ms);

    // Internals
    line++;
    my_tzprintfpos( tz, 2, line++,  "Date I: %02d/%02d/%04d", userdata[instance]->internal.date, userdata[instance]->internal.month & 0x1f, userdata[instance]->internal.century*100 + userdata[instance]->internal.year);
    my_tzprintfpos( tz, 2, line++,  "Hour I: %02d:%02d:%02d", userdata[instance]->internal.hours, userdata[instance]->internal.minutes, userdata[instance]->internal.seconds);

    line++;
    my_tzprintfpos( tz, 2, line++,  "Watchdog I: %02d.%02d", userdata[instance]->internal_watchdog_s, userdata[instance]->internal_watchdog_ms);

    line++;
    my_tzprintfpos( tz, 2, line++,  "Clock ms: %d", userdata[instance]->clock_ms);
    my_tzprintfpos( tz, 2, line++,  "Clock_us: %d", userdata[instance]->clock_us);


    if (oldvalid)
    {
        // Date
        if (userdata[instance]->external.date != userdata_old[instance]->external.date)
            mon_periphmod( 9, 8, 2, tz );

        if (userdata[instance]->external.month != userdata_old[instance]->external.month)
            mon_periphmod( 12, 8, 2, tz );

        if (userdata[instance]->external.century != userdata_old[instance]->external.century)
            mon_periphmod( 15, 8, 2, tz );

        if (userdata[instance]->external.year != userdata_old[instance]->external.year)
            mon_periphmod( 17, 8, 2, tz );

        // Hour
        if (userdata[instance]->external.hours != userdata_old[instance]->external.hours)
            mon_periphmod( 9, 8, 2, tz );

        if (userdata[instance]->external.minutes != userdata_old[instance]->external.minutes)
            mon_periphmod( 12, 8, 2, tz );

        if (userdata[instance]->external.seconds != userdata_old[instance]->external.seconds)
            mon_periphmod( 15, 9, 2, tz );



        // Date
        if (userdata[instance]->internal.date != userdata_old[instance]->internal.date)
            mon_periphmod( 10, 13, 2, tz );

        if (userdata[instance]->internal.month != userdata_old[instance]->internal.month)
            mon_periphmod( 13, 13, 2, tz );

        if (userdata[instance]->internal.century != userdata_old[instance]->internal.century)
            mon_periphmod( 16, 13, 2, tz );

        if (userdata[instance]->internal.year != userdata_old[instance]->internal.year)
            mon_periphmod( 18, 13, 2, tz );

        // Hour
        if (userdata[instance]->internal.hours != userdata_old[instance]->internal.hours)
            mon_periphmod( 10, 14, 2, tz );

        if (userdata[instance]->internal.minutes != userdata_old[instance]->internal.minutes)
            mon_periphmod( 13, 14, 2, tz );

        if (userdata[instance]->internal.seconds != userdata_old[instance]->internal.seconds)
            mon_periphmod( 16, 14, 2, tz );

    }

}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
void mon_plugin_store(struct machine *oric, unsigned int instance)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data+ptr
    memcpy(userdata_old[instance], userdata[instance], sizeof(struct DS1501));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
unsigned char bin2bcd(unsigned char value)
{
    if (value >= 200)
        value -= 200;

    if (value >= 100)
        value -= 100;

    return ( ((value / 10) << 4) + (value % 10));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "DS1601",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                plugin_create,
                plugin_shutdown,
                plugin_reset,
                plugin_read,
                plugin_write,
                plugin_ticktock,
                mon_plugin_update,
                mon_plugin_store,
    };

