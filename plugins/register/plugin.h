#define PERIPH_NAME_LEN 10

struct PERIPH {
    char osditem;
    char name[PERIPH_NAME_LEN+1];
    SDL_bool enable;
    unsigned short addr_start;
    unsigned short addr_end;
    SDL_bool (*reset)(struct machine *oric, void *userdata );
    SDL_bool (*shutdown)(struct machine *oric, void *userdata);
    unsigned char (*read)(struct machine *oric, void *userdata, unsigned short addr, SDL_bool fexec);
    SDL_bool (*write)(struct machine *oric, void *userdata, unsigned short addr, unsigned char data);
    void (*mon_update)(struct textzone *tz, void *userdata, unsigned short base_addr, SDL_bool oldvalid);
    void (*mon_store_state)(struct machine *oric, void *userdata);
    void *userdata;
};

struct PLUGIN {
    char name[PERIPH_NAME_LEN+1];
    unsigned short default_addr;
    unsigned short size;
    unsigned int (*create)(struct machine *oric);
    SDL_bool (*shutdown)(struct machine *oric, unsigned int instance);

    SDL_bool (*reset)(struct machine *oric, unsigned int instance);
    unsigned char (*read)(struct machine *oric, unsigned int instance, unsigned short addr, SDL_bool fexec);
    SDL_bool (*write)(struct machine *oric, unsigned int instance, unsigned short addr, unsigned char data);

    void (*mon_update)(struct textzone *tz, unsigned int instance, unsigned short base_addr, SDL_bool oldvalid);
    void (*mon_store_state)(struct machine *oric, unsigned int instance);
};

struct PLUGINS {
    unsigned short addr_start;
    unsigned short addr_end;
    unsigned int instance;
    SDL_bool enable;
    struct PLUGIN *periph;
};
