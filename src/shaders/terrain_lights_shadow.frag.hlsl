// terrain_lights_shadow.frag.hlsl — the torch-lit ground (terrain_lights.hlsli) that also receives the
// sun's shadow, as basic_textured_fog_shadow.frag does. Desktop only; pairs with terrain_lights.vert.
#define TERRAIN_LIGHTS_SUN_SHADOW 1
#include "terrain_lights.hlsli"
