#include "dock_mapinfo.h"
#include "../imgui_docks/dock_console.h"

DockMapInfo::DockMapInfo() {}

const char *DockMapInfo::label() {
	return "MapInfo";
}

extern qboolean	DISABLE_DEPTH_PREPASS;
extern qboolean	LODMODEL_MAP;
extern qboolean	DISABLE_MERGED_GLOWS;
extern qboolean	DISABLE_LIFTS_AND_PORTALS_MERGE;
extern int			MAP_MAX_VIS_RANGE;
extern qboolean	ENABLE_DISPLACEMENT_MAPPING;
extern float		DISPLACEMENT_MAPPING_STRENGTH;
extern qboolean	MAP_REFLECTION_ENABLED;
extern qboolean	DAY_NIGHT_CYCLE_ENABLED;
extern float		DAY_NIGHT_CYCLE_SPEED;
extern float		SUN_PHONG_SCALE;
extern vec3_t		SUN_COLOR_MAIN;
extern vec3_t		SUN_COLOR_SECONDARY;
extern vec3_t		SUN_COLOR_TERTIARY;
extern vec3_t		SUN_COLOR_AMBIENT;
extern int			LATE_LIGHTING_ENABLED;
extern qboolean	MAP_LIGHTMAP_DISABLED;
extern int			MAP_LIGHTMAP_ENHANCEMENT;
extern qboolean	MAP_USE_PALETTE_ON_SKY;
extern float		MAP_LIGHTMAP_MULTIPLIER;
extern vec3_t		MAP_AMBIENT_CSB;
extern vec3_t		MAP_AMBIENT_COLOR;
extern float		MAP_GLOW_MULTIPLIER;
extern vec3_t		MAP_AMBIENT_CSB_NIGHT;
extern vec3_t		MAP_AMBIENT_COLOR_NIGHT;
extern float		MAP_GLOW_MULTIPLIER_NIGHT;
extern float		SKY_LIGHTING_SCALE;
extern float		MAP_EMISSIVE_COLOR_SCALE;
extern float		MAP_EMISSIVE_COLOR_SCALE_NIGHT;
extern float		MAP_EMISSIVE_RADIUS_SCALE;
extern float		MAP_HDR_MIN;
extern float		MAP_HDR_MAX;
extern qboolean	AURORA_ENABLED;
extern qboolean	AURORA_ENABLED_DAY;
extern qboolean	AO_ENABLED;
extern qboolean	AO_BLUR;
extern qboolean	AO_DIRECTIONAL;
extern float		AO_MINBRIGHT;
extern float		AO_MULTBRIGHT;
extern qboolean	SHADOWS_ENABLED;
extern float		SHADOW_MINBRIGHT;
extern float		SHADOW_MAXBRIGHT;
extern qboolean	FOG_POST_ENABLED;
extern qboolean	FOG_STANDARD_ENABLE;
extern vec3_t		FOG_COLOR;
extern vec3_t		FOG_COLOR_SUN;
extern float		FOG_DENSITY;
extern float		FOG_ACCUMULATION_MODIFIER;
extern float		FOG_RANGE_MULTIPLIER;
extern qboolean	FOG_VOLUMETRIC_ENABLE;
extern float		FOG_VOLUMETRIC_DENSITY;
extern float		FOG_VOLUMETRIC_STRENGTH;
extern float		FOG_VOLUMETRIC_CLOUDINESS;
extern float		FOG_VOLUMETRIC_WIND;
extern float		FOG_VOLUMETRIC_VELOCITY;
extern vec3_t		FOG_VOLUMETRIC_COLOR;
extern qboolean	WATER_ENABLED;
extern qboolean	USE_OCEAN;
extern float		WATER_REFLECTIVENESS;
extern qboolean	WATER_FOG_ENABLED;
extern vec3_t		WATER_COLOR_SHALLOW;
extern vec3_t		WATER_COLOR_DEEP;
extern qboolean	GRASS_ENABLED;
extern int			GRASS_DENSITY;
extern float		GRASS_HEIGHT;
extern int			GRASS_DISTANCE;
extern float		GRASS_TYPE_UNIFORMALITY;
extern float		GRASS_DISTANCE_FROM_ROADS;
extern qboolean	PEBBLES_ENABLED;
extern int			PEBBLES_DENSITY;
extern int			PEBBLES_DISTANCE;
extern vec3_t		MOON_COLOR;
extern vec3_t		MOON_ATMOSPHERE_COLOR;
extern float		MOON_GLOW_STRENGTH;
extern float		MOON_ROTATION_RATE;
extern char		ROAD_TEXTURE[256];
extern qboolean	JKA_WEATHER_ENABLED;
extern qboolean	WZ_WEATHER_ENABLED;
extern qboolean	WZ_WEATHER_SOUND_ONLY;
extern float	MAP_WATER_LEVEL;

