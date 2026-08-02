#pragma once

#include "core/object/ref_counted.h"
#include "core/math/random_number_generator.h"
#include "core/variant/typed_array.h"

#include <array>

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
		DIRT3 = 7,
		ROCK = 8,
    };
    enum Decoration {
		CLOUD = 9,
		CLOUD2 = 10,
		CLOUD3 = 11,
		CLOUD4 = 12,
		CLOUD5 = 13,
		GRASS = 14,
		STALACTITE = 15,
		STALAGMITE = 16,
		TEST = 17,
    };
    enum Foreground {
		FOREGROUND_SIGNPOST = 18,
    };
    enum Interactable {
		AGRENIC = 19,
		CRYSTAL = 20,
		MEGAMOREL = 21,
		SIGNPOST = 22,
    };
    enum Mineable {
		AZUREEL = 23,
		COAL = 24,
		GOLD = 25,
		ILLEGIBLE_PARCHMENT = 26,
		IMALADITE = 27,
		IRON = 28,
		NULLSCRAP = 29,
		SALT = 30,
		SILVER = 31,
		SKELETON = 32,
		STRANGE_COINS = 33,
    };

    inline static constexpr int MAX_TILE{34};
    
    Tile() {}
    Tile(
        Layer p_layer, 
        int p_tile, 
        const String &p_name, 
        const LocalVector<State> &p_states, 
        const LocalVector<Vector2i> &p_state_frame_ranges, 
        PackedInt32Array &p_frame_durations_ms, 
        int p_width, 
        int p_height, 
        int p_linked_foreground_enum, 
        int p_random_group_length
    );

public:
    inline static constexpr int BASE_SIZE{50};
    inline static constexpr int BASE_EXTENT{25};
    inline static int prev_atlas_coord_y = 0;
    inline static LocalVector<LocalVector<Ref<Tile>>> layer_configs;
    inline static LocalVector<Ref<Tile>> tile_configs;
    inline static const PackedStringArray layer_names{ "background", "collision", "decoration", "foreground", "interactable", "mineable" };
    inline static std::array<HashMap<Vector2i, Ref<Tile>>, MAX_LAYER> atlas_coord_to_tile;
    inline static std::array<int, MAX_TILE> random_group_lengths;
    inline static std::array<Vector2i, MAX_TILE> tile_sizes;
    
    Layer layer = BACKGROUND;
    int tile = 0;
    String name;
    LocalVector<State> states;
    LocalVector<Vector2i> state_frame_ranges;
    LocalVector<Vector2i> state_atlas_coords;
    PackedInt32Array frame_durations_ms; 
    Vector2i g_size;
    int linked_foreground_i = -1;
    int random_group_length = -1;
    
public: 
    inline static Ref<Tile> get_atlas_coord_tile(Layer layer_i, Vector2i atlas_coord);
    static void init_layer_configs();
    static Ref<Tile> get_tile(int tile);
    inline static String get_layer_name(Layer layer_i);
    inline static const LocalVector<Ref<Tile>> &get_layer(Layer layer_i);
    inline static TypedArray<Tile> get_layer_tiles(Tile::Layer layer_i);
    static int get_variation_i(Ref<RandomNumberGenerator> rng, int tile_i);
    static Vector2i get_tile_size(int tile_i);
    
    virtual String to_string() override { 
        return vformat("Tile(tile_name:%s, tile_i:%d)", name, tile); 
    }
    String get_name() const;
    Ref<Tile> get_foreground();
    Ref<Tile> get_variation(Ref<RandomNumberGenerator> rng);
    Vector2i get_g_size() const;
    Layer get_layer_e() const;
    int get_tile_e() const;
    PackedInt32Array get_frame_durations_ms() const;
    Vector2i get_atlas_coords(State p_state = MAX_STATE);
    PackedInt32Array get_states() const;
    TypedArray<Vector2i> get_state_frame_ranges() const;
};

VARIANT_ENUM_CAST(Tile::Layer);
VARIANT_ENUM_CAST(Tile::State);
VARIANT_ENUM_CAST(Tile::Background);
VARIANT_ENUM_CAST(Tile::Collision);
VARIANT_ENUM_CAST(Tile::Decoration);
VARIANT_ENUM_CAST(Tile::Foreground);
VARIANT_ENUM_CAST(Tile::Interactable);
VARIANT_ENUM_CAST(Tile::Mineable);

