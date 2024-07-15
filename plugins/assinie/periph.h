#define PERIPH_NAME_LEN 10

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

SDL_bool periph_add(struct machine *oric, struct PLUGIN *plugin, char *name, unsigned short addr_start, SDL_bool enable);

SDL_bool periph_del(char *name);

SDL_bool periph_enable(char *name);

SDL_bool periph_disable(char *name);

SDL_bool periph_init_by_name(struct machine *oric, char *name);

SDL_bool periph_init_all(struct machine *oric);

unsigned char periph_read(struct machine *oric, unsigned short addr);

SDL_bool periph_write(struct machine *oric, unsigned short addr, unsigned char data);

int periph_find_by_name(char *name);

int periph_find_by_addr(unsigned short addr);

SDL_bool periph_present(unsigned short addr);
SDL_bool periph_enabled_by_id(int id);
SDL_bool mon_periph_enabled_by_id(int id);

void periph_list();

void periph_display(int i);


unsigned char periph_mon_read(struct machine *oric, unsigned short addr);

void mon_update_periph( struct machine *oric, int id );
int mon_periph_count();
void toggleperiph( struct machine *oric, struct osdmenuitem *mitem, int id );
void shut_periph(struct machine *oric);
SDL_bool periph_shut_by_id(struct machine *oric, int id);
SDL_bool periph_reset_all(struct machine *oric);
void mon_store_state_periph(struct machine *oric, SDL_bool oldvalid);
void mon_periph_oldvalid(SDL_bool oldvalid);
void mon_periphmod( int x, int y, int w, struct textzone *vtz );

SDL_bool periph_test(struct machine *oric);

