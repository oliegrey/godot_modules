#include "subgrid_probe.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "core/math/random_number_generator.h"

void SubgridProbe::_bind_methods() {
	ClassDB::bind_static_method(
		"SubgridProbe", D_METHOD("create", "grid_size", "segment_grid_size"),
		&SubgridProbe::create
	);

	ClassDB::bind_integer_constant(
		"SubgridProbe", "Sort", "DESCENDING", Sort::DESCENDING
	);
	ClassDB::bind_integer_constant(
		"SubgridProbe", "Sort", "ASCENDING", Sort::ASCENDING
	);

	ClassDB::bind_method(
		D_METHOD("advance_bucketing", "cell_i", "count"),
		&SubgridProbe::advance_bucketing
	);
	ClassDB::bind_method(
		D_METHOD("advance_gpos_bucketing", "gpos", "count"),
		&SubgridProbe::advance_gpos_bucketing,
		DEFVAL(1)
	);
	ClassDB::bind_method(
		D_METHOD(
			"choose_cells",
			"rng",
			"generative_occupancy",
			"lifetime_cell_count",
			"max_occupancy_delta",
			"sort"
		),
		&SubgridProbe::choose_cells,
		DEFVAL(Sort::DESCENDING)
	);
	ClassDB::bind_method(
		D_METHOD("reset_choose_cells"),
		&SubgridProbe::reset_choose_cells
	);
}

Ref<SubgridProbe> SubgridProbe::create(
	const Vector2i grid_size, const Vector2i segment_grid_size
) {
	Ref<SubgridProbe> subgrid_probe;
	subgrid_probe.instantiate();

	subgrid_probe->m_seg_grid_size = segment_grid_size;
	subgrid_probe->m_grid_size = grid_size;
	subgrid_probe->m_cell_count = grid_size.x * grid_size.y;
	subgrid_probe->m_subgrids_per_axis = (segment_grid_size + grid_size - Vector2i(1, 1)) / grid_size;
	subgrid_probe->m_subgrid_count = subgrid_probe->m_subgrids_per_axis.x * subgrid_probe->m_subgrids_per_axis.y;
	subgrid_probe->m_descending_offset = subgrid_probe->m_cell_count - 1;

	subgrid_probe->m_occupancy_buckets.resize(subgrid_probe->m_cell_count);
	subgrid_probe->m_possible_subgrid_cells.resize(subgrid_probe->m_cell_count);
	for (int cell_i{ 0 }; cell_i < subgrid_probe->m_cell_count; ++cell_i) {
		subgrid_probe->m_possible_subgrid_cells[cell_i] = cell_i;
	}

	for (int y{ 0 }; y < subgrid_probe->m_subgrids_per_axis.y; ++y) {
		for (int x{ 0 }; x < subgrid_probe->m_subgrids_per_axis.x; ++x) {
			int origin_x{ MIN(x * subgrid_probe->m_grid_size.x, segment_grid_size.x - grid_size.x) };
			int origin_y{ MIN(y * subgrid_probe->m_grid_size.y, segment_grid_size.y - grid_size.y) };
			int seg_cell_i{ origin_y * segment_grid_size.x + origin_x };
			subgrid_probe->m_occupancy_buckets[0].insert(seg_cell_i);
			subgrid_probe->m_subgrid_link.insert(seg_cell_i, 0);
		}
	}

	return subgrid_probe;
}

void SubgridProbe::advance_bucketing(int cell_i, int count) {
	int prev_occupancy{ m_subgrid_link[cell_i] };
	int occupancy{ prev_occupancy + count };
	m_occupancy_buckets[prev_occupancy].erase(cell_i);
	if (occupancy < m_cell_count) {
		m_occupancy_buckets[occupancy].insert(cell_i);
		m_subgrid_link[cell_i] = occupancy;
	}
}

void SubgridProbe::advance_gpos_bucketing(Vector2i gpos, int count) {
	Vector2i snapped_gpos{ (gpos / m_grid_size) * m_grid_size };
	const Vector2i max{ m_seg_grid_size - m_grid_size };
	snapped_gpos = MIN(snapped_gpos, max);
	int cell_i{ snapped_gpos.y * m_seg_grid_size.x + snapped_gpos.x };
	advance_bucketing(cell_i, count);
}

void SubgridProbe::choose_cells(
	Object* rng,
	Object* bit_occupancy,
	int lifetime_cell_count,
	int max_occupancy_delta,
	int sort
) {
	RandomNumberGenerator *_rng = Object::cast_to<RandomNumberGenerator>(rng);
	BitGrid2D *_bit_occupancy = Object::cast_to<BitGrid2D>(bit_occupancy);
	m_chosen_cells_i.resize(lifetime_cell_count);
	int offset{ sort == Sort::DESCENDING ? m_descending_offset : 0 };

	for (int bucket_i{ 0 }; bucket_i < m_cell_count; ++bucket_i) {
		HashSet<int> bucket{ m_occupancy_buckets[abs(offset - bucket_i)] };
		if (bucket.size() <= 0) { continue; }

		LocalVector<int> shuffled_subgrids;
		shuffled_subgrids.resize(bucket.size());

		int i{ 0 };
		for (int subgrid_i : bucket) {
			int j{ static_cast<int>(_rng->randi()) % (i + 1) };
			shuffled_subgrids[i] = shuffled_subgrids[j];
			shuffled_subgrids[j] = subgrid_i;
			i += 1;
		}

		for (int seg_subgrid_cell_i : shuffled_subgrids) {
			int subgrid_occupancy_count{ 0 };

			for (int subgrid_cell_i{ 0 }; subgrid_cell_i < m_cell_count; ++subgrid_cell_i) {
				if (m_amount_chosen >= lifetime_cell_count) {
					return;
				}
				else if (subgrid_occupancy_count >= max_occupancy_delta) {
					break;
				}

				int rand_i{
					subgrid_cell_i + static_cast<int>(_rng->randi()) %
					(m_cell_count - subgrid_cell_i)
				};
				int chosen_cell_i{ m_possible_subgrid_cells[rand_i] };
				m_possible_subgrid_cells[rand_i] = m_possible_subgrid_cells[subgrid_cell_i];
				m_possible_subgrid_cells[subgrid_cell_i] = chosen_cell_i;

				int seg_cell_i{
					seg_subgrid_cell_i +
					(chosen_cell_i / m_grid_size.x * m_seg_grid_size.x) +
					(chosen_cell_i % m_grid_size.x)
				};
				if (_bit_occupancy->is_cell_i_set(seg_cell_i)) {
					continue;
				}

				m_chosen_cells_i[m_amount_chosen] = seg_cell_i;
				m_amount_chosen += 1;
				subgrid_occupancy_count += 1;
			}
		}
	}
	if (m_amount_chosen < lifetime_cell_count) {
		m_chosen_cells_i.resize(m_amount_chosen);
		ERR_FAIL_MSG("unable to fufill desired cell count");
	}
}

void SubgridProbe::reset_choose_cells() {
	m_amount_chosen = 0;
	m_set_cell_i = 0;
	m_chosen_cells_i.clear();
	m_possible_subgrid_cells.clear();
	for (int cell_i{ 0 }; cell_i < m_cell_count; ++cell_i) {
		m_possible_subgrid_cells.push_back(cell_i);
	}
}
