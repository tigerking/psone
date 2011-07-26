#include "newGPU.h"
#include "minimal.h"

#include "ndk_log.h"

static u64 lastFrameDrawnTime;
u16 bgr2rgb[65536];

#ifdef PANDORA
extern "C" void convert_screen_320_240_BGR555_640_480_RGB565(void* dest, void* src, u32 dest_pitch_bytes);
#endif

#ifdef ANDROID
//u32 bgr2rgb32[65536];
extern "C" int check_paused();
#endif

///////////////////////////////////////////////////////////////////////////////
#include "raster.h"
#include "inner_Blit.h"

///////////////////////////////////////////////////////////////////////////////
void gpuDrawInit(void)
{
	int r,g,b,i;
	
	i = 0;
	for(b = 0; b < 32; b++)
	{
		for(g = 0; g < 32; g++)
		{
			for(r = 0; r < 32; r++)
			{
				u16 pixbgr = (b<<10) | (g<<5) | (r);
				bgr2rgb[i] = ((pixbgr&(0x1f001f<<10))>>10) | ((pixbgr&(0x1f001f<<5))<<1) | ((pixbgr&(0x1f001f<<0))<<11);
				bgr2rgb[i+32768] = bgr2rgb[i];
				i++;
			}
		}
	}

  GPU_force_res_change = 1;
	lastFrameDrawnTime = 0;
  //drawn = 0;
}

///////////////////////////////////////////////////////////////////////////////
void  gpuDebugRect(u16 col, u32 x0, u32 y0, u32 x1, u32 y1)
{
  if(x0>x1 || y0>y1 || x1>1024 || y1>512)
    return;

  x0 = (x0* PSX4ALL_WIDTH)/1024;  x1 = (x1*PSX4ALL_WIDTH )/1024;
  y0 = (y0*PSX4ALL_HEIGHT)/ 512;  y1 = (y1*PSX4ALL_HEIGHT)/ 512;

  u16* dest_screen16 = gp2x_screen16;
  dest_screen16 += (y0*PSX4ALL_WIDTH);

  // top
  if(y0!=y1)
  {
    for(u32 x=x0;x<x1;++x)  dest_screen16[x] = col;
    dest_screen16 += PSX4ALL_WIDTH;
  }
  
  //  middle
  if(x1)  --x1;
  while(++y0<y1)
  {
    dest_screen16[x0] = dest_screen16[x1] = col;
    dest_screen16 += PSX4ALL_WIDTH;
  }
  ++x1;

  // bottom
  {
    for(u32 x=x0;x<x1;++x)  dest_screen16[x] = col;
  }
}
///////////////////////////////////////////////////////////////////////////////
void  gpuDisplayVideoMem()
{
#if !defined(PANDORA) && !defined(ANDROID)

  LOGD("TIGER: gpuDisplayVideoMem...");

	u16* dest_screen16 = gp2x_screen16;
  for(int y=0;y<240;++y)
  {
  	u16* src_screen16  = &((u16*)GPU_FrameBuffer)[FRAME_OFFSET(0,(y<<9)/PSX4ALL_HEIGHT)];
    GPU_BlitWSSWSSWSSWSSWSSS(	src_screen16, dest_screen16, false);
    dest_screen16 += PSX4ALL_WIDTH;
  }

  gpuDebugRect(0x00FF, DrawingArea[0],    DrawingArea[1],   DrawingArea[2],   DrawingArea[3]);
  gpuDebugRect(0xFF00, DisplayArea[0],    DisplayArea[1],   DisplayArea[0]+DisplayArea[2], DisplayArea[1]+DisplayArea[3]);
  gpuDebugRect(0x0FF0, DirtyArea[0],      DirtyArea[1],     DirtyArea[2],     DirtyArea[3]);
  gpuDebugRect(0xFFFF, LastDirtyArea[0],  LastDirtyArea[1], LastDirtyArea[2], LastDirtyArea[3]);

  gp2x_video_flip();
#endif
}

static s16 old_res_horz = 0, old_res_vert = 0, old_rgb24 = 0;
	
