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
		"Region",
		D_METHOD(
			"create",
			"name",
			"slot",
			"g_size",
			"spawn_weight",
			"threshold",

			"blocked_sides",
			"blocked_tile_sets",
			"blocked_fill",
			"joining_sides",

			"internal_callable_or_tile_choices",
			"internal_weights",
			"internal_alignments",
			"internal_placements",

			"rand_length_addition",
			"mirror_axes"
		),
		&Region::create,
		DEFVAL(Vector2i(0, 0)),
		DEFVAL(Axis::NONE)
	);

	ClassDB::bind_static_method("Region", D_METHOD("finalize"), &Region::finalize);

	ClassDB::bind_method(D_METHOD("get_internal_choices_debug"), &Region::get_internal_choices_debug);

	ClassDB::bind_method(D_METHOD("get_name"), &Region::get_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "", "get_name");

	ClassDB::bind_method(D_METHOD("get_slot"), &Region::get_slot);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "slot", PROPERTY_HINT_ENUM, "Primary,Secondary"), "", "get_slot");

	ClassDB::bind_method(D_METHOD("get_size_e"), &Region::get_size_e);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size_e"), "", "get_size_e");

	ClassDB::bind_method(D_METHOD("get_size_i"), &Region::get_size_i);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size_i"), "", "get_size_i");

	ClassDB::bind_method(D_METHOD("get_spawn_weight"), &Region::get_spawn_weight);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spawn_weight"), "", "get_spawn_weight");

	ClassDB::bind_method(D_METHOD("get_threshold"), &Region::get_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "threshold"), "", "get_threshold");

	BIND_ENUM_CONSTANT(PRIMARY);
	BIND_ENUM_CONSTANT(SECONDARY);

	BIND_ENUM_CONSTANT(RANDOM);
	BIND_ENUM_CONSTANT(CENTER);
	BIND_ENUM_CONSTANT(START);
	BIND_ENUM_CONSTANT(END);
	BIND_ENUM_CONSTANT(FILL);
	BIND_ENUM_CONSTANT(FORCE_GPOS);
}

String Region::get_name() const { return name; }
Region::Slot Region::get_slot() const { return slot; }
Vector2i Region::get_size_e() const { return size.e; }
Vector2i Region::get_size_i() const { return size.i; }
float Region::get_spawn_weight() const { return spawn_weight; }
int Region::get_threshold() const { return threshold; }

