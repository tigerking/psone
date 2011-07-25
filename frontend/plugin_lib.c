/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "plugin_lib.h"
#include "linux/fbdev.h"
#include "common/fonts.h"
#include "common/input.h"
#include "omap.h"
#include "menu.h"
#include "pcnt.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"

void *pl_fbdev_buf;
int pl_frame_interval;
int keystate;
static int pl_fbdev_w, pl_fbdev_h, pl_fbdev_bpp;
static int flip_cnt, vsync_cnt, flips_per_sec, tick_per_sec;
static float vsps_cur;

static int get_cpu_ticks(void)
{
	static unsigned long last_utime;
	static int fd;
	unsigned long utime, ret;
	char buf[128];

	if (fd == 0)
		fd = open("/proc/self/stat", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	buf[0] = 0;
	read(fd, buf, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;

	sscanf(buf, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &utime);
	ret = utime - last_utime;
	last_utime = utime;
	return ret;
}

static void print_fps(void)
{
	if (pl_fbdev_bpp == 16)
		pl_text_out16(2, pl_fbdev_h - 10, "%2d %4.1f", flips_per_sec, vsps_cur);
}

static void print_cpu_usage(void)
{
	if (pl_fbdev_bpp == 16)
		pl_text_out16(pl_fbdev_w - 28, pl_fbdev_h - 10, "%3d", tick_per_sec);
}

void *pl_fbdev_set_mode(int w, int h, int bpp)
{
	void *ret;

	if (w == pl_fbdev_w && h == pl_fbdev_h && bpp == pl_fbdev_bpp)
		return pl_fbdev_buf;

	pl_fbdev_w = w;
	pl_fbdev_h = h;
	pl_fbdev_bpp = bpp;

	vout_fbdev_clear(layer_fb);
	ret = vout_fbdev_resize(layer_fb, w, h, bpp, 0, 0, 0, 0, 3);
	if (ret == NULL)
		fprintf(stderr, "failed to set mode\n");
	else
		pl_fbdev_buf = ret;

	menu_notify_mode_change(w, h, bpp);

	return pl_fbdev_buf;
}

void *pl_fbdev_flip(void)
{
	flip_cnt++;

	if (pl_fbdev_buf != NULL) {
		if (g_opts & OPT_SHOWFPS)
			print_fps();
		if (g_opts & OPT_SHOWCPU)
			print_cpu_usage();
	}

	// let's flip now
	pl_fbdev_buf = vout_fbdev_flip(layer_fb);
	return pl_fbdev_buf;
}

int pl_fbdev_open(void)
{
	pl_fbdev_buf = vout_fbdev_flip(layer_fb);
	omap_enable_layer(1);
	return 0;
}

void pl_fbdev_close(void)
{
	omap_enable_layer(0);
}

static void update_input(void)
{
	int actions[IN_BINDTYPE_COUNT] = { 0, };

	in_update(actions);
	if (actions[IN_BINDTYPE_EMU] & PEV_MENU)
		stop = 1;
	keystate = actions[IN_BINDTYPE_PLAYER12];

#ifdef X11
	extern int x11_update_keys(void);
	keystate |= x11_update_keys();
#endif
}

#define MAX_LAG_FRAMES 3

#define tvdiff(tv, tv_old) \
	((tv.tv_sec - tv_old.tv_sec) * 1000000 + tv.tv_usec - tv_old.tv_usec)
// assumes us < 1000000
#define tvadd(tv, us) { \
	tv.tv_usec += us; \
	if (tv.tv_usec >= 1000000) { \
		tv.tv_usec -= 1000000; \
		tv.tv_sec++; \
	} \
}

/* called on every vsync */
void pl_frame_limit(void)
{
	static struct timeval tv_old, tv_expect;
	static int vsync_cnt_prev;
	struct timeval now;
	int diff;

	vsync_cnt++;

	/* doing input here because the pad is polled
	 * thousands of times per frame for some reason */
	update_input();

	pcnt_end(PCNT_ALL);
	gettimeofday(&now, 0);

	if (now.tv_sec != tv_old.tv_sec) {
		diff = tvdiff(now, tv_old);
		vsps_cur = 0.0f;
		if (0 < diff && diff < 2000000)
			vsps_cur = 1000000.0f * (vsync_cnt - vsync_cnt_prev) / diff;
		vsync_cnt_prev = vsync_cnt;
		flips_per_sec = flip_cnt;
		flip_cnt = 0;
		tv_old = now;
		if (g_opts & OPT_SHOWCPU)
			tick_per_sec = get_cpu_ticks();
	}
#ifdef PCNT
	static int ya_vsync_count;
	if (++ya_vsync_count == PCNT_FRAMES) {
		pcnt_print(vsps_cur);
		ya_vsync_count = 0;
	}
#endif

	if (!(g_opts & OPT_NO_FRAMELIM)) {
		tvadd(tv_expect, pl_frame_interval);
		diff = tvdiff(tv_expect, now);
		if (diff > MAX_LAG_FRAMES * pl_frame_interval || diff < -MAX_LAG_FRAMES * pl_frame_interval) {
			//printf("pl_frame_limit reset, diff=%d, iv %d\n", diff, pl_frame_interval);
			tv_expect = now;
		}
		else if (diff > pl_frame_interval) {
			// yay for working usleep on pandora!
			//printf("usleep %d\n", diff - pl_frame_interval / 2);
			usleep(diff - pl_frame_interval / 2);
		}
	}

	pcnt_start(PCNT_ALL);
}

static void pl_text_out16_(int x, int y, const char *text)
{
	int i, l, len = strlen(text), w = pl_fbdev_w;
	unsigned short *screen = (unsigned short *)pl_fbdev_buf + x + y * w;
	unsigned short val = 0xffff;

	for (i = 0; i < len; i++, screen += 8)
	{
		for (l = 0; l < 8; l++)
		{
			unsigned char fd = fontdata8x8[text[i] * 8 + l];
			unsigned short *s = screen + l * w;
			if (fd&0x80) s[0] = val;
			if (fd&0x40) s[1] = val;
			if (fd&0x20) s[2] = val;
			if (fd&0x10) s[3] = val;
			if (fd&0x08) s[4] = val;
			if (fd&0x04) s[5] = val;
			if (fd&0x02) s[6] = val;
			if (fd&0x01) s[7] = val;
		}
	}
}

void pl_text_out16(int x, int y, const char *texto, ...)
{
	va_list args;
	char    buffer[256];

	va_start(args, texto);
	vsnprintf(buffer, sizeof(buffer), texto, args);
	va_end(args);

	pl_text_out16_(x, y, buffer);
}

static void pl_get_layer_pos(int *x, int *y, int *w, int *h)
{
	*x = g_layer_x;
	*y = g_layer_y;
	*w = g_layer_w;
	*h = g_layer_h;
}

extern int UseFrameSkip; // hmh

const struct rearmed_cbs pl_rearmed_cbs = {
	pl_get_layer_pos,
	pl_fbdev_open,
	pl_fbdev_set_mode,
	pl_fbdev_flip,
	pl_fbdev_close,
	&UseFrameSkip,
};

/* watchdog */
static void *watchdog_thread(void *unused)
{
	int vsync_cnt_old = 0;
	int seen_dead = 0;
	int sleep_time = 5;

	while (1)
	{
		sleep(sleep_time);

		if (stop) {
			seen_dead = 0;
			sleep_time = 5;
			continue;
		}
		if (vsync_cnt != vsync_cnt_old) {
			vsync_cnt_old = vsync_cnt;
			seen_dead = 0;
			sleep_time = 2;
			continue;
		}

		seen_dead++;
		sleep_time = 1;
		if (seen_dead > 1)
			fprintf(stderr, "watchdog: seen_dead %d\n", seen_dead);
		if (seen_dead > 4) {
			fprintf(stderr, "watchdog: lockup detected, aborting\n");
			// we can't do any cleanup here really, the main thread is
			// likely touching resources and would crash anyway
			abort();
		}
	}
}

void pl_start_watchdog(void)
{
	pthread_attr_t attr;
	pthread_t tid;
	int ret;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	ret = pthread_create(&tid, &attr, watchdog_thread, NULL);
	if (ret != 0)
		fprintf(stderr, "could not start watchdog: %d\n", ret);
}

