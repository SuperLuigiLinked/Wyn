> [!WARNING]
> These libraries are still early in development and subject to change frequently. Use at your own risk!

# Wyn

A collection of C Libraries to ease cross-platform game development.

Currently supports Windows, Linux, and MacOS, compiled with Clang + CMake.

On Windows, can be compiled MSVC, C11 minimum required.

---

## Wyn
Windowing library.
#### Backends:
* **Windows**: Win32
* **Linux**: Xlib
* **MacOS**: Cocoa

## Wyt
Threading/Timing library.
#### Backends:
* **Windows**: Win32
* **Linux**: Pthreads
* **MacOS**: Pthreads (+ libdispatch)

---