// returns final mirrored region
Ref<Region> Region::create(
	String _name,
	Slot _slot,
	Vector2i _g_size,
	int _spawn_weight,
	int _threshold,

	PackedInt32Array _blocked_sides,
	TypedArray<PackedInt32Array> _blocked_tile_sets,
	PackedInt32Array _blocked_fill,
	PackedInt32Array _joining_sides,

	TypedArray<Array> internal_callable_or_tile_choices,
	TypedArray<PackedInt32Array> internal_weights,
	TypedArray<Array> internal_alignments, // directions or gpos
	TypedArray<PackedInt32Array> internal_placements,

	Vector2i _rand_length_addition,
	Axis::E mirror_axes
) {
	ERR_FAIL_COND_V_MSG(
		_slot == Slot::SECONDARY && _joining_sides.size() <= 0, Ref<Region>(),
		"secondary region has no joining sides"
	);

	ERR_FAIL_COND_V_MSG(
		internal_callable_or_tile_choices.size() != internal_weights.size() ||
		internal_callable_or_tile_choices.size() != internal_alignments.size() ||
		internal_callable_or_tile_choices.size() != internal_placements.size(),
		Ref<Region>(),
		vformat(
			"internal arguments are not of equal length (%s, %s, %s, %s)",
			internal_callable_or_tile_choices.size(),
			internal_weights.size(),
			internal_alignments.size(),
			internal_placements.size()
		)
	);

	ERR_FAIL_COND_V_MSG(
		_blocked_sides.size() != _blocked_fill.size() ||
		_blocked_tile_sets.size() != _blocked_fill.size(),
		Ref<Region>(),
		vformat(
			"blocked data are not of equal length (%s, %s, %s)",
			_blocked_sides.size(), _blocked_tile_sets.size(), _blocked_fill.size()
		)
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

		region->name = _name + String::num_int64(m_axis);
		region->slot = _slot;

		const int64_t free_side_count{ Direction::MAX - _blocked_sides.size() };
		ERR_FAIL_COND_V_MSG(
			free_side_count <= 0, Ref<Region>(),
			vformat("region has no free sides (count: %s)", free_side_count)
		);
		region->free_sides.reserve(free_side_count);

		for (int dir_i{ 0 }; dir_i < Direction::MAX; ++dir_i) {
			const int64_t blocked_i{ _blocked_sides.find(dir_i) };
			Direction::E dir{ static_cast<Direction::E>(dir_i) };
			dir = Axis::mirror_direction(m_axis, dir);

			if (blocked_i != -1) {

				ERR_FAIL_INDEX_V(blocked_i, _blocked_fill.size(), Ref<Region>());
				ERR_FAIL_INDEX_V(blocked_i, _blocked_tile_sets.size(), Ref<Region>());

				

				PCG::Fill blocked_fill{ static_cast<PCG::Fill>(_blocked_fill[blocked_i]) };

				BlockedSide blocked_side{
					dir, blocked_fill, static_cast<PackedInt32Array>(_blocked_tile_sets[blocked_i])
				};

				region->blocked_sides.push_back(blocked_side);

			} else {
				region->free_sides.push_back(static_cast<Direction::E>(dir));
			}

		}

		ERR_FAIL_COND_V_MSG(
			region->blocked_sides.size() != _blocked_sides.size(), Ref<Region>(),
			"blocked_sides constructed with fewer members than expected"
		);

		/// silent crash before this point

		region->size = Size{_g_size, region->blocked_sides };

		for (int dir_i : _joining_sides) {
			Direction::E dir{ static_cast<Direction::E>(dir_i) };
			dir = Axis::mirror_direction(m_axis, dir);

			region->joining_sides.push_back(dir);
		}

		region->spawn_weight = mirror_weight;
		region->threshold    = _threshold;

		if (_slot == Slot::PRIMARY) {
			primary_regions.push_back(region);
		} else {
			secondary_regions.push_back(region);
		}

		region->rand_length_addition = _rand_length_addition;
	
		const int64_t choice_sets_size{ internal_callable_or_tile_choices.size() };
		region->internal_choices.resize(choice_sets_size);

		for (int i{ 0 }; i < choice_sets_size; ++i) {
			Array choices{ internal_callable_or_tile_choices[i] };
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

			Array alignments{ internal_alignments[i] };
			ERR_FAIL_COND_V_MSG(
				alignments.size() != choices.size(), Ref<Region>(),
				vformat("%s alignments provided, expected %s", alignments.size(), choices.size())
			);

			PackedInt32Array internal_placement{ static_cast<PackedInt32Array>(internal_placements[i]) };
			ERR_FAIL_COND_V_MSG(
				internal_placement.size() != choices.size(), Ref<Region>(),
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

					Variant alignment{ alignments[j] };
					if (alignments[j].get_type() == Variant::INT) {
						int alignment_i{ alignment };
						alignment = Axis::mirror_direction(m_axis, static_cast<Direction::E>(alignment_i));

					} else if (alignments[j].get_type() == Variant::VECTOR2I) {
						Vector2i alignment_v{ alignment };
						alignment = Axis::mirror_alignment(m_axis, alignment_v);
						print_line(vformat("mirrored alignment for %s is %s, original is %s", region->name, alignment, alignment_v));

					} else {
						ERR_FAIL_V_MSG(
							Ref<Region>(),
							vformat(
								"type provided for %s alignments pos %s is invalid %s",
								region->name, j, alignments[j].get_type()
							)
						);
					}

					region->internal_choices[i].choice_set[j] = (
						InternalEntry::make_callable(
							callable, callable_g_size, alignment, internal_placement.get(j)
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
						tile->g_size.x <= 0 || tile->g_size.y <= 0, Ref<Region>(), "tile area is zero"
					);

					Variant alignment{ alignments[j] };
					if (alignments[j].get_type() == Variant::INT) {
						int alignment_i{ alignment };
						alignment = Axis::mirror_direction(m_axis, static_cast<Direction::E>(alignment_i));

					} else if (alignments[j].get_type() == Variant::VECTOR2I) {
						Vector2i alignment_v{ alignment };
						alignment = Axis::mirror_alignment(m_axis, alignment_v);
						print_line(vformat("mirrored alignment for %s is %s, original is %s", region->name, alignment, alignment_v));

					} else {
						ERR_FAIL_V_MSG(
							Ref<Region>(),
							vformat(
								"type provided for %s alignments pos %s is invalid %s",
								region->name, j, alignments[j].get_type()
							)
						);
					}

					region->internal_choices[i].choice_set[j] = (
						InternalEntry::make_tile_ref(tile, alignment, internal_placement.get(j))
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
	ERR_FAIL_COND_MSG(primary_regions.size() <= 0, "no primary regions found");
	ERR_FAIL_COND_MSG(secondary_regions.size() <= 0, "no secondary regions found");

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
	primary_weight_sum = get_slot_weights_sum(Slot::PRIMARY, primary_weights.size() - 1);
}

int Region::get_slot_threshold_i(const Slot slot, const int gate) {
	const RegionVector &regions{ slot == Slot::PRIMARY ? primary_regions : secondary_regions };
	int threshold_i{ 0 };
	for (; threshold_i < static_cast<int>(regions.size()); ++threshold_i) {
		if (regions[threshold_i]->threshold > gate) {
			break;
		}
	}
	return threshold_i - 1;
}

float Region::get_slot_weights_sum(const Slot slot, const int threshold_i) {
	const PackedFloat32Array &weights{ slot == Slot::PRIMARY ? primary_weights : secondary_weights };
	float weights_sum = 0.0;
	for (int64_t i = 0; i <= threshold_i; ++i) {
		weights_sum += weights[i];
	}
	DEV_ASSERT(weights_sum > 0.0);
	return weights_sum;
}

Ref<Region> Region::get_slot_rand_region(
	const Slot slot, const int threshold_i, const float weights_sum, const float rand_float
) {
	const PackedFloat32Array &weights{ slot == Slot::PRIMARY ? primary_weights : secondary_weights };
	float remaining_distance = rand_float * weights_sum;
	for (int64_t i{ 0 }; i <= threshold_i; ++i) {
		remaining_distance -= weights[i];
		if (remaining_distance < 0) {
			const RegionVector &regions{ slot == Slot::PRIMARY ? primary_regions : secondary_regions };
			return regions[i];
		}
	}
	return Ref<Region>();
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
					"{tile_index(%s), layer(%s), size(%s), weight(%s), gpos_offset(%s), placement(%s)}, ",
					entry.tile->tile, entry.tile->layer, entry.size, choice_sets.norm_weights[j], entry.gpos_alignment, entry.placement
				);
			}
		}

		out += "]\t";
	}

	out += vformat("internal_choices.size() == %d", internal_choices.size());

	return out;
}
