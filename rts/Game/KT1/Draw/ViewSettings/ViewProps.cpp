/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Prop distances (RND-003 R5) and the map objects' zoom gate (RND-004) on the feature draw masks.

#include "ViewObjects.h"

#include <algorithm>

#include "System/Config/ConfigHandler.h"

namespace KT1::ViewSet {

namespace View = Core::View;

void ViewObjects::WriteProps() {
	for (const View::MaskWrite& w : writes) {
		const Core::DenseIndex::Slot slot = props.index.SlotOf(w.id);
		if (slot == Core::DenseIndex::NONE) continue;
		uint8_t m = 0;
		if (!ViewDoor::Mask(true, w.id, m)) continue;
		if (w.hide) {
			props.savedMask[slot] = m != 0 ? m : static_cast<uint8_t>(Shared().config.props.full_mask);
			ViewDoor::SetMask(true, w.id, 0);
		} else {
			ViewDoor::SetMask(true, w.id, props.savedMask[slot]);
		}
	}
	writes.clear();
}

void ViewObjects::AddProp(int id, const ViewDoor::FeatureFacts& f, bool atStart) {
	if (props.index.Has(id)) return;
	const View::PropCullConfig& c = Shared().config.props;
	View::PropFacts p;
	p.propClass = f.propClass;
	p.x = f.x;
	p.y = f.y;
	p.z = f.z;
	p.side = f.sideElmos;
	p.wreck = f.wreck;
	auto it = propCut.find(f.defId);
	if (it == propCut.end()) it = propCut.emplace(f.defId, View::PropCut(c, p, propScale)).first;
	float cut = it->second;
	if (cut < 0 && atStart) {                    // the map's own decor: a map object to the player too
		p.decor = true;
		cut = View::PropCut(c, p, propScale);
	}
	if (cut < 0) return;
	longestCut = std::max(longestCut, cut);
	View::AddProp(props, c, id, p, cut, writes);
	WriteProps();
}

void ViewObjects::UpdateProps(const ViewNow& now) {
	const View::ViewSettingsConfig& c = Shared().config;
	if (opt.zoomSwitch) {
		const View::ZoomSwitchConfig& z = c.zoom_switch;
		const float value = configHandler->IsSet(z.setting) ? static_cast<float>(configHandler->GetInt(z.setting)) : z.default_level;
		View::PropZoomPass(props, c, now.dist, View::ZoomSwitchLevel(z, value), writes);
		WriteProps();
	}
	const float cam[3] = {now.pos.x, now.pos.y, now.pos.z};
	View::PropDistancePass(props, c.props, cam, writes);
	WriteProps();
}

} // namespace KT1::ViewSet
