#
# Makefile for test cli
#

CC=$(CROSS_COMPILE)gcc

TARGETS = cli_test

LIBRARY_DIR ?= ../..

SRCDIR = src
INCDIR = include

SELECTOR_DIR ?= $(LIBRARY_DIR)/selector
CLI_DIR ?= $(LIBRARY_DIR)/cli
UTILS_DIR ?= $(LIBRARY_DIR)/utils

SELECTOR_SRCS = net.c

SRCS_CLI = test.c \
	   selector.c \
	   cli.c

SRCS_CLI += $(SELECTOR_SRCS)

OBJS_CLI := $(SRCS_CLI:.c=.o)

OBJS_ALL := $(OBJS_CLI)

CFLAGS += -Wall
CFLAGS += -I$(INCDIR) -I$(CLI_DIR) -I$(SELECTOR_DIR) -I$(SELECTOR_DIR)/selector -I$(UTILS_DIR)
CFLAGS += -D_GNU_SOURCE -g -Og

#LDFLAGS += -lcrypt

all: $(TARGETS)

cli_test: $(OBJS_CLI)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(TARGETS) $(OBJS_ALL)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

vpath %.c $(SRCDIR) $(SELECTOR_DIR) $(SELECTOR_DIR)/selector $(CLI_DIR) $(UTILS_DIR)
vpath %.h $(INCDIR) $(SELECTOR_DIR) $(SELECTOR_DIR)/selector $(CLI_DIR) $(UTILS_DIR)

