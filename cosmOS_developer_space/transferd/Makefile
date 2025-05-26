# Makefile for transferd (Cross-compilation for Luckfox Pico)

LUCKFOX_SDK_PATH ?= /home/felix/docker-shared/cosmOS-rv1106
SDK_PATH = $(LUCKFOX_SDK_PATH)
LIBC_TYPE = uclibc

ifeq ($(LIBC_TYPE), uclibc)
	TOOLCHAIN_BASE_NAME = arm-rockchip830-linux-uclibcgnueabihf
	COMPILER_PATH = $(SDK_PATH)/tools/linux/toolchain/$(TOOLCHAIN_BASE_NAME)/bin
	CC = $(COMPILER_PATH)/$(TOOLCHAIN_BASE_NAME)-gcc
	CXX = $(COMPILER_PATH)/$(TOOLCHAIN_BASE_NAME)-g++
	STRIP = $(COMPILER_PATH)/$(TOOLCHAIN_BASE_NAME)-strip
else ifeq ($(LIBC_TYPE), glibc)
	ifndef GLIBC_COMPILER
		$(error GLIBC_COMPILER environment variable is not set for LIBC_TYPE=glibc)
	endif
	CC = $(GLIBC_COMPILER)
	# Assuming CXX and STRIP would be similarly named or configured if glibc is fully supported
	CXX = $(GLIBC_COMPILER_PATH)/g++ # Adjust if GLIBC_COMPILER is just the gcc path
	STRIP = $(GLIBC_COMPILER_PATH)/strip # Adjust
else
	$(error Unsupported LIBC_TYPE: $(LIBC_TYPE))
endif

# Compiler flags
# Add any necessary sysroot, march, mfloat-abi flags if not handled by the toolchain's specs file
CFLAGS = -Wall -g -Iinclude 
LDFLAGS =

# Executable name
TARGET = transferd

# Source directories
SRC_DIR = src
SOURCES_SUBDIR = $(SRC_DIR)/sources

# Source files
SRCS = $(SRC_DIR)/config.c \
	   $(SRC_DIR)/daemon.c \
	   $(SRC_DIR)/log.c \
	   $(SRC_DIR)/transferd.c \
	   $(SOURCES_SUBDIR)/gpio.c \
	   $(SOURCES_SUBDIR)/yolo.c

# Object files: replace .c with .o and keep the path
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Rule to link the target executable
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Executable $(TARGET) created with $(LIBC_TYPE) toolchain."
	@echo "Compiler: $(CC)"
	# Optionally strip the executable for smaller size on embedded target
	$(STRIP) $@

# Rule to compile .c files in src/ to .o files in src/
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to compile .c files in src/sources/ to .o files in src/sources/
$(SOURCES_SUBDIR)/%.o: $(SOURCES_SUBDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean rule
clean:
	rm -f $(TARGET) $(OBJS)
	@echo "Cleaned up object files and executable."

# Phony targets
.PHONY: all clean

# Test print variables
print_vars:
	@echo "SDK_PATH: $(SDK_PATH)"
	@echo "LIBC_TYPE: $(LIBC_TYPE)"
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"