// todo
extern char		CURRENT_CLIMATE_OPTION[256];
extern char		CURRENT_WEATHER_OPTION[256];
#ifdef __OCEAN__
extern qboolean WATER_INITIALIZED;
extern qboolean WATER_FAST_INITIALIZED;
#endif //__OCEAN__

namespace ImGui {
	// roses are red, qboolean is no bool
	bool Checkbox(char *label, qboolean *var) {
		bool tmp = *var;
		bool ret = ImGui::Checkbox(label, &tmp);
		*var = (qboolean) tmp;
		return ret;
	}
}

void DockMapInfo::imgui() {
	ImGui::Checkbox("DISABLE_DEPTH_PREPASS", &DISABLE_DEPTH_PREPASS);
	ImGui::Checkbox("LODMODEL_MAP", &LODMODEL_MAP);
	ImGui::Checkbox("DISABLE_MERGED_GLOWS", &DISABLE_MERGED_GLOWS);
	ImGui::Checkbox("DISABLE_LIFTS_AND_PORTALS_MERGE", &DISABLE_LIFTS_AND_PORTALS_MERGE);
	ImGui::DragInt("MAP_MAX_VIS_RANGE", &MAP_MAX_VIS_RANGE);

	if (ImGui::CollapsingHeader("Displacement Mapping")) {
		ImGui::Checkbox("ENABLE_DISPLACEMENT_MAPPING", &ENABLE_DISPLACEMENT_MAPPING);
		ImGui::DragFloat("DISPLACEMENT_MAPPING_STRENGTH", &DISPLACEMENT_MAPPING_STRENGTH);
	}

	ImGui::Checkbox("MAP_REFLECTION_ENABLED", &MAP_REFLECTION_ENABLED);

	if (ImGui::CollapsingHeader("Day/Night Cycle")) {
		ImGui::Checkbox("DAY_NIGHT_CYCLE_ENABLED", &DAY_NIGHT_CYCLE_ENABLED);
		ImGui::DragFloat("DAY_NIGHT_CYCLE_SPEED", &DAY_NIGHT_CYCLE_SPEED);
	}

	if (ImGui::CollapsingHeader("Sun")) {
		ImGui::DragFloat("SUN_PHONG_SCALE", &SUN_PHONG_SCALE);
		ImGui::DragFloat3("SUN_COLOR_MAIN", SUN_COLOR_MAIN);
		ImGui::DragFloat3("SUN_COLOR_SECONDARY", SUN_COLOR_SECONDARY);
		ImGui::DragFloat3("SUN_COLOR_TERTIARY", SUN_COLOR_TERTIARY);
		ImGui::DragFloat3("SUN_COLOR_AMBIENT", SUN_COLOR_AMBIENT);
	}

	if (ImGui::CollapsingHeader("Map stuff:")) {
		ImGui::DragInt("LATE_LIGHTING_ENABLED", &LATE_LIGHTING_ENABLED);
		ImGui::Checkbox("MAP_LIGHTMAP_DISABLED", &MAP_LIGHTMAP_DISABLED);
		ImGui::DragInt("MAP_LIGHTMAP_ENHANCEMENT", &MAP_LIGHTMAP_ENHANCEMENT);
		ImGui::Checkbox("MAP_USE_PALETTE_ON_SKY", &MAP_USE_PALETTE_ON_SKY);
		ImGui::DragFloat("MAP_LIGHTMAP_MULTIPLIER", &MAP_LIGHTMAP_MULTIPLIER);
		ImGui::DragFloat3("MAP_AMBIENT_CSB", MAP_AMBIENT_CSB);
		ImGui::DragFloat3("MAP_AMBIENT_COLOR", MAP_AMBIENT_COLOR);
		ImGui::DragFloat("MAP_GLOW_MULTIPLIER", &DAY_NIGHT_CYCLE_SPEED);
		ImGui::DragFloat3("MAP_AMBIENT_CSB_NIGHT", MAP_AMBIENT_CSB_NIGHT);
		ImGui::DragFloat3("MAP_AMBIENT_COLOR_NIGHT", MAP_AMBIENT_COLOR_NIGHT);
		ImGui::DragFloat("MAP_GLOW_MULTIPLIER_NIGHT", &MAP_GLOW_MULTIPLIER_NIGHT);
		ImGui::DragFloat("SKY_LIGHTING_SCALE", &SKY_LIGHTING_SCALE);
		ImGui::DragFloat("MAP_EMISSIVE_COLOR_SCALE", &MAP_EMISSIVE_COLOR_SCALE);
		ImGui::DragFloat("MAP_EMISSIVE_COLOR_SCALE_NIGHT", &MAP_EMISSIVE_COLOR_SCALE_NIGHT);
		ImGui::DragFloat("MAP_EMISSIVE_RADIUS_SCALE", &MAP_EMISSIVE_RADIUS_SCALE);
		ImGui::DragFloat("MAP_HDR_MIN", &MAP_HDR_MIN);
		ImGui::DragFloat("MAP_HDR_MAX", &MAP_HDR_MAX);
		ImGui::Checkbox("AURORA_ENABLED", &AURORA_ENABLED);
		ImGui::Checkbox("AURORA_ENABLED_DAY", &AURORA_ENABLED_DAY);
	}

	if (ImGui::CollapsingHeader("Ambient Occlusion")) {
		ImGui::Checkbox("AO_ENABLED", &AO_ENABLED);
		ImGui::Checkbox("AO_BLUR", &AO_BLUR);
		ImGui::Checkbox("AO_DIRECTIONAL", &AO_DIRECTIONAL);
		ImGui::DragFloat("AO_MINBRIGHT", &AO_MINBRIGHT);
		ImGui::DragFloat("AO_MULTBRIGHT", &AO_MULTBRIGHT);
	}

	if (ImGui::CollapsingHeader("Shadows")) {
		ImGui::Checkbox("SHADOWS_ENABLED", &SHADOWS_ENABLED);
		ImGui::DragFloat("SHADOW_MINBRIGHT", &SHADOW_MINBRIGHT);
		ImGui::DragFloat("SHADOW_MAXBRIGHT", &SHADOW_MAXBRIGHT);
	}

	if (ImGui::CollapsingHeader("Fog")) {
		ImGui::Checkbox("FOG_POST_ENABLED", &FOG_POST_ENABLED);
		ImGui::Checkbox("FOG_STANDARD_ENABLE", &FOG_STANDARD_ENABLE);
		ImGui::DragFloat3("FOG_COLOR", FOG_COLOR);
		ImGui::DragFloat3("FOG_COLOR_SUN", FOG_COLOR_SUN);
		ImGui::DragFloat("FOG_DENSITY", &FOG_DENSITY);
		ImGui::DragFloat("FOG_ACCUMULATION_MODIFIER", &FOG_ACCUMULATION_MODIFIER);
		ImGui::DragFloat("FOG_RANGE_MULTIPLIER", &FOG_RANGE_MULTIPLIER);
		ImGui::Checkbox("FOG_VOLUMETRIC_ENABLE", &FOG_VOLUMETRIC_ENABLE);
		ImGui::DragFloat("FOG_VOLUMETRIC_DENSITY", &FOG_VOLUMETRIC_DENSITY);
		ImGui::DragFloat("FOG_VOLUMETRIC_STRENGTH", &FOG_VOLUMETRIC_STRENGTH);
		ImGui::DragFloat("FOG_VOLUMETRIC_CLOUDINESS", &FOG_VOLUMETRIC_CLOUDINESS);
		ImGui::DragFloat("FOG_VOLUMETRIC_WIND", &FOG_VOLUMETRIC_WIND);
		ImGui::DragFloat("FOG_VOLUMETRIC_VELOCITY", &FOG_VOLUMETRIC_VELOCITY);
		ImGui::DragFloat3("FOG_VOLUMETRIC_COLOR", FOG_VOLUMETRIC_COLOR);
	}

	if (ImGui::CollapsingHeader("Water stuff")) {
		ImGui::Checkbox("WATER_ENABLED", &WATER_ENABLED);
		ImGui::Checkbox("USE_OCEAN", &USE_OCEAN);
		ImGui::DragFloat("WATER_REFLECTIVENESS", &WATER_REFLECTIVENESS);
		ImGui::Checkbox("WATER_FOG_ENABLED", &WATER_FOG_ENABLED);
		ImGui::DragFloat3("WATER_COLOR_SHALLOW", WATER_COLOR_SHALLOW);
		ImGui::DragFloat3("WATER_COLOR_DEEP", WATER_COLOR_DEEP);
		ImGui::DragFloat("MAP_WATER_LEVEL", &MAP_WATER_LEVEL);
	}

	if (ImGui::CollapsingHeader("Grass")) {
		ImGui::Checkbox("GRASS_ENABLED", &GRASS_ENABLED);
		ImGui::DragInt("GRASS_DENSITY", &GRASS_DENSITY);
		ImGui::DragFloat("GRASS_HEIGHT", &GRASS_HEIGHT);
		ImGui::DragInt("GRASS_DISTANCE", &GRASS_DISTANCE);
		ImGui::DragFloat("GRASS_TYPE_UNIFORMALITY", &GRASS_TYPE_UNIFORMALITY);
		ImGui::DragFloat("GRASS_DISTANCE_FROM_ROADS", &GRASS_DISTANCE_FROM_ROADS);
	}

	if (ImGui::CollapsingHeader("Pebbles")) {
		ImGui::Checkbox("PEBBLES_ENABLED", &PEBBLES_ENABLED);
		ImGui::DragInt("PEBBLES_DENSITY", &PEBBLES_DENSITY);
		ImGui::DragInt("PEBBLES_DISTANCE", &PEBBLES_DISTANCE);
	}

	if (ImGui::CollapsingHeader("Moon")) {
		ImGui::DragFloat3("MOON_COLOR", MOON_COLOR);
		ImGui::DragFloat3("MOON_ATMOSPHERE_COLOR", MOON_ATMOSPHERE_COLOR);
		ImGui::DragFloat("MOON_GLOW_STRENGTH", &MOON_GLOW_STRENGTH);
		ImGui::DragFloat("MOON_ROTATION_RATE", &MOON_ROTATION_RATE);
	}

	if (ImGui::CollapsingHeader("Roads")) {
		ImGui::InputText("ROAD_TEXTURE", ROAD_TEXTURE, sizeof(ROAD_TEXTURE));
	}

	if (ImGui::CollapsingHeader("Weather")) {
		ImGui::Checkbox("JKA_WEATHER_ENABLED", &JKA_WEATHER_ENABLED);
		ImGui::Checkbox("WZ_WEATHER_ENABLED", &WZ_WEATHER_ENABLED);
		ImGui::Checkbox("WZ_WEATHER_SOUND_ONLY", &WZ_WEATHER_SOUND_ONLY);
	}
}