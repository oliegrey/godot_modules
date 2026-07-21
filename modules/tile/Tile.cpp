#include "Tile.h"

Tile::Tile(
    Layer p_layer, 
    int p_tile, 
    const String &p_name, 
    const LocalVector<State> &p_states, 
    const LocalVector<Vector2i> &p_state_frame_ranges, 
    const LocalVector<int32_t> &p_frame_durations_ms, 
    int p_width, 
    int p_height, 
    int p_linked_foreground_enum, 
    int p_random_group_length
) {
    layer = p_layer;
    tile = p_tile;
    name = p_name;
    states = p_states;
    state_frame_ranges = p_state_frame_ranges;
    frame_durations_ms = p_frame_durations_ms;
    g_size = Vector2i(p_width, p_height);
    linked_foreground_i = p_linked_foreground_enum;
    random_group_length = p_random_group_length;

    state_atlas_coords.resize(states.size());
    for (uint32_t i = 0; i < states.size(); i++) {
        state_atlas_coords[i].x = state_frame_ranges[i].x * GRID_SIZE;
        state_atlas_coords[i].y = prev_atlas_coord_y;
    }
    prev_atlas_coord_y = state_atlas_coords[0].y + g_size.y;
}

void Tile::_bind_methods() {
	BIND_ENUM_CONSTANT(BACKGROUND);
	BIND_ENUM_CONSTANT(COLLISION);
	BIND_ENUM_CONSTANT(DECORATION);
	BIND_ENUM_CONSTANT(FOREGROUND);
	BIND_ENUM_CONSTANT(INTERACTABLE);
	BIND_ENUM_CONSTANT(MINEABLE);
    BIND_ENUM_CONSTANT(MAX_LAYER);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(ACTIVE);
	BIND_ENUM_CONSTANT(IDLE);
    BIND_ENUM_CONSTANT(MAX_STATE);

	BIND_ENUM_CONSTANT(DUG);
	BIND_ENUM_CONSTANT(GAS);
	BIND_ENUM_CONSTANT(LAVA);
	BIND_ENUM_CONSTANT(SKY);

	BIND_ENUM_CONSTANT(BOULDER);
	BIND_ENUM_CONSTANT(DIRT);
	BIND_ENUM_CONSTANT(DIRT2);
	BIND_ENUM_CONSTANT(ROCK);

	BIND_ENUM_CONSTANT(CLOUD);
	BIND_ENUM_CONSTANT(CLOUD2);
	BIND_ENUM_CONSTANT(CLOUD3);
	BIND_ENUM_CONSTANT(CLOUD4);
	BIND_ENUM_CONSTANT(CLOUD5);
	BIND_ENUM_CONSTANT(GRASS);
	BIND_ENUM_CONSTANT(STALACTITE);
	BIND_ENUM_CONSTANT(STALAGMITE);

	BIND_ENUM_CONSTANT(FOREGROUND_SIGNPOST);

	BIND_ENUM_CONSTANT(AGRENIC);
	BIND_ENUM_CONSTANT(CRYSTAL);
	BIND_ENUM_CONSTANT(MEGAMOREL);
	BIND_ENUM_CONSTANT(SIGNPOST);

	BIND_ENUM_CONSTANT(AZUREEL);
	BIND_ENUM_CONSTANT(COAL);
	BIND_ENUM_CONSTANT(GOLD);
	BIND_ENUM_CONSTANT(ILLEGIBLE_PARCHMENT);
	BIND_ENUM_CONSTANT(IMALADITE);
	BIND_ENUM_CONSTANT(IRON);
	BIND_ENUM_CONSTANT(NULLSCRAP);
	BIND_ENUM_CONSTANT(SALT);
	BIND_ENUM_CONSTANT(SILVER);
	BIND_ENUM_CONSTANT(SKELETON);
	BIND_ENUM_CONSTANT(STRANGE_COINS);
    ClassDB::bind_static_method("Tile", D_METHOD("get_tile", "tile"), &Tile::get_tile);
    ClassDB::bind_method(D_METHOD("get_foreground"), &Tile::get_foreground);
    ClassDB::bind_method(D_METHOD("get_variation", "rng"), &Tile::get_variation);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "state"), &Tile::get_atlas_coords, DEFVAL(Tile::MAX_STATE));
}

