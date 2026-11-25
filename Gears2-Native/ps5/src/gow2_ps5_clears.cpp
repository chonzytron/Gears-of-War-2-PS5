// Gears of War 2 for PS5 - Direct3D's clears without the 4x MSAA trick (ps5_full_res_clears).
//
// The Xbox 360 Direct3D (D3D::ClearRect, under every clear of part of a surface) splits the rectangle in two: its
// edges, cleared as they are, and the part aligned to EDRAM tiles, cleared over the same memory as 4x MSAA at half the
// width and height - four times fewer pixels on the console, where switching costs nothing. Here the 1x and 4x views
// of that memory are separate images, and each switch is a copy of the whole surface each way: in Chapter 1 the depth
// buffer went to 4x and back six times a frame and the shadow depth five, 6 ms of the GPU's ~21 ms a frame on the
// Linux test build (the clear draws: vertex shader 1E6883FC, a 3-vertex rectangle, no pixel shader, MSAA 4x).
//
// With ps5_full_res_clears on, the whole rectangle takes Direct3D's own path for rectangles too small to split: one
// D3D::ClearRects of the rectangle at the surface's own MSAA. The same pixels get the same values, with no 4x.
//
// The functions are the XDK's (Gears 2 TU6 has no symbol names): matched against Dead Space 2's PDB-named
// D3D::ClearRect (0x82235EB8 there; the same prologue, the same three ClearRects calls, the same stack arguments) -
// this is the Dead Space 2 PS5 port's ps5_lowres_clears with Gears 2's addresses.
//
//   D3D::ClearRect  = sub_829A26E8 (device r3, left r4, top r5, right r6, bottom r7, colour r8, Z f1,
//                     GPU_SURFACEINFO r10, then on the caller's stack at +84 GPU_COLORINFO, +92 / +108 the rest)
//   D3D::ClearRects = sub_829A23F8 (device r3, count r4, rectangles r5, colour r6, Z f1, GPU_SURFACEINFO r8,
//                     GPU_COLORINFO r9, r10 and +84 two of the rest), as ClearRect itself calls it.
//
// GPL-3.0 (the PS5 layer).
#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_BOOL(ps5_full_res_clears, false, "PS5",
                    "Direct3D's clears at the surface's own MSAA, not the middle at 4x over the 1x memory as on the "
                    "Xbox 360 (each of those is a copy of the surface each way here)");

namespace {

uint32_t LoadGuest32(const uint8_t* base, uint32_t address) {
  uint32_t v;
  std::memcpy(&v, base + address, 4);
  return __builtin_bswap32(v);
}

void StoreGuest32(uint8_t* base, uint32_t address, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(base + address, &v, 4);
}

std::atomic<uint64_t> clears_seen{0};

}  // namespace

extern "C" void __imp__sub_829A23F8(PPCContext& __restrict ctx, uint8_t* base);  // D3D::ClearRects
extern "C" void __imp__sub_829A26E8(PPCContext& __restrict ctx, uint8_t* base);  // D3D::ClearRect

extern "C" void sub_829A26E8(PPCContext& __restrict ctx, uint8_t* base) {
  static const bool full = REXCVAR_GET(ps5_full_res_clears);
  if (!full) {
    __imp__sub_829A26E8(ctx, base);
    return;
  }
  if (clears_seen.fetch_add(1, std::memory_order_relaxed) == 0) {
    REXLOG_INFO("ps5 clears: Direct3D's clears at the surface's own MSAA (ps5_full_res_clears)");
  }
  const uint32_t caller_sp = ctx.r1.u32;
  const uint32_t color_info = LoadGuest32(base, caller_sp + 84);
  const uint32_t rest_92 = LoadGuest32(base, caller_sp + 92);
  const uint32_t rest_108 = LoadGuest32(base, caller_sp + 108);
  // A frame of ClearRect's own size for the call: back chain, the rectangle at +96, ClearRects' stack argument at +84.
  const uint32_t sp = caller_sp - 288;
  StoreGuest32(base, sp, caller_sp);
  StoreGuest32(base, sp + 96, ctx.r4.u32);
  StoreGuest32(base, sp + 100, ctx.r5.u32);
  StoreGuest32(base, sp + 104, ctx.r6.u32);
  StoreGuest32(base, sp + 108, ctx.r7.u32);
  StoreGuest32(base, sp + 84, rest_108);
  const uint32_t surface_info = ctx.r10.u32, color = ctx.r8.u32;
  ctx.r1.u64 = sp;
  ctx.r4.u64 = 1;
  ctx.r5.u64 = sp + 96;
  ctx.r6.u64 = color;
  ctx.r8.u64 = surface_info;
  ctx.r9.u64 = color_info;
  ctx.r10.u64 = rest_92;
  __imp__sub_829A23F8(ctx, base);
  ctx.r1.u64 = caller_sp;
}
