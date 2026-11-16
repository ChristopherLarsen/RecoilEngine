/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_RIBBON_PROJECTILE_H
#define SUPCOM_RIBBON_PROJECTILE_H

#include "Sim/Projectiles/Projectile.h"

class CWeaponProjectile;

// supcom: a textured ribbon trail behind any weapon projectile (weapon VFX gap 1). Unsynced, like the missile smoke trail:
// it only draws and cannot change the simulation. Turned on per weapon with customParams:
//   supcom_ribbon          "1"
//   supcom_ribbon_texture  projectile atlas texture name (default: the smoke trail texture)
//   supcom_ribbon_time     frames each segment lives (default 15)
//   supcom_ribbon_period   frames per segment (default 1)
//   supcom_ribbon_width    "w0 w1": half-width in elmos when a segment is new and when it dies (default "2 2")
//   supcom_ribbon_color0   "r g b a" when new, premultiplied (a = 0 adds light, a = 1 covers) (default "1 1 1 0")
//   supcom_ribbon_color1   "r g b a" when it dies (default "0 0 0 0")
class CSupComRibbonProjectile : public CProjectile
{
	CR_DECLARE_DERIVED(CSupComRibbonProjectile)
public:
	CSupComRibbonProjectile() = default;
	CSupComRibbonProjectile(const CUnit* owner, int weaponDefID, const float3& posNew, const float3& posOld, const float3& dirNew, const float3& dirOld);

	void Update() override;
	void Draw() override;
	int GetProjectilesCount() const override { return 2; }

	void UpdateEndPos(const float3& pos, const float3& dir);

	/// one-line hook in CProjectileHandler: after each synced weapon projectile updates
	static void OnWeaponUpdate(CWeaponProjectile* p);

private:
	float3 pos1, pos2, dir1, dir2;
	int creationTime = 0;
	int weaponDefID = -1;
};

#endif
