######## Android Makefile --Tiger King

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

# This is the target being built.
LOCAL_MODULE := libtigerpcsx

# All of the source files that we will compile.

LOCAL_SRC_FILES := \
	libpcsxcore/cdriso.c \
	libpcsxcore/cdrom.c \
	libpcsxcore/cheat.c \
	libpcsxcore/debug.c \
	libpcsxcore/decode_xa.c \
	libpcsxcore/disr3000a.c \
	libpcsxcore/gte.c \
	libpcsxcore/mdec.c \
	libpcsxcore/misc.c \
	libpcsxcore/plugins.c \
	libpcsxcore/ppf.c \
	libpcsxcore/psxbios.c \
	libpcsxcore/psxcommon.c \
	libpcsxcore/psxcounters.c \
	libpcsxcore/psxdma.c \
	libpcsxcore/psxhle.c \
	libpcsxcore/psxhw.c \
	libpcsxcore/psxinterpreter.c \
	libpcsxcore/psxmem.c \
	libpcsxcore/r3000a.c \
	libpcsxcore/sio.c \
	libpcsxcore/socket.c \
	libpcsxcore/spu.c \
	libpcsxcore/new_dynarec/new_dynarec.c \
	libpcsxcore/new_dynarec/linkage_arm.S \
	libpcsxcore/new_dynarec/pcsxmem.c \
	libpcsxcore/new_dynarec/emu_if.c \
	gpuAPI/newGPU/newGPU.cpp 	      \
	gpuAPI/newGPU/fixed.cpp         \
	gpuAPI/newGPU/core_Command.cpp  \
	gpuAPI/newGPU/core_Dma.cpp      \
	gpuAPI/newGPU/core_Draw.cpp     \
	gpuAPI/newGPU/core_Misc.cpp     \
	gpuAPI/newGPU/raster_Sprite.cpp \
	gpuAPI/newGPU/raster_Poly.cpp   \
	gpuAPI/newGPU/raster_Line.cpp   \
	gpuAPI/newGPU/raster_Image.cpp  \
	gpuAPI/newGPU/inner.cpp         \
	gpuAPI/newGPU/ARM_asm.S         \
	plugins/dfsound/spu.c \
	plugins/dfsound/cfg.c \
	plugins/dfsound/dma.c \
	plugins/dfsound/freeze.c \
	plugins/dfsound/registers.c \
	gpuAPI/gpuAPI.cpp \
	android/minimal.c \
	android/android.c 


### TIGER_KING options
LOCAL_CFLAGS += \
	-DTIGER_KING	

### TIGER_KING END


# All of the shared libraries we link against.
LOCAL_LDLIBS := \
	-ldl \
	-llog \
	-lz \


# Also need the JNI headers.
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/android \
	$(LOCAL_PATH)/libpcsxcore \

# Compiler flags.
LOCAL_CFLAGS += -O2 -fomit-frame-pointer -DNDEBUG -DANDROID \
	-DANDROID_ARMV7 -DGP2X -DARM_ARCH -DINLINE="inline" \
	-DDYNAREC -DPSXREC
	
LOCAL_CFLAGS += -ffast-math
ifeq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_CFLAGS += -DARMv5_ONLY
endif

include $(BUILD_SHARED_LIBRARY)


#### END of Android.mk