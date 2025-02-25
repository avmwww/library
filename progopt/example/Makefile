#
# Makefile for test progopt
#

CC=$(CROSS_COMPILE)gcc
STRIP=$(CROSS_COMPILE)strip

TARGET = test

SRCDIR = src
OBJDIR = obj
INCDIR = include

LIBRARY ?= ../..

PROGOPT=$(LIBRARY)/progopt
UTILS=$(LIBRARY)/utils

ifeq ($(findstring mingw,$(CROSS_COMPILE)),)
	TARGET_OS := Linux
else
	TARGET_OS := MINGW
endif

SRCS = test.c \
	progopt.c \

OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))

CFLAGS ?= $(C_FLAGS)
CFLAGS += -I$(INCDIR) -I$(PROGOPT) -I$(UTILS)
CFLAGS += -I$(INCDIR) -I.
CFLAGS += -Wall
CFLAGS += -D_GNU_SOURCE -g -Og

LDFLAGS ?= $(LD_FLAGS)

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@
#	$(STRIP) -s $@$(TARGET_POSTFIX)

clean:
	rm -rf $(TARGET) $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o : %.c $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

# VPATH
vpath %.c $(SRCDIR)
vpath %.h $(INCDIR)

vpath %.c $(PROGOPT) $(UTILS)

vpath %.o $(OBJDIR)

