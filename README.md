rjson
=====

A Playground-specific Binary JSON converter to normal JSON + beautifier.

There's used to be a [same program](https://auahdark.tumblr.com/post/139597445645/sif-internals-the-mystery-of-the-json-files) but the source code is no longer found (actually I lost the source code), so this program re-creates the functionality.

Compile
-----

### MSVC
```
cl /Ferjson.exe /O2 program.cpp JSonParser\*.c
```

### Clang
```
clang -O2 -o rjson program.cpp JSonParser/*.c
```

Add `.exe` suffix for `-o` on Windows.

### GCC
```
gcc -O2 -o rjson program.cpp JSonParser/*.c
```

Add `.exe` suffix for `-o` on Windows.

License
-----

* `program.cpp` - MIT License.
* yajl - ISC License, modified by KLab.
