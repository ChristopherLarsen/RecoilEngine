/* SupCom: a profiler timer per engine input receiver (drawing only), so Draw::Screen::InputReceivers splits into the
 * minimap, the GUI handler, the consoles and the rest (vision-rebuild draw analysis 2026-11-20). */
#ifndef SUPCOM_RECEIVER_TIMERS_H
#define SUPCOM_RECEIVER_TIMERS_H

#include "Game/UI/GuiHandler.h"
#include "Game/UI/InfoConsole.h"
#include "Game/UI/InputReceiver.h"
#include "Game/UI/MiniMap.h"
#include "Game/UI/TooltipConsole.h"
#include "System/TimeProfiler.h"

inline void SupComDrawReceiver(CInputReceiver* r)
{
	if (dynamic_cast<CMiniMap*>(r) != nullptr) { SCOPED_TIMER("Draw::Screen::Receiver::MiniMap"); r->Draw(); return; }
	if (dynamic_cast<CGuiHandler*>(r) != nullptr) { SCOPED_TIMER("Draw::Screen::Receiver::GuiHandler"); r->Draw(); return; }
	if (dynamic_cast<CTooltipConsole*>(r) != nullptr) { SCOPED_TIMER("Draw::Screen::Receiver::Tooltip"); r->Draw(); return; }
	if (dynamic_cast<CInfoConsole*>(r) != nullptr) { SCOPED_TIMER("Draw::Screen::Receiver::InfoConsole"); r->Draw(); return; }
	SCOPED_TIMER("Draw::Screen::Receiver::Other");
	r->Draw();
}

#endif
