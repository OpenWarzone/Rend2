#include "tr_local.h"
#include "../cgame/cg_public.h"
#if 0
#include <cstdio>
#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>

#include "../Recast/Recast/Recast.h"
#include "../Recast/InputGeom.h"
#include "../Recast/NavMeshGenerate.h"
//#include "../Recast/Sample_TileMesh.h"
//#include "../Recast/Sample_TempObstacles.h"
//#include "../Recast/Sample_Debug.h"
#endif

extern char currentMapName[128];

extern	world_t		s_worldData;

extern image_t	*R_FindImageFile(const char *name, imgType_t type, int flags);

#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_TERRAIN)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_ALL				(0xFFFFFFFFu)

#define MAP_INFO_TRACEMAP_SIZE 2048//4096

vec3_t  MAP_INFO_MINS;
vec3_t  MAP_INFO_MAXS;
vec3_t	MAP_INFO_SIZE;
vec3_t	MAP_INFO_PIXELSIZE;
vec3_t	MAP_INFO_SCATTEROFFSET;
float	MAP_INFO_MAXSIZE;
vec3_t  MAP_INFO_PLAYABLE_MINS;
vec3_t  MAP_INFO_PLAYABLE_MAXS;
vec3_t  MAP_INFO_PLAYABLE_SIZE;

void Mapping_Trace(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const int passEntityNum, const int contentmask)
{
	results->entityNum = ENTITYNUM_NONE;
	ri->CM_BoxTrace(results, start, end, mins, maxs, 0, contentmask, 0);
	results->entityNum = results->fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
}

void R_SetupMapInfo(void)
{
	world_t	*w;
	int i;

	VectorSet(MAP_INFO_MINS, 128000, 128000, 128000);
	VectorSet(MAP_INFO_MAXS, -128000, -128000, -128000);
	VectorSet(MAP_INFO_PLAYABLE_MINS, 128000, 128000, 128000);
	VectorSet(MAP_INFO_PLAYABLE_MAXS, -128000, -128000, -128000);

	w = &s_worldData;

	// Find the map min/maxs...
	for (i = 0; i < w->numsurfaces; i++)
	{
		msurface_t *surf = &w->surfaces[i];

		if (surf->cullinfo.type & CULLINFO_SPHERE)
		{
			vec3_t surfOrigin;

			VectorCopy(surf->cullinfo.localOrigin, surfOrigin);

			AddPointToBounds(surfOrigin, MAP_INFO_MINS, MAP_INFO_MAXS);

			if (!surf->shader->isSky)
			{
				AddPointToBounds(surfOrigin, MAP_INFO_PLAYABLE_MINS, MAP_INFO_PLAYABLE_MAXS);
			}
		}
		else if (surf->cullinfo.type & CULLINFO_BOX)
		{
			if (surf->cullinfo.bounds[0][0] < MAP_INFO_MINS[0])
				MAP_INFO_MINS[0] = surf->cullinfo.bounds[0][0];
			if (surf->cullinfo.bounds[0][0] > MAP_INFO_MAXS[0])
				MAP_INFO_MAXS[0] = surf->cullinfo.bounds[0][0];

			if (surf->cullinfo.bounds[1][0] < MAP_INFO_MINS[0])
				MAP_INFO_MINS[0] = surf->cullinfo.bounds[1][0];
			if (surf->cullinfo.bounds[1][0] > MAP_INFO_MAXS[0])
				MAP_INFO_MAXS[0] = surf->cullinfo.bounds[1][0];

			if (surf->cullinfo.bounds[0][1] < MAP_INFO_MINS[1])
				MAP_INFO_MINS[1] = surf->cullinfo.bounds[0][1];
			if (surf->cullinfo.bounds[0][1] > MAP_INFO_MAXS[1])
				MAP_INFO_MAXS[1] = surf->cullinfo.bounds[0][1];

			if (surf->cullinfo.bounds[1][1] < MAP_INFO_MINS[1])
				MAP_INFO_MINS[1] = surf->cullinfo.bounds[1][1];
			if (surf->cullinfo.bounds[1][1] > MAP_INFO_MAXS[1])
				MAP_INFO_MAXS[1] = surf->cullinfo.bounds[1][1];

			if (surf->cullinfo.bounds[0][2] < MAP_INFO_MINS[2])
				MAP_INFO_MINS[2] = surf->cullinfo.bounds[0][2];
			if (surf->cullinfo.bounds[0][2] > MAP_INFO_MAXS[2])
				MAP_INFO_MAXS[2] = surf->cullinfo.bounds[0][2];

			if (surf->cullinfo.bounds[1][2] < MAP_INFO_MINS[2])
				MAP_INFO_MINS[2] = surf->cullinfo.bounds[1][2];
			if (surf->cullinfo.bounds[1][2] > MAP_INFO_MAXS[2])
				MAP_INFO_MAXS[2] = surf->cullinfo.bounds[1][2];

			if (!surf->shader->isSky)
			{
				if (surf->cullinfo.bounds[0][0] < MAP_INFO_PLAYABLE_MINS[0])
					MAP_INFO_PLAYABLE_MINS[0] = surf->cullinfo.bounds[0][0];
				if (surf->cullinfo.bounds[0][0] > MAP_INFO_PLAYABLE_MAXS[0])
					MAP_INFO_PLAYABLE_MAXS[0] = surf->cullinfo.bounds[0][0];

				if (surf->cullinfo.bounds[1][0] < MAP_INFO_PLAYABLE_MINS[0])
					MAP_INFO_PLAYABLE_MINS[0] = surf->cullinfo.bounds[1][0];
				if (surf->cullinfo.bounds[1][0] > MAP_INFO_PLAYABLE_MAXS[0])
					MAP_INFO_PLAYABLE_MAXS[0] = surf->cullinfo.bounds[1][0];

				if (surf->cullinfo.bounds[0][1] < MAP_INFO_PLAYABLE_MINS[1])
					MAP_INFO_PLAYABLE_MINS[1] = surf->cullinfo.bounds[0][1];
				if (surf->cullinfo.bounds[0][1] > MAP_INFO_PLAYABLE_MAXS[1])
					MAP_INFO_PLAYABLE_MAXS[1] = surf->cullinfo.bounds[0][1];

				if (surf->cullinfo.bounds[1][1] < MAP_INFO_PLAYABLE_MINS[1])
					MAP_INFO_PLAYABLE_MINS[1] = surf->cullinfo.bounds[1][1];
				if (surf->cullinfo.bounds[1][1] > MAP_INFO_PLAYABLE_MAXS[1])
					MAP_INFO_PLAYABLE_MAXS[1] = surf->cullinfo.bounds[1][1];

				if (surf->cullinfo.bounds[0][2] < MAP_INFO_PLAYABLE_MINS[2])
					MAP_INFO_PLAYABLE_MINS[2] = surf->cullinfo.bounds[0][2];
				if (surf->cullinfo.bounds[0][2] > MAP_INFO_PLAYABLE_MAXS[2])
					MAP_INFO_PLAYABLE_MAXS[2] = surf->cullinfo.bounds[0][2];

				if (surf->cullinfo.bounds[1][2] < MAP_INFO_PLAYABLE_MINS[2])
					MAP_INFO_PLAYABLE_MINS[2] = surf->cullinfo.bounds[1][2];
				if (surf->cullinfo.bounds[1][2] > MAP_INFO_PLAYABLE_MAXS[2])
					MAP_INFO_PLAYABLE_MAXS[2] = surf->cullinfo.bounds[1][2];
			}
		}
	}

	// Move in from map edges a bit before starting traces...
	MAP_INFO_MINS[0] += 64.0;
	MAP_INFO_MINS[1] += 64.0;
	MAP_INFO_MINS[2] += 64.0;

	MAP_INFO_MAXS[0] -= 64.0;
	MAP_INFO_MAXS[1] -= 64.0;
	MAP_INFO_MAXS[2] -= 64.0;

	if (MAP_INFO_MINS[0] == 0.0 && MAP_INFO_MINS[1] == 0.0 && MAP_INFO_MINS[2] == 0.0
		&& MAP_INFO_MAXS[0] == 0.0 && MAP_INFO_MAXS[1] == 0.0 && MAP_INFO_MAXS[2] == 0.0)
	{// Fallback in case the map size is not found from the surfaces above...
		ri->Printf(PRINT_WARNING, "Couldn't find map size from map surfaces. Terrain? Using max.\n");
		VectorSet(MAP_INFO_MINS, -128000, -128000, -128000);
		VectorSet(MAP_INFO_MAXS, 128000, 128000, 128000);
	}

	memset(MAP_INFO_SIZE, 0, sizeof(MAP_INFO_SIZE));
	memset(MAP_INFO_PIXELSIZE, 0, sizeof(MAP_INFO_PIXELSIZE));
	memset(MAP_INFO_SCATTEROFFSET, 0, sizeof(MAP_INFO_SCATTEROFFSET));
	MAP_INFO_MAXSIZE = 0.0;

	MAP_INFO_SIZE[0] = MAP_INFO_MAXS[0] - MAP_INFO_MINS[0];
	MAP_INFO_SIZE[1] = MAP_INFO_MAXS[1] - MAP_INFO_MINS[1];
	MAP_INFO_SIZE[2] = MAP_INFO_MAXS[2] - MAP_INFO_MINS[2];
	MAP_INFO_PIXELSIZE[0] = MAP_INFO_TRACEMAP_SIZE / MAP_INFO_SIZE[0];
	MAP_INFO_PIXELSIZE[1] = MAP_INFO_TRACEMAP_SIZE / MAP_INFO_SIZE[1];
	MAP_INFO_SCATTEROFFSET[0] = MAP_INFO_SIZE[0] / MAP_INFO_TRACEMAP_SIZE;
	MAP_INFO_SCATTEROFFSET[1] = MAP_INFO_SIZE[1] / MAP_INFO_TRACEMAP_SIZE;

	MAP_INFO_PLAYABLE_SIZE[0] = MAP_INFO_PLAYABLE_MAXS[0] - MAP_INFO_PLAYABLE_MINS[0];
	MAP_INFO_PLAYABLE_SIZE[1] = MAP_INFO_PLAYABLE_MAXS[1] - MAP_INFO_PLAYABLE_MINS[1];
	MAP_INFO_PLAYABLE_SIZE[2] = MAP_INFO_PLAYABLE_MAXS[2] - MAP_INFO_PLAYABLE_MINS[2];

	MAP_INFO_MAXSIZE = MAP_INFO_SIZE[0];
	if (MAP_INFO_SIZE[1] > MAP_INFO_MAXSIZE) MAP_INFO_MAXSIZE = MAP_INFO_SIZE[1];

	ri->Printf(PRINT_WARNING, "MAPINFO: maxsze: %.2f. size: %.2f %.2f %.2f. mins: %.2f %.2f %.2f. maxs: %.2f %.2f %.2f.\n",
		MAP_INFO_MAXSIZE,
		MAP_INFO_SIZE[0], MAP_INFO_SIZE[1], MAP_INFO_SIZE[2],
		MAP_INFO_MINS[0], MAP_INFO_MINS[1], MAP_INFO_MINS[2],
		MAP_INFO_MAXS[0], MAP_INFO_MAXS[1], MAP_INFO_MAXS[2]);

	{
		char mapname[256] = { 0 };

		// because JKA uses mp/ dir, why??? so pointless...
		if (IniExists(va("maps/%s.mapInfo", currentMapName)))
			sprintf(mapname, "maps/%s.mapInfo", currentMapName);
		else if (IniExists(va("maps/mp/%s.mapInfo", currentMapName)))
			sprintf(mapname, "maps/mp/%s.mapInfo", currentMapName);
		else
			sprintf(mapname, "maps/%s.mapInfo", currentMapName);

		vec3_t	mapMins, mapMaxs;
		qboolean mapcoordsValid = qfalse;

		//
		// Try to load previously stored bounds from .mapInfo file...
		//
		mapMins[0] = atof(IniRead(mapname, "BOUNDS", "MINS0", "999999.0"));
		mapMins[1] = atof(IniRead(mapname, "BOUNDS", "MINS1", "999999.0"));
		mapMins[2] = atof(IniRead(mapname, "BOUNDS", "MINS2", "999999.0"));

		mapMaxs[0] = atof(IniRead(mapname, "BOUNDS", "MAXS0", "-999999.0"));
		mapMaxs[1] = atof(IniRead(mapname, "BOUNDS", "MAXS1", "-999999.0"));
		mapMaxs[2] = atof(IniRead(mapname, "BOUNDS", "MAXS2", "-999999.0"));

		vec3_t mapPlayableMins, mapPlayableMaxs, mapPlayableSize;

		mapPlayableMins[0] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_MINS0", "999999.0"));
		mapPlayableMins[1] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_MINS1", "999999.0"));
		mapPlayableMins[2] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_MINS2", "999999.0"));

		mapPlayableMaxs[0] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_MAXS0", "-999999.0"));
		mapPlayableMaxs[1] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_MAXS1", "-999999.0"));
		mapPlayableMaxs[2] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_MAXS2", "-999999.0"));

		mapPlayableSize[0] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_SIZE0", "-999999.0"));
		mapPlayableSize[1] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_SIZE1", "-999999.0"));
		mapPlayableSize[2] = atof(IniRead(mapname, "BOUNDS", "PLAYABLE_SIZE2", "-999999.0"));

		if (mapMins[0] < 999999.0 && mapMins[1] < 999999.0 && mapMins[2] < 999999.0
			&& mapMaxs[0] > -999999.0 && mapMaxs[1] > -999999.0 && mapMaxs[2] > -999999.0
			&& mapPlayableMins[0] < 999999.0 && mapPlayableMins[1] < 999999.0 && mapPlayableMins[2] < 999999.0
			&& mapPlayableMaxs[0] > -999999.0 && mapPlayableMaxs[1] > -999999.0 && mapPlayableMaxs[2] > -999999.0
			&& mapPlayableSize[0] > -999999.0 && mapPlayableSize[1] > -999999.0 && mapPlayableSize[2] > -999999.0)
		{
			mapcoordsValid = qtrue;
		}

		if (!mapcoordsValid)
		{// Write bounds we found to mapInfo file for this map...
		 //
		 // Write newly created info to our map's .mapInfo file for future usage...
		 //

			IniWrite(mapname, "BOUNDS", "MINS0", va("%f", MAP_INFO_MINS[0]));
			IniWrite(mapname, "BOUNDS", "MINS1", va("%f", MAP_INFO_MINS[1]));
			IniWrite(mapname, "BOUNDS", "MINS2", va("%f", MAP_INFO_MINS[2]));

			IniWrite(mapname, "BOUNDS", "MAXS0", va("%f", MAP_INFO_MAXS[0]));
			IniWrite(mapname, "BOUNDS", "MAXS1", va("%f", MAP_INFO_MAXS[1]));
			IniWrite(mapname, "BOUNDS", "MAXS2", va("%f", MAP_INFO_MAXS[2]));

			IniWrite(mapname, "BOUNDS", "PLAYABLE_MINS0", va("%f", MAP_INFO_PLAYABLE_MINS[0]));
			IniWrite(mapname, "BOUNDS", "PLAYABLE_MINS1", va("%f", MAP_INFO_PLAYABLE_MINS[1]));
			IniWrite(mapname, "BOUNDS", "PLAYABLE_MINS2", va("%f", MAP_INFO_PLAYABLE_MINS[2]));

			IniWrite(mapname, "BOUNDS", "PLAYABLE_MAXS0", va("%f", MAP_INFO_PLAYABLE_MAXS[0]));
			IniWrite(mapname, "BOUNDS", "PLAYABLE_MAXS1", va("%f", MAP_INFO_PLAYABLE_MAXS[1]));
			IniWrite(mapname, "BOUNDS", "PLAYABLE_MAXS2", va("%f", MAP_INFO_PLAYABLE_MAXS[2]));

			IniWrite(mapname, "BOUNDS", "PLAYABLE_SIZE0", va("%f", MAP_INFO_PLAYABLE_SIZE[0]));
			IniWrite(mapname, "BOUNDS", "PLAYABLE_SIZE1", va("%f", MAP_INFO_PLAYABLE_SIZE[1]));
			IniWrite(mapname, "BOUNDS", "PLAYABLE_SIZE2", va("%f", MAP_INFO_PLAYABLE_SIZE[2]));
		}
	}
}

