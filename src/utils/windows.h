//
// Created by nonun on 25. 1. 4.
//

#ifndef WINDOWS_H
#define WINDOWS_H

#ifdef _MSC_VER // Visual Studio
#define PACKED_STRUCT(name) \
	__pragma(pack(push, 1)) struct name __pragma(pack(pop))
#else // GCC, Clang
#define PACKED_STRUCT(name) struct __attribute__((packed)) name
#endif

#ifdef _WIN32
#include <windows.h>
#define sleep(seconds) Sleep((seconds) * 1000)
#endif

#endif //WINDOWS_H