///////////////////////////////////////////////////////////////////////////////
void gpuVideoOutput(void)
{
	//LOG_STEP();

	s16 h0, x0, y0, w0, h1;


	x0 = DisplayArea[0];
	y0 = DisplayArea[1];

	w0 = DisplayArea[2];
	h0 = DisplayArea[3];  // video mode

	h1 = DisplayArea[7] - DisplayArea[5]; // display needed
	if (h0 == 480)
		h1 = Min2(h1*2,480);

	u32 isRGB24 = (GPU_GP1 & 0x00200000 ? 32 : 0);

	/* Clear the screen if resolution changed to prevent interlacing and clipping to clash */
	if( (w0 != old_res_horz || h1 != old_res_vert || (s16)isRGB24 != old_rgb24) || GPU_force_res_change != 0 )
	{
		// Update old resolution
		old_res_horz = w0;
		old_res_vert = h1;
		old_rgb24 = (s16)isRGB24;
    GPU_force_res_change = 0;
#ifdef PANDORA
#if defined(WITH_SDL_VIDEO)
    if(!isRGB24)
    {
      gp2x_change_res(w0, h1, 16);
    }
    else
    {
      gp2x_change_res(800, 480, 24);
      gp2x_video_RGB_clearscreen16();
    }
#else
    gp2x_change_res(w0, h1, isRGB24 ? 24 : 16);
#endif
#elif defined(IPHONE)
    gp2x_change_res(w0, h1, isRGB24 ? 32 : 16);
#elif defined(ANDROID)
    gp2x_change_res(w0, h1, 16);
#else
		// Finally, clear the screen for this special case
		gp2x_video_RGB_clearscreen16();
#endif
	}

	u16* dest_screen16 = gp2x_screen16;
	u16* src_screen16  = &((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)];

#if defined(PANDORA) || defined(IPHONE) || defined(ANDROID)
#if !defined(PANDORA) && !defined(IPHONE) && !defined(ANDROID)
  if(h0==256)
  {
  	h0 = 240;
  	src_screen16  += ((h1-h0)>>1)*1024;
  	h1 = h0;
  }
#endif
#else
	//  Height centering
  int sizeShift = 1;
  if(h0==256)
    h0 = 240;
  else
  if(h0==480)
    sizeShift = 2;

  if(h1>h0)
  {
		src_screen16  += ((h1-h0)>>sizeShift)*1024;
    h1 = h0;
  }
  else
  if(h1<h0)
    dest_screen16 += ((h0-h1)>>sizeShift)*PSX4ALL_WIDTH;

	/* Main blitter */
  int incY = (h0==480) ? 2 : 1;
#endif

#if defined(PANDORA)
if(!isRGB24)
{
  u32* dest_screen32 = (u32*)gp2x_screen16;
	u32* src_screen32  = (u32*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);
	
  int x, y;
  for(y = 0; y < h1; y++)
  {
    for(x = 0; x < w0>>1; x++)
    {
      const u32 pixbgr = src_screen32[x];
      *dest_screen32++ = ((pixbgr&(0x1f001f<<10))>>10) | ((pixbgr&(0x1f001f<<5))<<1) | ((pixbgr&(0x1f001f<<0))<<11);
    }
    src_screen32 += 512;
  }
}
else
{
  u8* dest_screen8 = (u8*)gp2x_screen16;
	u8* src_screen8  = (u8*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);
	
  int x, y;
  for(y = 0; y < h1; y++)
  {
    for(x = 0; x < w0 * 3; x += 3)
    {
      *dest_screen8++ = src_screen8[x+2];
      *dest_screen8++ = src_screen8[x+1];
      *dest_screen8++ = src_screen8[x+0];
      //*dest_screen8++ = 255;
    }
    src_screen8 += 2048;
  }  
}
#elif defined(IPHONE)
  if(!isRGB24)
  {
    u32* dest_screen32 = (u32*)gp2x_screen16;
    u32* src_screen32  = (u32*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);
    
    int x, y;
    for(y = 0; y < h1; y++)
    {
      for(x = 0; x < w0>>1; x++)
      {
        const u32 pixbgr = src_screen32[x];
        *dest_screen32++ = ((pixbgr&(0x1f001f<<10))>>10) | ((pixbgr&(0x1f001f<<5))<<1) | ((pixbgr&(0x1f001f<<0))<<11);
      }
      src_screen32 += 512;
    }
  }
  else
  {
    u32* dest_screen32 = (u32*)gp2x_screen16;
    u8* src_screen8  = (u8*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);
    
    int x, y;
    for(y = 0; y < h1; y++)
    {
      for(x = 0; x < w0 * 3; x += 3)
      {
        *dest_screen32++ = (255 << 24) | (src_screen8[x+0] << 16) | (src_screen8[x+1] << 8) | src_screen8[x+2];
      }
      src_screen8 += 2048;
    }  
  }
#elif defined(ANDROID)
  if(!isRGB24)
  {
    u32* dest_screen32 = (u32*)gp2x_screen16;
    u32* src_screen32  = (u32*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);

    int x, y;

    //LOGD("CK: isRGB24= false (dest_screen32=%p) (h1=%d, w0=%d w0>>1=%d)", dest_screen32, h1,w0, (w0>>1));
    for(y = 0; y < h1; y++)
    {
      for(x = 0; x < w0>>1; x++)
      {
        const u32 pixbgr = src_screen32[x];
        *dest_screen32++ = ((pixbgr&(0x1f001f<<10))>>10) | ((pixbgr&(0x1f001f<<5))<<1) | ((pixbgr&(0x1f001f<<0))<<11);
      }
      src_screen32 += 512;
    }
    //LOGD("CK2: dest_screen32=%p, offset=%d", dest_screen32, ((char*)dest_screen32-(char*)gp2x_screen16 ));
	
  }
  else
  {
    u8* src_screen8  = (u8*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);
    
    int x, y;

    //LOGD("CK3: isRGB24=true");	
    for(y = 0; y < h1; y++)
    {
      for(x = 0; x < w0 * 3; x += 3)
      {
        *dest_screen16++ = gp2x_video_RGB_color16( src_screen8[x+0], src_screen8[x+1], src_screen8[x+2] );
      }
      src_screen8 += 2048;
    }  
  }
#else
{
	void (*GPU_Blitter)(void* src, u16* dst16, u32 height);

	switch( w0 )
	{
	case 256:
			if(h1 <= 240)
			{
				GPU_Blitter = GPU_Blit_256_240_ROT90CW_2X;
			}
			else
			{
				GPU_Blitter = GPU_Blit_256_480_ROT90CW_2X;
			}
		break;
	case 368:
			if(h1 <= 240) 
			{
				GPU_Blitter = GPU_Blit_368_240_ROT90CW_2X;
			}
			else
			{
				GPU_Blitter = GPU_Blit_368_480_ROT90CW_2X;
			}
		break;
	case 320:
			if(h1 <= 240) 
			{
#ifdef PANDORA
			  if(!isRGB24)
			  {
          convert_screen_320_240_BGR555_640_480_RGB565(dest_screen16, src_screen16, PSX4ALL_PITCH);
          return;
        }
#endif
				GPU_Blitter = GPU_Blit_320_240_ROT90CW_2X;
			}
			else
			{
				GPU_Blitter = GPU_Blit_320_480_ROT90CW_2X;
			}
		break;
	case 384:
			if(h1 <= 240)
			{
				GPU_Blitter = GPU_Blit_384_240_ROT90CW_2X;
			}
			else
			{
				GPU_Blitter = GPU_Blit_384_480_ROT90CW_2X;
			}
		break;
	case 512:
			if(h1 <= 240)
			{
				GPU_Blitter = GPU_Blit_512_240_ROT90CW_2X;
			}
			else
			{
				GPU_Blitter = GPU_Blit_512_480_ROT90CW_2X;
			}
		break;
	case 640:
    	if(h1 <= 240)
    	{
    		GPU_Blitter = GPU_Blit_640_240_ROT90CW_2X;
    	}
    	else
    	{
    		GPU_Blitter = GPU_Blit_640_480_ROT90CW_2X;
    	}
		break;
	default: return;
	}
  /* Blit span */
	GPU_Blitter(src_screen16,	dest_screen16, h1);
}
#endif

  if(displayFrameInfo)
  {
    int ypos;
#ifdef ENABLE_GPU_PRIM_STATS
    int polis = statF3 + statFT3 + statG3 + statGT3;
    ypos = 10;
    gp2x_printf(NULL, 0,(ypos+=10),"PPF (%4d): PPS (%5d): ", polis,  (polis*realRate)/100 );
    gp2x_printf(NULL, 0,(ypos+=10),"types F(%4d) FT(%4d) G(%4d)  GT(%4d)", statF3, statFT3, statG3, statGT3);
#endif
#ifdef ENABLE_GPU_PROFILLER
    if(displayGpuStats)
    {
      ypos = 30;
      gp2x_printf(NULL, 0,(ypos+=10),"dmaChainTime (%5d): %04.4g%% (%3dms)", dmaChainCount,  PROFILE_RATIO((100*dmaChainTime ),curFrame), gp2x_timer_raw_to_ticks(dmaChainTime) );
      gp2x_printf(NULL, 0,(ypos+=10),"gpuPolyTime  (%5d): %04.4g%% (%3dms)", gpuPolyCount,   PROFILE_RATIO((100*gpuPolyTime  ),curFrame), gp2x_timer_raw_to_ticks(gpuPolyTime)  );
      gp2x_printf(NULL, 0,(ypos+=10),"gpuPixelTime (%5d): %04.4g%% (%3dms)", gpuPixelCount,  PROFILE_RATIO((100*gpuPixelTime ),curFrame), gp2x_timer_raw_to_ticks(gpuPixelTime) );
      gp2x_printf(NULL, 0,(ypos+=10),"gpuRasterTime(%5d): %04.4g%% (%3dms)", gpuRasterCount, PROFILE_RATIO((100*gpuRasterTime),curFrame), gp2x_timer_raw_to_ticks(gpuRasterTime));
      gp2x_printf(NULL, 0,(ypos+=10),"dmaMemTime   (%5d): %04.4g%% (%3dms)", dmaMemCount,    PROFILE_RATIO((100*dmaMemTime),   curFrame), gp2x_timer_raw_to_ticks(dmaMemTime)   );
    }
#endif
  }
  PROFILE_RESET(gpuPolyTime,gpuPolyCount);
  PROFILE_RESET(gpuRasterTime,gpuRasterCount);
  PROFILE_RESET(gpuPixelTime,gpuPixelCount);
  PROFILE_RESET(dmaChainTime,dmaChainCount);
  PROFILE_RESET(dmaMemTime,dmaMemCount);
  statF3 = statFT3 = statG3 = statGT3 = 0;
  statLF = statLG  = statS  = statT   = 0;

  //LOG_LEAVE();
}

