#pragma once

#ifndef LUISA_COMPUTE_EMBREE_VERSION
#if __has_include(<embree4/rtcore.h>)
#define LUISA_COMPUTE_EMBREE_VERSION 4
#elif __has_include(<embree3/rtcore.h>)
#define LUISA_COMPUTE_EMBREE_VERSION 3
#else
#error "Embree not found."
#endif
#endif

#if LUISA_COMPUTE_EMBREE_VERSION == 3
#include <embree3/rtcore.h>
#else
#include <embree4/rtcore.h>
#endif
