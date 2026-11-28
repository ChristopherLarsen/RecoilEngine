/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ViewSettings: the parts that act on units and map features through their draw masks: model removal by size class,
// wreck removal, shadows by zoom and prop distances. Core decides (ViewCullState.h); this writes the masks.

#ifndef KT1_DRAW_VIEW_OBJECTS_H
#define KT1_DRAW_VIEW_OBJECTS_H

#include <map>
#include <set>
#include <vector>

#include "ViewNow.h"
#include "ViewShared.h"

#include "Game/KT1/Host/ViewDoor/ViewDoor.h"

namespace KT1::ViewSet {

class ViewObjects {
public:
	void Init(const ViewOptions& o);         // once per game: the unit defs
	// the parts' state from nothing: every live object arrives again (first frame, and after a switch back on)
	void Begin(const ViewNow& now);
	void Update(const ViewNow& now);
	void Release();                          // everything this class hid or changed goes back
	float LongestPropCut() const { return longestCut; }   // elmos
	int PropCount() const { return props.index.Size(); }

private:
	void BuildZooms();
	void Apply(const ViewDoor::Change& c);
	void UnitArrived(int id);
	void FeatureArrived(int id, bool atStart);
	void FeatureGone(int id);
	void AddProp(int id, const ViewDoor::FeatureFacts& f, bool atStart);
	void ShadowOne(bool feature, int id);
	void StartShadowSweeps();
	void StepShadowSweeps();
	float ShadowCut() const;                 // mu of camera zoom
	void UpdateShadows(const ViewNow& now);
	void UpdateCulls(const ViewNow& now);
	void UpdateProps(const ViewNow& now);
	// writes the masks of `writes`; `unitRule`: the unit's own mask back through the shadow rule (else a feature's)
	void WriteCull(Core::View::CullState& state, bool feature, bool unitRule);
	void WriteProps();

	ViewOptions opt;
	std::vector<ViewDoor::UnitDefFacts> defs;
	std::vector<float> unitZoom;             // by unit def id; below 0: not listed
	std::map<int, Core::View::WreckZoom> wreckZoom;   // by feature def id
	std::map<int, float> propCut;            // by feature def id, from the first feature of the def seen
	int detail = 1;
	float propScale = 0;
	float longestCut = 0;

	Core::View::CullState models, wrecks;
	Core::View::PropState props;
	std::set<int32_t> units, features;       // what has arrived
	std::set<int32_t> startFeatures;         // features of the map's start, kept across Begin
	std::vector<ViewDoor::Change> changes;
	std::vector<Core::View::MaskWrite> writes;
	std::vector<int32_t> swept;
};

} // namespace KT1::ViewSet

#endif
