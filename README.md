# LuaCommander plugin for CoppeliaSim

Lua REPL (read-eval-print loop) functionality for CoppeliaSim. It adds a text input to the CoppeliaSim main window, which allows entering and executing Lua code on the fly, like in a terminal.

Requires CoppeliaSim version 3.5.1 or greater.

![Screenshot](LuaCommander.gif)

### Compiling

1. Install required packages for [libPlugin](https://github.com/CoppeliaRobotics/libPlugin): see libPlugin's README
2. Download and install Qt (same version as CoppeliaSim, i.e. 5.9)
3. Checkout and compile
```
$ git clone --recursive https://github.com/CoppeliaRobotics/simExtLuaCmd.git
$ cmake .
$ cmake --build .
```
you may need to set the `CMAKE_PREFIX_PATH` environment variable to the `lib/cmake` subdirectory of your Qt installation, i.e. `/path/to/Qt//5.9.2/<platform>/lib/cmake`
