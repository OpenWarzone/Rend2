/* marker */
#define MAP_C



/* dependencies */
#include "q3map2.h"
#include "inifile.h"

extern void SetEntityBounds( entity_t *e );
extern void LoadEntityIndexMap( entity_t *e );
extern void AdjustBrushesForOrigin( entity_t *ent );

#define			MAX_FOREST_MODELS 64

float			TREE_SCALE_MULTIPLIER = 2.5;
char			TREE_MODELS[MAX_FOREST_MODELS][128] = { 0 };
float			TREE_OFFSETS[MAX_FOREST_MODELS] = { -4.0 };
float			TREE_SCALES[MAX_FOREST_MODELS] = { 1.0 };
float			TREE_FORCED_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
float			TREE_FORCED_BUFFER_DISTANCE[MAX_FOREST_MODELS] = { 0.0 };
float			TREE_FORCED_DISTANCE_FROM_SAME[MAX_FOREST_MODELS] = { 0.0 };

void FOLIAGE_LoadClimateData( char *filename )
{
	int i = 0;

	// Read all the tree info from the new .climate ini files...
	TREE_SCALE_MULTIPLIER = atof(IniRead(filename, "TREES", "treeScaleMultiplier", "1.0"));

	Sys_Printf("Tree scale for this climate is %f.\n", TREE_SCALE_MULTIPLIER);

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		strcpy(TREE_MODELS[i], IniRead(filename, "TREES", va("treeModel%i", i), ""));
		TREE_OFFSETS[i] = atof(IniRead(filename, "TREES", va("treeZoffset%i", i), "-4.0"));
		TREE_SCALES[i] = atof(IniRead(filename, "TREES", va("treeScale%i", i), "1.0"));
		TREE_FORCED_MAX_ANGLE[i] = atof(IniRead(filename, "TREES", va("treeForcedMaxAngle%i", i), "0.0"));
		TREE_FORCED_BUFFER_DISTANCE[i] = atof(IniRead(filename, "TREES", va("treeForcedBufferDistance%i", i), "0.0"));
		TREE_FORCED_DISTANCE_FROM_SAME[i] = atof(IniRead(filename, "TREES", va("treeForcedDistanceFromSame%i", i), "0.0"));

		if (strcmp(TREE_MODELS[i], ""))
			Sys_Printf("Tree %i. Model %s. Offset %f. Scale %f. MaxAngle %i. BufferDist %f. InstanceDist %f.\n", i, TREE_MODELS[i], TREE_OFFSETS[i], TREE_SCALES[i], TREE_FORCED_MAX_ANGLE[i], TREE_FORCED_BUFFER_DISTANCE[i], TREE_FORCED_DISTANCE_FROM_SAME[i]);
	}
}


#define			FOLIAGE_MAX_FOLIAGES 2097152

