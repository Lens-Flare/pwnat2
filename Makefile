# Commands
CC = gcc
LD = gcc

# Flags
override LFLAGS := -lcrypto $(LFLAGS)
override CFLAGS := -O2 --std=gnu11 -Wall -Wno-unknown-pragmas $(CFLAGS)

# Misc
UNAME = $(shell uname)

# Targets etc
COMMON = network common
TARGETS = consumer server provider

# Build Directories
BUILD_DIR = build
TARGET_DIR = $(BUILD_DIR)/target
OBJECT_DIR = $(BUILD_DIR)/object

# Objects
COMMON_OBJS = $(foreach dir,$(COMMON),$(patsubst %.c,$(OBJECT_DIR)/%.o,$(wildcard $(dir)/*.c)))
CONSUMER_OBJS = $(patsubst %.c,$(OBJECT_DIR)/%.o,$(wildcard consumer/*.c))
SERVER_OBJS = $(patsubst %.c,$(OBJECT_DIR)/%.o,$(wildcard server/*.c))
PROVIDER_OBJS = $(patsubst %.c,$(OBJECT_DIR)/%.o,$(wildcard provider/*.c))


# Make all of the build directories
$(shell mkdir $(TARGET_DIR) 2> /dev/null)
$(shell mkdir $(OBJECT_DIR) 2> /dev/null)
$(foreach dir,$(COMMON),$(shell mkdir $(OBJECT_DIR)/$(dir) 2> /dev/null))
$(foreach dir,$(TARGETS),$(shell mkdir $(OBJECT_DIR)/$(dir) 2> /dev/null))


# Some targets don't create files
.PHONY : all clean dirs


# Build all targets
all : $(foreach tgt,$(TARGETS),$(TARGET_DIR)/$(tgt))

# Build all with debugging symbols
debug :
	$(MAKE) CFLAGS=-g


# Link Consumer
$(TARGET_DIR)/consumer : $(COMMON_OBJS) $(CONSUMER_OBJS)
	$(LD) $(LFLAGS) $(COMMON_OBJS) $(CONSUMER_OBJS) -o $@

# Link Server
$(TARGET_DIR)/server : $(COMMON_OBJS) $(SERVER_OBJS)
	$(LD) $(LFLAGS) $(COMMON_OBJS) $(SERVER_OBJS) -o $@

# Link Provider
$(TARGET_DIR)/provider : $(COMMON_OBJS) $(PROVIDER_OBJS)
	$(LD) $(LFLAGS) $(COMMON_OBJS) $(PROVIDER_OBJS) -o $@


# Compile source
$(OBJECT_DIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@


# Clean up the build directories
clean :
	-rm -rf $(TARGET_DIR) $(OBJECT_DIR) 2> /dev/null

# Install the executables
install : all

# Uninstall the executables
uninstall :