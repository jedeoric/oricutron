#define PERIPH_NAME_LEN 10

struct PLUGIN {
    char name[PERIPH_NAME_LEN+1];
    Uint16 default_addr;
    Uint16 size;
    unsigned int (*create)(struct machine *oric);
    SDL_bool (*shutdown)(struct machine *oric, unsigned int instance);

    SDL_bool (*reset)(struct machine *oric, unsigned int instance);
    Uint8 (*read)(struct machine *oric, unsigned int instance, Uint16 addr, SDL_bool fexec);
    SDL_bool (*write)(struct machine *oric, unsigned int instance, Uint16 addr, Uint8 data);

    void (*ticktock)(struct machine *oric, unsigned int instance, int cycles);

    void (*mon_update)(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid);
    void (*mon_store_state)(struct machine *oric, unsigned int instance);
};

struct PLUGINS {
    Uint16 addr_start;
    Uint16 addr_end;
    unsigned int instance;
    SDL_bool enable;
    struct PLUGIN *periph;
};
