#include "region.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "core/math/random_number_generator.h"

#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include <algorithm>

#if defined(_MSC_VER)
#include <intrin.h>
static inline int ctz32(uint32_t x) {
	unsigned long index;
	_BitScanForward(&index, x);
	return static_cast<int>(index);
}
static inline int ctz64(uint64_t x) {
	unsigned long index;
	_BitScanForward64(&index, x);
	return static_cast<int>(index);
}
#else
static inline int ctz32(uint32_t x) {
	return __builtin_ctz(x);
}
static inline int ctz64(uint64_t x) {
	return __builtin_ctzll(x);
}
#endif

void Region::_bind_methods() {
	ClassDB::bind_static_method(
		"Region", D_METHOD("initialize", "max_g_size"), &Region::initialize
	);
	ClassDB::bind_static_method(
		"Region",
		D_METHOD(
			"create",
			"name",
			"slot",
			"g_size",
			"spawn_weight",
			"threshold",

			"blocked_sides",
			"blocked_fill",
			"joining_sides",

			"internal_callable_or_tile_choices",
			"internal_weights",
			"internal_gpos_alignments",
			"internal_placements",

			"rand_length_addition",
			"mirror_axes"
		),
		&Region::create,
		DEFVAL(Vector2i(0, 0)),
		DEFVAL(Axis::NONE)
	);

	ClassDB::bind_static_method("Region", D_METHOD("finalize"), &Region::finalize);

	ClassDB::bind_static_method(
		"Region",
		D_METHOD("get_weight_sum_bounded", "p_weights", "exl_upper_bound"),
		&Region::get_weight_sum_bounded
	);

	ClassDB::bind_method(
		D_METHOD("get_name"), &Region::get_name
	);
	ClassDB::bind_method(
		D_METHOD("get_slot"), &Region::get_slot
	);
	ClassDB::bind_method(
		D_METHOD("get_g_size"), &Region::get_g_size
	);
	ClassDB::bind_method(
		D_METHOD("get_spawn_weight"), &Region::get_spawn_weight
	);
	ClassDB::bind_method(
		D_METHOD("get_threshold"), &Region::get_threshold
	);
	ClassDB::bind_method(
		D_METHOD("get_g_size_inclusive"), &Region::get_g_size_inclusive
	);
	ClassDB::bind_method(
		D_METHOD("get_internal_choices_debug"), &Region::get_internal_choices_debug
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "name"),
		"", "get_name"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "slot", PROPERTY_HINT_ENUM, "Primary,Secondary"),
		"", "get_slot"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::VECTOR2I, "g_size"),
		"", "get_g_size"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_INT32_ARRAY, "blocked_sides"),
		"", "get_blocked_sides"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_INT32_ARRAY, "joining_sides"),
		"", "get_joining_sides"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::FLOAT, "spawn_weight"),
		"", "get_spawn_weight"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "threshold"),
		"", "get_threshold"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::VECTOR2I, "g_size_inclusive"),
		"", "get_g_size_inclusive"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_INT32_ARRAY, "blocked_fill"),
		"", "get_blocked_fill"
	);

	BIND_ENUM_CONSTANT(PRIMARY);
	BIND_ENUM_CONSTANT(SECONDARY);

	BIND_ENUM_CONSTANT(RANDOM);
	BIND_ENUM_CONSTANT(CENTER);
	BIND_ENUM_CONSTANT(START);
	BIND_ENUM_CONSTANT(END);
	BIND_ENUM_CONSTANT(FILL);
	BIND_ENUM_CONSTANT(FORCE_GPOS);

	ClassDB::bind_static_method("Region", D_METHOD("ALIGN_NONE"), &Region::ALIGN_NONE);
	ClassDB::bind_static_method("Region", D_METHOD("ALIGN_UP"), &Region::ALIGN_UP);
	ClassDB::bind_static_method("Region", D_METHOD("ALIGN_DOWN"), &Region::ALIGN_DOWN);
	ClassDB::bind_static_method("Region", D_METHOD("ALIGN_LEFT"), &Region::ALIGN_LEFT);
	ClassDB::bind_static_method("Region", D_METHOD("ALIGN_RIGHT"), &Region::ALIGN_RIGHT);
}

