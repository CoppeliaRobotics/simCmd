# LuaCommander plugin for CoppeliaSim

Lua REPL (read-eval-print loop) functionality for CoppeliaSim. It adds a text input to the CoppeliaSim main window, which allows entering and executing Lua code on the fly, like in a terminal.

Requires CoppeliaSim version 3.5.1 or greater.

![Screenshot](LuaCommander.gif)

### Compiling

1. Install required packages for simStubsGen: see simStubsGen's [README](https://github.com/CoppeliaRobotics/include/blob/master/simStubsGen/README.md)
2. Download and install Qt (same version as CoppeliaSim, i.e. 5.9)
3. Checkout, compile and install into CoppeliaSim:
```sh
$ git clone https://github.com/CoppeliaRobotics/simExtLuaCmd.git
$ cd simExtLuaCmd
$ git checkout coppeliasim-v4.5.0-rev0
$ mkdir -p build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ cmake --build .
$ cmake --install .
```

NOTE: replace `coppeliasim-v4.5.0-rev0` with the actual CoppeliaSim version you have.
