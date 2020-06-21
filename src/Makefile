SRCS = \
			 vgmck.c \
			 vgmck_2a03.c \
			 vgmck_ay8910.c \
			 vgmck_ay8930.c \
			 vgmck_dmg.c \
			 vgmck_huc6280.c \
			 vgmck_opl2.c \
			 vgmck_opl3.c \
			 vgmck_opl4.c \
			 vgmck_opll.c \
			 vgmck_opn2.c \
			 vgmck_pokey.c \
			 vgmck_qsound.c \
			 vgmck_sn76489.c \
			 vgmck_t6w28.c
SRCS_DEBUG = $(SRCS) vgmck_debug.c

OBJS        = $(SRCS:.c=.o)
OBJS_DEBUG  = $(SRCS:.c=_d.o)

MAIN       ?= vgmck
MAIN_DEBUG ?= vgmck_d

CC             = gcc

OSX_MINVERSION ?= 10.6

CFLAGS_COMMON  ?= -fms-extensions
LDFLAGS_COMMON ?= -lm

ifneq ($(OS),Windows_NT)
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		CFLAGS_COMMON  += -mmacosx-version-min=$(OSX_MINVERSION)
		LDFLAGS_COMMON += -mmacosx-version-min=$(OSX_MINVERSION)
	endif
endif

CFLAGS_EXTRA   ?=
LDFLAGS_EXTRA  ?=

CFLAGS         ?= $(CFLAGS_COMMON) $(CFLAGS_EXTRA) -O2
CFLAGS_DEBUG   ?= $(CFLAGS_COMMON) $(CFLAGS_EXTRA) -g
LDFLAGS        ?= $(LDFLAGS_COMMON) $(LDFLAGS_EXTRA) -s -O2
LDFLAGS_DEBUG  ?= $(LDFLAGS_COMMON) $(LDFLAGS_EXTRA) -g

.PHONY: all clean

all: $(MAIN) $(MAIN_DEBUG)

clean:
	rm $(OBJS) $(OBJS_DEBUG) $(MAIN) $(MAIN_DEBUG)

$(MAIN): $(OBJS)
	$(CC) -o $(MAIN) $(OBJS) $(LDFLAGS_DEBUG)

$(MAIN_DEBUG): $(OBJS_DEBUG)
	$(CC) -o $(MAIN_DEBUG) $(OBJS_DEBUG) $(LDFLAGS_DEBUG)

$(OBJS): $(SRCS)
	$(CC) -o $@ -c $(@:.o=.c) $(CFLAGS)

$(OBJS_DEBUG): $(SRCS)
	$(CC) -o $@ -c $(@:_d.o=.c) $(CFLAGS_DEBUG)

