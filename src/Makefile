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

CFLAGS_COMMON  ?= -fms-extensions
LDFLAGS_COMMON ?= -lm

CFLAGS         ?= $(CFLAGS_COMMON) -O2
CFLAGS_DEBUG   ?= $(CFLAGS_COMMON) -g
LDFLAGS        ?= $(LDFLAGS_COMMON) -s -O2
LDFLAGS_DEBUG  ?= $(LDFLAGS_COMMON) -g

.PHONY: all clean

all: $(MAIN) $(MAIN_DEBUG)

clean:
	rm $(OBJS) $(OBJS_DEBUG) $(MAIN) $(MAIN_DEBUG)

$(MAIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $(MAIN) $(OBJS)

$(MAIN_DEBUG): $(OBJS_DEBUG)
	$(CC) $(LDFLAGS_DEBUG) -o $(MAIN_DEBUG) $(OBJS_DEBUG)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -c $(@:.o=.c) -o $@

$(OBJS_DEBUG): $(SRCS)
	$(CC) $(CFLAGS_DEBUG) -c $(@:_d.o=.c) -o $@

