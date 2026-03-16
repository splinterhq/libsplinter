# Shim Makefile - CMake now handles the build for the most part.

# We use git to clean 
GIT := /bin/git

.PHONY: all prod stainless sense dev mini tiny clean distclean install uninstall setup_build tests help

help:
	@echo "Splinter's build is managed by CMake. This Makefile is a shim"
	@echo "to help people get started quickly. Any build configuration is"
	@echo "possible; here are targets that produce the most common"
	@echo "configurations:"
	@echo ""
	@echo "------------------------------------"	
	@echo "| Type 'make' + target_name to use |"
	@echo "------------------------------------"
	@echo "sense     | \"dev\" + Valgrind"
	@echo "dev       | \"prod\" + Llama + Rust"
	@echo "prod      | Numa + Lua + Embeddings"
	@echo "mini      | Embeddings only"
	@echo "tiny      | No Embeddings (KV Only)"
	@echo "clean     | Clean build artifacts"
	@echo "distclean | Purge repo"
	@echo "install   | Install Splinter"
	@echo "uninstall | Uninstall Splinter"
	@echo ""
	@echo "You can also run CMake manually, for example, if you wanted a "
	@echo "stripped-down build + Rust bindings for it. Edit this Makefile to see how."
	@echo ""
	@echo "If unsure where to start, try 'make mini' - it requires no dependencies and"
	@echo "enables many features."
	@echo ""

setup_build:
	@mkdir -p build

# "no rust :P" 
stainless: setup_build
	cd build && cmake -DWITH_EMBEDDINGS=ON -DWITH_LLAMA=ON -DWITH_LUA=ON -DWITH_NUMA=ON ..
	@$(MAKE) -C build

dev: setup_build
	cd build && cmake -DWITH_EMBEDDINGS=ON -DWITH_LLAMA=ON -DWITH_LUA=ON -DWITH_NUMA=ON -DWITH_RUST=ON ..
	@$(MAKE) -C build

sense: setup_build
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