///////////////////////////////////////////////////////////////////////////////
static int totalFrames =0;

extern "C" void  GPUupdateLace(void)
{
//#define FRAMESKIPS 5
//  static u32 frameskips[FRAMESKIPS] = {5,5,5,5,5};
//  static u32 frameskipper = 0;
  static u32 lastFPS = 0;
	u32 temp;
  u32 noSkip = 0;


#ifdef ANDROID
	check_paused();

	//LOGD("%d : GPUupdateLace, check_paused done", curFPS); //debug for reset issue. reset does not work yet.
#endif

	if(displayFrameInfo)
  {
    u32 curDraw = timeGetTime();
    //if(drawn)
    {
      curFPS++;
    }
/*
    if(curFPS > (Config.PsxType == 0 ? 60 : 50))
    {
      usleep(1000000);
    }
*/
    if(curDraw - lastFPSCheck >= 1000)
    {
      lastFPS = curFPS; // / BIAS_COUNTERS; // / (Config.PsxType == 0 ? 60 : 50);
      curFPS = 0;
      lastFPSCheck = timeGetTime();
    }
  }

//  if ( (GPU_GP1&0x08000000) ) // dma transfer NO update posible...
//  		return;

  DisplayCount[1] += (Config.PsxType == 0 ? 16714 : 20050); // DisplayCount[0];
	DrawingCount[2] &= 0xFF;
	DrawingCount[3] &= 0xFF;

	temp = timeGetTime() * 1000;
	if ((u32) (temp - DisplayCount[2]) > (u32) 2000000) {
		DisplayCount[1] = 0;
		DisplayCount[2] = temp;
	} else {
    u32 time_delta = temp - DisplayCount[2];
		if(time_delta < DisplayCount[1]) 
		{
      u32 wait_delta = DisplayCount[1] - time_delta;
		  if(wait_delta >= 100 && wait_delta < 100000)
		  {
        usleep(wait_delta);
        noSkip = 1;
		  }
			temp = timeGetTime() * 1000;
		}
		if (DisplayCount[1] > 5000000) {
			DisplayCount[1] = 0;
			DisplayCount[2] = temp;
		}
	}

  //drawn = 0;

	if(!noSkip)
	{
	  Skip = DrawingCount[2];	
  	if (DrawingCount[2]) {
  		DrawingCount[2]--;
  		Skip = DrawingCount[2];
  		return;
  	}
  	if (DrawingCount[0]) {
  		if (DisplayArea[3] > 240) {
  			if (DrawingCount[3]) {
  				DrawingCount[3]--;
  				return;
  			}
  			DrawingCount[3] = DrawingCount[1];
  		}
  	}
    DrawingCount[2] = DrawingCount[0];
	}
	else
	{
    Skip = 0;
	}

	u64 now = gp2x_timer_read();
	if (now - lastFrameDrawnTime < (Config.PsxType == 0 ? 16 : 20))
		return;
	lastFrameDrawnTime = now;

  
  if ( (GPU_GP1&0x00800000) ) // Display disabled
  		return;

	if(Skip == 0)
	{
	  //LOGD("CK111: gpuVideoOutput...");
	  gpuVideoOutput();
	  //LOGD("CK111: gpuVideoOutput done.");
	}

	//gpuVideoOutput();
	
  if(displayFrameInfo)
  {
	 gp2x_printf(NULL, 0, 0, "FPS: %d RES: %d x %d BITS: %d, Frame=%d", lastFPS, old_res_horz, old_res_vert, (old_rgb24 ? 24 : 16) , ++totalFrames);
  }

  //LOGD("call gp2x_video_flip...");
  gp2x_video_flip();
  //LOGD("call gp2x_video_flip done.");
}

