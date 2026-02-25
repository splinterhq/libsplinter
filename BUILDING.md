# Building Splinter

See the main Makefile, which contains the cmake options in the primary
build target. 

To install, use:

sudo -E make install

(note the -E, especially if you build rust bindings)

The best thing to do is view the Makefile first, and you'll see what 
the build options are. The `Makefile` in the repo root takes away the
extra CMake keystrokes, but it needs customized based on how you want
to build.
 