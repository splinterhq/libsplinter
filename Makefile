# Shim Makefile - CMake now handles the build for the most part.

# We use git to clean 
GIT := /bin/git

.PHONY: all clean distclean install uninstall tests help

all: build/Makefile
	@$(MAKE) -C build

build/Makefile:
	@mkdir -p build
	@cd build && cmake -DWITH_NUMA=ON -DWITH_LUA=ON -DWITH_EMBEDDINGS=ON ..

install: all
	@$(MAKE) -C build install

uninstall:
	@$(MAKE) -C build uninstall

tests: all
	@cd build && ctest --output-on-failure

clean:
	@$(MAKE) -C build clean

distclean:
	@$(GIT) clean -fdx || false
