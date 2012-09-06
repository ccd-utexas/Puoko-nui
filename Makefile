CAMERA_TYPE := NONE
#CAMERA_TYPE := PVCAM
#CAMERA_TYPE := PICAM
GUI_TYPE := FLTK

CC       = gcc
CXX      = g++
CFLAGS   = -g -c -Wall -Wno-unknown-pragmas -pedantic -Dlinux --std=c99 -D_GNU_SOURCE
CXXFLAGS = -g -Wall -Wno-unknown-pragmas -pedantic
LFLAGS   = -lcfitsio -lpthread -lftdi -lm
OBJS     = main.o camera.o timer.o preferences.o imagehandler.o platform.o

ifeq ($(CAMERA_TYPE),PVCAM)
	CFLAGS += -DUSE_PVCAM
	OBJS += camera_pvcam.o

    ifeq ($(MSYSTEM),MINGW32)
        CFLAGS += -Ic:/Program\ Files/Princeton\ Instruments/PVCAM/SDK/
        LFLAGS += -Lc:/Program\ Files/Princeton\ Instruments/PVCAM/SDK/ -lPvcam32
    else
        CFLAGS += -Ipvcam
        LFLAGS += -lpvcam -lraw1394 -ldl
    endif
endif

ifeq ($(CAMERA_TYPE),PICAM)
	CFLAGS += -DUSE_PICAM
	OBJS += camera_picam.o

    ifeq ($(MSYSTEM),MINGW32)
        CFLAGS += -Ic:/Program\ Files/Princeton\ Instruments/Picam/Includes
        LFLAGS += -Lc:/Program\ Files/Princeton\ Instruments/Picam/Libraries -lPicam
    else
        CFLAGS += -I/usr/local/picam/includes `apr-1-config --cflags --cppflags --includes`
        LFLAGS += -lpicam `apr-1-config --link-ld --libs`
    endif
endif

ifeq ($(GUI_TYPE),FLTK)
    CFLAGS += -D USE_FLTK_GUI
    CXXFLAGS += $(shell fltk-config --cxxflags )
    LFLAGS += $(shell fltk-config --ldflags )
    OBJS += gui_fltk.o
else
    LFLAGS += -lpanel -lncurses
    OBJS += gui_ncurses.o
endif

ifeq ($(MSYSTEM),MINGW32)
    CFLAGS += -DWIN32 -I/usr/local/include -Icfitsio/include -Incurses/include -Incurses/include/ncurses -Iftdi/include
    LFLAGS += -L/usr/local/lib -Lcfitsio/lib -Lncurses/lib -Lftdi/lib
endif

puokonui : $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

clean:
	-rm $(OBJS) puokonui

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $<

%.o : %.c
	$(CC) -c $(CFLAGS) $<
