#include "region.h"

#include <algorithm>

void Region::_bind_methods() {
	ClassDB::bind_static_method(
		"Region", D_METHOD("initialize", "max_g_size"), &Region::initialize
	);
	ClassDB::bind_static_method(
		"Region",
		D_METHOD(
			"create",
			"_name",
			"_slot",
			"_size",
			"_blocked_sides",
			"_joining_sides",
			"_spawn_weight",
			"_threshold"
		),
		&Region::create
	);
	ClassDB::bind_static_method("Region", D_METHOD("finalize"), &Region::finalize);
}

void Region::initialize(Vector2i max_g_size) {
	int max_flat_size{ max_g_size.x + max_g_size.y * max_g_size.x };
	for (int dir{ 0 }; dir < m_region_tree.size(); ++dir) {
		m_region_tree[dir].resize(max_flat_size);
	}
	m_max_g_size = max_g_size;
}

Ref<Region> Region::create(
	String _name,
	Slot _slot,
	Vector2i _g_size,
	PackedInt32Array _blocked_sides,
	PackedInt32Array _joining_sides,
	int _spawn_weight,
	int _threshold
) {
	ERR_FAIL_COND_V_MSG(
		m_region_tree[0].size() == 0, Ref<Region>(), "Region is not intialized"
	);

	Ref<Region> region;
	region.instantiate();
	region->name          = _name;
	region->slot          = _slot;
	region->g_size        = _g_size;
	region->blocked_sides = _blocked_sides;
	region->joining_sides = _joining_sides;
	region->spawn_weight  = _spawn_weight;
	region->threshold     = _threshold;

	if (_slot == Slot::PRIMARY) {
		m_primary_regions.push_back(region);
		return region;
	}

	region->m_flat_size_i = _g_size.x + _g_size.y * m_max_g_size.x;

	ERR_FAIL_COND_V_MSG(
		region->m_flat_size_i < m_max_g_size.x + m_max_g_size.y * m_max_g_size.x,
		Ref<Region>(),
		"size of region is larger than max size provided in intialization"
	);

	for (int i{ 0 }; i < _joining_sides.size(); ++i) {
		Direction direction{ static_cast<Direction>(_joining_sides[i]) };
		Direction inv_direction{ invert_direction(direction) };
		m_dir_to_region[inv_direction].push_back(region);
		m_region_tree[inv_direction][region->m_flat_size_i].push_back(region);
	}

	return region;
}

void Region::finalize() {
	for (int dir{ 0 }; dir < m_region_tree.size(); ++dir) {
		std::sort(
			m_dir_to_region[dir].ptr(),
			m_dir_to_region[dir].ptr() + m_region_tree[dir].size(),
			[](const Ref<Region> &a, const Ref<Region> &b) {
				return a->threshold < b->threshold;
			}
		);

		for (uint32_t i{ 0 }; i < m_region_tree[dir].size(); ++i) {
			std::sort(
				m_region_tree[dir][i].ptr(),
				m_region_tree[dir][i].ptr() + m_region_tree[dir][i].size(),
				[](const Ref<Region> &a, const Ref<Region> &b) {
					return a->threshold < b->threshold;
				}
			);
		}
	}
}

PackedFloat32Array Region::get_normalized_weights(
	const LocalVector<Ref<Region>> &regions, const int regions_max
) {
	PackedFloat32Array normalized_weights;
	normalized_weights.resize(regions_max);
	float total{ 0.0 };
	for (Ref<Region> region : regions) {
		total += region->spawn_weight;
	}
	for (int i{ 0 }; i < regions_max; ++i) {
		normalized_weights.set(i, regions[i]->spawn_weight / total);
	}
	return normalized_weights;
}

