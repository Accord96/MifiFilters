#pragma once

#include <fltkernel.h>
#include <ntddk.h>
#include <ntstrsafe.h>

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

#define TRACE( Format, ... ) DbgPrintEx(0, 0, (#Format"\r\n"), ##__VA_ARGS__);

#include "utils/memory.hpp"
#include "utils/registry.hpp"

#include "filter/settings.hpp"
#include "filter/logger.hpp"
#include "filter/callback.hpp"