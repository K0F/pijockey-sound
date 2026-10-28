
CC=gcc

CFLAGS =-Wall
CFLAGS+=-Wextra
CFLAGS+=-Wwrite-strings
CFLAGS+=-Wpointer-arith
CFLAGS+=-Wfloat-equal
CFLAGS+=-Wcast-align
ifeq (yes, $(DEBUG))
  CFLAGS+=-g
else
  CFLAGS+=-O2 -s
  #CFLAGS+=-DNDEBUG #prevent warning in firmware...
endif

INCLUDE =-I/opt/vc/include
INCLUDE+=-I/opt/vc/include/interface/vcos/pthreads
# vcos/pthreads is obsoluted. this path for old firmware.
INCLUDE+=-I/opt/vc/include/interface/vmcs_host/linux
INCLUDE+=-I/usr/include
INCLUDE+=-I/usr/include/interface/vcos/pthreads
INCLUDE+=-I/usr/include/interface/vmcs_host/linux
INCLUDE+=-I/usr/include/libdrm
LDFLAGS=-L/opt/vc/lib -L/usr/lib -L/usr/lib/aarch64-linux-gnu -L/usr/lib/arm-linux-gnueabihf

HAS_BCM_HOST := $(shell test -f /usr/include/bcm_host.h -o -f /opt/vc/include/bcm_host.h && echo 1 || echo 0)

ifeq ($(HAS_BCM_HOST),1)
LIBS =-lbcm_host
LIBS+=-lEGL
LIBS+=-lGLESv2
LIBS+=-ldrm
LIBS+=-lgbm
LIBS+=-lX11
LIBS+=-lm
else
INCLUDE+=-Icompat
SOURCES+=bcm_host_compat.c
LIBS =-lEGL
LIBS+=-lGLESv2
LIBS+=-ldrm
LIBS+=-lgbm
LIBS+=-lX11
LIBS+=-lm
endif

TARGET=pj

SOURCES+=video_drm.c
SOURCES+=main.c
SOURCES+=pj.c
SOURCES+=video.c
SOURCES+=video_egl.c
SOURCES+=graphics.c

OBJECTS=$(subst .c,.o, $(SOURCES))


all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

clean:
	rm -f *~
	rm -f $(OBJECTS) $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

gen: depend

depend:
	$(CC) -MM -w $(INCLUDE) $(SOURCES) > depend.inc

include depend.inc
