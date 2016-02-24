extern "C" {
#include "../qcommon/q_shared.h"
#include "../game/g_local.h"
#include "../game/surfaceflags.h"

	extern qboolean InFOV( vec3_t spot, vec3_t from, vec3_t fromAngles, int hFOV, int vFOV );

	// =======================================================================================================================================
	//
	//                                                             Foliage Rendering...
	//
	// =======================================================================================================================================


#define			FOLIAGE_MAX_FOLIAGES 1048576

	//
	// BEGIN - FOLIAGE OPTIONS
	//

	//#define		__NO_PLANTS__ // Disable plants...

#define			__FOLIAGE_DENSITY__ 0
	//
	// END - FOLIAGE OPTIONS
	//

	char		CURRENT_CLIMATE_OPTION[256] = { 0 };

#define		PLANT_SCALE_MULTIPLIER 1.0

float		TREE_SCALE_MULTIPLIER = 1.0;

	qboolean	FOLIAGE_LOADED = qfalse;
	int			FOLIAGE_NUM_POSITIONS = 0;
	vec3_t		FOLIAGE_POSITIONS[FOLIAGE_MAX_FOLIAGES];
	int			FOLIAGE_TREE_SELECTION[FOLIAGE_MAX_FOLIAGES];
	float		FOLIAGE_TREE_SCALE[FOLIAGE_MAX_FOLIAGES];

	float		FOLIAGE_AREA_SIZE =				512;
	float		FOLIAGE_VISIBLE_DISTANCE =		FOLIAGE_AREA_SIZE*2.5;
	float		FOLIAGE_TREE_VISIBLE_DISTANCE = FOLIAGE_AREA_SIZE*70.0;

#define		FOLIAGE_AREA_MAX				131072
#define		FOLIAGE_AREA_MAX_FOLIAGES		256

	int			FOLIAGE_AREAS_COUNT = 0;
	int			FOLIAGE_AREAS_LIST_COUNT[FOLIAGE_AREA_MAX];
	int			FOLIAGE_AREAS_LIST[FOLIAGE_AREA_MAX][FOLIAGE_AREA_MAX_FOLIAGES];
	vec3_t		FOLIAGE_AREAS_MINS[FOLIAGE_AREA_MAX];
	vec3_t		FOLIAGE_AREAS_MAXS[FOLIAGE_AREA_MAX];

	float		FOLIAGE_TREE_RADIUS[16] = { 0 };
	float		FOLIAGE_TREE_BILLBOARD_SIZE[16] = { 0 };

	int IN_RANGE_AREAS_LIST_COUNT = 0;
	int IN_RANGE_AREAS_LIST[1024];
	int IN_RANGE_TREE_AREAS_LIST_COUNT = 0;
	int IN_RANGE_TREE_AREAS_LIST[8192];

	qboolean FOLIAGE_In_Bounds( int areaNum, int foliageNum )
	{
		if (foliageNum >= FOLIAGE_NUM_POSITIONS) return qfalse;

		if (FOLIAGE_AREAS_MINS[areaNum][0] < FOLIAGE_POSITIONS[foliageNum][0]
		&& FOLIAGE_AREAS_MINS[areaNum][1] < FOLIAGE_POSITIONS[foliageNum][1]
		&& FOLIAGE_AREAS_MAXS[areaNum][0] >= FOLIAGE_POSITIONS[foliageNum][0]
		&& FOLIAGE_AREAS_MAXS[areaNum][1] >= FOLIAGE_POSITIONS[foliageNum][1])
		{
			return qtrue;
		}

		return qfalse;
	}

	void FOLIAGE_Setup_Foliage_Areas( void )
	{
		int		areaNum = 0, i = 0;
		vec3_t	mins, maxs, mapMins, mapMaxs;

		VectorSet(mapMins, 128000, 128000, 0);
		VectorSet(mapMaxs, -128000, -128000, 0);

		// Find map bounds first... Reduce area numbers...
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			if (FOLIAGE_POSITIONS[i][0] < mapMins[0])
				mapMins[0] = FOLIAGE_POSITIONS[i][0];

			if (FOLIAGE_POSITIONS[i][0] > mapMaxs[0])
				mapMaxs[0] = FOLIAGE_POSITIONS[i][0];

			if (FOLIAGE_POSITIONS[i][1] < mapMins[1])
				mapMins[1] = FOLIAGE_POSITIONS[i][1];

			if (FOLIAGE_POSITIONS[i][1] > mapMaxs[1])
				mapMaxs[1] = FOLIAGE_POSITIONS[i][1];
		}

		mapMins[0] -= 1024.0;
		mapMins[1] -= 1024.0;
		mapMaxs[0] += 1024.0;
		mapMaxs[1] += 1024.0;

		VectorSet(mins, mapMins[0], mapMins[1], 0);
		VectorSet(maxs, mapMins[0] + FOLIAGE_AREA_SIZE, mapMins[1] + FOLIAGE_AREA_SIZE, 0);

		FOLIAGE_AREAS_COUNT = 0;

		for (areaNum = 0; areaNum < FOLIAGE_AREA_MAX; areaNum++)
		{
			if (mins[1] > mapMaxs[1]) break; // found our last area...

			FOLIAGE_AREAS_LIST_COUNT[areaNum] = 0;

			while (FOLIAGE_AREAS_LIST_COUNT[areaNum] == 0 && mins[1] <= mapMaxs[1])
			{// While loop is so we can skip zero size areas for speed...
				VectorCopy(mins, FOLIAGE_AREAS_MINS[areaNum]);
				VectorCopy(maxs, FOLIAGE_AREAS_MAXS[areaNum]);

				// Assign foliages to the area lists...
				for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
				{
					if (FOLIAGE_In_Bounds(areaNum, i))
					{
						FOLIAGE_AREAS_LIST[areaNum][FOLIAGE_AREAS_LIST_COUNT[areaNum]] = i;
						FOLIAGE_AREAS_LIST_COUNT[areaNum]++;
					}
				}

				mins[0] += FOLIAGE_AREA_SIZE;
				maxs[0] = mins[0] + FOLIAGE_AREA_SIZE;

				if (mins[0] > mapMaxs[0])
				{
					mins[0] = mapMins[0];
					maxs[0] = mapMins[0] + FOLIAGE_AREA_SIZE;

					mins[1] += FOLIAGE_AREA_SIZE;
					maxs[1] = mins[1] + FOLIAGE_AREA_SIZE;
				}
			}
		}

		FOLIAGE_AREAS_COUNT = areaNum;

		trap->Print("Generated %i foliage areas. %i total foliages.\n", FOLIAGE_AREAS_COUNT, FOLIAGE_NUM_POSITIONS);
	}
}

