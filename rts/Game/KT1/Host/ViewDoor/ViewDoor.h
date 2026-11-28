/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: ViewDoor, what ViewSettings (a draw system, which includes no Sim/ header) needs of units and features:
// their draw masks, what their definitions say, where map objects stand, and which ones came and went. Draw state only:
// a draw mask is never read by the simulation. Values and ids, never a pointer to a live object.

#ifndef KT1_HOST_VIEWDOOR_H
#define KT1_HOST_VIEWDOOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace KT1::ViewDoor {

struct Change {
	enum Kind : uint8_t { Arrived, Gone };   // Arrived: created, given or newly seen
	Kind kind = Arrived;
	bool feature = false;
	bool atStart = false;                    // a feature the map placed before the first step
	int id = 0;
};

struct UnitDefFacts {
	int defId = 0;
	std::string name;
	bool commandIcon = false;                // the custom parameter icon_cmd is present
	int xsize = 0, zsize = 0;                // footprint, map squares
	int tier = 1;                            // customParams.eco_tier
	std::string model;                       // the model's file name without folder and .s3o, lower case; empty: not an s3o
	std::string wreck;                       // customParams.pm_wreck
};

struct FeatureFacts {
	int defId = 0;
	float x = 0, y = 0, z = 0;
	std::string propClass;                   // customParams.prop_class
	bool wreck = false;                      // customParams.pm_wreck
	float sideElmos = 0;                     // the collision box's longer side; without one, twice the def's radius
	bool seen = false;                       // the local view may read it
};

void Listen();                               // starts collecting changes; every live object is queued as Arrived
void Stop();
void Take(std::vector<Change>& out);         // moves the queued changes into out (cleared first)

int MaxUnits();                              // unit ids are below this
int MaxFeatures();                           // feature ids are below this
std::vector<UnitDefFacts> UnitDefs();
int FeatureDefId(const std::string& name);   // -1: none
std::vector<int> FeatureIds();               // ascending
int UnitDefOf(int id);                       // -1: gone
bool Feature(int id, FeatureFacts& out);     // false: gone

// the engine draw mask; false: gone
bool Mask(bool feature, int id, uint8_t& out);
void SetMask(bool feature, int id, uint8_t mask);

} // namespace KT1::ViewDoor

#endif
