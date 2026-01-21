CC ?= gcc
AR ?= ar
CFLAGS := -std=c11 -O2 -Wall -Wextra -D_GNU_SOURCE -fPIC -I3rdparty/
PREFIX ?= /usr/local
GIT ?= /usr/bin/git

# Library objects
SHARED_LIBS = libsplinter.so libsplinter_p.so
STATIC_LIBS = libsplinter.a libsplinter_p.a
SHARED_HEADERS = splinter.h

# Good enough for now (if the CLI gets appreciably large, it needs a directory)
CLI_SOURCES := $(shell echo splinter_cli_*.c) 3rdparty/linenoise.c 3rdparty/libgrawk.c
CLI_HEADERS := splinter_cli.h 3rdparty/linenoise.h

# Helpers & tests
BIN_PROGS = splinter_stress splinter_cli splinterp_stress splinterp_cli
TESTS = splinter_test splinterp_test

# Default target
all: $(SHARED_LIBS) $(STATIC_LIBS) $(BIN_PROGS) $(TESTS)

# Object build
splinter.o: splinter.c splinter.h build.h
	$(CC) $(CFLAGS) -c splinter.c -o $@

# Memory-backed shared object
libsplinter.so: splinter.o
	$(CC) -shared -Wl,-soname,libsplinter.so -o $@ $^

# Memory-backed static library
libsplinter.a: splinter.o
	$(AR) rcs $@ $^

# Persistent-mode object and shared object
splinter_p.o: splinter.c splinter.h
	$(CC) $(CFLAGS) -DSPLINTER_PERSISTENT -c splinter.c -o $@

libsplinter_p.so: splinter_p.o
	$(CC) -shared -Wl,-soname,libsplinter_p.so -o $@ $^

# Persistent-mode static library
libsplinter_p.a: splinter_p.o
	$(AR) rcs $@ $^

# Test binary (uses memory-backed .so by default)
splinter_test: splinter_test.c splinter.o
	$(CC) $(CFLAGS) -o $@ splinter_test.c splinter.o

# Persistent version of the test binary
splinterp_test: splinter_test.c splinter_p.o
	$(CC) $(CFLAGS) -DSPLINTER_PERSISTENT -o $@ splinter_test.c splinter_p.o

# Useful for actually testing libsplinter performance
splinter_stress: splinter_stress.c splinter.o
	$(CC) $(CFLAGS) -o $@ splinter_stress.c splinter.o

# Persistent version of the performance test (here be dragons!)
splinterp_stress: splinter_stress.c splinter_p.o
	$(CC) $(CFLAGS) -DSPLINTER_PERSISTENT -o $@ splinter_stress.c splinter_p.o

# A basic, but extensible CLI to navigate and manage stores
splinter_cli: $(CLI_SOURCES) $(CLI_HEADERS) splinter.o splinter.h
	$(CC) $(CFLAGS) -o $@ $(CLI_SOURCES) splinter.o

# Persistent version of the CLI (soon I hope to have only one)
splinterp_cli: $(CLI_SOURCES) $(CLI_HEADERS) splinter_p.o splinter.h
	$(CC) $(CFLAGS) -DSPLINTER_PERSISTENT -o $@ $(CLI_SOURCES) splinter_p.o

# Better experience if non-root tries root targets
.PHONY: be_root

be_root:
	@if [ "$$(id -u)" != "0" ]; then \
		echo "Error: Try that again, but as root." >&2; \
		/bin/false >/dev/null 2>&1; \
	fi

# Install artifacts
.PHONY: install

install: $(SHARED_LIBS) $(STATIC_LIBS) $(BIN_PROGS) be_root
	install -m 0755 $(SHARED_LIBS) $(PREFIX)/lib/
	install -m 0644 $(STATIC_LIBS) $(PREFIX)/lib/
	install -m 0755 $(BIN_PROGS) $(PREFIX)/bin/
	ln -s -f $(PREFIX)/bin/splinter_cli $(PREFIX)/bin/splinterctl
	ln -s -f $(PREFIX)/bin/splinterp_cli $(PREFIX)/bin/splinterpctl
	install -m 0644 $(SHARED_HEADERS) $(PREFIX)/include/
	ldconfig

# Un-install artifacts
.PHONY: uninstall

uninstall: be_root
	rm -f $(PREFIX)/lib/$(SHARED_LIBS)
	rm -f $(PREFIX)/lib/$(STATIC_LIBS)
	rm -f $(PREFIX)/bin/$(BIN_PROGS)
	rm -f $(PREFIX)/bin/splinterctl
	rm -f $(PREFIX)/bin/splinterpctl
	rm -f $(PREFIX)/include/$(SHARED_HEADERS)
	ldconfig

# Clean artifacts
.PHONY: clean

clean:
	rm -f $(BIN_PROGS) $(SHARED_LIBS) $(STATIC_LIBS) $(TESTS)
	rm -f *-tap-test
	rm -f mrsw_test_*
	rm -f *.o

# Clean artifacts and bindings
.PHONY: distclean

distclean:
	@$(GIT) clean -fdx || false

# Tests
.PHONY: tests test valtest

tests: splinter_test splinter_stress splinterp_test splinterp_stress
	./splinter_test
	./splinterp_test
	@echo ""
	@echo "You can run ./splinter_stress / splinterp_stress to run stress tests."
	@echo "See ./splinter_stress --help for more."
	@echo ""
	@echo "You can/should also run tests under valgrind if you have it installed."
	@echo "Enable via HAVE_VALGRIND_H in config.h if you haven't already."

test: splinter_test splinterp_test splinter_stress splinterp_stress 
	@echo ""
	@echo "Use 'make tests' to run Splinter's unit tests without Valgrind."
	@echo "Use 'make valtest' to run Splinter's unit tests under Valgrind."
	@echo ""
	@echo "Use './splinter_stress or ./splinterp_stress to run torture tests,"
	@echo "for in-memory and persistent stores respectively. Note that integrity"
	@echo "failures are generally *expected* in this test due to highly-abnormal"
	@echo "use; think of it like a break test."
	@echo ""
	@echo "If you have valgrind/valgrind.h installed, edit 'config.h' to enable"
	@echo "tighter test-Valgrind integration." 

valtest: splinter_test splinterp_test
	valgrind -s --leak-check=full ./splinter_test || false
	valgrind -s --leak-check=full ./splinterp_test || false

# Everything
.PHONY: world

world: all valtest
