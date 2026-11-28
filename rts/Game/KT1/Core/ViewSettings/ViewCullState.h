/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the draw-mask state of ViewSettings: model and wreck removal, the shadow cut and the prop distances
// (LUA-FREE H01, FRAME §4; replaces gfx_model_cull, gfx_wreck_cull, gfx_shadow_zoom and gfx_prop_distance with their
// libraries). No engine include.
//
// Core decides what is hidden or shown; the adapter reads and writes the engine's draw masks. A MaskWrite with hide:
// the adapter saves the object's mask (when it is not 0) in `savedMask` and writes 0; without: it writes the saved mask
// (or full_mask) back, through ShadowMask so a restored model does not bring its shadow back beyond the setting.

#ifndef KT1_CORE_VIEW_CULL_STATE_H
#define KT1_CORE_VIEW_CULL_STATE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "ZoomLevel.h"

namespace KT1::Core::View {

struct MaskWrite {
	int32_t id = -1;
	bool hide = false;
};

// ---- models and wrecks ------------------------------------------------------------------------------------------------

enum class ModelClass : uint8_t { Small, Lead, Large };

struct ModelDefFacts {
	std::string_view name;
	int xsize = 0, zsize = 0;        // footprint in squares
	bool commandIcon = false;        // the custom parameter icon_cmd is present
};

ModelClass ClassOf(const ModelCullConfig& config, const ModelDefFacts& def);
float CullZoom(const ViewSettingsConfig& config, ModelClass cls, int detail);   // elmos from the focus

// the removal zoom of a wreck feature def: the farthest of the units that leave it; never when one is an experimental
struct WreckZoom {
	bool known = false;
	bool never = false;
	float zoom = 0.0f;
};
void MergeWreckZoom(WreckZoom& wreck, float zoom, bool never);

// One set of cullable objects (units, or wreck features): each belongs to the set of its removal zoom.
struct CullState {
	DenseIndex index;
	Column<int32_t> set;             // index into zoom / culled / members
	Column<uint8_t> hidden;
	Column<uint8_t> savedMask;       // adapter-written: the mask before it was hidden
	std::vector<float> zoom;         // the distinct removal zooms
	std::vector<uint8_t> culled;     // per zoom: hidden now
	std::vector<std::vector<int32_t>> members;

	void Init(int32_t maxIds);
	void Clear();                    // forget every object (the option changed): Restore first
};

void AddCullable(CullState& state, int32_t id, float zoom, std::vector<MaskWrite>& out);
void RemoveCullable(CullState& state, int32_t id);
// walks only the sets whose zoom the distance crossed
void StepCull(CullState& state, float distance, std::vector<MaskWrite>& out);
void RestoreCull(CullState& state, std::vector<MaskWrite>& out);   // everything shown again

// ---- the shadow cut -----------------------------------------------------------------------------------------------------

struct IdSweep {
	std::vector<int32_t> ids;
	size_t next = 0;
	bool far = false;
	bool active = false;
};

struct ShadowState {
	int setting = 0;                 // the player's, mu of camera zoom
	bool far = false;                // shadows off
	bool decalsFar = false;          // decal pass skipped
	IdSweep units, features;
};

int ShadowClamp(const ShadowCutConfig& config, float value);                    // to the range and step
float ShadowCutZoom(const ShadowCutConfig& config, int setting, bool capOn);    // mu: the smaller of setting and cap
bool ShadowNext(bool far, float zoom, float cut, float hysteresis);
uint8_t ShadowMask(const ShadowCutConfig& config, uint8_t mask, bool far);      // a mask of 0 stays 0
void StartSweep(IdSweep& sweep, std::span<const int32_t> ids, bool far);        // replaces an unfinished one
// appends up to `batch` ids to `out`; true once the sweep is done
bool StepSweep(IdSweep& sweep, int batch, std::vector<int32_t>& out);

// ---- props --------------------------------------------------------------------------------------------------------------

struct PropFacts {
	std::string_view propClass;      // the feature def's custom parameter prop_class; empty: not a map prop
	float x = 0.0f, y = 0.0f, z = 0.0f;
	float side = 0.0f;               // the longer collision box side, elmos (2 x radius when there is no box)
	bool wreck = false;              // the custom parameter pm_wreck
	bool decor = false;              // placed by the map at the start without a prop class
};

// the kind's last distance in elmos after the scale and its floor; below 0: left to the engine distance
float PropCut(const PropCullConfig& config, const PropFacts& prop, float lodScale);
int PropSize(const PropCullConfig& config, const PropFacts& prop);   // size class: 0 small, 1 middle, 2 large

constexpr int PROP_SIZES = 3;        // DESIGN: the size classes of the zoom gate
constexpr uint8_t HIDDEN_BY_DISTANCE = 1, HIDDEN_BY_ZOOM = 2;   // DESIGN: bits of PropState::hiddenBy; hidden while either

struct PropCell {
	int64_t key = 0;
	float x0 = 0.0f, z0 = 0.0f;
	float ymin = 0.0f, ymax = 0.0f;
	std::vector<float> cuts;                      // the distinct last distances in this cell
	std::vector<uint8_t> inside;                  // per cut: 0 all beyond, 1 all within, 2 mixed, 3 to be looked at
	std::vector<std::vector<int32_t>> members;    // per cut
};

struct PropState {
	DenseIndex index;                             // feature ids
	Column<float> x, y, z, cut;
	Column<uint8_t> size;
	Column<uint8_t> hiddenBy;
	Column<uint8_t> savedMask;                    // adapter-written
	Column<int32_t> cell;                         // index into cells
	std::vector<PropCell> cells;
	std::vector<int32_t> bySize[PROP_SIZES];
	bool gone[PROP_SIZES] = {};                   // hidden by the zoom gate
	float lastCam[3] = {};
	bool dirty = true;

	void Init(int32_t maxFeatureIds);
};

void AddProp(PropState& state, const PropCullConfig& config, int32_t id, const PropFacts& prop, float cut, std::vector<MaskWrite>& out);
void RemoveProp(PropState& state, int32_t id);
// each cell against the camera: whole cells switch once, mixed cells are checked prop by prop; skipped for small moves
void PropDistancePass(PropState& state, const PropCullConfig& config, const float (&cam)[3], std::vector<MaskWrite>& out);
// the zoom gate per size class (RND-004): `distance` camera-to-focus, `level` the zoom switch setting
void PropZoomPass(PropState& state, const ViewSettingsConfig& config, float distance, float level, std::vector<MaskWrite>& out);
void RestoreProps(PropState& state, std::vector<MaskWrite>& out);

} // namespace KT1::Core::View

#endif
