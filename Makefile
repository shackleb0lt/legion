# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Iinc
LDFLAGS := -lpthread -lrt

# Build modes
DEBUG_FLAGS :=-g -O0 -Wformat=2 -Wconversion -Wimplicit-fallthrough

RELEASE_C_FLAGS := -O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2
RELEASE_LD_FLAGS := -s -Wl,-z,noexecstack -Wl,-z,defs -Wl,-z,nodump

# RELEASE_C_FLAGS := -O2 -s -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
# RELEASE_LD_FLAGS :=-pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,defs -Wl,-z,nodump

# Project structure
SRC_DIR := src
INC_DIR := inc
BUILD_DIR := bld

# Output executable name
TARGET := $(BUILD_DIR)/legion

# Find all source files
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
# Construct the list of object files
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))

# Default target
.PHONY: all
all: release

# Release build target
.PHONY: release
release: CFLAGS += $(RELEASE_C_FLAGS)
release: LDFLAGS += $(RELEASE_LD_FLAGS)
release: $(TARGET)

# Debug build target
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Build executable target
$(TARGET): $(OBJ_FILES) | $(BUILD_DIR)
	@echo "Generating executable ...."
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ_FILES) -o $(TARGET) 
	@echo "Build successful: $(TARGET)"

# Compile source files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling object file $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@echo "Creating Build Directory"
	@mkdir -p $@

# Clean up build files
.PHONY: clean
clean:
	@echo "Removing Build Directory and Contents"
	@rm -rf $(BUILD_DIR)
