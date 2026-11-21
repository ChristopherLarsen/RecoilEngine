#pragma once

#include "System/float3.h"
#include "Rendering/Common/ModelDrawerData.h"

class CFeature;
class CCamera;

class CFeatureDrawerData : public CFeatureDrawerDataBase {
public:
	// supcom: a feature Lua masked to 0 leaves the draw bins, and comes back when unmasked (SupComFeatureDrawSkip.h)
	void SupComSetMasked(const CFeature* f, bool masked) { if (f->model == nullptr) return; if (masked) DelObject(f, false); else UpdateObject(f, false); }
	// CEventClient interface
	bool WantsEvent(const std::string & eventName) override {
		return
			eventName == "RenderFeaturePreCreated" ||
			eventName == "RenderFeatureCreated"    ||
			eventName == "RenderFeatureDestroyed";
	}
	void RenderFeaturePreCreated(const CFeature* feature) override;
	void RenderFeatureCreated(const CFeature* feature) override;
	void RenderFeatureDestroyed(const CFeature* feature) override;
public:
	CFeatureDrawerData(bool& mtModelDrawer_);
	virtual ~CFeatureDrawerData();
public:
	void ConfigNotify(const std::string& key, const std::string& value);
public:
	void Update() override;
	bool IsAlpha(const CFeature* co) const override;
protected:
	void UpdateObjectDrawFlags(CSolidObject* o) const override;
private:
	static void UpdateDrawPos(CFeature* f);
public:
	float featureDrawDistance;
	float featureFadeDistance;
};