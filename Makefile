# Comment below line to see actual linker and compiler flags while running makefile
.SILENT:

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Iinc
LDLIBS := -lpthread -lrt -lz -lbrotlienc -lssl -lcrypto

SRC_DIR := src
INC_DIR := inc
LIB_DIR := lib
BUILD_DIR := bld

# Build modes and flags
DEBUG_FLAGS := -O0 -g -Wformat=2 -Wconversion -Wimplicit-fallthrough -DDEBUG
RELEASE_FLAGS := -O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2
RELEASE_LDFLAGS := -s -Wl,-z,noexecstack -Wl,-z,defs -Wl,-z,nodump

# Library  directories
THREADPOOL_SOURCES	:= $(wildcard $(LIB_DIR)/threadpool/*.c)
HASHTALBE_SOURCES	:= $(wildcard $(LIB_DIR)/hashtable/*.c)
LOGGGER_SOURCES		:= $(wildcard $(LIB_DIR)/logger/*.c)

# Static libraries
LIB_THREADPOOL 	:= $(BUILD_DIR)/libthreadpool.a
LIB_HASHTABLE 	:= $(BUILD_DIR)/libhashtable.a
LIB_LOGGER 		:= $(BUILD_DIR)/liblogger.a

# Generated static libraries
LIB_NAMES := -lthreadpool -lhashtable -llogger

#Source Files
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)

# Object files
SRC_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))

# Final executable
TARGET := $(BUILD_DIR)/legion

# Default target
.PHONY: all
all: debug

# Debug build target
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: LDFLAGS += $(LDLIBS)
debug: $(TARGET)

# Release build target
.PHONY: release
release: CFLAGS += $(RELEASE_FLAGS)
release: LDFLAGS += $(RELEASE_LDFLAGS) $(LDLIBS)
release: $(TARGET)

# Build executable target
$(TARGET): $(LIB_THREADPOOL) $(LIB_HASHTABLE) $(LIB_LOGGER) $(SRC_OBJECTS)
	@echo "Linking executable $(TARGET)"
	$(CC) $(CFLAGS) $(SRC_OBJECTS) -L$(BUILD_DIR) $(LDFLAGS) $(LIB_NAMES) -o $(TARGET)

# Build static libraries
$(LIB_THREADPOOL): $(BUILD_DIR)/threadpool.o | $(BUILD_DIR)
	@echo "Creating static library $(LIB_THREADPOOL)"
	ar rcs $@ $<

$(LIB_HASHTABLE): $(BUILD_DIR)/hashtable.o | $(BUILD_DIR)
	@echo "Creating static library $(LIB_HASHTABLE)"
	ar rcs $@ $<

$(LIB_LOGGER): $(BUILD_DIR)/logger.o | $(BUILD_DIR)
	@echo "Creating static library $(LIB_LOGGER)"
	ar rcs $@ $<

# Compile library object files
$(BUILD_DIR)/threadpool.o: $(THREADPOOL_SOURCES) | $(BUILD_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hashtable.o: $(HASHTALBE_SOURCES) | $(BUILD_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/logger.o: $(LOGGGER_SOURCES) | $(BUILD_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Compile source object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create the build directory
$(BUILD_DIR):
	@echo "Creating build directory $(BUILD_DIR)"
	mkdir -p $(BUILD_DIR)

# Clean up build files
.PHONY: clean
clean:
	@echo "Cleaning up build files"
	rm -rf $(BUILD_DIR)

# Keeping this here. might use it in future
# RELEASE_C_FLAGS := -O2 -s -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
# RELEASE_LD_FLAGS :=-pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,defs -Wl,-z,nodump