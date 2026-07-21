#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/math/vector2i.h"
#include "core/math/random_number_generator.h"
#include "core/string/ustring.h"
#include "core/error/error_macros.h"

using namespace godot;

class Tile : public RefCounted {
    GDCLASS(Tile, RefCounted);

protected:
    static void _bind_methods();

public:
    enum Layer {
		BACKGROUND,
		COLLISION,
		DECORATION,
		FOREGROUND,
		INTERACTABLE,
		MINEABLE,
        MAX_LAYER,
    };

    enum State {
		NONE,
		ACTIVE,
		IDLE,
        MAX_STATE,
    };

    enum Background {
		DUG = 0,
		GAS = 1,
		LAVA = 2,
		SKY = 3,
    };

    enum Collision {
		BOULDER = 4,
		DIRT = 5,
		DIRT2 = 6,
		ROCK = 7,
    };

    enum Decoration {
		CLOUD = 8,
		CLOUD2 = 9,
		CLOUD3 = 10,
		CLOUD4 = 11,
		CLOUD5 = 12,
		GRASS = 13,
		STALACTITE = 14,
		STALAGMITE = 15,
    };

    enum Foreground {
		FOREGROUND_SIGNPOST = 16,
    };

    enum Interactable {
		AGRENIC = 17,
		CRYSTAL = 18,
		MEGAMOREL = 19,
		SIGNPOST = 20,
    };

    enum Mineable {
		AZUREEL = 21,
		COAL = 22,
		GOLD = 23,
		ILLEGIBLE_PARCHMENT = 24,
		IMALADITE = 25,
		IRON = 26,
		NULLSCRAP = 27,
		SALT = 28,
		SILVER = 29,
		SKELETON = 30,
		STRANGE_COINS = 31,
    };

    Tile() {}
    Tile(
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
    );

    static constexpr int GRID_SIZE = 50;

    Layer layer = BACKGROUND;
    int tile = 0;
    String name;
    LocalVector<State> states;
    LocalVector<Vector2i> state_frame_ranges;
    LocalVector<Vector2i> state_atlas_coords;
    LocalVector<int32_t> frame_durations_ms;
    Vector2i g_size;
    int linked_foreground_i = -1;
    int random_group_length = -1;

    inline static int prev_atlas_coord_y = 0;

    inline static LocalVector<LocalVector<Ref<Tile>>> layer_configs;
    inline static LocalVector<Ref<Tile>> tile_configs;
    inline static void init_layer_configs();
    inline static Ref<Tile> get_tile(int tile);
    inline static const LocalVector<Ref<Tile>> &get_layer(Layer layer);
    Ref<Tile> get_foreground();
    Ref<Tile> get_variation(Ref<RandomNumberGenerator> rng);
    Vector2i get_atlas_coords(State p_state = MAX_STATE);
};

VARIANT_ENUM_CAST(Tile::Layer);
VARIANT_ENUM_CAST(Tile::State);
VARIANT_ENUM_CAST(Tile::Background);
VARIANT_ENUM_CAST(Tile::Collision);
VARIANT_ENUM_CAST(Tile::Decoration);
VARIANT_ENUM_CAST(Tile::Foreground);
VARIANT_ENUM_CAST(Tile::Interactable);
VARIANT_ENUM_CAST(Tile::Mineable);