void Tile::init_layer_configs() {
    layer_configs.clear();
    prev_atlas_coord_y = 0;

    LocalVector<Ref<Tile>> background_layer;
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::BACKGROUND, 0, String("Dug"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        background_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::IDLE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 2));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(250);
		frame_durations_ms.push_back(250);
		frame_durations_ms.push_back(250);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::BACKGROUND, 1, String("Gas"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        background_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::IDLE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 1));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(500);
		frame_durations_ms.push_back(500);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::BACKGROUND, 2, String("Lava"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        background_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::BACKGROUND, 3, String("Sky"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        background_layer.push_back(t);
        tile_configs.push_back(t);
    }
    layer_configs.push_back(background_layer);

    LocalVector<Ref<Tile>> collision_layer;
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::COLLISION, 0, String("Boulder"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        collision_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::COLLISION, 1, String("Dirt"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, 1)));
        collision_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::COLLISION, 2, String("Dirt2"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        collision_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::COLLISION, 3, String("Rock"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        collision_layer.push_back(t);
        tile_configs.push_back(t);
    }
    layer_configs.push_back(collision_layer);

    LocalVector<Ref<Tile>> decoration_layer;
    {
        LocalVector<State> states;
		states.push_back(State::IDLE);
		states.push_back(State::ACTIVE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
		state_frame_ranges.push_back(Vector2i(1, 3));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(500);
		frame_durations_ms.push_back(75);
		frame_durations_ms.push_back(75);
		frame_durations_ms.push_back(75);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 0, String("Cloud"), states, state_frame_ranges, frame_durations_ms, 2, 2, -1, 4)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 1, String("Cloud2"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 2, String("Cloud3"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 3, String("Cloud4"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 4, String("Cloud5"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 5, String("Grass"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 6, String("Stalactite"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::DECORATION, 7, String("Stalagmite"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        decoration_layer.push_back(t);
        tile_configs.push_back(t);
    }
    layer_configs.push_back(decoration_layer);

    LocalVector<Ref<Tile>> foreground_layer;
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::FOREGROUND, 0, String("Signpost"), states, state_frame_ranges, frame_durations_ms, 1, 2, 0, -1)));
        foreground_layer.push_back(t);
        tile_configs.push_back(t);
    }
    layer_configs.push_back(foreground_layer);

    LocalVector<Ref<Tile>> interactable_layer;
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::INTERACTABLE, 0, String("Agrenic"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        interactable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::INTERACTABLE, 1, String("Crystal"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        interactable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::INTERACTABLE, 2, String("Megamorel"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        interactable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::INTERACTABLE, 3, String("Signpost"), states, state_frame_ranges, frame_durations_ms, 1, 1, 0, -1)));
        interactable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    layer_configs.push_back(interactable_layer);

    LocalVector<Ref<Tile>> mineable_layer;
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 0, String("Azureel"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 1, String("Coal"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 2, String("Gold"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 3, String("IllegibleParchment"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 4, String("Imaladite"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 5, String("Iron"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 6, String("Nullscrap"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 7, String("Salt"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 8, String("Silver"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 9, String("Skeleton"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    {
        LocalVector<State> states;
		states.push_back(State::NONE);
        LocalVector<Vector2i> state_frame_ranges;
		state_frame_ranges.push_back(Vector2i(0, 0));
        LocalVector<int32_t> frame_durations_ms;
		frame_durations_ms.push_back(100);
        Ref<Tile> t = Ref<Tile>(memnew(Tile(Layer::MINEABLE, 10, String("StrangeCoins"), states, state_frame_ranges, frame_durations_ms, 1, 1, -1, -1)));
        mineable_layer.push_back(t);
        tile_configs.push_back(t);
    }
    layer_configs.push_back(mineable_layer);

}

Ref<Tile> Tile::get_tile(int tile) { return tile_configs[tile]; }

Ref<Tile> Tile::get_foreground() {
    if (linked_foreground_i == -1) {
        return Ref<Tile>();
    }
    return tile_configs[linked_foreground_i];
}

const LocalVector<Ref<Tile>> &Tile::get_layer(Layer layer) { return layer_configs[static_cast<int>(layer)]; }

Ref<Tile> Tile::get_variation(Ref<RandomNumberGenerator> rng){
    int i { rng->randi_range(tile, tile + random_group_length - 1) };
    return tile_configs[i];
}

Vector2i Tile::get_atlas_coords(State p_state) {
    State target_state = (p_state == MAX_STATE) ? states[0] : p_state;
    for (uint32_t i = 0; i < states.size(); i++) {
        if (states[i] == target_state) {
            return state_atlas_coords[i];
        }
    }
    ERR_FAIL_V_MSG(Vector2i(), "get_atlas_coords: requested state is not in this tile's states list.");
}
