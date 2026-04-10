# Shim Makefile - CMake now handles the build for the most part.
# You probably want to run ./configure --help if you're looking at this.

# We use git to clean 
GIT := /bin/git

prod:
	@$(MAKE) -C build

install:
	@$(MAKE) -C build install

tests:
	@cd build && ctest --output-on-failure

clean:
	@$(MAKE) -C build clean

distclean:
	@$(GIT) clean -fdx || false