void R_CreateDefaultDetail(void)
{
	byte	data;
	int		i = 0;

	// write tga
	fileHandle_t f;

	f = ri->FS_FOpenFileWrite("gfx/defaultDetail.tga", qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++) {
		for (int j = 0; j < MAP_INFO_TRACEMAP_SIZE; j++) {
			int pixel = irand(96, 160);
			data = pixel; ri->FS_Write(&data, sizeof(data), f);	// b
			data = pixel; ri->FS_Write(&data, sizeof(data), f);	// g
			data = pixel; ri->FS_Write(&data, sizeof(data), f);	// r
			data = 1.0; ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);
}

void R_CreateRandom2KImage(char *variation)
{
	byte	data;
	int		i = 0;

	// write tga
	fileHandle_t f;

	if (!strcmp(variation, "splatControl"))
		f = ri->FS_FOpenFileWrite("gfx/splatControlImage.tga", qfalse);
	else
		f = ri->FS_FOpenFileWrite(va("gfx/random2K%s.tga", variation), qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++) {
		for (int j = 0; j < MAP_INFO_TRACEMAP_SIZE; j++) {
			data = irand(0, 255); ri->FS_Write(&data, sizeof(data), f);	// b
			data = irand(0, 255); ri->FS_Write(&data, sizeof(data), f);	// g
			data = irand(0, 255); ri->FS_Write(&data, sizeof(data), f);	// r
			data = irand(0, 255); ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);
}

#if 0
void R_CreateGrassImages(void)
{
#define GRASS_TEX_SCALE 200
#define GRASS_NUM_MASKS 10

	//
	// Generate mask images...
	//

	UINT8	*red[GRASS_TEX_SCALE];
	UINT8	*green[GRASS_TEX_SCALE];
	UINT8	*blue[GRASS_TEX_SCALE];
	UINT8	*alpha[GRASS_TEX_SCALE];

	for (int i = 0; i < GRASS_TEX_SCALE; i++)
	{
		red[i] = (UINT8*)malloc(sizeof(UINT8)*GRASS_TEX_SCALE);
		green[i] = (UINT8*)malloc(sizeof(UINT8)*GRASS_TEX_SCALE);
		blue[i] = (UINT8*)malloc(sizeof(UINT8)*GRASS_TEX_SCALE);
		alpha[i] = (UINT8*)malloc(sizeof(UINT8)*GRASS_TEX_SCALE);

		memset(red[i], 0, sizeof(UINT8)*GRASS_TEX_SCALE);
		memset(green[i], 0, sizeof(UINT8)*GRASS_TEX_SCALE);
		memset(blue[i], 0, sizeof(UINT8)*GRASS_TEX_SCALE);
		memset(alpha[i], 0, sizeof(UINT8)*GRASS_TEX_SCALE);
	}

	//Load NUM_DIFFERENT_LAYERS alpha maps into the material.
	//Each one has the same pixel pattern, but a few less total pixels.
	//This way, the grass gradually thins as it gets to the top.
	for (int l = 0; l < GRASS_NUM_MASKS; l++)
	{
		//Thin the density as it approaches the top layer
		//The bottom layer will have 1000, the top layer 100.
		float density = l / (float)60;
		int numGrass = (int)(4000 - ((3500 * density) + 500));

		//Generate the points
		for (int j = 0; j < numGrass; j++)
		{
			int curPointX = irand(0, GRASS_TEX_SCALE - 1);
			int curPointY = irand(0, GRASS_TEX_SCALE - 1);

			green[curPointX][curPointY] = (1 - (density * 255));
		}

		// Hopefully now we have a map image... Save it...
		byte	data;
		int		i = 0;

		// write tga
		fileHandle_t f = ri->FS_FOpenFileWrite(va("grassImage/grassMask%i.tga", (GRASS_NUM_MASKS - 1) - l), qfalse);

		// header
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
		data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
		data = GRASS_TEX_SCALE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
		data = GRASS_TEX_SCALE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
		data = GRASS_TEX_SCALE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
		data = GRASS_TEX_SCALE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
		data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
		data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

		for (int x = 0; x < GRASS_TEX_SCALE; x++) {
			for (int y = 0; y < GRASS_TEX_SCALE; y++) {
				data = 0; ri->FS_Write(&data, sizeof(data), f);	// b
				data = green[x][y]; ri->FS_Write(&data, sizeof(data), f);	// g
				data = 0; ri->FS_Write(&data, sizeof(data), f);	// r
				data = 255; ri->FS_Write(&data, sizeof(data), f);	// a
			}
		}

		// footer
		i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
		i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
		ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

		ri->FS_FCloseFile(f);
	}

	for (int i = 0; i < GRASS_TEX_SCALE; i++)
	{
		free(red[i]);
		free(green[i]);
		free(blue[i]);
		free(alpha[i]);
	}

	//
	// Generate grass image...
	//

	// Hopefully now we have a map image... Save it...
	byte	data;
	int		i = 0;

	// write tga
	fileHandle_t f = ri->FS_FOpenFileWrite("grassImage/grassImage.tga", qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = GRASS_TEX_SCALE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = GRASS_TEX_SCALE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = GRASS_TEX_SCALE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = GRASS_TEX_SCALE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < GRASS_TEX_SCALE; i++) {
		for (int j = 0; j < GRASS_TEX_SCALE; j++) {
			data = irand(0, 20) + 15; ri->FS_Write(&data, sizeof(data), f);	// b
			data = irand(0, 70) + 45; ri->FS_Write(&data, sizeof(data), f);	// g
			data = irand(0, 20) + 20; ri->FS_Write(&data, sizeof(data), f);	// r
			data = irand(0, 255); ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);
}
#endif

void R_CreateBspMapImage(void)
{
	ri->Printf(PRINT_WARNING, "^1*** ^3%s^5: Generating world map. The game will appear to freeze while constructing. Please wait...\n", "WORLD-MAP");

	// Now we have a mins/maxs for map, we can generate a map image...
	float	z;
	UINT8	*red[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*green[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*blue[MAP_INFO_TRACEMAP_SIZE];

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		red[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		green[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		blue[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
	}

	// Create the map...
#pragma omp parallel for schedule(dynamic) num_threads(8)
	for (int imageX = 0; imageX < MAP_INFO_TRACEMAP_SIZE; imageX++)
	{
		int x = MAP_INFO_MINS[0] + (imageX * MAP_INFO_SCATTEROFFSET[0]);

		for (int imageY = 0; imageY < MAP_INFO_TRACEMAP_SIZE; imageY++)
		{
			int y = MAP_INFO_MINS[1] + (imageY * MAP_INFO_SCATTEROFFSET[1]);

			for (z = MAP_INFO_MAXS[2]; z > MAP_INFO_MINS[2]; z -= 48.0)
			{
				trace_t		tr;
				vec3_t		pos, down;
				qboolean	FOUND = qfalse;

				VectorSet(pos, x, y, z);
				VectorSet(down, x, y, -65536);

				Mapping_Trace(&tr, pos, NULL, NULL, down, ENTITYNUM_NONE, /*MASK_ALL*/MASK_PLAYERSOLID | CONTENTS_WATER/*|CONTENTS_OPAQUE*/);

				if (tr.startsolid || tr.allsolid)
				{// Try again from below this spot...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.endpos[2] <= MAP_INFO_MINS[2])
				{// Went off map...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					break;
				}

				if (tr.surfaceFlags & SURF_SKY)
				{// Sky...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.surfaceFlags & SURF_NODRAW)
				{// don't generate a drawsurface at all
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				float DIST_FROM_ROOF = MAP_INFO_MAXS[2] - tr.endpos[2];
				float HEIGHT_COLOR_MULT = ((1.0 - (DIST_FROM_ROOF / MAP_INFO_SIZE[2])) * 0.5) + 0.5;

				if (tr.contents & CONTENTS_WATER)
				{
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.3 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 1.0 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				}

				int MATERIAL_TYPE = (tr.materialType);

				switch (MATERIAL_TYPE)
				{
				case MATERIAL_WATER:			// 13			// light covering of water on a surface
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.3 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 1.0 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_SHORTGRASS:		// 5			// manicured lawn
				case MATERIAL_LONGGRASS:		// 6			// long jungle grass
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.4 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_SAND:				// 8			// sandy beach
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.8 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.8 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_ROCK:				// 23			//
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.3 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_SOLIDWOOD:		// 1			// freshly cut timber
				case MATERIAL_HOLLOWWOOD:		// 2			// termite infested creaky wood
				case MATERIAL_DRYLEAVES:		// 19			// dried up leaves on the floor
				case MATERIAL_GREENLEAVES:		// 20			// fresh leaves still on a tree
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.5 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_CONCRETE:			// 11			// hardened concrete pavement
				case MATERIAL_PLASTER:			// 28			// drywall style plaster
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
#if 0
				case MATERIAL_CARPET:			// 27			// lush carpet
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_GRAVEL:			// 9			// lots of small stones
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.2 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.5 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.5 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_TILES:			// 26			// tiled floor
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.4 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.4 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.4 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_SOLIDWOOD:		// 1			// freshly cut timber
				case MATERIAL_HOLLOWWOOD:		// 2			// termite infested creaky wood
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.2 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.2 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_SOLIDMETAL:		// 3			// solid girders
				case MATERIAL_HOLLOWMETAL:		// 4			// hollow metal machines -- UQ1: Used for weapons to force lower parallax and high reflection...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_FABRIC:			// 21			// Cotton sheets
				case MATERIAL_CANVAS:			// 22			// tent material
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.8 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.4 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.4 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_MARBLE:			// 12			// marble floors
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.8 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.8 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.8 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
#endif
				case MATERIAL_SNOW:				// 14			// freshly laid snow
				case MATERIAL_ICE:				// 15			// packed snow/solid ice
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 1 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 1 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 1 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_MUD:				// 17			// wet soil
				case MATERIAL_DIRT:				// 7			// hard mud
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.2 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
#if 0
				case MATERIAL_FLESH:			// 16			// hung meat, corpses in the world
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.3 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.3 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_RUBBER:			// 24			// hard tire like rubber
				case MATERIAL_PLASTIC:			// 25			//
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_ARMOR:			// 30			// body armor
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.7 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_SHATTERGLASS:		// 29			// glass with the Crisis Zone style shattering
				case MATERIAL_GLASS:			// 10			//
				case MATERIAL_BPGLASS:			// 18			// bulletproof glass
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				case MATERIAL_COMPUTER:			// 31			// computers/electronic equipment
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.9 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.1 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
#endif
				default:
					/*
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0*255*HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0*255*HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0*255*HEIGHT_COLOR_MULT;
					*/
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0.6 * 255 * HEIGHT_COLOR_MULT;
					FOUND = qtrue;
					break;
				}

				if (FOUND) break;
			}
		}
	}

	// Hopefully now we have a map image... Save it...
	byte data;

	// write tga
	fileHandle_t f = ri->FS_FOpenFileWrite(va("mapImage/%s.tga", currentMapName), qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++) {
		for (int j = 0; j < MAP_INFO_TRACEMAP_SIZE; j++) {
			data = blue[i][j]; ri->FS_Write(&data, sizeof(data), f);	// b
			data = green[i][j]; ri->FS_Write(&data, sizeof(data), f);	// g
			data = red[i][j]; ri->FS_Write(&data, sizeof(data), f);	// r
			data = 255; ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	int i;

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		free(red[i]);
		free(green[i]);
		free(blue[i]);
	}
}

void R_CreateHeightMapImage(void)
{
	ri->Printf(PRINT_WARNING, "^1*** ^3%s^5: Generating world heightmap. The game will appear to freeze while constructing. Please wait...\n", "HEIGHTMAP");

	// Now we have a mins/maxs for map, we can generate a map image...
	float	z;
	UINT8	*red[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*green[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*blue[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*alpha[MAP_INFO_TRACEMAP_SIZE];

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		red[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		green[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		blue[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		alpha[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
	}

	// Create the map...
	//#pragma omp parallel for schedule(dynamic) num_threads(8)
	//for (int x = (int)MAP_INFO_MINS[0]; x < (int)MAP_INFO_MAXS[0]; x += MAP_INFO_SCATTEROFFSET[0])
	for (int imageX = 0; imageX < MAP_INFO_TRACEMAP_SIZE; imageX++)
	{
		int x = MAP_INFO_MINS[0] + (imageX * MAP_INFO_SCATTEROFFSET[0]);

		for (int imageY = 0; imageY < MAP_INFO_TRACEMAP_SIZE; imageY++)
		{
			int y = MAP_INFO_MINS[1] + (imageY * MAP_INFO_SCATTEROFFSET[1]);

			qboolean	HIT_WATER = qfalse;

			for (z = MAP_INFO_MAXS[2]; z > MAP_INFO_MINS[2]; z -= 48.0)
			{
				trace_t		tr;
				vec3_t		pos, down;
				qboolean	FOUND = qfalse;

				VectorSet(pos, x, y, z);
				VectorSet(down, x, y, -65536);

				if (HIT_WATER)
					Mapping_Trace(&tr, pos, NULL, NULL, down, ENTITYNUM_NONE, MASK_PLAYERSOLID);
				else
					Mapping_Trace(&tr, pos, NULL, NULL, down, ENTITYNUM_NONE, MASK_PLAYERSOLID | CONTENTS_WATER/*|CONTENTS_OPAQUE*/);

				if (tr.startsolid || tr.allsolid)
				{// Try again from below this spot...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.endpos[2] < MAP_INFO_MINS[2] - 256.0)
				{// Went off map...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					break;
				}

				if (tr.surfaceFlags & SURF_SKY)
				{// Sky...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.surfaceFlags & SURF_NODRAW)
				{// don't generate a drawsurface at all
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (!HIT_WATER && tr.contents & CONTENTS_WATER)
				{
					HIT_WATER = qtrue;
					continue;
				}

				float DIST_FROM_ROOF = MAP_INFO_MAXS[2] - tr.endpos[2];
				float distScale = DIST_FROM_ROOF / MAP_INFO_SIZE[2];
				if (distScale > 1.0) distScale = 1.0;
				float HEIGHT_COLOR_MULT = (1.0 - distScale);

				float isUnderWater = 0;

				if (HIT_WATER)
				{
					isUnderWater = 1.0;
				}

				red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = HEIGHT_COLOR_MULT * 255;		// height map
				green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = HEIGHT_COLOR_MULT * 255;		// height map
				blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = HEIGHT_COLOR_MULT * 255;		// height map
				alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = isUnderWater * 255;			// is under water

				HIT_WATER = qfalse;
				break;
			}
		}
	}

	// Hopefully now we have a map image... Save it...
	byte data;

	// write tga
	fileHandle_t f = ri->FS_FOpenFileWrite(va("heightMapImage/%s.tga", currentMapName), qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++) {
		for (int j = 0; j < MAP_INFO_TRACEMAP_SIZE; j++) {
			data = blue[i][j]; ri->FS_Write(&data, sizeof(data), f);	// b
			data = green[i][j]; ri->FS_Write(&data, sizeof(data), f);	// g
			data = red[i][j]; ri->FS_Write(&data, sizeof(data), f);	// r
			data = alpha[i][j]; ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	int i;

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		free(red[i]);
		free(green[i]);
		free(blue[i]);
		free(alpha[i]);
	}
}

#if 0
void R_CreateFoliageMapImage(void)
{
	// Now we have a mins/maxs for map, we can generate a map image...
	float	z;
	UINT8	*red[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*green[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*blue[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*alpha[MAP_INFO_TRACEMAP_SIZE];

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		red[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		green[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		blue[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		alpha[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
	}

	// Create the map...
#pragma omp parallel for schedule(dynamic) num_threads(8)
	//for (int x = (int)MAP_INFO_MINS[0]; x < (int)MAP_INFO_MAXS[0]; x += MAP_INFO_SCATTEROFFSET[0])
	for (int imageX = 0; imageX < MAP_INFO_TRACEMAP_SIZE; imageX++)
	{
		int x = MAP_INFO_MINS[0] + (imageX * MAP_INFO_SCATTEROFFSET[0]);

		for (int imageY = 0; imageY < MAP_INFO_TRACEMAP_SIZE; imageY++)
		{
			int y = MAP_INFO_MINS[1] + (imageY * MAP_INFO_SCATTEROFFSET[1]);

			for (z = MAP_INFO_MAXS[2]; z > MAP_INFO_MINS[2]; z -= 48.0)
			{
				trace_t		tr;
				vec3_t		pos, down;
				qboolean	FOUND = qfalse;

				VectorSet(pos, x, y, z);
				VectorSet(down, x, y, -65536);

				Mapping_Trace(&tr, pos, NULL, NULL, down, ENTITYNUM_NONE, /*MASK_ALL*/MASK_PLAYERSOLID | CONTENTS_WATER/*|CONTENTS_OPAQUE*/);

				if (tr.startsolid || tr.allsolid)
				{// Try again from below this spot...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.endpos[2] <= MAP_INFO_MINS[2])
				{// Went off map...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					break;
				}

				if (tr.surfaceFlags & SURF_SKY)
				{// Sky...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.surfaceFlags & SURF_NODRAW)
				{// don't generate a drawsurface at all
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				float DIST_FROM_ROOF = MAP_INFO_MAXS[2] - tr.endpos[2];
				float HEIGHT_COLOR_MULT = (1.0 - (DIST_FROM_ROOF / MAP_INFO_SIZE[2]));

				int MATERIAL_TYPE = (tr.materialType);

				switch (MATERIAL_TYPE)
				{
				case MATERIAL_SHORTGRASS:		// 5					// manicured lawn
				case MATERIAL_LONGGRASS:		// 6					// long jungle grass
				case MATERIAL_MUD:				// 17					// wet soil
				case MATERIAL_DIRT:				// 7					// hard mud
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = HEIGHT_COLOR_MULT * 255;		// height map
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = irand(0, 255);				// foliage option 1
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = irand(0, 255);				// foliage option 2
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = irand(0, 255);				// foliage option 3
					FOUND = qtrue;
					break;
				default:
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					FOUND = qtrue;
					break;
				}

				if (FOUND) break;
			}
		}
	}

	// Hopefully now we have a map image... Save it...
	byte data;

	// write tga
	fileHandle_t f = ri->FS_FOpenFileWrite(va("foliageMapImage/%s.tga", currentMapName), qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++) {
		for (int j = 0; j < MAP_INFO_TRACEMAP_SIZE; j++) {
			data = blue[i][j]; ri->FS_Write(&data, sizeof(data), f);	// b
			data = green[i][j]; ri->FS_Write(&data, sizeof(data), f);	// g
			data = red[i][j]; ri->FS_Write(&data, sizeof(data), f);	// r
			data = alpha[i][j]; ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	int i;

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		free(red[i]);
		free(green[i]);
		free(blue[i]);
		free(alpha[i]);
	}
}
#endif

float ROAD_GetSlope(vec3_t normal)
{
	float pitch;
	vec3_t slopeangles;
	vectoangles(normal, slopeangles);

	pitch = slopeangles[0];

	if (pitch > 180)
		pitch -= 360;

	if (pitch < -180)
		pitch += 360;

	pitch += 90.0f;

	return pitch;
}

qboolean ROAD_CheckSlope(float pitch)
{
#define MAX_ROAD_SLOPE 32.0
	if (pitch > MAX_ROAD_SLOPE || pitch < -MAX_ROAD_SLOPE)
		return qfalse;

	return qtrue;
}

void R_CreateRoadMapImage(void)
{
	ri->Printf(PRINT_WARNING, "^1*** ^3%s^5: Draw red on the map to add roads. Blue shows water. Green shows height map. Black is a bad area for roads.\n", "ROADS");
	ri->Printf(PRINT_WARNING, "^1*** ^3%s^5: It is highly recomended to save the final picture as a JPG or PNG file and delete the original TGA.\n", "ROADS");

	// Now we have a mins/maxs for map, we can generate a map image...
	float	z;
	UINT8	*red[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*green[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*blue[MAP_INFO_TRACEMAP_SIZE];
	UINT8	*alpha[MAP_INFO_TRACEMAP_SIZE];

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		red[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		green[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		blue[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
		alpha[i] = (UINT8*)malloc(sizeof(UINT8)*MAP_INFO_TRACEMAP_SIZE);
	}

	// Create the map...
#pragma omp parallel for schedule(dynamic) num_threads(8)
	//for (int x = (int)MAP_INFO_MINS[0]; x < (int)MAP_INFO_MAXS[0]; x += MAP_INFO_SCATTEROFFSET[0])
	for (int imageX = 0; imageX < MAP_INFO_TRACEMAP_SIZE; imageX++)
	{
		int x = MAP_INFO_MINS[0] + (imageX * MAP_INFO_SCATTEROFFSET[0]);

		for (int imageY = 0; imageY < MAP_INFO_TRACEMAP_SIZE; imageY++)
		{
			int y = MAP_INFO_MINS[1] + (imageY * MAP_INFO_SCATTEROFFSET[1]);

			for (z = MAP_INFO_MAXS[2]; z > MAP_INFO_MINS[2]; z -= 48.0)
			{
				trace_t		tr;
				vec3_t		pos, down;
				qboolean	FOUND = qfalse;

				VectorSet(pos, x, y, z);
				VectorSet(down, x, y, -65536);

				Mapping_Trace(&tr, pos, NULL, NULL, down, ENTITYNUM_NONE, /*MASK_ALL*/MASK_PLAYERSOLID | CONTENTS_WATER/*|CONTENTS_OPAQUE*/);

				if (tr.startsolid || tr.allsolid)
				{// Try again from below this spot...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.endpos[2] <= MAP_INFO_MINS[2])
				{// Went off map...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 255;
					break;
				}

				if (tr.surfaceFlags & SURF_SKY)
				{// Sky...
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				if (tr.surfaceFlags & SURF_NODRAW)
				{// don't generate a drawsurface at all
					red[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					green[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					blue[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					alpha[(MAP_INFO_TRACEMAP_SIZE-1)-imageY][imageX] = 0;
					continue;
				}

				float DIST_FROM_ROOF = MAP_INFO_MAXS[2] - tr.endpos[2];
				float HEIGHT_COLOR_MULT = (1.0 - (DIST_FROM_ROOF / MAP_INFO_SIZE[2]));

				int MATERIAL_TYPE = (tr.materialType);

				//
				// Draw red on the map to add roads. Blue shows water. Green shows height map. Black is a bad area for roads.
				//
				switch (MATERIAL_TYPE)
				{
					case MATERIAL_WATER:
					{
						// Water... Draw blue...
						red[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
						green[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
						blue[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 255;
						alpha[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 255;
						FOUND = qtrue;
						break;
					}
					case MATERIAL_SHORTGRASS:		// 5					// manicured lawn
					case MATERIAL_LONGGRASS:		// 6					// long jungle grass
					case MATERIAL_MUD:				// 17					// wet soil
					case MATERIAL_DIRT:				// 7					// hard mud
					case MATERIAL_SNOW:				// 14					// snow
					{
						float slope = ROAD_GetSlope(tr.plane.normal);
						UINT8 brightness = 0;

						if (slope < 0) slope *= -1.0;

						if (slope < MAX_ROAD_SLOPE)
						{// In valid range... Work out brightness...
							float temp = 1.0 - (slope / MAX_ROAD_SLOPE);
							brightness = UINT8(float(255.0) * float(temp));
						}

						red[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
						green[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = brightness;
						blue[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
						alpha[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 255;
						FOUND = qtrue;
						break;
					}
					default:
					{
						extern qboolean RB_ShouldUseGeometryGrass(int materialType);
						if (RB_ShouldUseGeometryGrass(MATERIAL_TYPE))
						{
							float slope = ROAD_GetSlope(tr.plane.normal);
							UINT8 brightness = 0;

							if (slope < 0) slope *= -1.0;

							if (slope < MAX_ROAD_SLOPE)
							{// In valid range... Work out brightness...
								float temp = 1.0 - (slope / MAX_ROAD_SLOPE);
								brightness = UINT8(float(255.0) * float(temp));
							}

							red[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
							green[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = brightness;
							blue[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
							alpha[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 255;
						}
						else
						{
							// Material isn't a road-able surface type...  Draw black...
							red[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
							green[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
							blue[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 0;
							alpha[(MAP_INFO_TRACEMAP_SIZE - 1) - imageY][imageX] = 255;
						}
						FOUND = qtrue;
						break;
					}
				}

				if (FOUND) break;
			}
		}
	}

	// Hopefully now we have a map image... Save it...
	byte data;

	// write tga
	fileHandle_t f = ri->FS_FOpenFileWrite(va("maps/%s_roads.tga", currentMapName), qfalse);

	// header
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 0
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 1
	data = 2; ri->FS_Write(&data, sizeof(data), f);	// 2 : uncompressed type
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 3
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 4
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 5
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 6
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 7
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 8
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 9
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 10
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 11
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 12 : width
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 13 : width
	data = MAP_INFO_TRACEMAP_SIZE & 255; ri->FS_Write(&data, sizeof(data), f);	// 14 : height
	data = MAP_INFO_TRACEMAP_SIZE >> 8; ri->FS_Write(&data, sizeof(data), f);	// 15 : height
	data = 32; ri->FS_Write(&data, sizeof(data), f);	// 16 : pixel size
	data = 0; ri->FS_Write(&data, sizeof(data), f);	// 17

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++) {
		for (int j = 0; j < MAP_INFO_TRACEMAP_SIZE; j++) {
			data = blue[i][j]; ri->FS_Write(&data, sizeof(data), f);	// b
			data = green[i][j]; ri->FS_Write(&data, sizeof(data), f);	// g
			data = red[i][j]; ri->FS_Write(&data, sizeof(data), f);	// r
			data = alpha[i][j]; ri->FS_Write(&data, sizeof(data), f);	// a
		}
	}

	int i;

	// footer
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// extension area offset, 4 bytes
	i = 0; ri->FS_Write(&i, sizeof(i), f);	// developer directory offset, 4 bytes
	ri->FS_Write("TRUEVISION-XFILE.\0", 18, f);

	ri->FS_FCloseFile(f);

	for (int i = 0; i < MAP_INFO_TRACEMAP_SIZE; i++)
	{
		free(red[i]);
		free(green[i]);
		free(blue[i]);
		free(alpha[i]);
	}
}

qboolean	DISABLE_DEPTH_PREPASS = qfalse;
qboolean	LODMODEL_MAP = qfalse;
qboolean	DISABLE_MERGED_GLOWS = qfalse;
qboolean	DISABLE_LIFTS_AND_PORTALS_MERGE = qtrue;
int			MAP_MAX_VIS_RANGE = 0;
qboolean	ENABLE_DISPLACEMENT_MAPPING = qfalse;
float		DISPLACEMENT_MAPPING_STRENGTH = 18.0;
qboolean	MAP_REFLECTION_ENABLED = qfalse;
qboolean	TERRAIN_TESSELLATION_ENABLED = qtrue;
float		TERRAIN_TESSELLATION_LEVEL = 11.0;
float		TERRAIN_TESSELLATION_OFFSET = 16.0;
float		TERRAIN_TESSELLATION_MIN_SIZE = 512.0;
qboolean	DAY_NIGHT_CYCLE_ENABLED = qfalse;
float		DAY_NIGHT_CYCLE_SPEED = 1.0;
float		SUN_PHONG_SCALE = 1.0;
float		SUN_VOLUMETRIC_SCALE = 1.0;
vec3_t		SUN_COLOR_MAIN = { 0.85 };
vec3_t		SUN_COLOR_SECONDARY = { 0.4 };
vec3_t		SUN_COLOR_TERTIARY = { 0.2 };
vec3_t		SUN_COLOR_AMBIENT = { 0.85 };
qboolean	PROCEDURAL_SKY_ENABLED = qfalse;
qboolean	PROCEDURAL_CLOUDS_ENABLED = qtrue;
float		PROCEDURAL_CLOUDS_CLOUDSCALE = 1.1;
float		PROCEDURAL_CLOUDS_SPEED = 0.003;
float		PROCEDURAL_CLOUDS_DARK = 0.5;
float		PROCEDURAL_CLOUDS_LIGHT = 0.3;
float		PROCEDURAL_CLOUDS_CLOUDCOVER = 0.2;
float		PROCEDURAL_CLOUDS_CLOUDALPHA = 2.0;
float		PROCEDURAL_CLOUDS_SKYTINT = 0.5;
int			LATE_LIGHTING_ENABLED = 0;
qboolean	MAP_LIGHTMAP_DISABLED = qfalse;
int			MAP_LIGHTMAP_ENHANCEMENT = 1;
int			MAP_LIGHTING_METHOD = 0;
qboolean	MAP_USE_PALETTE_ON_SKY = qfalse;
float		MAP_LIGHTMAP_MULTIPLIER = 1.0;
vec3_t		MAP_AMBIENT_CSB = { 1 };
vec3_t		MAP_AMBIENT_COLOR = { 1 };
float		MAP_GLOW_MULTIPLIER = 1.0;
vec3_t		MAP_AMBIENT_CSB_NIGHT = { 1 };
vec3_t		MAP_AMBIENT_COLOR_NIGHT = { 1 };
float		MAP_GLOW_MULTIPLIER_NIGHT = 1.0;
float		SKY_LIGHTING_SCALE = 1.0;
float		MAP_EMISSIVE_COLOR_SCALE = 1.0;
float		MAP_EMISSIVE_COLOR_SCALE_NIGHT = 1.0;
float		MAP_EMISSIVE_RADIUS_SCALE = 1.0;
float		MAP_EMISSIVE_RADIUS_SCALE_NIGHT = 1.0;
float		MAP_HDR_MIN = 26.0;
float		MAP_HDR_MAX = 209.0;
qboolean	AURORA_ENABLED = qtrue;
qboolean	AURORA_ENABLED_DAY = qfalse;
qboolean	AO_ENABLED = qtrue;
qboolean	AO_BLUR = qtrue;
qboolean	AO_DIRECTIONAL = qfalse;
float		AO_MINBRIGHT = 0.3;
float		AO_MULTBRIGHT = 1.0;
qboolean	SHADOWS_ENABLED = qfalse;
float		SHADOW_MINBRIGHT = 0.7;
float		SHADOW_MAXBRIGHT = 1.0;
qboolean	FOG_POST_ENABLED = qtrue;
qboolean	FOG_STANDARD_ENABLE = qtrue;
vec3_t		FOG_COLOR = { 0 };
vec3_t		FOG_COLOR_SUN = { 0 };
float		FOG_DENSITY = 0.5;
float		FOG_ACCUMULATION_MODIFIER = 3.0;
float		FOG_RANGE_MULTIPLIER = 1.0;
qboolean	FOG_VOLUMETRIC_ENABLE = qfalse;
float		FOG_VOLUMETRIC_DENSITY = 1.0;
float		FOG_VOLUMETRIC_STRENGTH = 1.0;
float		FOG_VOLUMETRIC_CLOUDINESS = 1.0;
float		FOG_VOLUMETRIC_WIND = 1.0;
float		FOG_VOLUMETRIC_VELOCITY = 0.001;
vec3_t		FOG_VOLUMETRIC_COLOR = { 0 };
qboolean	WATER_ENABLED = qfalse;
qboolean	WATER_USE_OCEAN = qfalse;
qboolean	WATER_FARPLANE_ENABLED = qfalse;
float		WATER_REFLECTIVENESS = 0.28;
float		WATER_WAVE_HEIGHT = 64.0;
qboolean	WATER_FOG_ENABLED = qfalse;
vec3_t		WATER_COLOR_SHALLOW = { 0 };
vec3_t		WATER_COLOR_DEEP = { 0 };
qboolean	GRASS_ENABLED = qtrue;
qboolean	GRASS_UNDERWATER_ONLY = qfalse;
int			GRASS_WIDTH_REPEATS = 0;
int			GRASS_DENSITY = 2;
float		GRASS_HEIGHT = 48.0;
int			GRASS_DISTANCE = 2048;
float		GRASS_MAX_SLOPE = 10.0;
float		GRASS_TYPE_UNIFORMALITY = 0.97;
float		GRASS_TYPE_UNIFORMALITY_SCALER = 0.008;
float		GRASS_DISTANCE_FROM_ROADS = 0.25;
float		GRASS_SCALES[16] = { 1.0 };
vec3_t		MOON_COLOR = { 0.2f };
vec3_t		MOON_ATMOSPHERE_COLOR = { 1.0 };
float		MOON_GLOW_STRENGTH = 0.5;
float		MOON_ROTATION_RATE = 0.08;
char		ROAD_TEXTURE[256] = { 0 };

qboolean	JKA_WEATHER_ENABLED = qfalse;
qboolean	WZ_WEATHER_ENABLED = qfalse;
qboolean	WZ_WEATHER_SOUND_ONLY = qfalse;

extern float	MAP_WATER_LEVEL;

char		CURRENT_CLIMATE_OPTION[256] = { 0 };
char		CURRENT_WEATHER_OPTION[256] = { 0 };

#define MAX_FOLIAGE_ALLOWED_MATERIALS 64

int FOLIAGE_ALLOWED_MATERIALS_NUM = 0;
int FOLIAGE_ALLOWED_MATERIALS[MAX_FOLIAGE_ALLOWED_MATERIALS] = { 0 };

qboolean R_SurfaceIsAllowedFoliage(int materialType)
{
	for (int i = 0; i < FOLIAGE_ALLOWED_MATERIALS_NUM; i++)
	{
		if (materialType == FOLIAGE_ALLOWED_MATERIALS[i]) return qtrue;
	}

	return qfalse;
}

#ifdef __OCEAN__
extern qboolean WATER_INITIALIZED;
extern qboolean WATER_FAST_INITIALIZED;
#endif //__OCEAN__

void SetupWeather(char *mapname); // below...

void MAPPING_LoadMapInfo(void)
{
	qglFinish();

	for (int i = 0; i < 16; i++)
	{// Init all grass scales...
		GRASS_SCALES[i] = 1.0;
	}

#ifdef __OCEAN__
	MAP_WATER_LEVEL = 131072.0;
	WATER_INITIALIZED = qfalse;
	WATER_FAST_INITIALIZED = qfalse;
#endif //__OCEAN__

	char mapname[256] = { 0 };
	
	// because JKA uses mp/ dir, why??? so pointless...
	if (IniExists(va("maps/%s.mapInfo", currentMapName)))
		sprintf(mapname, "maps/%s.mapInfo", currentMapName);
	else if (IniExists(va("maps/mp/%s.mapInfo", currentMapName)))
		sprintf(mapname, "maps/mp/%s.mapInfo", currentMapName);
	else
		sprintf(mapname, "maps/%s.mapInfo", currentMapName);

	ri->Printf(PRINT_ALL, "^4*** ^3Warzone^4: ^5Using mapInfo file: %s.\n", mapname);

	//
	// Horrible hacks for basejka maps... Don't use them! Fix your maps for warzone!
	//
	LODMODEL_MAP = (atoi(IniRead(mapname, "FIXES", "LODMODEL_MAP", "0")) > 0) ? qtrue : qfalse;
	DISABLE_DEPTH_PREPASS = (atoi(IniRead(mapname, "FIXES", "DISABLE_DEPTH_PREPASS", "0")) > 0) ? qtrue : qfalse;
	MAP_MAX_VIS_RANGE = atoi(IniRead(mapname, "FIXES", "MAP_MAX_VIS_RANGE", "0"));
	DISABLE_MERGED_GLOWS = (atoi(IniRead(mapname, "FIXES", "DISABLE_MERGED_GLOWS", "0")) > 0) ? qtrue : qfalse;
	DISABLE_LIFTS_AND_PORTALS_MERGE = (atoi(IniRead(mapname, "FIXES", "DISABLE_LIFTS_AND_PORTALS_MERGE", "1")) > 0) ? qtrue : qfalse;

	//
	// Misc effect enablers...
	//
	ENABLE_DISPLACEMENT_MAPPING = (atoi(IniRead(mapname, "EFFECTS", "ENABLE_DISPLACEMENT_MAPPING", "0")) > 0) ? qtrue : qfalse;

	if (ENABLE_DISPLACEMENT_MAPPING)
	{
		DISPLACEMENT_MAPPING_STRENGTH = atof(IniRead(mapname, "EFFECTS", "DISPLACEMENT_MAPPING_STRENGTH", "18.0"));
	}

	MAP_REFLECTION_ENABLED = (atoi(IniRead(mapname, "EFFECTS", "ENABLE_SCREEN_SPACE_REFLECTIONS", "0")) > 0) ? qtrue : qfalse;

	//
	// Tessellation...
	//
	TERRAIN_TESSELLATION_ENABLED = (atoi(IniRead(mapname, "TESSELLATION", "TERRAIN_TESSELLATION_ENABLED", "1")) > 0) ? qtrue : qfalse;

	if (TERRAIN_TESSELLATION_ENABLED)
	{
		TERRAIN_TESSELLATION_LEVEL = atof(IniRead(mapname, "TESSELLATION", "TERRAIN_TESSELLATION_LEVEL", "32.0"));
		TERRAIN_TESSELLATION_OFFSET = atof(IniRead(mapname, "TESSELLATION", "TERRAIN_TESSELLATION_OFFSET", "24.0"));
		TERRAIN_TESSELLATION_MIN_SIZE = atof(IniRead(mapname, "TESSELLATION", "TERRAIN_TESSELLATION_MIN_SIZE", "512.0"));

		// Fuck it, this is generic instead... I can't be fucked doing collision loading for map based ones as well, for now...
		tr.tessellationMapImage = R_FindImageFile(va("maps/%s_tess.tga", currentMapName), IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);

		if (!tr.tessellationMapImage)
		{
			tr.tessellationMapImage = R_FindImageFile("gfx/tessControlImage", IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);
		}
	}

	//
	// Sun + Day/Night...
	//
	int dayNightEnableValue = atoi(IniRead(mapname, "DAY_NIGHT_CYCLE", "DAY_NIGHT_CYCLE_ENABLED", "0"));
	DAY_NIGHT_CYCLE_SPEED = atof(IniRead(mapname, "DAY_NIGHT_CYCLE", "DAY_NIGHT_CYCLE_SPEED", "1.0"));

	DAY_NIGHT_CYCLE_ENABLED = dayNightEnableValue ? qtrue : qfalse;

	if (!DAY_NIGHT_CYCLE_ENABLED)
	{// Also check under SUN section...
		dayNightEnableValue = atoi(IniRead(mapname, "SUN", "DAY_NIGHT_CYCLE_ENABLED", "0"));
		DAY_NIGHT_CYCLE_SPEED = atof(IniRead(mapname, "SUN", "DAY_NIGHT_CYCLE_SPEED", "1.0"));
		DAY_NIGHT_CYCLE_ENABLED = dayNightEnableValue ? qtrue : qfalse;
	}

	if (dayNightEnableValue != -1 && !DAY_NIGHT_CYCLE_ENABLED)
	{// Leave -1 in ini file to override and force it off, just in case...
		if (StringContainsWord(mapname, "mandalore")
			|| StringContainsWord(mapname, "endor")
			|| StringContainsWord(mapname, "ilum")
			|| StringContainsWord(mapname, "taanab")
			|| StringContainsWord(mapname, "tatooine")
			|| StringContainsWord(mapname, "scarif")
			&& !StringContainsWord(mapname, "tatooine_nights"))
		{
			DAY_NIGHT_CYCLE_ENABLED = qtrue;
		}
	}

	SUN_PHONG_SCALE = atof(IniRead(mapname, "SUN", "SUN_PHONG_SCALE", "1.0"));
	SUN_VOLUMETRIC_SCALE = atof(IniRead(mapname, "SUN", "SUN_VOLUMETRIC_SCALE", "1.0"));

	SUN_COLOR_MAIN[0] = atof(IniRead(mapname, "SUN", "SUN_COLOR_MAIN_R", "0.85"));
	SUN_COLOR_MAIN[1] = atof(IniRead(mapname, "SUN", "SUN_COLOR_MAIN_G", "0.85"));
	SUN_COLOR_MAIN[2] = atof(IniRead(mapname, "SUN", "SUN_COLOR_MAIN_B", "0.85"));
	SUN_COLOR_SECONDARY[0] = atof(IniRead(mapname, "SUN", "SUN_COLOR_SECONDARY_R", "0.4"));
	SUN_COLOR_SECONDARY[1] = atof(IniRead(mapname, "SUN", "SUN_COLOR_SECONDARY_G", "0.4"));
	SUN_COLOR_SECONDARY[2] = atof(IniRead(mapname, "SUN", "SUN_COLOR_SECONDARY_B", "0.4"));
	SUN_COLOR_TERTIARY[0] = atof(IniRead(mapname, "SUN", "SUN_COLOR_TERTIARY_R", "0.2"));
	SUN_COLOR_TERTIARY[1] = atof(IniRead(mapname, "SUN", "SUN_COLOR_TERTIARY_G", "0.2"));
	SUN_COLOR_TERTIARY[2] = atof(IniRead(mapname, "SUN", "SUN_COLOR_TERTIARY_B", "0.2"));
	SUN_COLOR_AMBIENT[0] = atof(IniRead(mapname, "SUN", "SUN_COLOR_AMBIENT_R", "0.85"));
	SUN_COLOR_AMBIENT[1] = atof(IniRead(mapname, "SUN", "SUN_COLOR_AMBIENT_G", "0.85"));
	SUN_COLOR_AMBIENT[2] = atof(IniRead(mapname, "SUN", "SUN_COLOR_AMBIENT_B", "0.85"));

	//
	// Procedural Sky...
	//
	PROCEDURAL_SKY_ENABLED = (atoi(IniRead(mapname, "SKY", "PROCEDURAL_SKY_ENABLED", "0")) > 0) ? qtrue : qfalse;

	//
	// Clouds....
	//
	PROCEDURAL_CLOUDS_ENABLED = (atoi(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_ENABLED", "1")) > 0) ? qtrue : qfalse;
	PROCEDURAL_CLOUDS_CLOUDSCALE = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_CLOUDSCALE", "1.1"));
	PROCEDURAL_CLOUDS_SPEED = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_SPEED", "0.003"));
	PROCEDURAL_CLOUDS_DARK = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_DARK", "0.5"));
	PROCEDURAL_CLOUDS_LIGHT = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_LIGHT", "0.3"));
	PROCEDURAL_CLOUDS_CLOUDCOVER = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_CLOUDCOVER", "0.2"));
	PROCEDURAL_CLOUDS_CLOUDALPHA = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_CLOUDALPHA", "2.0"));
	PROCEDURAL_CLOUDS_SKYTINT = atof(IniRead(mapname, "CLOUDS", "PROCEDURAL_CLOUDS_SKYTINT", "0.5"));

	//
	// Aurora....
	//
	AURORA_ENABLED = (atoi(IniRead(mapname, "AURORA", "AURORA_ENABLED", "1")) > 0) ? qtrue : qfalse;
	AURORA_ENABLED_DAY = (atoi(IniRead(mapname, "AURORA", "AURORA_ENABLED_DAY", "0")) > 0) ? qtrue : qfalse;

	//
	// Weather...
	//
	JKA_WEATHER_ENABLED = (atoi(IniRead(mapname, "ATMOSPHERICS", "JKA_WEATHER_ENABLED", "0")) > 0) ? qtrue : qfalse;
	WZ_WEATHER_ENABLED = (atoi(IniRead(mapname, "ATMOSPHERICS", "WZ_WEATHER_ENABLED", "0")) > 0) ? qtrue : qfalse;
	WZ_WEATHER_SOUND_ONLY = (atoi(IniRead(mapname, "ATMOSPHERICS", "WZ_WEATHER_SOUND_ONLY", "0")) > 0) ? qtrue : qfalse;

	SetupWeather(mapname);

	//
	// Palette...
	//
	LATE_LIGHTING_ENABLED = atoi(IniRead(mapname, "PALETTE", "LATE_LIGHTING_ENABLED", "0"));
	MAP_LIGHTMAP_DISABLED = atoi(IniRead(mapname, "PALETTE", "MAP_LIGHTMAP_DISABLED", "0")) ? qtrue : qfalse;
	MAP_LIGHTMAP_ENHANCEMENT = atoi(IniRead(mapname, "PALETTE", "MAP_LIGHTMAP_ENHANCEMENT", "1"));
	MAP_LIGHTING_METHOD = atoi(IniRead(mapname, "PALETTE", "MAP_LIGHTING_METHOD", "0"));
	MAP_USE_PALETTE_ON_SKY = atoi(IniRead(mapname, "PALETTE", "MAP_USE_PALETTE_ON_SKY", "0")) ? qtrue : qfalse;

	MAP_LIGHTMAP_MULTIPLIER = atof(IniRead(mapname, "PALETTE", "MAP_LIGHTMAP_MULTIPLIER", "1.0"));

	MAP_AMBIENT_COLOR[0] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_COLOR_R", "1.0"));
	MAP_AMBIENT_COLOR[1] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_COLOR_G", "1.0"));
	MAP_AMBIENT_COLOR[2] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_COLOR_B", "1.0"));

	MAP_AMBIENT_COLOR_NIGHT[0] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_COLOR_NIGHT_R", va("%f", MAP_AMBIENT_COLOR[0])));
	MAP_AMBIENT_COLOR_NIGHT[1] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_COLOR_NIGHT_G", va("%f", MAP_AMBIENT_COLOR[1])));
	MAP_AMBIENT_COLOR_NIGHT[2] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_COLOR_NIGHT_B", va("%f", MAP_AMBIENT_COLOR[2])));

	MAP_AMBIENT_CSB[0] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_CONTRAST", "1.0"));
	MAP_AMBIENT_CSB[1] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_SATURATION", "1.0"));
	MAP_AMBIENT_CSB[2] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_BRIGHTNESS", "1.0"));

	MAP_AMBIENT_CSB_NIGHT[0] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_CONTRAST_NIGHT", va("%f", MAP_AMBIENT_CSB[0]*0.9)));
	MAP_AMBIENT_CSB_NIGHT[1] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_SATURATION_NIGHT", va("%f", MAP_AMBIENT_CSB[1]*0.9)));
	MAP_AMBIENT_CSB_NIGHT[2] = atof(IniRead(mapname, "PALETTE", "MAP_AMBIENT_BRIGHTNESS_NIGHT", va("%f", MAP_AMBIENT_CSB[2]*0.55)));

	MAP_HDR_MIN = atof(IniRead(mapname, "PALETTE", "MAP_HDR_MIN", "26.0"));
	MAP_HDR_MAX = atof(IniRead(mapname, "PALETTE", "MAP_HDR_MAX", "209.0"));

	MAP_GLOW_MULTIPLIER = atof(IniRead(mapname, "PALETTE", "MAP_GLOW_MULTIPLIER", "1.0"));

	MAP_GLOW_MULTIPLIER_NIGHT = atof(IniRead(mapname, "PALETTE", "MAP_GLOW_MULTIPLIER_NIGHT", va("%f", MAP_GLOW_MULTIPLIER)));

	SKY_LIGHTING_SCALE = atof(IniRead(mapname, "PALETTE", "SKY_LIGHTING_SCALE", "1.0"));

	//
	// Ambient Occlusion...
	//
	AO_ENABLED = (atoi(IniRead(mapname, "AMBIENT_OCCLUSION", "AO_ENABLED", "1")) > 0) ? qtrue : qfalse;

	if (AO_ENABLED)
	{
		AO_BLUR = (atoi(IniRead(mapname, "AMBIENT_OCCLUSION", "AO_BLUR", "1")) > 0) ? qtrue : qfalse;
		AO_DIRECTIONAL = (atoi(IniRead(mapname, "AMBIENT_OCCLUSION", "AO_DIRECTIONAL", "0")) > 0) ? qtrue : qfalse;
		AO_MINBRIGHT = atof(IniRead(mapname, "AMBIENT_OCCLUSION", "AO_MINBRIGHT", "0.3"));
		AO_MULTBRIGHT = atof(IniRead(mapname, "AMBIENT_OCCLUSION", "AO_MULTBRIGHT", "1.0"));
	}

	//
	// Emission...
	//
	MAP_EMISSIVE_COLOR_SCALE = atof(IniRead(mapname, "EMISSION", "MAP_EMISSIVE_COLOR_SCALE", "1.0"));
	MAP_EMISSIVE_COLOR_SCALE_NIGHT = atof(IniRead(mapname, "EMISSION", "MAP_EMISSIVE_COLOR_SCALE_NIGHT", "1.0"));
	MAP_EMISSIVE_RADIUS_SCALE = atof(IniRead(mapname, "EMISSION", "MAP_EMISSIVE_RADIUS_SCALE", "1.0"));
	MAP_EMISSIVE_RADIUS_SCALE_NIGHT = atof(IniRead(mapname, "EMISSION", "MAP_EMISSIVE_RADIUS_SCALE_NIGHT", "1.0"));

	//
	// Fog...
	//
	FOG_POST_ENABLED = (atoi(IniRead(mapname, "FOG", "DISABLE_FOG", "1")) > 0) ? qfalse : qtrue;

	if (FOG_POST_ENABLED)
	{
		FOG_STANDARD_ENABLE = (atoi(IniRead(mapname, "FOG", "FOG_STANDARD_ENABLE", "1")) > 0) ? qtrue : qfalse;

		if (FOG_STANDARD_ENABLE)
		{
			FOG_COLOR[0] = atof(IniRead(mapname, "FOG", "FOG_COLOR_R", "0.5"));
			FOG_COLOR[1] = atof(IniRead(mapname, "FOG", "FOG_COLOR_G", "0.6"));
			FOG_COLOR[2] = atof(IniRead(mapname, "FOG", "FOG_COLOR_B", "0.7"));
			FOG_COLOR_SUN[0] = atof(IniRead(mapname, "FOG", "FOG_COLOR_SUN_R", "1.0"));
			FOG_COLOR_SUN[1] = atof(IniRead(mapname, "FOG", "FOG_COLOR_SUN_G", "0.9"));
			FOG_COLOR_SUN[2] = atof(IniRead(mapname, "FOG", "FOG_COLOR_SUN_B", "0.7"));
			FOG_DENSITY = atof(IniRead(mapname, "FOG", "FOG_DENSITY", "0.5"));
			FOG_ACCUMULATION_MODIFIER = atof(IniRead(mapname, "FOG", "FOG_ACCUMULATION_MODIFIER", "3.0"));
			FOG_RANGE_MULTIPLIER = atof(IniRead(mapname, "FOG", "FOG_RANGE_MULTIPLIER", "1.0"));
		}

		FOG_VOLUMETRIC_ENABLE = (atoi(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_ENABLE", "0")) > 0) ? qtrue : qfalse;

		if (FOG_VOLUMETRIC_ENABLE)
		{
			FOG_VOLUMETRIC_DENSITY = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_DENSITY", "0.01"));
			FOG_VOLUMETRIC_STRENGTH = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_STRENGTH", "1.0"));
			FOG_VOLUMETRIC_VELOCITY = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_VELOCITY", "0.1"));
			FOG_VOLUMETRIC_CLOUDINESS = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_CLOUDINESS", "1.0"));
			FOG_VOLUMETRIC_WIND = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_WIND", "1.0"));
			FOG_VOLUMETRIC_COLOR[0] = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_COLOR_R", "1.0"));
			FOG_VOLUMETRIC_COLOR[1] = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_COLOR_G", "1.0"));
			FOG_VOLUMETRIC_COLOR[2] = atof(IniRead(mapname, "FOG", "FOG_VOLUMETRIC_COLOR_B", "1.0"));
		}
	}

	//
	// Shadows...
	//
	SHADOWS_ENABLED = (atoi(IniRead(mapname, "SHADOWS", "SHADOWS_ENABLED", "0")) > 0) ? qtrue : qfalse;
	SHADOW_MINBRIGHT = atof(IniRead(mapname, "SHADOWS", "SHADOW_MINBRIGHT", "0.7"));
	SHADOW_MAXBRIGHT = atof(IniRead(mapname, "SHADOWS", "SHADOW_MAXBRIGHT", "1.0"));

	if (r_lowVram->integer)
	{
		SHADOWS_ENABLED = qfalse;
	}

	//
	// Water...
	//
	WATER_ENABLED = (atoi(IniRead(mapname, "WATER", "WATER_ENABLED", "1")) > 0) ? qtrue : qfalse;

	if (WATER_ENABLED /*&& !WATER_INITIALIZED*/)
	{
		WATER_USE_OCEAN = (atoi(IniRead(mapname, "WATER", "WATER_OCEAN_ENABLED", "0")) > 0) ? qtrue : qfalse;
		WATER_FARPLANE_ENABLED = (atoi(IniRead(mapname, "WATER", "WATER_FARPLANE_ENABLED", "0")) > 0) ? qtrue : qfalse;
		WATER_FOG_ENABLED = (atoi(IniRead(mapname, "WATER", "WATER_FOG_ENABLED", "0")) > 0) ? qtrue : qfalse;
		WATER_REFLECTIVENESS = Q_clamp(0.0, atof(IniRead(mapname, "WATER", "WATER_REFLECTIVENESS", "0.28")), 1.0);
		WATER_WAVE_HEIGHT = atof(IniRead(mapname, "WATER", "WATER_WAVE_HEIGHT", "64.0"));
		WATER_COLOR_SHALLOW[0] = atof(IniRead(mapname, "WATER", "WATER_COLOR_SHALLOW_R", "0.0078"));
		WATER_COLOR_SHALLOW[1] = atof(IniRead(mapname, "WATER", "WATER_COLOR_SHALLOW_G", "0.5176"));
		WATER_COLOR_SHALLOW[2] = atof(IniRead(mapname, "WATER", "WATER_COLOR_SHALLOW_B", "0.7"));
		WATER_COLOR_DEEP[0] = atof(IniRead(mapname, "WATER", "WATER_COLOR_DEEP_R", "0.0059"));
		WATER_COLOR_DEEP[1] = atof(IniRead(mapname, "WATER", "WATER_COLOR_DEEP_G", "0.1276"));
		WATER_COLOR_DEEP[2] = atof(IniRead(mapname, "WATER", "WATER_COLOR_DEEP_B", "0.18"));
		float NEW_MAP_WATER_LEVEL = atof(IniRead(mapname, "WATER", "MAP_WATER_LEVEL", "131072.0"));

		if (NEW_MAP_WATER_LEVEL < 131000.0 && NEW_MAP_WATER_LEVEL > -131000.0)
		{
			MAP_WATER_LEVEL = NEW_MAP_WATER_LEVEL;
		}
		else
		{
			if (!(MAP_WATER_LEVEL < 131000.0 && MAP_WATER_LEVEL > -131000.0))
			{
				MAP_WATER_LEVEL = 131072.0;
			}
		}

		if (!(MAP_WATER_LEVEL < 131000.0 && MAP_WATER_LEVEL > -131000.0))
		{
			MAP_WATER_LEVEL = 131072.0;
		}
	}

	//
	// Climate...
	//
	const char		*climateName = IniRead(mapname, "CLIMATE", "CLIMATE_TYPE", "");
	memset(CURRENT_CLIMATE_OPTION, 0, sizeof(CURRENT_CLIMATE_OPTION));
	strncpy(CURRENT_CLIMATE_OPTION, climateName, strlen(climateName));

	//
	// Grass...
	//
	GRASS_ENABLED = (atoi(IniRead(mapname, "GRASS", "GRASS_ENABLED", "0")) > 0) ? qtrue : qfalse;

	FOLIAGE_ALLOWED_MATERIALS_NUM = 0;

	if (GRASS_ENABLED)
	{
		GRASS_UNDERWATER_ONLY = (atoi(IniRead(mapname, "GRASS", "GRASS_UNDERWATER_ONLY", "0")) > 0) ? qtrue : qfalse;
		GRASS_DENSITY = atoi(IniRead(mapname, "GRASS", "GRASS_DENSITY", "2"));
		GRASS_WIDTH_REPEATS = atoi(IniRead(mapname, "GRASS", "GRASS_WIDTH_REPEATS", "0"));
		GRASS_HEIGHT = atof(IniRead(mapname, "GRASS", "GRASS_HEIGHT", "42.0"));
		GRASS_DISTANCE = atoi(IniRead(mapname, "GRASS", "GRASS_DISTANCE", "4096"));
		GRASS_MAX_SLOPE = atof(IniRead(mapname, "GRASS", "GRASS_MAX_SLOPE", "10.0"));
		GRASS_TYPE_UNIFORMALITY = atof(IniRead(mapname, "GRASS", "GRASS_TYPE_UNIFORMALITY", "0.97"));
		GRASS_TYPE_UNIFORMALITY_SCALER = atof(IniRead(mapname, "GRASS", "GRASS_TYPE_UNIFORMALITY_SCALER", "0.008"));
		GRASS_DISTANCE_FROM_ROADS = Q_clamp(0.0, atof(IniRead(mapname, "GRASS", "GRASS_DISTANCE_FROM_ROADS", "0.25")), 0.9);

		for (int i = 0; i < 16; i++)
		{// Init all grass scales...
			if (i < 3)
				GRASS_SCALES[i] = atof(IniRead(mapname, "GRASS", va("GRASS_SCALES%i", i), "1.0"));
			else
				GRASS_SCALES[i] = atof(IniRead(mapname, "GRASS", va("GRASS_SCALES%i", i), "1.25"));
		}
		
		// Parse any specified extra surface material types to add grasses to...
		extern const char *materialNames[MATERIAL_LAST];

		for (int m = 0; m < 8; m++)
		{
			char grassMaterial[64] = { 0 };
			strcpy(grassMaterial, IniRead(mapname, "GRASS", va("GRASS_ALLOW_MATERIAL%i", m), ""));

			if (!grassMaterial || !grassMaterial[0] || grassMaterial[0] == '\0' || strlen(grassMaterial) <= 1) continue;

			for (int i = 0; i < MATERIAL_LAST; i++)
			{
				if (!Q_stricmp(grassMaterial, materialNames[i]))
				{// Got one, add it to the allowed list...
					FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = i;
					FOLIAGE_ALLOWED_MATERIALS_NUM++;
					break;
				}
			}
		}
	}

	//
	// Moon...
	//
	if (DAY_NIGHT_CYCLE_ENABLED)
	{
		tr.moonImage = R_FindImageFile(IniRead(mapname, "MOON", "moonImage", "gfx/random"), IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		if (!tr.moonImage) tr.moonImage = R_FindImageFile("gfx/random", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		MOON_COLOR[0] = atof(IniRead(mapname, "MOON", "MOON_COLOR_R", "0.2"));
		MOON_COLOR[1] = atof(IniRead(mapname, "MOON", "MOON_COLOR_G", "0.2"));
		MOON_COLOR[2] = atof(IniRead(mapname, "MOON", "MOON_COLOR_B", "0.2"));
		MOON_ATMOSPHERE_COLOR[0] = atof(IniRead(mapname, "MOON", "MOON_ATMOSPHERE_COLOR_R", "1.0"));
		MOON_ATMOSPHERE_COLOR[1] = atof(IniRead(mapname, "MOON", "MOON_ATMOSPHERE_COLOR_G", "1.0"));
		MOON_ATMOSPHERE_COLOR[2] = atof(IniRead(mapname, "MOON", "MOON_ATMOSPHERE_COLOR_B", "1.0"));
		MOON_GLOW_STRENGTH = atof(IniRead(mapname, "MOON", "MOON_GLOW_STRENGTH", "0.5"));
		MOON_ROTATION_RATE = atof(IniRead(mapname, "MOON", "MOON_ROTATION_RATE", "0.08"));
	}


	if (!SHADOWS_ENABLED
		&& (StringContainsWord(mapname, "mandalore")
			|| StringContainsWord(mapname, "endor")
			|| StringContainsWord(mapname, "ilum")
			|| StringContainsWord(mapname, "taanab")
			|| StringContainsWord(mapname, "tatooine")
			|| StringContainsWord(mapname, "scarif"))
		&& !StringContainsWord(mapname, "tatooine_nights"))
	{
		if (!r_lowVram->integer)
		{
			SHADOWS_ENABLED = qtrue;
		}
	}

	if (StringContainsWord(mapname, "tatooine")
		&& !StringContainsWord(mapname, "tatooine_nights"))
	{
		FOG_POST_ENABLED = qfalse;
	}

	if (AURORA_ENABLED)
	{
		tr.auroraImage[0] = R_FindImageFile("gfx/misc/aurora1", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.auroraImage[1] = R_FindImageFile("gfx/misc/aurora2", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
	}
	
	if (r_groundFoliage->integer)
	{
		tr.groundFoliageImage[0] = R_FindImageFile(IniRead(mapname, "FOLIAGE", "GROUNDFOLIAGE_IMAGE1", "models/warzone/groundFoliage/groundFoliage00.png"), IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.groundFoliageImage[1] = R_FindImageFile(IniRead(mapname, "FOLIAGE", "GROUNDFOLIAGE_IMAGE2", "models/warzone/groundFoliage/groundFoliage01.png"), IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.groundFoliageImage[2] = R_FindImageFile(IniRead(mapname, "FOLIAGE", "GROUNDFOLIAGE_IMAGE3", "models/warzone/groundFoliage/groundFoliage02.png"), IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.groundFoliageImage[3] = R_FindImageFile(IniRead(mapname, "FOLIAGE", "GROUNDFOLIAGE_IMAGE4", "models/warzone/groundFoliage/groundFoliage03.png"), IMGTYPE_COLORALPHA, IMGFLAG_NONE);
	}

	if (MAP_MAX_VIS_RANGE && tr.distanceCull != MAP_MAX_VIS_RANGE)
	{
		tr.distanceCull = MAP_MAX_VIS_RANGE;
	}

	{
		// Roads maps... Try to load map based image first...
		tr.roadsMapImage = R_FindImageFile(va("maps/%s_roads.tga", currentMapName), IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);

		if (!tr.roadsMapImage)
		{// No default image? Use black...
			tr.roadsMapImage = tr.blackImage;
		}
	}

	memset(ROAD_TEXTURE, 0, sizeof(ROAD_TEXTURE));
	if (tr.roadsMapImage != tr.blackImage)
	{
		strcpy(ROAD_TEXTURE, IniRead(mapname, "ROADS", "ROADS_TEXTURE", "textures/roads/defaultRoad01.png"));
		tr.roadImage = R_FindImageFile(ROAD_TEXTURE, IMGTYPE_COLORALPHA, IMGFLAG_NONE);
	}

	//
	// Override climate file climate options with mapInfo ones, if found...
	//
	if ((GRASS_ENABLED && r_foliage->integer))
	{
		if (!GRASS_UNDERWATER_ONLY)
		{
			char grassImages[16][512] = { 0 };

			for (int i = 0; i < 16; i++)
			{
				strcpy(grassImages[i], IniRead(mapname, "GRASS", va("grassImage%i", i), "models/warzone/plants/grassblades02"));

				if (!R_TextureFileExists(grassImages[i]))
				{
					strcpy(grassImages[i], "models/warzone/plants/grassblades02");
				}
			}

			/*for (int i = 0; i < 16; i++)
			{
				tr.grassImage[i] = R_FindImageFile(grassImages[i], IMGTYPE_COLORALPHA, IMGFLAG_NONE);
			}*/

			tr.grassAliasImage = R_BakeTextures(grassImages, 16, "grass", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		}

		char seaGrassImages[4][512] = { 0 };

		for (int i = 0; i < 4; i++)
		{
			strcpy(seaGrassImages[i], IniRead(mapname, "GRASS", va("seaGrassImage%i", i), va("models/warzone/foliage/seagrass%i", i)));

			if (!R_TextureFileExists(seaGrassImages[i]))
			{
				strcpy(seaGrassImages[i], "models/warzone/plants/grassblades02");
			}
		}

		/*for (int i = 0; i < 4; i++)
		{
			tr.seaGrassImage[i] = R_FindImageFile(seaGrassImages[i], IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		}*/

		tr.seaGrassAliasImage = R_BakeTextures(seaGrassImages, 4, "seaGrass", IMGTYPE_COLORALPHA, IMGFLAG_NONE);

		// Grass maps... Try to load map based image first...
		tr.defaultGrassMapImage = R_FindImageFile(va("maps/%s_grass.tga", currentMapName), IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);

		if (!tr.defaultGrassMapImage)
		{// No map based image? Use default...
			tr.defaultGrassMapImage = R_FindImageFile("gfx/grassmap.tga", IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);
		}

		if (!tr.defaultGrassMapImage)
		{// No default image? Use white...
			tr.defaultGrassMapImage = tr.whiteImage;
		}
	}

	if (WATER_ENABLED)
	{// Water...
		tr.waterFoamImage[0] = R_FindImageFile("textures/water/waterFoamGrey.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.waterFoamImage[1] = R_FindImageFile("textures/water/waterFoamGrey02.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.waterFoamImage[2] = R_FindImageFile("textures/water/waterFoamGrey03.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		tr.waterFoamImage[3] = R_FindImageFile("textures/water/waterFoamGrey04.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		//tr.waterHeightImage = R_FindImageFile("textures/water/waterHeightMap.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);

		//if (r_glslWater->integer < 2)
		//	tr.waterNormalImage = R_FindImageFile("textures/water/waterNormalMap.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
		//else
		tr.waterNormalImage = R_FindImageFile("textures/water/waves.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);

		tr.waterCausicsImage = R_FindImageFile("textures/water/waterCausicsMap.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
	}

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Lodmodels are ^7%s^5 on this map.\n", LODMODEL_MAP ? "USED" : "UNUSED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Depth prepass is ^7%s^5 and max vis range is ^7%s^5 on this map.\n", DISABLE_DEPTH_PREPASS ? "DISABLED" : "ENABLED", MAP_MAX_VIS_RANGE ? va("%i", MAP_MAX_VIS_RANGE) : "default");

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Glow <textname>_g support is ^7%s^5 and lifts and portals merging is ^7%s^5 on this map.\n", DISABLE_MERGED_GLOWS ? "DISABLED" : "ENABLED", DISABLE_LIFTS_AND_PORTALS_MERGE ? "ENABLED" : "DISABLED");
	
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Displacement mapping support is ^7%s^5 and screen space reflections are ^7%s^5 on this map.\n", ENABLE_DISPLACEMENT_MAPPING ? "ENABLED" : "DISABLED", MAP_REFLECTION_ENABLED ? "ENABLED" : "DISABLED");

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Terrain tessellation is ^7%s^5 and terrain tessellation max level ^7%.4f^5 on this map.\n", TERRAIN_TESSELLATION_ENABLED ? "ENABLED" : "DISABLED", TERRAIN_TESSELLATION_LEVEL);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Terrain tessellation max offset is ^7%.4f^5 and terrain tessellation minimum vert size is ^7%.4f^5 on this map.\n", TERRAIN_TESSELLATION_OFFSET, TERRAIN_TESSELLATION_MIN_SIZE);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Day night cycle is ^7%s^5 and Day night cycle speed modifier is ^7%.4f^5 on this map.\n", DAY_NIGHT_CYCLE_ENABLED ? "ENABLED" : "DISABLED", DAY_NIGHT_CYCLE_SPEED);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Sun phong scale is ^7%.4f^5 and sun volumetric scale is ^7%.4f^5 on this map.\n", SUN_PHONG_SCALE, SUN_VOLUMETRIC_SCALE);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Sun color (main) ^7%.4f %.4f %.4f^5 (secondary) ^7%.4f %.4f %.4f^5 (tertiary) ^7%.4f %.4f %.4f^5 (ambient) ^7%.4f %.4f %.4f^5 on this map.\n", SUN_COLOR_MAIN[0], SUN_COLOR_MAIN[1], SUN_COLOR_MAIN[2], SUN_COLOR_SECONDARY[0], SUN_COLOR_SECONDARY[1], SUN_COLOR_SECONDARY[2], SUN_COLOR_TERTIARY[0], SUN_COLOR_TERTIARY[1], SUN_COLOR_TERTIARY[2], SUN_COLOR_AMBIENT[0], SUN_COLOR_AMBIENT[1], SUN_COLOR_AMBIENT[2]);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Procedural sky is ^7%s^5 on this map.\n", PROCEDURAL_SKY_ENABLED ? "ENABLED" : "DISABLED");

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Procedural clouds are ^7%s^5 and cloud scale is ^7%.4f^5 on this map.\n", PROCEDURAL_CLOUDS_ENABLED ? "ENABLED" : "DISABLED", PROCEDURAL_CLOUDS_CLOUDSCALE);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Cloud speed is ^7%.4f^5 and cloud cover is ^7%.4f^5 on this map.\n", PROCEDURAL_CLOUDS_SPEED, PROCEDURAL_CLOUDS_CLOUDCOVER);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Cloud dark is ^7%.4f^5 and cloud light is ^7%.4f^5 on this map.\n", PROCEDURAL_CLOUDS_DARK, PROCEDURAL_CLOUDS_LIGHT);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Cloud alpha is ^7%.4f^5 and cloud tint is ^7%.4f^5 on this map.\n", PROCEDURAL_CLOUDS_CLOUDALPHA, PROCEDURAL_CLOUDS_SKYTINT);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Late lighting is ^7%s^5 and lightmaps are ^7%s^5 on this map.\n", LATE_LIGHTING_ENABLED ? "ENABLED" : "DISABLED", MAP_LIGHTMAP_DISABLED ? "DISABLED" : "ENABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Lighting method is ^7%s^5 on this map.\n", MAP_LIGHTING_METHOD ? "Standard" : "PBR");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Use of palette on sky is ^7%s^5 and lightmap cap is ^7%.4f^5 on this map.\n", MAP_USE_PALETTE_ON_SKY ? "ENABLED" : "DISABLED", MAP_LIGHTMAP_MULTIPLIER);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Use of alt lightmap method is ^7%s^5 on this map.\n", (MAP_LIGHTMAP_ENHANCEMENT == 2) ? "FULL" : (MAP_LIGHTMAP_ENHANCEMENT == 1) ? "HYBRID" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Map HDR min is ^7%.4f^5 and map HDR max is ^7%.4f^5 on this map.\n", MAP_HDR_MIN, MAP_HDR_MAX);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Sky lighting scale is ^7%.4f^5 on this map.\n", SKY_LIGHTING_SCALE);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Day ambient color is ^7%.4f %.4f %.4f^5 and csb is ^7%.4f %.4f %.4f^5 on this map.\n", MAP_AMBIENT_COLOR[0], MAP_AMBIENT_COLOR[1], MAP_AMBIENT_COLOR[2], MAP_AMBIENT_CSB[0], MAP_AMBIENT_CSB[1], MAP_AMBIENT_CSB[2]);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Night ambient color is ^7%.4f %.4f %.4f^5 and csb is ^7%.4f %.4f %.4f^5 on this map.\n", MAP_AMBIENT_COLOR_NIGHT[0], MAP_AMBIENT_COLOR_NIGHT[1], MAP_AMBIENT_COLOR_NIGHT[2], MAP_AMBIENT_CSB_NIGHT[0], MAP_AMBIENT_CSB_NIGHT[1], MAP_AMBIENT_CSB_NIGHT[2]);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Day glow multiplier is ^7%.4f^5 and night glow multiplier is ^7%.4f^5 on this map.\n", MAP_GLOW_MULTIPLIER, MAP_GLOW_MULTIPLIER_NIGHT);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Emissive color scale is ^7%.4f^5 and night emissive color scale is ^7%.4f^5 on this map.\n", MAP_EMISSIVE_COLOR_SCALE, MAP_EMISSIVE_COLOR_SCALE_NIGHT);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Emissive radius scale is ^7%.4f^5 and night emissive radius scale is ^7%.4f^5on this map.\n", MAP_EMISSIVE_RADIUS_SCALE, MAP_EMISSIVE_RADIUS_SCALE_NIGHT);
	
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Shadows are ^7%s^5 on this map. Minimum brightness is ^7%.4f^5 and Maximum brightness is ^7%.4f^5 on this map.\n", SHADOWS_ENABLED ? "ENABLED" : "DISABLED", SHADOW_MINBRIGHT, SHADOW_MAXBRIGHT);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Ambient Occlusion is ^7%s^5 on this map. Minimum bright is ^7%.4f^5. Maximum bright is ^7%.4f^5.\n", AO_ENABLED ? "ENABLED" : "DISABLED", AO_MINBRIGHT, AO_MULTBRIGHT);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Ambient Occlusion blur is ^7%s^5 and Directional Occlusion is ^7%s^5 on this map.\n", AO_BLUR ? "ENABLED" : "DISABLED", AO_DIRECTIONAL ? "ENABLED" : "DISABLED");
	
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Auroras are ^7%s^5 and Day Auroras are ^7%s^5 on this map.\n", AURORA_ENABLED ? "ENABLED" : "DISABLED", AURORA_ENABLED_DAY ? "ENABLED" : "DISABLED");

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Fog is ^7%s^5 on this map.\n", FOG_POST_ENABLED ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Standard fog is ^7%s^5 on this map.\n", FOG_STANDARD_ENABLE ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Fog density is ^7%.4f^5 Fog range multiplier is ^7%.4f^5 on this map.\n", FOG_DENSITY, FOG_RANGE_MULTIPLIER);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Fog accumulation modifier is ^7%.4f^5 on this map.\n", FOG_ACCUMULATION_MODIFIER);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Fog color (main) ^7%.4f %.4f %.4f^5 (sun) ^7%.4f %.4f %.4f^5 on this map.\n", FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2], FOG_COLOR_SUN[0], FOG_COLOR_SUN[1], FOG_COLOR_SUN[2]);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Volumetric fog is ^7%s^5 on this map.\n", FOG_VOLUMETRIC_ENABLE ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Volumetric fog density is ^7%.4f^5 and Volumetric fog cloudiness is ^7%.4f^5 on this map.\n", FOG_VOLUMETRIC_DENSITY, FOG_VOLUMETRIC_CLOUDINESS);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Volumetric fog wind is ^7%.4f^5 and Volumetric fog velocity is ^7%.4f^5 on this map.\n", FOG_VOLUMETRIC_WIND, FOG_VOLUMETRIC_VELOCITY);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Volumetric fog strength is ^7%.4f^5 on this map.\n", FOG_VOLUMETRIC_STRENGTH);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Volumetric fog color ^7%.4f %.4f %.4f^5 on this map.\n", FOG_VOLUMETRIC_COLOR[0], FOG_VOLUMETRIC_COLOR[1], FOG_VOLUMETRIC_COLOR[2]);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Enhanced water is ^7%s^5 and water fog is ^7%s^5 on this map.\n", WATER_ENABLED ? "ENABLED" : "DISABLED", WATER_FOG_ENABLED ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Water color (shallow) ^7%.4f %.4f %.4f^5 (deep) ^7%.4f %.4f %.4f^5 on this map.\n", WATER_COLOR_SHALLOW[0], WATER_COLOR_SHALLOW[1], WATER_COLOR_SHALLOW[2], WATER_COLOR_DEEP[0], WATER_COLOR_DEEP[1], WATER_COLOR_DEEP[2]);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Water reflectiveness is ^7%.4f^5 and oceans are ^7%s^5 on this map.\n", WATER_REFLECTIVENESS, WATER_USE_OCEAN ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Water far plane is ^7%s^5 and wave height is ^7%.4f^5 on this map.\n", WATER_FARPLANE_ENABLED ? "ENABLED" : "DISABLED", WATER_WAVE_HEIGHT);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Road texture is ^7%s^5 and road control texture is %s on this map.\n", ROAD_TEXTURE, (!tr.roadsMapImage || tr.roadsMapImage == tr.blackImage) ? "none" : tr.roadsMapImage->imgName);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Grass is ^7%s^5 and underwater grass only is ^7%s^5 on this map.\n", GRASS_ENABLED ? "ENABLED" : "DISABLED", GRASS_UNDERWATER_ONLY ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Grass density is ^7%i^5 and grass distance is ^7%i^5 on this map.\n", GRASS_DENSITY, GRASS_DISTANCE);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Grass width repeats is ^7%i^5 and grass max slope is ^7%.4f^5 on this map.\n", GRASS_WIDTH_REPEATS, GRASS_MAX_SLOPE);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Grass height is ^7%.4f^5 and grass distance from roads is ^7%.4f^5 on this map.\n", GRASS_HEIGHT, GRASS_DISTANCE_FROM_ROADS);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Grass uniformality is ^7%.4f^5 and grass uniformality scaler is ^7%.4f^5 on this map.\n", GRASS_TYPE_UNIFORMALITY, GRASS_TYPE_UNIFORMALITY_SCALER);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Moon seed texture is ^7%s^5 and moon rotation rate is ^7%.4f^5 on this map.\n", tr.moonImage->imgName, MOON_ROTATION_RATE);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Moon color is ^7%.4f %.4f %.4f^5 and moon glow strength ^7%.4f^5 on this map.\n", MOON_COLOR[0], MOON_COLOR[1], MOON_COLOR[2], MOON_GLOW_STRENGTH);
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Moon atmosphere color is ^7%.4f %.4f %.4f^5 on this map.\n", MOON_ATMOSPHERE_COLOR[0], MOON_ATMOSPHERE_COLOR[1], MOON_ATMOSPHERE_COLOR[2]);

	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5JKA weather is ^7%s^5 and WZ weather is ^7%s^5 on this map.\n", JKA_WEATHER_ENABLED ? "ENABLED" : "DISABLED", WZ_WEATHER_ENABLED ? "ENABLED" : "DISABLED");
	ri->Printf(PRINT_ALL, "^4*** ^3MAP-INFO^4: ^5Atmospheric name is ^7%s^5 and WZ weather sound only is ^7%s^5 on this map.\n", CURRENT_WEATHER_OPTION, WZ_WEATHER_SOUND_ONLY ? "ENABLED" : "DISABLED");

	qglFinish();
}

extern void RB_SetupGlobalWeatherZone(void);
extern void RE_WorldEffectCommand_REAL(const char *command, qboolean noHelp);

extern qboolean CONTENTS_INSIDE_OUTSIDE_FOUND;

void SetupWeather(char *mapname)
{
	if (JKA_WEATHER_ENABLED && WZ_WEATHER_ENABLED)
	{
		WZ_WEATHER_SOUND_ONLY = qtrue;
	}

	char *atmosphericString = (char*)IniRead(mapname, "ATMOSPHERICS", "WEATHER_TYPE", "");

	if (strlen(atmosphericString) <= 1)
	{// Check old config file for redundancy...
		atmosphericString = (char*)IniRead(va("maps/%s.atmospherics", currentMapName), "ATMOSPHERICS", "WEATHER_TYPE", "");

		if (strlen(atmosphericString) > 1)
		{// Redundancy...
			if (JKA_WEATHER_ENABLED)
				WZ_WEATHER_SOUND_ONLY = qtrue;
			else
				WZ_WEATHER_ENABLED = qtrue;
		}
	}

	if (WZ_WEATHER_SOUND_ONLY)
	{// If WZ weather is in sound only mode, then JKA weather system is enabled...
		JKA_WEATHER_ENABLED = qtrue;
		WZ_WEATHER_ENABLED = qfalse;
		WZ_WEATHER_SOUND_ONLY = qtrue;
	}

	if (JKA_WEATHER_ENABLED && strlen(atmosphericString) > 1)
	{
		strcpy(CURRENT_WEATHER_OPTION, atmosphericString);

		RE_WorldEffectCommand_REAL("clear", qtrue);

		if (JKA_WEATHER_ENABLED && !CONTENTS_INSIDE_OUTSIDE_FOUND)
			RB_SetupGlobalWeatherZone();

		/* Convert WZ weather names to JKA ones... */
		if (!Q_stricmp(CURRENT_WEATHER_OPTION, "rain"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7rain^5 for this map.\n");
			RE_WorldEffectCommand_REAL("lightrain", qtrue);
			//RE_WorldEffectCommand_REAL("light_fog", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "heavyrain"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7heavyrain^5 for this map.\n");
			RE_WorldEffectCommand_REAL("rain", qtrue);
			//RE_WorldEffectCommand_REAL("fog", qtrue);
			RE_WorldEffectCommand_REAL("wind", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "rainstorm") || !Q_stricmp(CURRENT_WEATHER_OPTION, "storm"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7rainstorm^5 for this map.\n");
			RE_WorldEffectCommand_REAL("heavyrain", qtrue);
			//RE_WorldEffectCommand_REAL("heavyrainfog", qtrue);
			RE_WorldEffectCommand_REAL("gustingwind", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "vividrain"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7vividrain^5 for this map.\n");
			RE_WorldEffectCommand_REAL("vividrain", qtrue);
			//RE_WorldEffectCommand_REAL("light_fog", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "vividrain2"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7vividrain2^5 for this map.\n");
			RE_WorldEffectCommand_REAL("vividrain2", qtrue);
			//RE_WorldEffectCommand_REAL("light_fog", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "snow"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7snow^5 for this map.\n");
			RE_WorldEffectCommand_REAL("snow", qtrue);
			//RE_WorldEffectCommand_REAL("fog", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "heavysnow"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7heavysnow^5 for this map.\n");
			RE_WorldEffectCommand_REAL("snow", qtrue);
			//RE_WorldEffectCommand_REAL("heavyrainfog", qtrue);
			RE_WorldEffectCommand_REAL("wind", qtrue);
		}
		else if (!Q_stricmp(CURRENT_WEATHER_OPTION, "snowstorm"))
		{
			ri->Printf(PRINT_ALL, "^1*** ^3JKA^5 atmospherics set to ^7snowstorm^5 for this map.\n");
			RE_WorldEffectCommand_REAL("snow", qtrue);
			//RE_WorldEffectCommand_REAL("heavyrainfog", qtrue);
			RE_WorldEffectCommand_REAL("gustingwind", qtrue);
		}
		/* Nothing? Try to use what was set in the mapInfo setting directly... */
		else
		{
			RE_WorldEffectCommand_REAL(CURRENT_WEATHER_OPTION, qtrue);
		}
	}
	else
	{
		strcpy(CURRENT_WEATHER_OPTION, "none");
	}
}

extern const char *materialNames[MATERIAL_LAST];
extern void ParseMaterial(const char **text);

qboolean MAPPING_LoadMapClimateInfo(void)
{
	if (strlen(CURRENT_CLIMATE_OPTION) <= 1)
	{// Only look in climate file if we didn't load it from mapInfo...
		const char		*climateName = NULL;

		char mapname[256] = { 0 };

		// because JKA uses mp/ dir, why??? so pointless...
		if (IniExists(va("foliage/%s.climateInfo", currentMapName)))
			sprintf(mapname, "foliage/%s.climateInfo", currentMapName);
		else if (IniExists(va("foliage/mp/%s.climateInfo", currentMapName)))
			sprintf(mapname, "foliage/mp/%s.climateInfo", currentMapName);
		else
			sprintf(mapname, "foliage/%s.climateInfo", currentMapName);

		climateName = IniRead(mapname, "CLIMATE", "CLIMATE_TYPE", "");

		memset(CURRENT_CLIMATE_OPTION, 0, sizeof(CURRENT_CLIMATE_OPTION));
		strncpy(CURRENT_CLIMATE_OPTION, climateName, strlen(climateName));

		if (CURRENT_CLIMATE_OPTION[0] == '\0')
		{
			ri->Printf(PRINT_ALL, "^1*** ^3%s^5: No climate setting found in climateInfo or mapInfo files.\n", "CLIMATE");
			return qfalse;
		}

		ri->Printf(PRINT_ALL, "^1*** ^3%s^5: Successfully loaded climateInfo file ^7foliage/%s.climateInfo^5. Using ^3%s^5 climate option.\n", "CLIMATE", currentMapName, CURRENT_CLIMATE_OPTION);
	}
	else
	{
		ri->Printf(PRINT_ALL, "^1*** ^3%s^5: Successfully loaded climate from mapInfo file ^7maps/%s.mapInfo^5. Using ^3%s^5 climate option.\n", "CLIMATE", currentMapName, CURRENT_CLIMATE_OPTION);
	}

	return qtrue;
}

void R_GenerateNavMesh(void)
{
#if 0
	InputGeom* geom = 0;
	Sample* sample = 0;

	geom = new InputGeom;

	if (!geom->load(&ctx, path))
	{
		delete geom;
		geom = 0;

		// Destroy the sample if it already had geometry loaded, as we've just deleted it!
		if (sample && sample->getInputGeom())
		{
			delete sample;
			sample = 0;
		}

		showLog = true;
		logScroll = 0;
		ctx.dumpLog("Geom load log %s:", meshName.c_str());
	}

	if (sample && geom)
	{
		sample->handleMeshChanged(geom);
	}

	if (geom || sample)
	{
		const float* bmin = 0;
		const float* bmax = 0;
		if (geom)
		{
			bmin = geom->getNavMeshBoundsMin();
			bmax = geom->getNavMeshBoundsMax();
		}
	}

	if (geom)
	{
		char text[64];
		snprintf(text, 64, "Verts: %.1fk  Tris: %.1fk",
			geom->getMesh()->getVertCount() / 1000.0f,
			geom->getMesh()->getTriCount() / 1000.0f);

		ri->Printf(PRINT_WARNING, "%s", text);
	}

	sample->handleBuild();
#endif
}

void R_LoadMapInfo(void)
{
	R_SetupMapInfo();

	if (!R_TextureFileExists("gfx/random2K.tga"))
	{
		R_CreateRandom2KImage("");
		tr.random2KImage[0] = R_FindImageFile("gfx/random2K.tga", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.random2KImage[0] = R_FindImageFile("gfx/random2K.tga", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}

	if (!R_TextureFileExists("gfx/random2Ka.tga"))
	{
		R_CreateRandom2KImage("a");
		tr.random2KImage[1] = R_FindImageFile("gfx/random2Ka.tga", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.random2KImage[1] = R_FindImageFile("gfx/random2Ka.tga", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}


	tr.ssdoNoiseImage = R_FindImageFile("gfx/ssdoNoise.png", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);

#if 0
	if (!R_TextureFileExists("gfx/defaultDetail.tga"))
	{
		R_CreateDefaultDetail();
		tr.defaultDetail = R_FindImageFile("gfx/defaultDetail.tga", IMGTYPE_DETAILMAP, IMGFLAG_NO_COMPRESSION | IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.defaultDetail = R_FindImageFile("gfx/defaultDetail.tga", IMGTYPE_DETAILMAP, IMGFLAG_NO_COMPRESSION | IMGFLAG_NOLIGHTSCALE);
	}
#else
	tr.defaultDetail = tr.whiteImage;
#endif

#if 0 // Gonna reuse random2k[1] to save some vram...
	if (!R_TextureFileExists("gfx/splatControlImage.tga"))
	{
		R_CreateRandom2KImage("splatControl");
		tr.defaultSplatControlImage = R_FindImageFile("gfx/splatControlImage.tga", IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.defaultSplatControlImage = R_FindImageFile("gfx/splatControlImage.tga", IMGTYPE_SPLATCONTROLMAP, IMGFLAG_NOLIGHTSCALE);
	}
#else
	tr.defaultSplatControlImage = tr.random2KImage[1];
#endif

	if (r_colorCorrection->integer)
	{
		// Color Palette... Try to load map based image first...
		tr.paletteImage = R_FindImageFile(va("maps/%s_palette.png", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);

		if (!tr.paletteImage)
		{// No map based image? Use default...
			tr.paletteImage = R_FindImageFile("gfx/palette.png", IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
		}

		if (!tr.paletteImage)
		{// No default image? Use white...
			tr.paletteImage = tr.whiteImage;
		}
	}

	if ((r_foliage->integer && GRASS_ENABLED))
	{
		MAPPING_LoadMapClimateInfo();

		// UQ1: Might add these materials to the climate definition ini files later... meh...
		if (!strcmp(CURRENT_CLIMATE_OPTION, "springpineforest"))
		{
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_MUD; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_DIRT; FOLIAGE_ALLOWED_MATERIALS_NUM++;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "endorredwoodforest"))
		{
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_MUD; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_DIRT; FOLIAGE_ALLOWED_MATERIALS_NUM++;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "snowpineforest"))
		{
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_MUD; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_DIRT; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_SNOW; FOLIAGE_ALLOWED_MATERIALS_NUM++;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "tropicalold"))
		{
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_MUD; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_DIRT; FOLIAGE_ALLOWED_MATERIALS_NUM++;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "tropical"))
		{
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_MUD; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_DIRT; FOLIAGE_ALLOWED_MATERIALS_NUM++;
		}
		else // Default to new tropical...
		{
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_MUD; FOLIAGE_ALLOWED_MATERIALS_NUM++;
			FOLIAGE_ALLOWED_MATERIALS[FOLIAGE_ALLOWED_MATERIALS_NUM] = MATERIAL_DIRT; FOLIAGE_ALLOWED_MATERIALS_NUM++;
		}

		float TREE_SCALE_MULTIPLIER = 1.0;

		if (ri->FS_FileExists(va("climates/%s.climate", CURRENT_CLIMATE_OPTION)))
		{// Check if we have a climate file in climates/ for this map...
			// Have a climate file in climates/
			TREE_SCALE_MULTIPLIER = atof(IniRead(va("climates/%s.climate", CURRENT_CLIMATE_OPTION), "TREES", "treeScaleMultiplier", "1.0"));
		}
		else
		{// Seems we have no climate file in climates/ for the map... Check maps/
			TREE_SCALE_MULTIPLIER = atof(IniRead(va("maps/%s.climate", currentMapName), "TREES", "treeScaleMultiplier", "1.0"));
		}
	}

	tr.shinyImage = R_FindImageFile("textures/common/env_chrome.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
	//tr.shinyImage = R_FindImageFile("textures/common/env_shiny.jpg", IMGTYPE_COLORALPHA, IMGFLAG_NONE);

#if 0
	if (!R_TextureFileExists(va("mapImage/%s.tga", currentMapName)))
	{
		R_CreateBspMapImage();
		tr.mapImage = R_FindImageFile(va("mapImage/%s.tga", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.mapImage = R_FindImageFile(va("mapImage/%s.tga", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
#endif

#if 0
	if (!R_TextureFileExists(va("heightMapImage/%s.tga", currentMapName)))
	{
		R_CreateHeightMapImage();
		tr.heightMapImage = R_FindImageFile(va("heightMapImage/%s.tga", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.heightMapImage = R_FindImageFile(va("heightMapImage/%s.tga", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
#endif

#if 0
	if (!R_TextureFileExists(va("foliageMapImage/%s.tga", currentMapName)))
	{
		R_CreateFoliageMapImage();
		tr.foliageMapImage = R_FindImageFile(va("foliageMapImage/%s.tga", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
	else
	{
		tr.foliageMapImage = R_FindImageFile(va("foliageMapImage/%s.tga", currentMapName), IMGTYPE_COLORALPHA, IMGFLAG_NOLIGHTSCALE);
	}
#endif
}