extern "C" {

	int FOLIAGE_AreaNumForOrg( vec3_t moveOrg )
	{
		for (int areaNum = 0; areaNum < FOLIAGE_AREAS_COUNT; areaNum++)
		{
			if (FOLIAGE_AREAS_MINS[areaNum][0] < moveOrg[0]
				&& FOLIAGE_AREAS_MINS[areaNum][1] < moveOrg[1]
				&& FOLIAGE_AREAS_MAXS[areaNum][0] >= moveOrg[0]
				&& FOLIAGE_AREAS_MAXS[areaNum][1] >= moveOrg[1])
			{
				return areaNum;
			}
		}

		return qfalse;
	}

	void FOLIAGE_SetupClosestAreas( gentity_t *ent, vec3_t moveOrg )
	{
		qboolean areaChanged = qfalse;

		if (ent->CURRENT_AREA < 0 
			|| ent->CURRENT_AREA >= FOLIAGE_AREAS_COUNT
			|| !(FOLIAGE_AREAS_MINS[ent->CURRENT_AREA][0] < moveOrg[0] && FOLIAGE_AREAS_MINS[ent->CURRENT_AREA][1] < moveOrg[1] && FOLIAGE_AREAS_MAXS[ent->CURRENT_AREA][0] >= moveOrg[0] && FOLIAGE_AREAS_MAXS[ent->CURRENT_AREA][1] >= moveOrg[1]))
		{// If we have no valid CURRENT_AREA, or have move outside the current one, update...
			ent->CURRENT_AREA = FOLIAGE_AreaNumForOrg( ent->r.currentOrigin );
			areaChanged = qtrue;
		}

		if (areaChanged || DistanceHorizontal(ent->CLOSE_AREA_PREVIOUS_ORG, ent->r.currentOrigin) > 256)
		{// Only update when the entity is moving...
			ent->CLOSE_AREA_LIST_COUNT = 0;
			VectorCopy(moveOrg, ent->CLOSE_AREA_PREVIOUS_ORG);

			for (int areaNum = 0; areaNum < FOLIAGE_AREAS_COUNT; areaNum++)
			{
				float DIST = DistanceHorizontal(FOLIAGE_AREAS_MINS[areaNum], moveOrg);
				float DIST2 = DistanceHorizontal(FOLIAGE_AREAS_MAXS[areaNum], moveOrg);

				if (DIST < FOLIAGE_AREA_SIZE * 2.0 || DIST2 < FOLIAGE_AREA_SIZE * 2.0)
				{
					ent->CLOSE_AREA_LIST[ent->CLOSE_AREA_LIST_COUNT] = areaNum;
					ent->CLOSE_AREA_LIST_COUNT++;
				}
			}
		}
	}

	qboolean FOLIAGE_TreeSolidBlocking( gentity_t *ent, vec3_t moveOrg )
	{
		if (ent->client && ent->client->sess.sessionTeam == FACTION_SPECTATOR) return qfalse;

		FOLIAGE_SetupClosestAreas( ent, moveOrg );

		for (int areaListPos = 0; areaListPos < ent->CLOSE_AREA_LIST_COUNT; areaListPos++)
		{
			int areaNum = ent->CLOSE_AREA_LIST[areaListPos];

			for (int treeNum = 0; treeNum < FOLIAGE_AREAS_LIST_COUNT[areaNum]; treeNum++)
			{
				int		THIS_TREE_NUM = FOLIAGE_AREAS_LIST[areaNum][treeNum];
				int		THIS_TREE_TYPE = FOLIAGE_TREE_SELECTION[THIS_TREE_NUM]-1;
				float	TREE_RADIUS = FOLIAGE_TREE_RADIUS[THIS_TREE_TYPE] * FOLIAGE_TREE_SCALE[THIS_TREE_NUM]*TREE_SCALE_MULTIPLIER;
				float	TREE_HEIGHT = FOLIAGE_TREE_SCALE[THIS_TREE_NUM]*2.5*FOLIAGE_TREE_BILLBOARD_SIZE[FOLIAGE_TREE_SELECTION[THIS_TREE_NUM]-1]*TREE_SCALE_MULTIPLIER;
				float	DIST = DistanceHorizontal(FOLIAGE_POSITIONS[THIS_TREE_NUM], moveOrg);
				float	hDist =  DistanceHorizontal(FOLIAGE_POSITIONS[THIS_TREE_NUM], ent->r.currentOrigin);

				if (DIST <= TREE_RADIUS 
					&& (moveOrg[2] >= FOLIAGE_POSITIONS[THIS_TREE_NUM][2] && moveOrg[2] <= FOLIAGE_POSITIONS[THIS_TREE_NUM][2] + (TREE_HEIGHT*2.0))
					&& DIST < hDist 				// Move pos would be closer
					&& hDist > TREE_RADIUS)			// Not already stuck in tree...
				{
					//trap->Print("SERVER: Blocked by tree %i. Radius %f. Distance %f. Type %i. AreaNum %i\n", THIS_TREE_NUM, TREE_RADIUS, DIST, THIS_TREE_TYPE, areaNum);
					return qtrue;
				}
			}
		}

		return qfalse;
	}

	extern int BG_SiegeGetPairedValue(char *buf, char *key, char *outbuf);

	qboolean FOLIAGE_LoadMapClimateInfo( void )
	{
		fileHandle_t	f;
		int				fLen = 0;
		char			fileBuffer[4096];
		char			parseBuf[4096];
		vmCvar_t		mapname;

		trap->Cvar_Register( &mapname, "mapname", "", CVAR_ROM | CVAR_SERVERINFO );

		memset(CURRENT_CLIMATE_OPTION, 0, sizeof(CURRENT_CLIMATE_OPTION));

		fLen = trap->FS_Open(va("foliage/%s.climateInfo", mapname.string), &f, FS_READ);

		if (!f || fLen < 0)
		{//couldn't open file, just use the defaults
			trap->Print( "^1*** ^3%s^5: No map climate info file ^7foliage/%s.climateInfo^5. Using default climate option.\n", GAME_VERSION, mapname.string );
			return qfalse;
		}

		if (fLen == 0)
		{//file was empty, just use the defaults
			trap->FS_Close(f);
			trap->Print( "^1*** ^3%s^5: No map climate info file ^7foliage/%s.climateInfo^5. Using default climate option.\n", GAME_VERSION, mapname.string );
			return qfalse;
		}

		if (fLen >= 4096)
		{
			trap->Print("^1Error: foliage/%s.climateInfo is over the climateInfo filesize limit.^7\n", mapname.string);
			trap->FS_Close(f);
			trap->Print( "^1*** ^3%s^5: No map climate info file ^7foliage/%s.climateInfo^5. Using default climate option.\n", GAME_VERSION, mapname.string );
			return qfalse;
		}

		trap->FS_Read(fileBuffer, fLen, f);
		fileBuffer[fLen] = 0;
		trap->FS_Close(f);

		if (BG_SiegeGetPairedValue(fileBuffer, "climateSelection", parseBuf))
		{
			strcpy(CURRENT_CLIMATE_OPTION, parseBuf);
		}

		trap->Print( "^1*** ^3%s^5: Successfully loaded climateInfo file ^7foliage/%s.climateInfo^5. Using ^3%s^5 climate option.\n", GAME_VERSION, mapname.string, CURRENT_CLIMATE_OPTION );

		return qtrue;
	}

	qboolean FOLIAGE_LoadFoliagePositions( void )
	{
		fileHandle_t	f;
		int				i = 0;
		int				fileCount = 0;
		int				treeCount = 0;
		vmCvar_t		mapname;

		trap->Cvar_Register( &mapname, "mapname", "", CVAR_ROM | CVAR_SERVERINFO );

		trap->FS_Open( va( "foliage/%s.foliage", mapname.string), &f, FS_READ );

		if ( !f )
		{
			return qfalse;
		}

		trap->FS_Read( &fileCount, sizeof(int), f );

		for (i = 0; i < fileCount; i++)
		{
			vec3_t	unneededVec3;
			int		unneededInt;
			float	unneededFloat;

			trap->FS_Read( &FOLIAGE_POSITIONS[treeCount], sizeof(vec3_t), f );
			trap->FS_Read( &unneededVec3, sizeof(vec3_t), f );
			trap->FS_Read( &unneededInt, sizeof(int), f );
			trap->FS_Read( &unneededFloat, sizeof(float), f );
			trap->FS_Read( &unneededFloat, sizeof(float), f );
			trap->FS_Read( &FOLIAGE_TREE_SELECTION[treeCount], sizeof(int), f );
			trap->FS_Read( &unneededFloat, sizeof(float), f );
			trap->FS_Read( &FOLIAGE_TREE_SCALE[treeCount], sizeof(float), f );

			if (FOLIAGE_TREE_SELECTION[treeCount] > 0)
			{// Only keep positions with trees...
				treeCount++;
			}
		}

		FOLIAGE_NUM_POSITIONS = treeCount;

		trap->FS_Close(f);

		Com_Printf( "*** %s: Successfully loaded %i foliage points from foliage file foliage/%s.foliage. Found %i trees.\n", GAME_VERSION,
			fileCount, mapname.string, FOLIAGE_NUM_POSITIONS );

		FOLIAGE_Setup_Foliage_Areas();

		return qtrue;
	}

	qboolean FOLIAGE_IgnoreFoliageOnMap( void )
	{
		vmCvar_t		mapname;

		trap->Cvar_Register( &mapname, "mapname", "", CVAR_ROM | CVAR_SERVERINFO );

		if (StringContainsWord(mapname.string, "eisley")
			|| StringContainsWord(mapname.string, "desert")
			|| StringContainsWord(mapname.string, "tatooine")
			|| StringContainsWord(mapname.string, "hoth")
			|| StringContainsWord(mapname.string, "mp/ctf1")
			|| StringContainsWord(mapname.string, "mp/ctf2")
			|| StringContainsWord(mapname.string, "mp/ctf4")
			|| StringContainsWord(mapname.string, "mp/ctf5")
			|| StringContainsWord(mapname.string, "mp/ffa1")
			|| StringContainsWord(mapname.string, "mp/ffa2")
			|| StringContainsWord(mapname.string, "mp/ffa3")
			|| StringContainsWord(mapname.string, "mp/ffa4")
			|| StringContainsWord(mapname.string, "mp/ffa5")
			|| StringContainsWord(mapname.string, "mp/duel1")
			|| StringContainsWord(mapname.string, "mp/duel2")
			|| StringContainsWord(mapname.string, "mp/duel3")
			|| StringContainsWord(mapname.string, "mp/duel4")
			|| StringContainsWord(mapname.string, "mp/duel5")
			|| StringContainsWord(mapname.string, "mp/duel7")
			|| StringContainsWord(mapname.string, "mp/duel9")
			|| StringContainsWord(mapname.string, "mp/duel10")
			|| StringContainsWord(mapname.string, "bespin_streets")
			|| StringContainsWord(mapname.string, "bespin_platform"))
		{// Ignore this map... We know we don't need grass here...
			return qtrue;
		}

		return qfalse;
	}

	void FOLIAGE_LoadTrees( void )
	{
		int spot = 0;

		Com_Printf("*** Warzone: Loading Trees.\n");

		if (FOLIAGE_IgnoreFoliageOnMap())
		{// Ignore this map... We know we don't need grass here...
			FOLIAGE_NUM_POSITIONS = 0;
			FOLIAGE_LOADED = qtrue;
			Com_Printf("*** Warzone: Trees are ignored on this map.\n");
			return;
		}

		if (!FOLIAGE_LOADED)
		{
			FOLIAGE_LoadFoliagePositions();
			FOLIAGE_LOADED = qtrue;
			FOLIAGE_LoadMapClimateInfo();
		}

		if (FOLIAGE_NUM_POSITIONS <= 0)
		{
			Com_Printf("*** Warzone: No foliage positions found for map.\n");
			return;
		}

		TREE_SCALE_MULTIPLIER = 3.0;

		if (!strcmp(CURRENT_CLIMATE_OPTION, "springpineforest"))
		{
			FOLIAGE_TREE_BILLBOARD_SIZE[0] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[1] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[2] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[3] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[4] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[5] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[6] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[7] = 183.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[8] = 203.0;

			FOLIAGE_TREE_RADIUS[0] = 24.0;
			FOLIAGE_TREE_RADIUS[1] = 24.0;
			FOLIAGE_TREE_RADIUS[2] = 24.0;
			FOLIAGE_TREE_RADIUS[3] = 24.0;
			FOLIAGE_TREE_RADIUS[4] = 24.0;
			FOLIAGE_TREE_RADIUS[5] = 24.0;
			FOLIAGE_TREE_RADIUS[6] = 24.0;
			FOLIAGE_TREE_RADIUS[7] = 38.0;
			FOLIAGE_TREE_RADIUS[8] = 38.0;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "endorpineforest"))
		{
			TREE_SCALE_MULTIPLIER = 4.0;

			FOLIAGE_TREE_BILLBOARD_SIZE[0] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[1] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[2] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[3] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[4] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[5] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[6] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[7] = 183.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[8] = 203.0;

			FOLIAGE_TREE_RADIUS[0] = 24.0;
			FOLIAGE_TREE_RADIUS[1] = 24.0;
			FOLIAGE_TREE_RADIUS[2] = 24.0;
			FOLIAGE_TREE_RADIUS[3] = 24.0;
			FOLIAGE_TREE_RADIUS[4] = 24.0;
			FOLIAGE_TREE_RADIUS[5] = 24.0;
			FOLIAGE_TREE_RADIUS[6] = 24.0;
			FOLIAGE_TREE_RADIUS[7] = 38.0;
			FOLIAGE_TREE_RADIUS[8] = 38.0;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "snowpineforest"))
		{
			FOLIAGE_TREE_BILLBOARD_SIZE[0] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[1] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[2] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[3] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[4] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[5] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[6] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[7] = 203.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[8] = 203.0;

			FOLIAGE_TREE_RADIUS[0] = 24.0;
			FOLIAGE_TREE_RADIUS[1] = 24.0;
			FOLIAGE_TREE_RADIUS[2] = 24.0;
			FOLIAGE_TREE_RADIUS[3] = 24.0;
			FOLIAGE_TREE_RADIUS[4] = 24.0;
			FOLIAGE_TREE_RADIUS[5] = 24.0;
			FOLIAGE_TREE_RADIUS[6] = 24.0;
			FOLIAGE_TREE_RADIUS[7] = 38.0;
			FOLIAGE_TREE_RADIUS[8] = 38.0;
		}
		else if (!strcmp(CURRENT_CLIMATE_OPTION, "tropicalold"))
		{
			FOLIAGE_TREE_BILLBOARD_SIZE[0] = 118.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[1] = 118.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[2] = 204.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[3] = 112.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[4] = 163.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[5] = 163.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[6] = 183.6;
			FOLIAGE_TREE_BILLBOARD_SIZE[7] = 183.6;
			FOLIAGE_TREE_BILLBOARD_SIZE[8] = 153.0;

			FOLIAGE_TREE_RADIUS[0] = 24.0;
			FOLIAGE_TREE_RADIUS[1] = 24.0;
			FOLIAGE_TREE_RADIUS[2] = 72.0;
			FOLIAGE_TREE_RADIUS[3] = 52.0;
			FOLIAGE_TREE_RADIUS[4] = 52.0;
			FOLIAGE_TREE_RADIUS[5] = 52.0;
			FOLIAGE_TREE_RADIUS[6] = 72.0;
			FOLIAGE_TREE_RADIUS[7] = 72.0;
			FOLIAGE_TREE_RADIUS[8] = 72.0;
		}
		else // Default to new tropical...
		{
			FOLIAGE_TREE_BILLBOARD_SIZE[0] = 128.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[1] = 118.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[2] = 204.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[3] = 112.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[4] = 183.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[5] = 183.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[6] = 203.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[7] = 203.0;
			FOLIAGE_TREE_BILLBOARD_SIZE[8] = 153.0;

			FOLIAGE_TREE_RADIUS[0] = 24.0;
			FOLIAGE_TREE_RADIUS[1] = 24.0;
			FOLIAGE_TREE_RADIUS[2] = 72.0;
			FOLIAGE_TREE_RADIUS[3] = 52.0;
			FOLIAGE_TREE_RADIUS[4] = 38.0;
			FOLIAGE_TREE_RADIUS[5] = 38.0;
			FOLIAGE_TREE_RADIUS[6] = 38.0;
			FOLIAGE_TREE_RADIUS[7] = 38.0;
			FOLIAGE_TREE_RADIUS[8] = 72.0;
		}
	}
}

