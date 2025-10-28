# SQLite Utils Library Makefile
# Compatible with Makefile.libs structure

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -fPIC -std=c99
LDFLAGS = -shared
LIBS = -lsqlite3
TARGET ?= ../../libs/libkylsqlite.so

# Source files
SOURCES = sqlite_utils.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = sqlite_utils.h

# Test files
TEST_SOURCE = test_sqlite.c
TEST_BINARY = test_sqlite

# Installation directories
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

.PHONY: all clean install uninstall test

all: $(TARGET)

# Build shared library
$(TARGET): $(OBJECTS)
	mkdir -p $(dir $(TARGET))
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build test program
test: $(TEST_BINARY)

$(TEST_BINARY): $(TEST_SOURCE) $(SHARED_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCE) -L. -lsqlite_utils $(LIBS)

# Run tests
check: test
	LD_LIBRARY_PATH=. ./$(TEST_BINARY)

# Install library and headers
install: $(TARGET)
	install -d $(LIBDIR)
	install -d $(INCLUDEDIR)
	install -m 644 $(TARGET) $(LIBDIR)
	install -m 644 $(HEADERS) $(INCLUDEDIR)
	ldconfig

# Uninstall library and headers
uninstall:
	rm -f $(LIBDIR)/libkylsqlite.so
	rm -f $(INCLUDEDIR)/sqlite_utils.h
	ldconfig

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_BINARY) *.db libsqlite_utils.so libsqlite_utils.a

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Example usage
example:
	@echo "Example usage:"
	@echo "  make all          - Build both shared and static libraries"
	@echo "  make test         - Build test program"
	@echo "  make check        - Run tests"
	@echo "  make install      - Install library system-wide"
	@echo "  make clean        - Clean build files"

# Package info
info:
	@echo "SQLite Utils Library"
	@echo "===================="
	@echo "Version: 1.0.0"
	@echo "Features:"
	@echo "  - Database connection management"
	@echo "  - SQL execution and prepared statements"
	@echo "  - Parameter binding and result handling"
	@echo "  - Transaction management with savepoints"
	@echo "  - Schema introspection"
	@echo "  - Database utilities (backup, vacuum, integrity)"
	@echo "  - Comprehensive error handling"
	@echo ""
	@echo "Dependencies:"
	@echo "  - SQLite3 library (libsqlite3-dev)"
	@echo ""
	@echo "Build targets:"
	@echo "  - $(SHARED_LIB) (shared library)"
	@echo "  - $(STATIC_LIB) (static library)"