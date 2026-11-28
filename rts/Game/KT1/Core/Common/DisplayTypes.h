/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the id ranges of the families' typed display events (LUA-FREE H01). No engine include.
//
// A family's event is a plain struct in its own header, at most Core::SimHost::DISPLAY_TEXT bytes, with
//     static constexpr int32_t TYPE = Core::DisplayRange::<FAMILY> + n;
// The sim sends it with DisplayEvents::Send(event); the draw or audio side registers with DisplaySink::Listen<T>.
// Each family owns its range, so two port sessions never edit one list.

#ifndef KT1_CORE_DISPLAY_TYPES_H
#define KT1_CORE_DISPLAY_TYPES_H

#include <cstdint>

namespace KT1::Core::DisplayRange {

constexpr int32_t SPAN = 100;
constexpr int32_t ORDERS = 100;
constexpr int32_t COMBAT = 200;
constexpr int32_t EFFECTS = 300;
constexpr int32_t FRAME = 400;

} // namespace KT1::Core::DisplayRange

#endif