Vector2i Region::ALIGN_NONE() { return A_NONE; }
Vector2i Region::ALIGN_UP() { return A_UP; }
Vector2i Region::ALIGN_DOWN() { return A_DOWN; }
Vector2i Region::ALIGN_LEFT() { return A_LEFT; }
Vector2i Region::ALIGN_RIGHT() { return A_RIGHT; }
String Region::get_name() const { return name; }
Region::Slot Region::get_slot() const { return slot; }
Vector2i Region::get_g_size() const { return g_size; }
float Region::get_spawn_weight() const { return spawn_weight; }
int Region::get_threshold() const { return threshold; }
Vector2i Region::get_g_size_inclusive() const { return g_size_inclusive; }

// returns final mirrored region
Ref<Region> Region::create(
	String _name,
	Slot _slot,
	Vector2i _g_size,
	int _spawn_weight,
	int _threshold,

	PackedInt32Array _blocked_sides,
	PackedInt32Array _blocked_fill,
	PackedInt32Array _blocked_tiles,
	PackedInt32Array _joining_sides,

	TypedArray<Array> internal_class_or_tile_choices,
	TypedArray<PackedInt32Array> internal_weights,
	TypedArray<PackedVector2Array> internal_gpos_alignments,
	TypedArray<PackedInt32Array> internal_placements,

	Vector2i _rand_length_addition,
	Axis::E mirror_axes
) {
	ERR_FAIL_COND_V_MSG(
		_slot == Slot::SECONDARY && _joining_sides.size() > 0, Ref<Region>(),
		"secondary region has no joining sides"
	);

	Ref<Region> region;

	LocalVector<Axis::E> all_mirror_axes;
	if (mirror_axes == Axis::NONE) {
		all_mirror_axes = LocalVector<Axis::E>{ mirror_axes };
	}
	else if (mirror_axes == Axis::X || mirror_axes == Axis::Y) {
		all_mirror_axes = LocalVector<Axis::E>{ Axis::NONE, mirror_axes };
	}
	else {
		all_mirror_axes = LocalVector<Axis::E>{ Axis::NONE, Axis::X, Axis::Y, mirror_axes };
	}

	const float mirror_weight{ _spawn_weight / static_cast<float>(all_mirror_axes.size())};

	for (Axis::E m_axis : all_mirror_axes) {
		region.instantiate();

		region->name             = _name + String::num_int64(m_axis);
		region->slot             = _slot;
		region->g_size           = _g_size;
		region->g_size_inclusive = _g_size;

		for (int dir_i : _blocked_sides) {
			Direction::E dir{ static_cast<Direction::E>(dir_i) };
			dir = Axis::mirror_direction(m_axis, dir);

			PCG::Fill blocked_fill{ static_cast<PCG::Fill>(_blocked_fill[dir_i]) };

			BlockedSide blocked_side{ dir, blocked_fill, _blocked_tiles };

			region->blocked_sides.push_back(blocked_side);

			if (dir == Direction::UP || dir == Direction::DOWN) {
				region->g_size_inclusive.y += 1;
			} else if (dir == Direction::LEFT || dir == Direction::RIGHT) {
				region->g_size_inclusive.x += 1;
			}
		}
		for (int dir_i : _joining_sides) {
			Direction::E dir{ static_cast<Direction::E>(dir_i) };
			dir = Axis::mirror_direction(m_axis, dir);

			region->joining_sides.push_back(dir);
		}

		region->spawn_weight = mirror_weight;
		region->threshold    = _threshold;

		ERR_FAIL_COND_V_MSG(region->g_size_inclusive.x > 8, Ref<Region>(), "g_size_inclusive.x > 8");
		ERR_FAIL_COND_V_MSG(region->g_size_inclusive.y > 8, Ref<Region>(), "g_size_inclusive.y > 8");

		if (_slot == Slot::PRIMARY) {
			primary_regions.push_back(region);
		} else {
			secondary_regions.push_back(region);
		}

		region->rand_length_addition = _rand_length_addition;

		ERR_FAIL_COND_V_MSG(
			internal_class_or_tile_choices.size() != internal_weights.size() ||
			internal_class_or_tile_choices.size() != internal_gpos_alignments.size() ||
			internal_class_or_tile_choices.size() != internal_placements.size(),
			Ref<Region>(), "choices, weights, anchor_dirs and placements must be of equal length"
		);
	
		int64_t choice_sets_size{ internal_class_or_tile_choices.size() };
		region->internal_choices.resize(choice_sets_size);

		for (int i{ 0 }; i < internal_class_or_tile_choices.size(); ++i) {
			Array choices{ internal_class_or_tile_choices[i] };
			region->internal_choices[i].choice_set.resize(choices.size());

			PackedFloat32Array *n_weights{ &region->internal_choices[i].norm_weights };
			PackedInt32Array internal_weight{
				static_cast<PackedInt32Array>(internal_weights[i])
			};

			float total_weight{ 0 };
			ERR_FAIL_COND_V_MSG(internal_weight.size() <= 0, Ref<Region>(), "no weights provided");
			for (int weight : internal_weight) {
				ERR_FAIL_COND_V_MSG(weight <= 0, Ref<Region>(), "weights must be > 0"); 
				total_weight += static_cast<float>(weight);
			}

			n_weights->resize(internal_weight.size());
			float cum_weight{ 0.0 }; 
			for (int j{ 0 }; j < n_weights->size(); ++j) {
				cum_weight += internal_weight[j] / total_weight;
				n_weights->set(j, cum_weight);
			}

			PackedVector2Array internal_gpos_alignment{
				static_cast<PackedVector2Array>(internal_gpos_alignments[i])
			};
			for (int j{ 0 }; j < internal_gpos_alignment.size(); ++j) {
				Vector2 gpos_alignment{ internal_gpos_alignment[j] };
				if (m_axis == Axis::X || m_axis == Axis::ALL) {
					gpos_alignment.x *= -1;
				}
				if (m_axis == Axis::Y || m_axis == Axis::ALL) {
					gpos_alignment.y *= -1;
				}
				internal_gpos_alignment.set(j, gpos_alignment);
			}

			PackedInt32Array internal_placement{
				static_cast<PackedInt32Array>(internal_placements[i])
			};

			ERR_FAIL_COND_V_MSG(
				internal_placement.size() != internal_gpos_alignment.size(), Ref<Region>(),
				vformat("internal configs are not of the same length for %s", i)
			);

			for (int j{ 0 }; j < choices.size(); ++j) {
				Variant variant{ choices[j] };
				ERR_FAIL_COND_V_MSG(
					variant.get_type() == Variant::NIL, Ref<Region>(),
					vformat("passed value is null for choice %d", j)
				);
				Variant::Type type{ variant.get_type() };

				if (type == Variant::DICTIONARY) {
					Dictionary dict{ variant };

					ERR_FAIL_COND_V_MSG(
						!dict.has("g_size"), Ref<Region>(),
						vformat("entry %d of %s dictionary missing g_size", j, _name)
					);
					ERR_FAIL_COND_V_MSG(
						!dict.has("callable"), Ref<Region>(),
						vformat("entry %d of %s dictionary missing callable", j, _name)
					);

					Vector2i callable_g_size{ dict["g_size"] };
					Callable callable{ dict["callable"] };

					ERR_FAIL_COND_V_MSG(
						!callable.is_valid(), Ref<Region>(),
						vformat("callable in entry %d of %s is invalid", j, _name)
					);
					ERR_FAIL_COND_V_MSG(
						callable_g_size.x <= 0 || callable_g_size.y <= 0, Ref<Region>(),
						vformat("callable_g_size area is <= 0 in entry %d of %s", j, _name)
					);
					ERR_FAIL_COND_V_MSG(
						callable_g_size.x > _g_size.x || callable_g_size.y > _g_size.y, Ref<Region>(),
						vformat("callable_g_size(%s) is larger than region _g_size(%s)", callable_g_size, _g_size)
					);

					region->internal_choices[i].choice_set[j] = (
						InternalEntry::make_callable(
							callable,
							callable_g_size,
							internal_gpos_alignment.get(j),
							internal_placement.get(j)
						)
					);
				}

				else if (type == Variant::INT) {
					Ref<Tile> tile{ Tile::get_tile(variant) };
					ERR_FAIL_NULL_V_MSG(
						tile, Ref<Region>(),
						vformat("not valid tile enum or dictionary for entry %d of %s", j, _name)
					);
					ERR_FAIL_COND_V_MSG(
						tile->g_size.x <= 0 && tile->g_size.y <= 0, Ref<Region>(), "tile area is zero"
					);

					region->internal_choices[i].choice_set[j] = (
						InternalEntry::make_tile_ref(
							tile, internal_gpos_alignment.get(j), internal_placement.get(j)
						)
					);

				} else {
					ERR_FAIL_V_MSG(
						Ref<Region>(),
						vformat("incompatible type passed to Region::create for entry %d of %s", j, _name)
					);
				}
			}
		}
	}

	return region;
}

