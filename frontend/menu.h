void menu_init(void);
void menu_prepare_emu(void);
void menu_loop(void);
void menu_finish(void);

void menu_notify_mode_change(int w, int h, int bpp);

enum opts {
	OPT_SHOWFPS = 1 << 0,
	OPT_SHOWCPU = 1 << 1,
	OPT_NO_FRAMELIM = 1 << 2,
};

extern int g_opts;
