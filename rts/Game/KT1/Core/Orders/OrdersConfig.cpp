/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the config's overlay behind OrdersConfig.h (ord_orders.lua GG.Ord.UseFixture over V.ord).

#include "OrdersConfig.h"

namespace KT1::Core::Orders {

namespace {

// a Lua key whose value goes into one float member as it is
float* PlainMember(OrdersConfig& c, std::string_view key) {
	const struct {
		std::string_view key;
		float* member;
	} rows[] = {
		{"patrol_check_interval", &c.patrol.patrol_check_s},
		{"patrol_repair_threshold", &c.patrol.repair_threshold},
		{"guard_check_interval", &c.guard.check_s},
		{"load_time", &c.transport.load_s},
		{"unload_time", &c.transport.unload_s},
		{"drop_accuracy", &c.transport.drop_accuracy_mu},
		{"selfdestruct_delay", &c.selfdestruct_delay_s},
		{"give_up_time", &c.track.give_up_s},
		{"fight_check_interval", &c.patrol.fight_check_s},
	};
	for (const auto& row: rows) {
		if (row.key == key)
			return row.member;
	}
	return nullptr;
}

} // namespace

bool SetValue(OrdersConfig& config, std::string_view key, float value) {
	if (key == "queue_min") {
		config.queue_min = static_cast<int>(value);
		return true;
	}
	if (key == "guard_radius") {
		config.guard.half_radius_mu = value / 2;   // DESIGN: the whole radius, half of it in use
		return true;
	}
	float* member = PlainMember(config, key);
	if (member == nullptr)
		return false;
	*member = value;
	return true;
}

OrdersConfig WithFixture(const OrdersConfig& base, std::string_view name) {
	OrdersConfig out = base;
	if (name.empty())
		return out;
	for (const Fixture& fixture: base.fixtures) {
		if (fixture.name != name)
			continue;
		for (const FixtureValue& v: fixture.values)
			SetValue(out, v.key, v.value);
		break;
	}
	return out;
}

bool SetValue(AMoveConfig& config, std::string_view key, float value) {
	if (key == "fight_resume_tick")
		config.resume_tick_s = value;
	else if (key == "fight_guard_factor")
		config.guard_factor = value;
	else if (key == "fight_stop_share")
		config.stop_share = value;
	else
		return false;
	return true;
}

AMoveConfig WithFixture(const AMoveConfig& base, const OrdersConfig& orders, std::string_view name) {
	AMoveConfig out = base;
	for (const Fixture& fixture: orders.fixtures) {
		if (name.empty() || fixture.name != name)
			continue;
		for (const FixtureValue& v: fixture.values)
			SetValue(out, v.key, v.value);
		break;
	}
	return out;
}

} // namespace KT1::Core::Orders