void Region::finalize() {
	std::sort(
		primary_regions.ptr(),
		primary_regions.ptr() + primary_regions.size(),
		[](const Ref<Region> &a, const Ref<Region> &b) {
			return a->threshold < b->threshold;
		}
	);

	std::sort(
		secondary_regions.ptr(),
		secondary_regions.ptr() + secondary_regions.size(),
		[](const Ref<Region> &a, const Ref<Region> &b) {
			return a->threshold < b->threshold;
		}
	);


	for (int dir{ 0 }; dir < Direction::MAX; ++dir) {
		std::sort(
			m_dir_to_region[dir].ptr(),
			m_dir_to_region[dir].ptr() + m_dir_to_region[dir].size(),
			[](const Ref<Region> &a, const Ref<Region> &b) {
				return a->threshold < b->threshold;
			}
		);
	}

	secondary_weights.resize(secondary_regions.size());
	for (uint64_t i{ 0 }; i < secondary_regions.size(); ++i) {
		secondary_weights.set(i, secondary_regions[i]->spawn_weight);
	}

	primary_weights.resize(primary_regions.size());
	for (uint64_t i{ 0 }; i < primary_regions.size(); ++i) {
		primary_weights.set(i, primary_regions[i]->spawn_weight);
	}
	primary_weight_sum = get_weight_sum_bounded(primary_weights, primary_weights.size());
}