int				FOLIAGE_NUM_POSITIONS = 0;
qboolean		*FOLIAGE_ASSIGNED;
vec3_t			FOLIAGE_POSITIONS[FOLIAGE_MAX_FOLIAGES];
vec3_t			FOLIAGE_NORMALS[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_ANGLES[FOLIAGE_MAX_FOLIAGES];
int				FOLIAGE_TREE_SELECTION[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_SCALE[FOLIAGE_MAX_FOLIAGES];

qboolean FOLIAGE_LoadFoliagePositions( char *filename )
{
	FILE			*f;
	int				i = 0;
	int				fileCount = 0;
	int				treeCount = 0;

	f = fopen (filename, "rb");

	if ( !f )
	{
		return qfalse;
	}

	fread( &fileCount, sizeof(int), 1, f );

	for (i = 0; i < fileCount; i++)
	{
		//vec3_t	unneededVec3;
		int		unneededInt;
		float	unneededFloat;

		fread( &FOLIAGE_POSITIONS[treeCount], sizeof(vec3_t), 1, f );
		fread( &FOLIAGE_NORMALS[treeCount], sizeof(vec3_t), 1, f );
		fread( &unneededInt, sizeof(int), 1, f );
		fread( &unneededFloat, sizeof(float), 1, f );
		fread( &unneededFloat, sizeof(float), 1, f );
		fread( &FOLIAGE_TREE_SELECTION[treeCount], sizeof(int), 1, f );
		fread( &FOLIAGE_TREE_ANGLES[treeCount], sizeof(float), 1, f );
		fread( &FOLIAGE_TREE_SCALE[treeCount], sizeof(float), 1, f );

		if (FOLIAGE_TREE_SELECTION[treeCount] > 0)
		{// Only keep positions with trees...
			treeCount++;
		}
	}

	FOLIAGE_NUM_POSITIONS = treeCount;

	fclose(f);

	return qtrue;
}

#define M_PI		3.14159265358979323846f

void vectoangles( const vec3_t value1, vec3_t angles ) {
	float	forward;
	float	yaw, pitch;

	if ( value1[1] == 0 && value1[0] == 0 ) {
		yaw = 0;
		if ( value1[2] > 0 ) {
			pitch = 90;
		}
		else {
			pitch = 270;
		}
	}
	else {
		if ( value1[0] ) {
			yaw = ( atan2 ( value1[1], value1[0] ) * 180 / M_PI );
		}
		else if ( value1[1] > 0 ) {
			yaw = 90;
		}
		else {
			yaw = 270;
		}
		if ( yaw < 0 ) {
			yaw += 360;
		}

		forward = sqrt ( value1[0]*value1[0] + value1[1]*value1[1] );
		pitch = ( atan2(value1[2], forward) * 180 / M_PI );
		if ( pitch < 0 ) {
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

extern float Distance(vec3_t pos1, vec3_t pos2);

//#define QRAND_MAX 32768
static uint32_t	holdrand = 0x89abcdef;

void Rand_Init(int seed)
{
	holdrand = seed;
}

int irand(int min, int max)
{
	int		result;

	//assert((max - min) < QRAND_MAX);

	max++;
	holdrand = (holdrand * 214013L) + 2531011L;
	result = holdrand >> 17;
	result = ((result * (max - min)) >> 15) + min;
	return(result);
}

void ReassignModels ( void )
{
	int				i;
	float			*BUFFER_RANGES;
	float			*SAME_RANGES;
	int				POSSIBLES[MAX_FOREST_MODELS] = { 0 };
	float			POSSIBLES_BUFFERS[MAX_FOREST_MODELS] = { 0.0 };
	float			POSSIBLES_SAME_RANGES[MAX_FOREST_MODELS] = { 0.0 };
	float			POSSIBLES_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
	int				NUM_POSSIBLES = 0;

	FOLIAGE_ASSIGNED = (qboolean*)malloc(sizeof(qboolean)*FOLIAGE_MAX_FOLIAGES);
	BUFFER_RANGES = (float*)malloc(sizeof(float)*FOLIAGE_MAX_FOLIAGES);
	SAME_RANGES = (float*)malloc(sizeof(float)*FOLIAGE_MAX_FOLIAGES);

	memset(FOLIAGE_ASSIGNED, qfalse, sizeof(qboolean)*FOLIAGE_MAX_FOLIAGES);
	memset(BUFFER_RANGES, 0.0, sizeof(float)*FOLIAGE_MAX_FOLIAGES);
	memset(SAME_RANGES, 0.0, sizeof(float)*FOLIAGE_MAX_FOLIAGES);

	Rand_Init(FOLIAGE_POSITIONS[0][0]);
	
	//Sys_Printf("Finding angles models.\n");

	// Find non-angle models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (TREE_FORCED_MAX_ANGLE[i] == 0.0)
		{
			continue;
		}

		if (!strcmp(TREE_MODELS[i], ""))
		{
			continue;
		}

		POSSIBLES[NUM_POSSIBLES] = i;
		POSSIBLES_BUFFERS[NUM_POSSIBLES] = TREE_FORCED_BUFFER_DISTANCE[i];
		POSSIBLES_SAME_RANGES[NUM_POSSIBLES] = TREE_FORCED_DISTANCE_FROM_SAME[i];
		POSSIBLES_MAX_ANGLE[NUM_POSSIBLES] = TREE_FORCED_MAX_ANGLE[i];
		NUM_POSSIBLES++;
	}

	//Sys_Printf("Assign angles models from %i possibles.\n", NUM_POSSIBLES);

	// First add any non-steep options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			float pitch;
			qboolean bad = qfalse;
			int j;

			printLabelledProgress("RandomizeNoAngleModels", i, FOLIAGE_NUM_POSITIONS);

			if (FOLIAGE_ASSIGNED[i])
			{
				continue;
			}

			int selected = irand(0,NUM_POSSIBLES-1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (j == i)
				{
					continue;
				}

				if (!FOLIAGE_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(FOLIAGE_POSITIONS[i], FOLIAGE_POSITIONS[j]);

				if (dist <= BUFFER_RANGES[j])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					break;
				}

				if ((FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= SAME_RANGES[j]) || dist <= POSSIBLES_SAME_RANGES[selected])
				{// Not within this object's same type range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			vec3_t angles;
			vectoangles( FOLIAGE_NORMALS[i], angles );
			pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;

			if (pitch > POSSIBLES_MAX_ANGLE[selected] || pitch < -POSSIBLES_MAX_ANGLE[selected])
			{// Bad slope...
				//Sys_Printf("Position %i angles too great (%f).\n", i, pitch);
				continue;
			}

			//Sys_Printf("Position %i angles OK! (%f).\n", i, pitch);

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
			FOLIAGE_ASSIGNED[i] = qtrue;
		}
	}

	NUM_POSSIBLES = 0;

	//Sys_Printf("Finding other models.\n");

	// Find other models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (TREE_FORCED_MAX_ANGLE[i] != 0.0)
		{
			continue;
		}

		if (!strcmp(TREE_MODELS[i], ""))
		{
			continue;
		}

		POSSIBLES[NUM_POSSIBLES] = i;
		POSSIBLES_BUFFERS[NUM_POSSIBLES] = TREE_FORCED_BUFFER_DISTANCE[i];
		POSSIBLES_SAME_RANGES[NUM_POSSIBLES] = TREE_FORCED_DISTANCE_FROM_SAME[i];
		NUM_POSSIBLES++;
	}

	//Sys_Printf("Assign other models from %i possibles.\n", NUM_POSSIBLES);

	// Now add other options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			qboolean bad = qfalse;
			int tries = 0;
			int j;

			printLabelledProgress("RandomizeModels", i, FOLIAGE_NUM_POSITIONS);

			if (FOLIAGE_ASSIGNED[i])
			{
				continue;
			}

			int selected = irand(0,NUM_POSSIBLES-1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (tries > 32)
				{
					bad = qtrue;
					break;
				}

				if (j == i)
				{
					continue;
				}

				if (!FOLIAGE_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(FOLIAGE_POSITIONS[i], FOLIAGE_POSITIONS[j]);

				if (dist <= BUFFER_RANGES[j])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					break;
				}

				if ((FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= SAME_RANGES[j]) || dist <= POSSIBLES_SAME_RANGES[selected])
				{// Not within this object's same type range... OK!
					selected = irand(0,NUM_POSSIBLES-1);
					tries++;
				}
			}

			if (bad)
			{
				continue;
			}

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
			FOLIAGE_ASSIGNED[i] = qtrue;
		}
	}

	free(BUFFER_RANGES);
	free(SAME_RANGES);

	int count = 0;

	for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
	{
		if (FOLIAGE_ASSIGNED[i]) 
		{
			/*if (TREE_FORCED_MAX_ANGLE[FOLIAGE_TREE_SELECTION[i]] != 0.0)
				Sys_Printf("Model %i was reassigned as %s. [MAX_ANGLE_MODEL]\n", i, TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]);
			else
				Sys_Printf("Model %i was reassigned as %s.\n", i, TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]);*/

			count++;
		}
	}

	Sys_Printf("%9d of %i positions successfully reassigned to new models.\n", count, FOLIAGE_NUM_POSITIONS-1);
}

//#define __ADD_TREES_EARLY__ // Add trees to map's brush list instanty...

extern void MoveBrushesToWorld( entity_t *ent );
extern qboolean StringContainsWord(const char *haystack, const char *needle);

void GenerateMapForest ( void )
{
	if (generateforest)
	{
		if (FOLIAGE_NUM_POSITIONS > 0)
		{// First re-assign model types based on buffer ranges and instance type ranges...
			int i;

#if defined(__ADD_TREES_EARLY__)
			Sys_Printf( "Adding %i trees to bsp.\n", FOLIAGE_NUM_POSITIONS );
#endif

			ReassignModels();

			for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
			{
				printLabelledProgress("GenerateMapForest", i, FOLIAGE_NUM_POSITIONS);

				if (!FOLIAGE_ASSIGNED[i]) 
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				const char		*classname, *value;
				float			lightmapScale;
				vec3_t          lightmapAxis;
				int			    smoothNormals;
				int				vertTexProj;
				char			shader[ MAX_QPATH ];
				shaderInfo_t	*celShader = NULL;
				brush_t			*brush;
				parseMesh_t		*patch;
				qboolean		funcGroup;
				char			castShadows, recvShadows;
				qboolean		forceNonSolid, forceNoClip, forceNoTJunc, forceMeta;
				vec3_t          minlight, minvertexlight, ambient, colormod;
				float           patchQuality, patchSubdivision;

				/* setup */
				entitySourceBrushes = 0;
				mapEnt = &entities[ numEntities ];
				numEntities++;
				memset( mapEnt, 0, sizeof( *mapEnt ) );

				mapEnt->mapEntityNum = 0;

				//mapEnt->mapEntityNum = numMapEntities;
				//numMapEntities++;

				//mapEnt->mapEntityNum = 0 - numMapEntities;

				//mapEnt->forceSubmodel = qtrue;

				VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);
				mapEnt->origin[2] += TREE_OFFSETS[FOLIAGE_TREE_SELECTION[i]];

				{
					char str[32];
					sprintf( str, "%f %f %f", mapEnt->origin[ 0 ], mapEnt->origin[ 1 ], mapEnt->origin[ 2 ] );
					SetKeyValue( mapEnt, "origin", str );
				}

				{
					char str[32];
					sprintf( str, "%f", FOLIAGE_TREE_SCALE[i]*2.0*TREE_SCALE_MULTIPLIER*TREE_SCALES[FOLIAGE_TREE_SELECTION[i]] );
					SetKeyValue( mapEnt, "modelscale", str );
				}

				{
					char str[32];
					sprintf( str, "%f", FOLIAGE_TREE_ANGLES[i] );
					SetKeyValue( mapEnt, "angle", str );
				}

				/*{
				char str[32];
				vec3_t angles;
				vectoangles( FOLIAGE_NORMALS[i], angles );
				angles[PITCH] += 90;
				angles[YAW] = 270.0 - FOLIAGE_TREE_ANGLES[i];
				sprintf( str, "%f %f %f", angles[0], angles[1], angles[2] );
				SetKeyValue( mapEnt, "angles", str );
				}*/

				/*{
				char str[32];
				sprintf( str, "%i", 2 );
				SetKeyValue( mapEnt, "spawnflags", str );
				}*/

				//Sys_Printf( "Generated tree at %f %f %f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


				/* ydnar: get classname */
				SetKeyValue( mapEnt, "classname", "misc_model");
				classname = ValueForKey( mapEnt, "classname" );

				SetKeyValue( mapEnt, "model", TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]); // test tree

				//Sys_Printf( "Generated tree at %f %f %f. Model %s.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], TROPICAL_TREES[FOLIAGE_TREE_SELECTION[i]] );

				funcGroup = qfalse;

				/* get explicit shadow flags */
				GetEntityShadowFlags( mapEnt, NULL, &castShadows, &recvShadows, (funcGroup || mapEnt->mapEntityNum == 0) ? qtrue : qfalse );

				/* vortex: get lightmap scaling value for this entity */
				GetEntityLightmapScale( mapEnt, &lightmapScale, 0);

				/* vortex: get lightmap axis for this entity */
				GetEntityLightmapAxis( mapEnt, lightmapAxis, NULL );

				/* vortex: per-entity normal smoothing */
				GetEntityNormalSmoothing( mapEnt, &smoothNormals, 0);

				/* vortex: per-entity _minlight, _ambient, _color, _colormod  */
				GetEntityMinlightAmbientColor( mapEnt, NULL, minlight, minvertexlight, ambient, colormod, qtrue );
				if( mapEnt == &entities[ 0 ] )
				{
					/* worldspawn have it empty, since it's keys sets global parms */
					VectorSet( minlight, 0, 0, 0 );
					VectorSet( minvertexlight, 0, 0, 0 );
					VectorSet( ambient, 0, 0, 0 );
					VectorSet( colormod, 1, 1, 1 );
				}

				/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
				GetEntityPatchMeta( mapEnt, &forceMeta, &patchQuality, &patchSubdivision, 1.0, patchSubdivisions);

				/* vortex: vertical texture projection */
				if( strcmp( "", ValueForKey( mapEnt, "_vtcproj" ) ) || strcmp( "", ValueForKey( mapEnt, "_vp" ) ) )
				{
					vertTexProj = IntForKey(mapEnt, "_vtcproj");
					if (vertTexProj <= 0.0f)
						vertTexProj = IntForKey(mapEnt, "_vp");
				}
				else
					vertTexProj = 0;

				/* ydnar: get cel shader :) for this entity */
				value = ValueForKey( mapEnt, "_celshader" );
				if( value[ 0 ] == '\0' )	
					value = ValueForKey( &entities[ 0 ], "_celshader" );
				if( value[ 0 ] != '\0' )
				{
					sprintf( shader, "textures/%s", value );
					celShader = ShaderInfoForShader( shader );
					//Sys_FPrintf (SYS_VRB, "Entity %d (%s) has cel shader %s\n", mapEnt->mapEntityNum, classname, celShader->shader );
				}
				else
					celShader = NULL;

				/* vortex: _nonsolid forces detail non-solid brush */
				forceNonSolid = ((IntForKey(mapEnt, "_nonsolid") > 0) || (IntForKey(mapEnt, "_ns") > 0)) ? qtrue : qfalse;

				/* vortex: preserve original face winding, don't clip by bsp tree */
				forceNoClip = ((IntForKey(mapEnt, "_noclip") > 0) || (IntForKey(mapEnt, "_nc") > 0)) ? qtrue : qfalse;

				/* vortex: do not apply t-junction fixing (preserve original face winding) */
				forceNoTJunc = ((IntForKey(mapEnt, "_notjunc") > 0) || (IntForKey(mapEnt, "_ntj") > 0)) ? qtrue : qfalse;

				/* attach stuff to everything in the entity */
				for( brush = mapEnt->brushes; brush != NULL; brush = brush->next )
				{
					brush->entityNum = mapEnt->mapEntityNum;
					brush->mapEntityNum = mapEnt->mapEntityNum;
					brush->castShadows = castShadows;
					brush->recvShadows = recvShadows;
					brush->lightmapScale = lightmapScale;
					VectorCopy( lightmapAxis, brush->lightmapAxis ); /* vortex */
					brush->smoothNormals = smoothNormals; /* vortex */
					brush->noclip = forceNoClip; /* vortex */
					brush->noTJunc = forceNoTJunc; /* vortex */
					brush->vertTexProj = vertTexProj; /* vortex */
					VectorCopy( minlight, brush->minlight ); /* vortex */
					VectorCopy( minvertexlight, brush->minvertexlight ); /* vortex */
					VectorCopy( ambient, brush->ambient ); /* vortex */
					VectorCopy( colormod, brush->colormod ); /* vortex */
					brush->celShader = celShader;
					if (forceNonSolid == qtrue)
					{
						brush->detail = qtrue;
						brush->nonsolid = qtrue;
						brush->noclip = qtrue;
					}
				}

				for( patch = mapEnt->patches; patch != NULL; patch = patch->next )
				{
					patch->entityNum = mapEnt->mapEntityNum;
					patch->mapEntityNum = mapEnt->mapEntityNum;
					patch->castShadows = castShadows;
					patch->recvShadows = recvShadows;
					patch->lightmapScale = lightmapScale;
					VectorCopy( lightmapAxis, patch->lightmapAxis ); /* vortex */
					patch->smoothNormals = smoothNormals; /* vortex */
					patch->vertTexProj = vertTexProj; /* vortex */
					patch->celShader = celShader;
					patch->patchMeta = forceMeta; /* vortex */
					patch->patchQuality = patchQuality; /* vortex */
					patch->patchSubdivisions = patchSubdivision; /* vortex */
					VectorCopy( minlight, patch->minlight ); /* vortex */
					VectorCopy( minvertexlight, patch->minvertexlight ); /* vortex */
					VectorCopy( ambient, patch->ambient ); /* vortex */
					VectorCopy( colormod, patch->colormod ); /* vortex */
					patch->nonsolid = forceNonSolid;
				}

				/* vortex: store map entity num */
				{
					char buf[32];
					sprintf( buf, "%i", mapEnt->mapEntityNum ); 
					SetKeyValue( mapEnt, "_mapEntityNum", buf );
				}

				/* ydnar: gs mods: set entity bounds */
				SetEntityBounds( mapEnt );

				/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
				LoadEntityIndexMap( mapEnt );

				/* get entity origin and adjust brushes */
				GetVectorForKey( mapEnt, "origin", mapEnt->origin );
				if ( mapEnt->originbrush_origin[ 0 ] || mapEnt->originbrush_origin[ 1 ] || mapEnt->originbrush_origin[ 2 ] )
					AdjustBrushesForOrigin( mapEnt );


#if defined(__ADD_TREES_EARLY__)
				AddTriangleModels( 0, qtrue, qtrue );
				EmitBrushes( mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes );
				//MoveBrushesToWorld( mapEnt );
				numEntities--;
#endif
			}

			free(FOLIAGE_ASSIGNED);

#if defined(__ADD_TREES_EARLY__)
			Sys_Printf( "Finished adding trees.\n" );
#endif
		}
	}
}
