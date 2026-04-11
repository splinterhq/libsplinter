# Shim Makefile - CMake now handles the build for most part.
# You probably want to run ./configure --help if you're looking at this.

# We use git to clean 
GIT := /bin/git

# Check if build directory exists
BUILD_DIR_EXISTS := $(shell test -d build && echo yes)

define check_build_dir
	@if [ ! -d build ]; then \
		echo "Error: build/ directory not found."; \
		echo "Please run './configure --help' to see configuration options."; \
		exit 1; \
	fi
endef

prod:
	$(check_build_dir)
	@$(MAKE) -C build

install:
	$(check_build_dir)
	@$(MAKE) -C build install

tests:
	$(check_build_dir)
	@cd build && ctest --output-on-failure

clean:
	$(check_build_dir)
	@$(MAKE) -C build clean

distclean:
	@$(GIT) clean -fdx || false
