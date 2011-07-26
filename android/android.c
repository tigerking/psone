#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

//#include "sio.h"
#include "psxcommon.h"
#include "minimal.h"

//#include "../gpuAPI/gpuAPI.h"

void (*render_audio)(s16 *data, u32 size);
void (*draw_screen)(void *buf);


int SCREEN_W	= SCREEN_W_DEFAULT; //368; //320;
int SCREEN_H	= SCREEN_H_DEFAULT;//480; //;
int  SCREEN_PITCH=SCREEN_PITCH_DEFAULT ;//368*2;

#define TESTSIZE    24192

int g_key_states =0;

int enable_audio=1;
extern int             bEndThread;

u32 last_ticks;
u32 last_render_ticks;

static pthread_mutex_t sound_mutex;
static pthread_cond_t sound_cond;
pthread_t sound_thread=0;
volatile int sound_thread_exit=0;
unsigned int sound_buffer_bytes = 0;
unsigned int sound_new_buffer = 0;
static void *sound_thread_play(void *none);
static unsigned char sound_buffer[32768];
static unsigned char sound_buffer_silence[32768];

unsigned char screenBuffer[1024*1024*2];

int g_game_paused=0;


extern void (*GPU_updateLace)(void);
void GPUupdateLace(void);



#include "ndk_log.h"

#ifdef SysMessage
#undef SysMessage
#endif

#ifdef SysPrintf
#undef SysPrintf
#endif

extern PcsxConfig   Config;

int psxRunning=0;

void SysPrintf(
    const char*     fmt,
    ... )
{
#if 0
    va_list list;
    char    msg[512];

    va_start( list, fmt );
    vsprintf( msg, fmt, list );
    va_end( list );

    if ( Config.PsxOut )
    {
        static char linestart = 1;
        int         l = strlen( msg );

        printf( linestart ? " * %s" : "%s", msg );

        if ( l > 0 && msg[l - 1] == '\n' )
        {
            linestart = 1;
        }
        else
        {
            linestart = 0;
        }
    }

#ifdef EMU_LOG
#ifndef LOG_STDOUT
    fprintf( emuLog, "%s", msg );
#endif
#endif
#endif
}


void SysMessage(
    const char*     fmt,
    ... )
{
#if 0
    va_list list;
    char    msg[512];

    va_start( list, fmt );
    vsprintf( msg, fmt, list );
    va_end( list );

    if ( Config.PsxOut )
    {
        static char linestart = 1;
        int         l = strlen( msg );

        printf( linestart ? " * %s" : "%s", msg );

        if ( l > 0 && msg[l - 1] == '\n' )
        {
            linestart = 1;
        }
        else
        {
            linestart = 0;
        }
    }

#ifdef EMU_LOG
#ifndef LOG_STDOUT
    fprintf( emuLog, "%s", msg );
#endif
#endif
#endif
}


void* SysLoadLibrary( const char* lib )
{
    return dlopen( lib, RTLD_NOW );
}


void* SysLoadSym(
    void*           lib,
    const char*     sym )
{
    return dlsym( lib, sym );
}


const char* SysLibError( void )
{
    return dlerror();
}


void SysCloseLibrary( void* lib )
{
    dlclose( lib );
}


static void SysDisableScreenSaver( void )
{
#if 0
    static time_t           fake_key_timer = 0;
    static char             first_time = 1, has_test_ext = 0, t = 1;
    Display*                display;
    extern unsigned long    gpuDisp;

    display = (Display*)gpuDisp;

    if ( first_time )
    {
        // check if xtest is available
        int a, b, c, d;
        has_test_ext = XTestQueryExtension( display, &a, &b, &c, &d );

        first_time = 0;
    }

    if ( has_test_ext && fake_key_timer < time( NULL ) )
    {
        XTestFakeRelativeMotionEvent( display, t *= -1, 0, 0 );
        fake_key_timer = time( NULL ) + 55;
    }

#endif
}


void SysUpdate( void )
{
#if 0
    PADhandleKey( PAD1_keypressed() );
    PADhandleKey( PAD2_keypressed() );
    SysDisableScreenSaver();
#endif
}

/* ADB TODO Replace RunGui() with StartGui ()*/
void SysRunGui( void )
{
#if 0
    StartGui();
#endif
}


int SysInit( void )
{
#ifdef EMU_LOG
#ifndef LOG_STDOUT
    emuLog = fopen( "emuLog.txt", "wb" );
#else
    emuLog = stdout;
#endif
    setvbuf( emuLog, NULL, _IONBF, 0 );
#endif
    if ( EmuInit() == -1 )
    {
        printf( _( "PSX emulator couldn't be initialized.\n" ) );
        return -1;
    }

    LoadMcds( Config.Mcd1, Config.Mcd2 );   /* TODO Do we need to have this here, or in the calling main() function?? */

    if ( Config.Debug )
    {
        StartDebugger();
    }

    return 0;
}


static void dummy_lace( void )
{
}


