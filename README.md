# LuaCommander plugin for V-REP

Lua REPL (read-eval-print loop) functionality for V-REP. It adds a text input to the V-REP main window, which allows entering and executing Lua code on the fly, like in a terminal.

Requires V-REP version 3.5.1 or greater.

![Screenshot](LuaCommander.gif)

### Compiling

1. Install required packages for [v_repStubsGen](https://github.com/CoppeliaRobotics/v_repStubsGen): see v_repStubsGen's [README](external/v_repStubsGen/README.md)
2. Download and install Qt (same version as V-REP, i.e. 5.9)
3. Checkout and compile
```
$ git clone --recursive https://github.com/CoppeliaRobotics/v_repExtLuaCommander.git
$ cmake .
$ cmake --build .
```
you may need to set the `CMAKE_PREFIX_PATH` environment variable to the `lib/cmake` subdirectory of your Qt installation, i.e. `/path/to/Qt//5.9.2/<platform>/lib/cmake`
