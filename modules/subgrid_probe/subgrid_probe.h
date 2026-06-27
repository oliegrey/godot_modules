#pragma once

#include "core/object/ref_counted.h"

class SubgridProbe : public RefCounted {
	GDCLASS(SubgridProbe, RefCounted);

public:
	enum class Sort { DESCENDING, ASCENDING };

private:
	Vector2i m_seg_grid_size;
	Vector2i m_grid_size;
	int m_cell_count;
	Vector2i m_subgrids_per_axis;
	int m_subgrid_count;
	int m_descending_offset;
	LocalVector<HashSet<int>> m_occupancy_buckets; // empty(subgrid_cell_i1, subgrid_cell_i2, ...), ..., full(...)
	HashMap<int, int> m_subgrid_link; // subgrid_cell_i: bucket_i/occupancy
	LocalVector<int> m_possible_subgrid_cells;
	int m_amount_chosen{ 0 };
	LocalVector<int> m_chosen_cells_i;
	int m_set_cell_i{ 0 };

protected:
	static void _bind_methods();

public:
	static Ref<SubgridProbe> create(
		const Vector2i grid_size, const Vector2i segment_grid_size
	);

	void advance_bucketing(int cell_i, int count);
	void advance_gpos_bucketing(Vector2i gpos, int count = 1);
	void choose_cells(
		Object *rng,
		Object *generative_occupancy,
		int lifetime_cell_count,
		int max_occupancy_delta,
		Sort sort = Sort::DESCENDING
	);
	void reset_choose_cells();
};

VARIANT_ENUM_CAST(SubgridProbe::Sort);
