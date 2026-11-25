// Gears of War 2 for PS5 - the precompiled header of the recompiled game code.
//
// generated/tu6/gow2_pch.h as the code generator writes it (the generated tree
// stays byte for byte what the PC build compiles), with the one thing that
// differs on the PS5 set after it.
#pragma once

#include "gow2_pch.h"

// The 0xE0000000 range (the Xbox 360's physical memory with 4 KB pages) starts
// 4 KB into the physical memory it mirrors. Where host memory is mapped in
// units bigger than 4 KB the view can't start there, so the runtime maps it
// from the start and adds the 4 KB to every address instead
// (rex::memory::detail::PhysicalHostOffset, E0HostOffset). The generated
// header adds nothing off Windows (COGBox patch 110 maps the view in place on
// Windows; Linux has 4 KB pages); the PS5 has 16 KB pages and 64 KB direct
// memory units and needs the 4 KB in code. The two must agree, or the game and
// the kernel calls it makes read and write 4 KB apart. (The Linux test build
// keeps the generated header's 0.)
#include <rex/platform.h>
#if REX_PLATFORM_PS5
#undef REX_PHYS_HOST_OFFSET
#define REX_PHYS_HOST_OFFSET(addr) (((u32)(addr) >= 0xE0000000u) ? 0x1000u : 0u)
#endif
#if !REX_PLATFORM_PS5
// The Linux test build: the offset the runtime chose (0x1000 when
// REX_TEST_HOST_PAGE_SIZE=16384 makes it work in 16 KB pages as on the console,
// else 0), read from a variable set before any game code runs.
extern "C" unsigned gow2_test_e0_host_offset;
#undef REX_PHYS_HOST_OFFSET
#define REX_PHYS_HOST_OFFSET(addr) (((u32)(addr) >= 0xE0000000u) ? gow2_test_e0_host_offset : 0u)
#endif