int Region::get_size_or_larger_i(uint64_t bitmap, const Vector2i size) {
	uint64_t mask{ dominance_mask[size.x - 1][size.y - 1] };
	const uint64_t masked_bitmap{ bitmap & mask };
	if (masked_bitmap == 0) {
		return -1;
	}
	return ctz64(masked_bitmap);
}

int Region::get_size_or_larger_i(uint64_t bitmap, const int cell_count) {
	return get_size_or_larger_i(bitmap, Vector2i{ cell_count % 8, cell_count / 8 });
}

float Region::get_weight_sum_bounded(
	const PackedFloat32Array &p_weights, const int exl_upper_bound
) {
	const float *weights = p_weights.ptr();
	float weights_sum = 0.0;
	for (int64_t i = 0; i < exl_upper_bound; ++i) {
		weights_sum += weights[i];
	}
	return weights_sum;
}

void Region::debug_region(Vector2i gpos, Vector2i rand_g_size_inc, Ref<Region> region, int w_seg) {
	SceneTree *tree{ SceneTree::get_singleton() };
	Node *main{ tree->get_root()->get_node(NodePath("Main")) };

	for (int y{ 0 }; y < rand_g_size_inc.y; ++y) {
		for (int x{ 0 }; x < rand_g_size_inc.x; ++x) {
			Label *label{ memnew(Label) };

			label->add_theme_font_size_override("font_size", 12);
			label->set_text(region->name);
			label->set_autowrap_mode(TextServer::AUTOWRAP_ARBITRARY);
			label->set_custom_minimum_size(Vector2(50, 50));

			Vector2 pos;
			pos.x = (gpos.x + x) * 50;
			pos.y = (gpos.y + y + w_seg * m_seg_g_size.y) * 50;
			label->set_position(pos);

			label->set_z_index(999);

			main->add_child(label);
		}
	}
}

String Region::get_internal_choices_debug() const {
	String out{ "" };

	for (uint32_t i{ 0 }; i < internal_choices.size(); ++i) {
		const InternalChoiceSet &choice_sets{ internal_choices[i] };

		out += vformat("choice_set[%d] entries=[", i);

		for (uint32_t j{ 0 }; j < choice_sets.choice_set.size(); ++j) {
			const InternalEntry &entry{ choice_sets.choice_set[j] };

			if (entry.type == InternalEntry::TYPE_CALLABLE) {
				out += vformat(
					"{callable(%s), size(%s), weight(%d), gpos_offset(%s), placement(%s)}, ",
					entry.callable, entry.size, choice_sets.norm_weights[j], entry.gpos_alignment, entry.placement
				);

			} else {
				out += vformat(
					"{tile_index(%s), layer_offset(%s), size(%s), weight(%s), gpos_offset(%s), placement(%s)}, ",
					entry.tile_index, entry.layer_offset, entry.size, choice_sets.norm_weights[j], entry.gpos_alignment, entry.placement
				);
			}
		}

		out += "]\t";
	}

	out += vformat("internal_choices.size() == %d", internal_choices.size());

	return out;
}

