# Shim Makefile - CMake now handles the build for the most part.

# We use git to clean 
GIT := /bin/git

.PHONY: all prod nerd dev mini tiny clean distclean install uninstall setup_build tests help

help:
	@echo "Build targets:"
	@echo "----------------------------------"
	@echo "dev       | \"nerd\" + Valgrind"
	@echo "nerd      | \"prod\" + Llama + Rust"
	@echo "prod      | Numa + Lua + Embeddings"
	@echo "mini      | Embeddings only"
	@echo "tiny      | No Embeddings (KV Only)"
	@echo "clean     | Clean build artifacts"
	@echo "distclean | Purge repo"
	@echo "install   | Install Splinter"
	@echo "uninstall | Uninstall Splinter"
	@echo ""
	@echo "You can also run CMake manually (edit this Makefile to see how)"
	@echo ""

setup_build:
	@mkdir -p build

dev: setup_build
	cd build && cmake -DWITH_EMBEDDINGS=ON -DWITH_LLAMA=ON -DWITH_LUA=ON -DWITH_NUMA=ON -DWITH_RUST=ON -DWITH_VALGRIND=ON ..
	@$(MAKE) -C build

nerd: setup_build
	cd build && cmake -DWITH_EMBEDDINGS=ON -DWITH_LLAMA=ON -DWITH_LUA=ON -DWITH_NUMA=ON -DWITH_RUST=ON ..
	@$(MAKE) -C build

prod: setup_build
	cd build && cmake -DWITH_EMBEDDINGS=ON -DWITH_LUA=ON -DWITH_NUMA=ON ..
	@$(MAKE) -C build

mini: setup_build
	cd build && cmake -DWITH_EMBEDDINGS=ON ..
	@$(MAKE) -C build

tiny: setup_build
	cd build && cmake .. 
	@$(MAKE) -C build 

all: help

install:
	@$(MAKE) -C build install

uninstall:
	@$(MAKE) -C build uninstall

tests:
	@cd build && ctest --output-on-failure

clean:
	@$(MAKE) -C build clean
	@rm -f bind_watch bind_watch.c

distclean:
	@$(GIT) clean -fdx || false