void SysReset( void )
{
    LOG_ENTER();

    // rearmed hack: EmuReset() runs some code when real BIOS is used,
    // but we usually do reset from menu while GPU is not open yet,
    // so we need to prevent updateLace() call..
    void*   real_lace = GPUupdateLace;
    GPU_updateLace = dummy_lace;

    LOG_STEP();
    EmuReset();

    LOG_STEP();

    // hmh core forgets this
    CDR_stop();

    GPU_updateLace = real_lace;

    LOG_LEAVE();
}


void SysClose( void )
{
    EmuShutdown();
    ReleasePlugins();

    StopDebugger();

    if ( emuLog != NULL )
    {
        fclose( emuLog );
    }
}



/* Start Sound Core */
void SetupSound(void)
{
  LOG_STEP();
#ifndef NOSOUND
	if( enable_audio == 0 ) return;
#if 0
  last_ticks = 0;
  last_render_ticks = 0;
#endif
#if 0
  sound_buffer_bytes = 0;
  sound_new_buffer = 0;
  sound_thread_exit = 0;
	pthread_create( &sound_thread, NULL, sound_thread_play, NULL);
#else
  memset(sound_buffer_silence, 0, 882 * 2);
#endif
#endif
}

/* Stop Sound Core */
void RemoveSound(void)
{
#ifndef NOSOUND
	if( enable_audio == 0 ) return;
#if 0
  sound_thread_exit = 1;
  pthread_cond_signal(&sound_cond);
#endif
#endif
}

static void *sound_thread_play(void *none)
{
#ifndef NOSOUND
#if 0
	if( enable_audio == 0 ) return NULL;
	pthread_mutex_init(&sound_mutex, NULL);
	pthread_cond_init(&sound_cond, NULL);
  start_audio();
  while(!sound_thread_exit && !bEndThread)
  {
  	pthread_mutex_lock(&sound_mutex);
  	pthread_cond_wait(&sound_cond, &sound_mutex);
  	pthread_mutex_unlock(&sound_mutex);
  	if(pSpuBuffer)
      render_audio((s16*)pSpuBuffer, sound_buffer_bytes);
  }
  sound_thread_exit = 0;
  pthread_cond_destroy(&sound_cond);
  pthread_mutex_destroy(&sound_mutex);
  end_audio();
#endif
  return NULL;
#endif
}

void CheckPauseSound()
{
   extern int stop;

   while(is_paused() && (bEndThread == 0) && (!stop) )
  {
#ifndef TIGER_KING  
    usleep(20000);
    if(!render_audio) {
	    render_audio((s16*)sound_buffer_silence, 882 * 2);
    	}
#else
    LOGD("game is paused, audio thread waiting ..");
    sleep(1);
#endif
  }
   
   if(stop) {
   	LOGD("QUIT audio thread since game is quiting");
       bEndThread =1;
	return;
   }

}

/* Feed Sound Data */
void SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
    //LOG_ENTER();
#ifndef NOSOUND

    //LOGD("ENTER SoundFeedStreamData(%p, %d), enable_audio=%d", pSound, lBytes, enable_audio);

  //u32 render_ticks;
	if( enable_audio == 0 ) return;
  
  if(pSound == NULL || lBytes <= 0 || lBytes > 32768) // || sound_new_buffer != 0) 
  {
    //LOG_LEAVE();
    return;
  }

  //memcpy(sound_buffer, pSound, lBytes);
  //sound_buffer_bytes = lBytes;
  //pthread_cond_signal(&sound_cond);

#ifdef TIGER_KING
   extern int stop;
   if(!stop) {
	   render_audio((s16*)pSound, lBytes);
   }
#else
  render_audio((s16*)pSound, lBytes);
#endif
  
#if 0
	if(last_render_ticks == 0)
	{
	  last_render_ticks = timeGetTime_spu();
	}
	
  render_ticks = timeGetTime_spu();
	if(render_ticks < last_render_ticks + 5)
	{
    u32 render_delta = (last_render_ticks + 5) - render_ticks;
    if(render_delta < 5)
    {
      usleep(render_delta * 1000);
    }
	}
	last_render_ticks = timeGetTime_spu();
#endif
#endif

    //LOG_LEAVE();
}

unsigned long SoundGetBytesBuffered(void)
{
	if( enable_audio == 0 ) return TESTSIZE+1;
	
	if(last_ticks == 0)
	{
	  last_ticks = timeGetTime_spu();
	}
	
	if(timeGetTime_spu() >= last_ticks + 2)
	{
	  last_ticks = timeGetTime_spu();
         return 0;
	}
	
  return TESTSIZE+1;
}


extern void updateScreenSize(int w, int h, int bpp, void *screenPtr)
{

    SCREEN_W	= w;
    SCREEN_H	       = h;
    SCREEN_PITCH=SCREEN_W*bpp/8;

}

int get_key_states(){
	return g_key_states;
}


int is_paused(){
	return g_game_paused;
}

int check_paused(){
	while(g_game_paused) {
		sleep(1);
	}
}


void flip_screen()
{
     if(!draw_screen) draw_screen(screenBuffer);
}
