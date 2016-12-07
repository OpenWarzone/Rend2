/* -------------------------------------------------------------------------------

Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define MODEL_C


//#define __MODEL_SIMPLIFICATION__


/* dependencies */
#include "q3map2.h"


extern qboolean StringContainsWord(const char *haystack, const char *needle);
extern qboolean RemoveDuplicateBrushPlanes(brush_t *b);
extern void CullSides(entity_t *e);
extern void CullSidesStats(void);

extern int g_numHiddenFaces, g_numCoinFaces;


/*
PicoPrintFunc()
callback for picomodel.lib
*/

void PicoPrintFunc(int level, const char *str)
{
	if (str == NULL)
		return;
	switch (level)
	{
	case PICO_NORMAL:
		Sys_Printf("%s\n", str);
		break;

	case PICO_VERBOSE:
		Sys_FPrintf(SYS_VRB, "%s\n", str);
		break;

	case PICO_WARNING:
		Sys_Printf("WARNING: %s\n", str);
		break;

	case PICO_ERROR:
		Sys_Printf("ERROR: %s\n", str);
		break;

	case PICO_FATAL:
		Error("ERROR: %s\n", str);
		break;
	}
}



/*
PicoLoadFileFunc()
callback for picomodel.lib
*/

void PicoLoadFileFunc(char *name, byte **buffer, int *bufSize)
{
	*bufSize = vfsLoadFile((const char*)name, (void**)buffer, 0);
}

/*
FindModel() - ydnar
finds an existing picoModel and returns a pointer to the picoModel_t struct or NULL if not found
*/

picoModel_t *FindModel(const char *name, int frame)
{
	int	i;

	/* init */
	if (numPicoModels <= 0)
		memset(picoModels, 0, sizeof(picoModels));

	/* dummy check */
	if (name == NULL || name[0] == '\0')
		return NULL;

	/* search list */
	for (i = 0; i < MAX_MODELS; i++)
	if (picoModels[i] != NULL && !strcmp(PicoGetModelName(picoModels[i]), name) && PicoGetModelFrameNum(picoModels[i]) == frame)
		return picoModels[i];

	/* no matching picoModel found */
	return NULL;
}



/*
LoadModel() - ydnar
loads a picoModel and returns a pointer to the picoModel_t struct or NULL if not found
*/

picoModel_t *LoadModel(const char *name, int frame)
{
	int	i;
	picoModel_t	*model, **pm;

	/* init */
	if (numPicoModels <= 0)
		memset(picoModels, 0, sizeof(picoModels));

	/* dummy check */
	if (name == NULL || name[0] == '\0')
		return NULL;

	/* try to find existing picoModel */
	model = FindModel(name, frame);
	if (model != NULL)
		return model;

	/* none found, so find first non-null picoModel */
	pm = NULL;
	for (i = 0; i < MAX_MODELS; i++)
	{
		if (picoModels[i] == NULL)
		{
			pm = &picoModels[i];
			break;
		}
	}

	/* too many picoModels? */
	if (pm == NULL)
		Error("MAX_MODELS (%d) exceeded, there are too many model files referenced by the map.", MAX_MODELS);

	/* attempt to parse model */
	*pm = PicoLoadModel((char*)name, frame);

	/* if loading failed, make a bogus model to silence the rest of the warnings */
	if (*pm == NULL)
	{
		Sys_Printf("LoadModel: failed to load model %s frame %i. Fix your map!", name, frame);

		/* allocate a new model */
		*pm = PicoNewModel();
		if (*pm == NULL)
			return NULL;

		/* set data */
		PicoSetModelName(*pm, (char *)name);
		PicoSetModelFrameNum(*pm, frame);
	}

	/* debug code */
#if 0
	{
		int				numSurfaces, numVertexes;
		picoSurface_t	*ps;


		Sys_Printf( "Model %s\n", name );
		numSurfaces = PicoGetModelNumSurfaces( *pm );
		for( i = 0; i < numSurfaces; i++ )
		{
			ps = PicoGetModelSurface( *pm, i );
			numVertexes = PicoGetSurfaceNumVertexes( ps );
			Sys_Printf( "Surface %d has %d vertexes\n", i, numVertexes );
		}
	}
#endif

	/* set count */
	if (*pm != NULL)
		numPicoModels++;

	/* return the picoModel */
	return *pm;
}

/*
PreloadModel() - vortex
preloads picomodel, returns true once loaded a model
*/

qboolean PreloadModel(const char *name, int frame)
{
	picoModel_t	*model;

	/* try to find model */
	model = FindModel(name, frame);
	if (model != NULL)
		return qfalse;

	/* get model */
	model = LoadModel(name, frame);
	if (model != NULL)
		return qtrue;

	/* fail */
	return qfalse;
}

/*
InsertModel() - ydnar
adds a picomodel into the bsp
*/

float Distance(vec3_t pos1, vec3_t pos2)
{
	vec3_t vLen;
	VectorSubtract(pos1, pos2, vLen);
	return VectorLength(vLen);
}

extern void LoadShaderImages(shaderInfo_t *si);
extern void Decimate(picoModel_t *model, char *fileNameOut);

int numSolidSurfs = 0, numHeightCulledSurfs = 0, numSizeCulledSurfs = 0, numExperimentalCulled = 0;

//#define __USE_CULL_BOX_SYSTEM__
//#define __USE_CULL_BOX_SYSTEM2__

#ifndef __USE_CULL_BOX_SYSTEM2__
#ifdef __USE_CULL_BOX_SYSTEM__
void AddQuadStamp2(vec3_t quadVerts[4], unsigned int *numIndexes, unsigned int *indexes, unsigned int *numVerts, vec3_t *xyz)
{
	int             ndx;

	ndx = *numVerts;

	// triangle indexes for a simple quad
	indexes[*numIndexes] = ndx;
	indexes[*numIndexes + 1] = ndx + 1;
	indexes[*numIndexes + 2] = ndx + 3;

	indexes[*numIndexes + 3] = ndx + 3;
	indexes[*numIndexes + 4] = ndx + 1;
	indexes[*numIndexes + 5] = ndx + 2;

	xyz[ndx + 0][0] = quadVerts[0][0];
	xyz[ndx + 0][1] = quadVerts[0][1];
	xyz[ndx + 0][2] = quadVerts[0][2];

	xyz[ndx + 1][0] = quadVerts[1][0];
	xyz[ndx + 1][1] = quadVerts[1][1];
	xyz[ndx + 1][2] = quadVerts[1][2];

	xyz[ndx + 2][0] = quadVerts[2][0];
	xyz[ndx + 2][1] = quadVerts[2][1];
	xyz[ndx + 2][2] = quadVerts[2][2];

	xyz[ndx + 3][0] = quadVerts[3][0];
	xyz[ndx + 3][1] = quadVerts[3][1];
	xyz[ndx + 3][2] = quadVerts[3][2];

	*numVerts += 4;
	*numIndexes += 6;
}

void AddCube(const vec3_t mins, const vec3_t maxs, unsigned int *numIndexes, unsigned int *indexes, unsigned int *numVerts, vec3_t *xyz)
{
	vec3_t quadVerts[4];

	VectorSet(quadVerts[0], mins[0], mins[1], mins[2]);
	VectorSet(quadVerts[1], mins[0], maxs[1], mins[2]);
	VectorSet(quadVerts[2], mins[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[3], mins[0], mins[1], maxs[2]);
	AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, xyz);

	VectorSet(quadVerts[0], maxs[0], mins[1], maxs[2]);
	VectorSet(quadVerts[1], maxs[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[2], maxs[0], maxs[1], mins[2]);
	VectorSet(quadVerts[3], maxs[0], mins[1], mins[2]);
	AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, xyz);

	VectorSet(quadVerts[0], mins[0], mins[1], maxs[2]);
	VectorSet(quadVerts[1], mins[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[2], maxs[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[3], maxs[0], mins[1], maxs[2]);
	AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, xyz);

	VectorSet(quadVerts[0], maxs[0], mins[1], mins[2]);
	VectorSet(quadVerts[1], maxs[0], maxs[1], mins[2]);
	VectorSet(quadVerts[2], mins[0], maxs[1], mins[2]);
	VectorSet(quadVerts[3], mins[0], mins[1], mins[2]);
	AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, xyz);

	VectorSet(quadVerts[0], mins[0], mins[1], mins[2]);
	VectorSet(quadVerts[1], mins[0], mins[1], maxs[2]);
	VectorSet(quadVerts[2], maxs[0], mins[1], maxs[2]);
	VectorSet(quadVerts[3], maxs[0], mins[1], mins[2]);
	AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, xyz);

	VectorSet(quadVerts[0], maxs[0], maxs[1], mins[2]);
	VectorSet(quadVerts[1], maxs[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[2], mins[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[3], mins[0], maxs[1], mins[2]);
	AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, xyz);
}
#endif //__USE_CULL_BOX_SYSTEM__
#endif //__USE_CULL_BOX_SYSTEM2__

void InsertModel(char *name, int frame, int skin, m4x4_t transform, float uvScale, remap_t *remap, shaderInfo_t *celShader, int entityNum, int mapEntityNum, char castShadows, char recvShadows, int spawnFlags, float lightmapScale, vec3_t lightmapAxis, vec3_t minlight, vec3_t minvertexlight, vec3_t ambient, vec3_t colormod, float lightmapSampleSize, int shadeAngle, int vertTexProj, qboolean noAlphaFix, float pushVertexes, qboolean skybox, int *added_surfaces, int *added_verts, int *added_triangles, int *added_brushes, qboolean cullSmallSolids)
{
	int					s, numSurfaces;
	m4x4_t				identity, nTransform;
	picoModel_t			*model;
	float				top = -999999, bottom = 999999;
	bool				ALLOW_CULL_HALF_SIZE = false;
#ifdef __USE_CULL_BOX_SYSTEM__
	bool				isTreeSolid = false;
	shaderInfo_t		*solidSi = NULL;
	vec3_t				TREE_MINS, TREE_MAXS;
	VectorSet(TREE_MINS, 999999.9, 999999.9, 999999.9);
	VectorSet(TREE_MAXS, -999999.9, -999999.9, -999999.9);
#endif //__USE_CULL_BOX_SYSTEM__

	if (StringContainsWord(name, "forestpine")
		|| StringContainsWord(name, "junglepalm")
		|| StringContainsWord(name, "cypress")
		|| StringContainsWord(name, "cedar"))
	{// Special case for high pine trees, we can cull much more for FPS yay! :)
		ALLOW_CULL_HALF_SIZE = true;
	}

	/* get model */
	model = LoadModel(name, frame);

	if (model == NULL)
		return;

	//printf("DEBUG: Inserting model %s.\n", name);

	qboolean haveLodModel = qfalse;

#ifdef __MODEL_SIMPLIFICATION__
	picoModel_t *lodModel = NULL;
	char fileNameIn[128] = { 0 };
	char tempfileNameOut[128] = { 0 };

	strcpy(tempfileNameOut, model->fileName);
	StripExtension( tempfileNameOut );
	sprintf(fileNameIn, "%s_lod.obj", tempfileNameOut);
	lodModel = FindModel( name, frame );

	if (lodModel)
	{
		haveLodModel = qtrue;
	}
#endif //__MODEL_SIMPLIFICATION__

	/* handle null matrix */
	if (transform == NULL)
	{
		m4x4_identity(identity);
		transform = identity;
	}

	/* hack: Stable-1_2 and trunk have differing row/column major matrix order
	   this transpose is necessary with Stable-1_2
	   uncomment the following line with old m4x4_t (non 1.3/spog_branch) code */
	//%	m4x4_transpose( transform );

	/* create transform matrix for normals */
	memcpy(nTransform, transform, sizeof(m4x4_t));
	if (m4x4_invert(nTransform))
		Sys_Warning(mapEntityNum, "Can't invert model transform matrix, using transpose instead");
	m4x4_transpose(nTransform);

	/* each surface on the model will become a new map drawsurface */
	numSurfaces = PicoGetModelNumSurfaces(model);

	for (s = 0; s < numSurfaces; s++)
	{
		int					i;
		picoVec_t			*xyz;
		picoSurface_t		*surface;

		/* get surface */
		surface = PicoGetModelSurface(model, s);

		if (surface == NULL)
			continue;

		/* only handle triangle surfaces initially (fixme: support patches) */
		if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
			continue;

		/* copy vertexes */
		for (i = 0; i < PicoGetSurfaceNumVertexes(surface); i++)
		{
			vec3_t xyz2;
			/* xyz and normal */
			xyz = PicoGetSurfaceXYZ(surface, i);
			VectorCopy(xyz, xyz2);
			m4x4_transform_point(transform, xyz2);

			if (top < xyz2[2]) top = xyz2[2];
			if (bottom > xyz2[2]) bottom = xyz2[2];
		}
	}

	//Sys_Printf("top: %f. bottom: %f.\n", top, bottom);

	//Sys_Printf( "Model %s has %d surfaces\n", name, numSurfaces );

#pragma omp parallel for ordered num_threads((numSurfaces < numthreads) ? numSurfaces : numthreads)
	for (s = 0; s < numSurfaces; s++)
	{
		int					i;
		char				*picoShaderName;
		char				shaderName[MAX_QPATH];
		remap_t				*rm, *glob;
		shaderInfo_t		*si;
		mapDrawSurface_t	*ds;
		picoSurface_t		*surface;
		picoIndex_t			*indexes;
		bool				isTriangles = true;

#pragma omp critical
		{
			/* get surface */
			surface = PicoGetModelSurface(model, s);
		}

		if (surface == NULL)
			continue;

#pragma omp critical
		{
			/* only handle triangle surfaces initially (fixme: support patches) */
			if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
				isTriangles = false;
		}

		if (!isTriangles)
			continue;

#pragma omp critical
		{
			/* allocate a surface (ydnar: gs mods) */
			ds = AllocDrawSurface(SURFACE_TRIANGLES);
		}

		ds->entityNum = entityNum;
		ds->mapEntityNum = mapEntityNum;
		ds->castShadows = castShadows;
		ds->recvShadows = recvShadows;
		ds->noAlphaFix = noAlphaFix;
		ds->skybox = skybox;

		if (added_surfaces != NULL)
			*added_surfaces += 1;

#pragma omp critical
		{
			/* get shader name */
			/* vortex: support .skin files */
			picoShaderName = PicoGetSurfaceShaderNameForSkin(surface, skin);
		}

		/* handle shader remapping */
		glob = NULL;
		for (rm = remap; rm != NULL; rm = rm->next)
		{
			if (rm->from[0] == '*' && rm->from[1] == '\0')
				glob = rm;
			else if (!Q_stricmp(picoShaderName, rm->from))
			{
				Sys_FPrintf(SYS_VRB, "Remapping %s to %s\n", picoShaderName, rm->to);
				picoShaderName = rm->to;
				glob = NULL;
				break;
			}
		}

		if (glob != NULL)
		{
			Sys_FPrintf(SYS_VRB, "Globbing %s to %s\n", picoShaderName, glob->to);
			picoShaderName = glob->to;
		}

#pragma omp critical
		{
			/* shader renaming for sof2 */
			if (renameModelShaders)
			{
				strcpy(shaderName, picoShaderName);
				StripExtension(shaderName);
				if (spawnFlags & 1)
					strcat(shaderName, "_RMG_BSP");
				else
					strcat(shaderName, "_BSP");
				si = ShaderInfoForShader(shaderName);
			}
			else
				si = ShaderInfoForShader(picoShaderName);

			LoadShaderImages(si);
		}

		/* warn for missing shader */
		if (si->warnNoShaderImage == qtrue)
		{
			if (mapEntityNum >= 0)
				Sys_Warning(mapEntityNum, "Failed to load shader image '%s'", si->shader);
			else
			{
				/* external entity, just show single warning */
				Sys_Warning("Failed to load shader image '%s' for model '%s'", si->shader, PicoGetModelFileName(model));
				si->warnNoShaderImage = qfalse;
			}
		}

		/* set shader */
		ds->shaderInfo = si;

		/* set shading angle */
		ds->smoothNormals = shadeAngle;

		/* force to meta? */
		if ((si != NULL && si->forceMeta) || (spawnFlags & 4))	/* 3rd bit */
			ds->type = SURFACE_FORCED_META;

		/* fix the surface's normals (jal: conditioned by shader info) */
		if (!(spawnFlags & 64) && (shadeAngle <= 0.0f || ds->type != SURFACE_FORCED_META))
			PicoFixSurfaceNormals(surface);

		/* set sample size */
		if (lightmapSampleSize > 0.0f)
			ds->sampleSize = lightmapSampleSize;

		/* set lightmap scale */
		if (lightmapScale > 0.0f)
			ds->lightmapScale = lightmapScale;

		/* set lightmap axis */
		if (lightmapAxis != NULL)
			VectorCopy(lightmapAxis, ds->lightmapAxis);

		/* set minlight/ambient/colormod */
		if (minlight != NULL)
		{
			VectorCopy(minlight, ds->minlight);
			VectorCopy(minlight, ds->minvertexlight);
		}
		if (minvertexlight != NULL)
			VectorCopy(minvertexlight, ds->minvertexlight);
		if (ambient != NULL)
			VectorCopy(ambient, ds->ambient);
		if (colormod != NULL)
			VectorCopy(colormod, ds->colormod);

		/* set vertical texture projection */
		if (vertTexProj > 0)
			ds->vertTexProj = vertTexProj;

		/* set particulars */
		ds->numVerts = PicoGetSurfaceNumVertexes(surface);
#pragma omp critical
		{
			ds->verts = (bspDrawVert_t *)safe_malloc(ds->numVerts * sizeof(ds->verts[0]));
			memset(ds->verts, 0, ds->numVerts * sizeof(ds->verts[0]));
		}

		if (added_verts != NULL)
			*added_verts += ds->numVerts;

		ds->numIndexes = PicoGetSurfaceNumIndexes(surface);
#pragma omp critical
		{
			ds->indexes = (int *)safe_malloc(ds->numIndexes * sizeof(ds->indexes[0]));
			memset(ds->indexes, 0, ds->numIndexes * sizeof(ds->indexes[0]));
		}

		if (added_triangles != NULL)
			*added_triangles += (ds->numIndexes / 3);

		/* copy vertexes */
		for (i = 0; i < ds->numVerts; i++)
		{
			int					j;
			bspDrawVert_t		*dv;
			vec3_t				forceVecs[2];
			picoVec_t			*xyz, *normal, *st;
			picoByte_t			*color;

			/* get vertex */
			dv = &ds->verts[i];

			/* vortex: create forced tcGen vecs for vertical texture projection */
			if (ds->vertTexProj > 0)
			{
				forceVecs[0][0] = 1.0 / ds->vertTexProj;
				forceVecs[0][1] = 0.0;
				forceVecs[0][2] = 0.0;
				forceVecs[1][0] = 0.0;
				forceVecs[1][1] = 1.0 / ds->vertTexProj;
				forceVecs[1][2] = 0.0;
			}

			/* xyz and normal */
			xyz = PicoGetSurfaceXYZ(surface, i);
			VectorCopy(xyz, dv->xyz);
			m4x4_transform_point(transform, dv->xyz);

			normal = PicoGetSurfaceNormal(surface, i);
			VectorCopy(normal, dv->normal);
			m4x4_transform_normal(nTransform, dv->normal);
			VectorNormalize(dv->normal, dv->normal);

			/* ydnar: tek-fu celshading support for flat shaded shit */
			if (flat)
			{
				dv->st[0] = si->stFlat[0];
				dv->st[1] = si->stFlat[1];
			}

			/* vortex: entity-set _vp/_vtcproj will force drawsurface to "tcGen ivector ( value 0 0 ) ( 0 value 0 )"  */
			else if (ds->vertTexProj > 0)
			{
				/* project the texture */
				dv->st[0] = DotProduct(forceVecs[0], dv->xyz);
				dv->st[1] = DotProduct(forceVecs[1], dv->xyz);
			}

			/* ydnar: gs mods: added support for explicit shader texcoord generation */
			else if (si->tcGen)
			{
				/* project the texture */
				dv->st[0] = DotProduct(si->vecs[0], dv->xyz);
				dv->st[1] = DotProduct(si->vecs[1], dv->xyz);
			}

			/* normal texture coordinates */
			else
			{
				st = PicoGetSurfaceST(surface, 0, i);
				dv->st[0] = st[0];
				dv->st[1] = st[1];
			}

			/* scale UV by external key */
			dv->st[0] *= uvScale;
			dv->st[1] *= uvScale;

			/* set lightmap/color bits */
			color = PicoGetSurfaceColor(surface, 0, i);

			for (j = 0; j < MAX_LIGHTMAPS; j++)
			{
				dv->lightmap[j][0] = 0.0f;
				dv->lightmap[j][1] = 0.0f;
				if (spawnFlags & 32) // spawnflag 32: model color -> alpha hack
				{
					dv->color[j][0] = 255.0f;
					dv->color[j][1] = 255.0f;
					dv->color[j][2] = 255.0f;
					dv->color[j][3] = color[0] * 0.3f + color[1] * 0.59f + color[2] * 0.11f;
				}
				else
				{
					dv->color[j][0] = color[0];
					dv->color[j][1] = color[1];
					dv->color[j][2] = color[2];
					dv->color[j][3] = color[3];
				}
			}
		}

		/* copy indexes */
		indexes = PicoGetSurfaceIndexes(surface, 0);

		for (i = 0; i < ds->numIndexes; i++)
			ds->indexes[i] = indexes[i];

		/* deform vertexes */
		DeformVertexes(ds, pushVertexes);

		/* set cel shader */
		ds->celShader = celShader;

		/* walk triangle list */
		for (i = 0; i < ds->numIndexes; i += 3)
		{
			bspDrawVert_t		*dv;
			int					j;

			vec3_t points[4];

			/* make points and back points */
			for (j = 0; j < 3; j++)
			{
				/* get vertex */
				dv = &ds->verts[ds->indexes[i + j]];

				/* copy xyz */
				VectorCopy(dv->xyz, points[j]);
			}
		}

		/* ydnar: giant hack land: generate clipping brushes for model triangles */
		if (!haveLodModel && (si->clipModel || (spawnFlags & 2)) && !noclipmodel)	/* 2nd bit */
		{
			vec4_t plane, reverse, pa, pb, pc;

			if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
			{
				continue;
			}

			/* temp hack */
			if (!si->clipModel
				&& ((si->compileFlags & C_TRANSLUCENT) || !(si->compileFlags & C_SOLID)))
			{
				continue;
			}

			/* overflow check */
			if ((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
			{
				continue;
			}

#ifdef __USE_CULL_BOX_SYSTEM__
			if (si->isTreeSolid && !(si->skipSolidCull || si->isMapObjectSolid))
			{
				// vec3_t		TREE_MINS, TREE_MAXS;
				float			LOWEST_HEIGHT = bottom;
				vec3_t			LOWER_MINS, LOWER_MAXS;
				vec3_t			UPPER_MINS, UPPER_MAXS;

				isTreeSolid = true;
				if (!solidSi) solidSi = si;

				VectorSet(LOWER_MINS, 999999.9, 999999.9, 999999.9);
				VectorSet(LOWER_MAXS, -999999.9, -999999.9, -999999.9);
				VectorSet(UPPER_MINS, 999999.9, 999999.9, 999999.9);
				VectorSet(UPPER_MAXS, -999999.9, -999999.9, -999999.9);

				//				Sys_Printf("LOWEST_HEIGHT is %f.\n", LOWEST_HEIGHT);

				/* walk triangle list - find the mins and maxs near the lowest point */
				for (i = 0; i < ds->numIndexes; i += 3)
				{
					int					j;
					vec3_t				points[4], backs[3];

					/* overflow hack */
					if ((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
					{
						Sys_Warning(mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name);
						break;
					}

					/* make points and back points */
					for (j = 0; j < 3; j++)
					{
						bspDrawVert_t		*dv;
						int					k;

						/* get vertex */
						dv = &ds->verts[ds->indexes[i + j]];

						/* copy xyz */
						VectorCopy(dv->xyz, points[j]);
						VectorCopy(dv->xyz, backs[j]);
					}

					VectorCopy(points[0], points[3]); // for cyclic usage

					/* make plane for triangle */
					// div0: add some extra spawnflags:
					//   0: snap normals to axial planes for extrusion
					//   8: extrude with the original normals
					//  16: extrude only with up/down normals (ideal for terrain)
					//  24: extrude by distance zero (may need engine changes)
					{
						int z;

						for (z = 0; z < 4; z++)
						{
							if (points[z][2] <= bottom + 128.0 && points[z][2] >= bottom + 64.0)
							{// Only look at points near the base of the tree, so that we can get trunk thickness...
								if (points[z][0] < LOWER_MINS[0]) LOWER_MINS[0] = points[z][0];
								if (points[z][1] < LOWER_MINS[1]) LOWER_MINS[1] = points[z][1];
								//if (points[z][2] < LOWER_MINS[2]) LOWER_MINS[2] = points[z][2];

								if (points[z][0] > LOWER_MAXS[0]) LOWER_MAXS[0] = points[z][0];
								if (points[z][1] > LOWER_MAXS[1]) LOWER_MAXS[1] = points[z][1];
								//if (points[z][2] > LOWER_MAXS[2]) LOWER_MAXS[2] = points[z][2];
							}
						}
					}
				}

				//Sys_Printf("LOWER_MINS is %f %f %f. LOWER_MAXS is %f %f %f.\n", LOWER_MINS[0], LOWER_MINS[1], LOWER_MINS[2], LOWER_MAXS[0], LOWER_MAXS[1], LOWER_MAXS[2]);

				/* walk triangle list - find the highest point within the lower mins and maxs (x and y only) */
				for (i = 0; i < ds->numIndexes; i += 3)
				{
					int					j;
					vec3_t				points[4], backs[3];

					/* overflow hack */
					if ((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
					{
						Sys_Warning(mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name);
						break;
					}

					/* make points and back points */
					for (j = 0; j < 3; j++)
					{
						bspDrawVert_t		*dv;
						int					k;

						/* get vertex */
						dv = &ds->verts[ds->indexes[i + j]];

						/* copy xyz */
						VectorCopy(dv->xyz, points[j]);
						VectorCopy(dv->xyz, backs[j]);
					}

					VectorCopy(points[0], points[3]); // for cyclic usage

					/* make plane for triangle */
					// div0: add some extra spawnflags:
					//   0: snap normals to axial planes for extrusion
					//   8: extrude with the original normals
					//  16: extrude only with up/down normals (ideal for terrain)
					//  24: extrude by distance zero (may need engine changes)
					{
						int z;

						for (z = 0; z < 4; z++)
						{
							if (points[z][2] <= bottom + 128.0 && points[z][2] >= bottom + 64.0)
							{// Only look at points near the base of the tree, so that we can get trunk thickness...
								if (points[z][0] <= LOWER_MAXS[0] && points[z][0] >= LOWER_MINS[0]
									&& points[z][1] <= LOWER_MAXS[1] && points[z][1] >= LOWER_MINS[1])
								{// Only look at points within the mins and maxs of the base of the tree (x and y), so that we can get trunk height...
									if (points[z][0] < UPPER_MINS[0]) UPPER_MINS[0] = points[z][0];
									if (points[z][1] < UPPER_MINS[1]) UPPER_MINS[1] = points[z][1];
									//if (points[z][2] < UPPER_MINS[2]) UPPER_MINS[2] = points[z][2];

									if (points[z][0] > UPPER_MAXS[0]) UPPER_MAXS[0] = points[z][0];
									if (points[z][1] > UPPER_MAXS[1]) UPPER_MAXS[1] = points[z][1];
									//if (points[z][2] > UPPER_MAXS[2]) UPPER_MAXS[2] = points[z][2];
								}
							}
						}
					}
				}

				//Sys_Printf("UPPER_MINS is %f %f %f. UPPER_MAXS is %f %f %f.\n", UPPER_MINS[0], UPPER_MINS[1], UPPER_MINS[2], UPPER_MAXS[0], UPPER_MAXS[1], UPPER_MAXS[2]);

#pragma omp critical
				{
					if (LOWER_MINS[0] < TREE_MINS[0]) TREE_MINS[0] = LOWER_MINS[0];
					if (LOWER_MINS[1] < TREE_MINS[1]) TREE_MINS[1] = LOWER_MINS[1];
					TREE_MINS[2] = bottom;

					if (UPPER_MAXS[0] > TREE_MAXS[0]) TREE_MAXS[0] = UPPER_MAXS[0];
					if (UPPER_MAXS[1] > TREE_MAXS[1]) TREE_MAXS[1] = UPPER_MAXS[1];
					TREE_MAXS[2] = top;
				}

				if (TREE_MINS[0] == 999999.9 && TREE_MINS[1] == 999999.9 && TREE_MAXS[0] == -999999.9 && TREE_MAXS[1] == -999999.9)
				{// Failed... Fall back to old method...
					isTreeSolid = false;
				}
			}

			if (!isTreeSolid)
			{// Edither this is not a tree, or we failed to find a valid box... Fallback to old method... *sigh*
				/* walk triangle list */
				for (i = 0; i < ds->numIndexes; i += 3)
				{
					int					j;
					vec3_t				points[4], backs[3];

					/* overflow hack */
					if ((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
					{
						Sys_Warning(mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name);
						break;
					}

					/* make points and back points */
					for (j = 0; j < 3; j++)
					{
						bspDrawVert_t		*dv;
						int					k;

						/* get vertex */
						dv = &ds->verts[ds->indexes[i + j]];

						/* copy xyz */
						VectorCopy(dv->xyz, points[j]);
						VectorCopy(dv->xyz, backs[j]);

						/* find nearest axial to normal and push back points opposite */
						/* note: this doesn't work as well as simply using the plane of the triangle, below */
						for (k = 0; k < 3; k++)
						{
							if (fabs(dv->normal[k]) >= fabs(dv->normal[(k + 1) % 3]) &&
								fabs(dv->normal[k]) >= fabs(dv->normal[(k + 2) % 3]))
							{
								backs[j][k] += dv->normal[k] < 0.0f ? 64.0f : -64.0f;
								break;
							}
						}
					}

					VectorCopy(points[0], points[3]); // for cyclic usage

					/* make plane for triangle */
					// div0: add some extra spawnflags:
					//   0: snap normals to axial planes for extrusion
					//   8: extrude with the original normals
					//  16: extrude only with up/down normals (ideal for terrain)
					//  24: extrude by distance zero (may need engine changes)
					if (PlaneFromPoints(plane, points[0], points[1], points[2]))
					{
						double				normalEpsilon_save;
						double				distanceEpsilon_save;

						vec3_t bestNormal;
						float backPlaneDistance = 2;

						if (spawnFlags & 8) // use a DOWN normal
						{
							if (spawnFlags & 16)
							{
								// 24: normal as is, and zero width (broken)
								VectorCopy(plane, bestNormal);
							}
							else
							{
								// 8: normal as is
								VectorCopy(plane, bestNormal);
							}
						}
						else
						{
							if (spawnFlags & 16)
							{
								// 16: UP/DOWN normal
								VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
							}
							else
							{
								// 0: axial normal
								if (fabs(plane[0]) > fabs(plane[1])) // x>y
								if (fabs(plane[1]) > fabs(plane[2])) // x>y, y>z
									VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
								else // x>y, z>=y
								if (fabs(plane[0]) > fabs(plane[2])) // x>z, z>=y
									VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
								else // z>=x, x>y
									VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
								else // y>=x
								if (fabs(plane[1]) > fabs(plane[2])) // y>z, y>=x
									VectorSet(bestNormal, 0, (plane[1] >= 0 ? 1 : -1), 0);
								else // z>=y, y>=x
									VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
							}
						}

						/* regenerate back points */
						for (j = 0; j < 3; j++)
						{
							bspDrawVert_t		*dv;

							/* get vertex */
							dv = &ds->verts[ds->indexes[i + j]];

							// shift by some units
							VectorMA(dv->xyz, -64.0f, bestNormal, backs[j]); // 64 prevents roundoff errors a bit
						}

						/* make back plane */
						VectorScale(plane, -1.0f, reverse);
						reverse[3] = -plane[3];
						if ((spawnFlags & 24) != 24)
							reverse[3] += DotProduct(bestNormal, plane) * backPlaneDistance;
						// that's at least sqrt(1/3) backPlaneDistance, unless in DOWN mode; in DOWN mode, we are screwed anyway if we encounter a plane that's perpendicular to the xy plane)

						normalEpsilon_save = normalEpsilon;
						distanceEpsilon_save = distanceEpsilon;

						if (PlaneFromPoints(pa, points[2], points[1], backs[1]) &&
							PlaneFromPoints(pb, points[1], points[0], backs[0]) &&
							PlaneFromPoints(pc, points[0], points[2], backs[2]))
						{
							//Sys_Printf("top: %f. bottom: %f.\n", top, bottom);

							numSolidSurfs++;

							if ((cullSmallSolids || si->isTreeSolid) && !(si->skipSolidCull || si->isMapObjectSolid))
							{// Cull small stuff and the tops of trees...
								vec3_t mins, maxs;
								vec3_t size;
								float sz;
								int z;

								VectorSet(mins, 999999, 999999, 999999);
								VectorSet(maxs, -999999, -999999, -999999);

								for (z = 0; z < 4; z++)
								{
									if (points[z][0] < mins[0]) mins[0] = points[z][0];
									if (points[z][1] < mins[1]) mins[1] = points[z][1];
									if (points[z][2] < mins[2]) mins[2] = points[z][2];

									if (points[z][0] > maxs[0]) maxs[0] = points[z][0];
									if (points[z][1] > maxs[1]) maxs[1] = points[z][1];
									if (points[z][2] > maxs[2]) maxs[2] = points[z][2];
								}

								if (top != -999999 && bottom != -999999)
								{
									float s = top - bottom;
									float newtop = bottom + (s / 2.0);
									//float newtop = bottom + (s / 4.0);

									if (ALLOW_CULL_HALF_SIZE) newtop = bottom + (s / 4.0); // Special case for high pine trees, we can cull much more for FPS yay! :)

									//Sys_Printf("newtop: %f. top: %f. bottom: %f. mins: %f. maxs: %f.\n", newtop, top, bottom, mins[2], maxs[2]);

									if (mins[2] > newtop)
									{
										//Sys_Printf("CULLED: %f > %f.\n", maxs[2], newtop);
										numHeightCulledSurfs++;
										continue;
									}
								}

								VectorSubtract(maxs, mins, size);
								//sz = VectorLength(size);
								sz = maxs[0] - mins[0];
								if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
								if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

								if (sz < 36)
								{
									//Sys_Printf("CULLED: %f < 30. (%f %f %f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
									numSizeCulledSurfs++;
									continue;
								}
							}
							else if (cullSmallSolids || si->isTreeSolid)
							{// Only cull stuff too small to fall through...
								vec3_t mins, maxs;
								vec3_t size;
								float sz;
								int z;

								VectorSet(mins, 999999, 999999, 999999);
								VectorSet(maxs, -999999, -999999, -999999);

								for (z = 0; z < 4; z++)
								{
									if (points[z][0] < mins[0]) mins[0] = points[z][0];
									if (points[z][1] < mins[1]) mins[1] = points[z][1];
									if (points[z][2] < mins[2]) mins[2] = points[z][2];

									if (points[z][0] > maxs[0]) maxs[0] = points[z][0];
									if (points[z][1] > maxs[1]) maxs[1] = points[z][1];
									if (points[z][2] > maxs[2]) maxs[2] = points[z][2];
								}

								VectorSubtract(maxs, mins, size);
								//sz = VectorLength(size);
								sz = maxs[0] - mins[0];
								if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
								if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

								if (sz <= 16)
								{
									//Sys_Printf("CULLED: %f < 30. (%f %f %f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
									numSizeCulledSurfs++;
									continue;
								}
							}

							//#define __FORCE_TREE_META__
#if defined(__FORCE_TREE_META__)
							if (meta) si->forceMeta = qtrue; // much slower...
#endif

#pragma omp ordered
							{
#pragma omp critical
								{

									/* build a brush */ // -- UQ1: Moved - Why allocate when its not needed...
									buildBrush = AllocBrush(24/*48*/); // UQ1: 48 seems to be more then is used... Wasting memory...
									buildBrush->entityNum = mapEntityNum;
									buildBrush->mapEntityNum = mapEntityNum;
									buildBrush->original = buildBrush;
									buildBrush->contentShader = si;
									buildBrush->compileFlags = si->compileFlags;
									buildBrush->contentFlags = si->contentFlags;

									if (si->isTreeSolid || si->isMapObjectSolid || (si->compileFlags & C_DETAIL))
									{
										buildBrush->detail = qtrue;
									}
									else if (si->compileFlags & C_STRUCTURAL) // allow forced structural brushes here
									{
										buildBrush->detail = qfalse;

										// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
										if (normalEpsilon > 0)
											normalEpsilon = 0;
										if (distanceEpsilon > 0)
											distanceEpsilon = 0;
									}
									else
									{
										buildBrush->detail = qtrue;
									}

									/* set up brush sides */
									buildBrush->numsides = 5;
									buildBrush->sides[0].shaderInfo = si;

									for (j = 1; j < buildBrush->numsides; j++)
									{
										buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
										buildBrush->sides[j].culled = qtrue;
									}

									buildBrush->sides[0].planenum = FindFloatPlane(plane, plane[3], 3, points);
									buildBrush->sides[1].planenum = FindFloatPlane(pa, pa[3], 2, &points[1]); // pa contains points[1] and points[2]
									buildBrush->sides[2].planenum = FindFloatPlane(pb, pb[3], 2, &points[0]); // pb contains points[0] and points[1]
									buildBrush->sides[3].planenum = FindFloatPlane(pc, pc[3], 2, &points[2]); // pc contains points[2] and points[0] (copied to points[3]
									buildBrush->sides[4].planenum = FindFloatPlane(reverse, reverse[3], 3, backs);

									/* add to entity */
									if (CreateBrushWindings(buildBrush))
									{
										int numsides;

										AddBrushBevels();
										//%	EmitBrushes( buildBrush, NULL, NULL );

										numsides = buildBrush->numsides;

										if (!RemoveDuplicateBrushPlanes(buildBrush))
										{// UQ1: Testing - This would create a mirrored plane... free it...
											free(buildBrush);
											//Sys_Printf("Removed a mirrored plane\n");
										}
										else
										{
											//if (buildBrush->numsides < numsides) Sys_Printf("numsides reduced from %i to %i.\n", numsides, buildBrush->numsides);

											buildBrush->next = entities[mapEntityNum].brushes;
											entities[mapEntityNum].brushes = buildBrush;
											entities[mapEntityNum].numBrushes++;
											if (added_brushes != NULL)
												*added_brushes += 1;
										}
									}
									else
									{
										free(buildBrush);
									}
								} // #pragma omp critical
							} // #pragma omp ordered
						}
						else
						{
							continue;
						}

						normalEpsilon = normalEpsilon_save;
						distanceEpsilon = distanceEpsilon_save;
					}
				}
			}
#else //!__USE_CULL_BOX_SYSTEM__
			/* walk triangle list */
			for( i = 0; i < ds->numIndexes; i += 3 )
			{
				int					j;
				vec3_t				points[4], backs[3];

				/* overflow hack */
				if( (nummapplanes + 64) >= (MAX_MAP_PLANES >> 1) )
				{
					Sys_Warning( mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name );
					break;
				}

				/* make points and back points */
				for( j = 0; j < 3; j++ )
				{
					bspDrawVert_t		*dv;
					int					k;

					/* get vertex */
					dv = &ds->verts[ ds->indexes[ i + j ] ];

					/* copy xyz */
					VectorCopy( dv->xyz, points[ j ] );
					VectorCopy( dv->xyz, backs[ j ] );

					/* find nearest axial to normal and push back points opposite */
					/* note: this doesn't work as well as simply using the plane of the triangle, below */
					for( k = 0; k < 3; k++ )
					{
						if( fabs( dv->normal[ k ] ) >= fabs( dv->normal[ (k + 1) % 3 ] ) &&
							fabs( dv->normal[ k ] ) >= fabs( dv->normal[ (k + 2) % 3 ] ) )
						{
							backs[ j ][ k ] += dv->normal[ k ] < 0.0f ? 64.0f : -64.0f;
							break;
						}
					}
				}

				VectorCopy( points[0], points[3] ); // for cyclic usage

				/* make plane for triangle */
				// div0: add some extra spawnflags:
				//   0: snap normals to axial planes for extrusion
				//   8: extrude with the original normals
				//  16: extrude only with up/down normals (ideal for terrain)
				//  24: extrude by distance zero (may need engine changes)
				if( PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] ) )
				{
					double				normalEpsilon_save;
					double				distanceEpsilon_save;

					vec3_t bestNormal;
					float backPlaneDistance = 2;

					if(spawnFlags & 8) // use a DOWN normal
					{
						if(spawnFlags & 16)
						{
							// 24: normal as is, and zero width (broken)
							VectorCopy(plane, bestNormal);
						}
						else
						{
							// 8: normal as is
							VectorCopy(plane, bestNormal);
						}
					}
					else
					{
						if(spawnFlags & 16)
						{
							// 16: UP/DOWN normal
							VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
						}
						else
						{
							// 0: axial normal
							if(fabs(plane[0]) > fabs(plane[1])) // x>y
							if(fabs(plane[1]) > fabs(plane[2])) // x>y, y>z
								VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
							else // x>y, z>=y
							if(fabs(plane[0]) > fabs(plane[2])) // x>z, z>=y
								VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
							else // z>=x, x>y
								VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
							else // y>=x
							if(fabs(plane[1]) > fabs(plane[2])) // y>z, y>=x
								VectorSet(bestNormal, 0, (plane[1] >= 0 ? 1 : -1), 0);
							else // z>=y, y>=x
								VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
						}
					}

					/* regenerate back points */
					for( j = 0; j < 3; j++ )
					{
						bspDrawVert_t		*dv;

						/* get vertex */
						dv = &ds->verts[ ds->indexes[ i + j ] ];

						// shift by some units
						VectorMA(dv->xyz, -64.0f, bestNormal, backs[j]); // 64 prevents roundoff errors a bit
					}

					/* make back plane */
					VectorScale( plane, -1.0f, reverse );
					reverse[ 3 ] = -plane[ 3 ];
					if((spawnFlags & 24) != 24)
						reverse[3] += DotProduct(bestNormal, plane) * backPlaneDistance;
					// that's at least sqrt(1/3) backPlaneDistance, unless in DOWN mode; in DOWN mode, we are screwed anyway if we encounter a plane that's perpendicular to the xy plane)

					normalEpsilon_save = normalEpsilon;
					distanceEpsilon_save = distanceEpsilon;

					if( PlaneFromPoints( pa, points[ 2 ], points[ 1 ], backs[ 1 ] ) &&
						PlaneFromPoints( pb, points[ 1 ], points[ 0 ], backs[ 0 ] ) &&
						PlaneFromPoints( pc, points[ 0 ], points[ 2 ], backs[ 2 ] ) )
					{
						vec3_t mins, maxs;
						int z;

						VectorSet(mins, 999999, 999999, 999999);
						VectorSet(maxs, -999999, -999999, -999999);

						for (z = 0; z < 4; z++)
						{
							if (points[z][0] < mins[0]) mins[0] = points[z][0];
							if (points[z][1] < mins[1]) mins[1] = points[z][1];
							if (points[z][2] < mins[2]) mins[2] = points[z][2];

							if (points[z][0] > maxs[0]) maxs[0] = points[z][0];
							if (points[z][1] > maxs[1]) maxs[1] = points[z][1];
							if (points[z][2] > maxs[2]) maxs[2] = points[z][2];
						}

						//Sys_Printf("top: %f. bottom: %f.\n", top, bottom);

						numSolidSurfs++;

						if ((cullSmallSolids || si->isTreeSolid) && !(si->skipSolidCull || si->isMapObjectSolid))
						{// Cull small stuff and the tops of trees...
							vec3_t size;
							float sz;

							if (top != -999999 && bottom != -999999)
							{
								float s = top - bottom;
								float newtop = bottom + (s / 2.0);
								//float newtop = bottom + (s / 4.0);

								if (ALLOW_CULL_HALF_SIZE) newtop = bottom + (s / 4.0); // Special case for high pine trees, we can cull much more for FPS yay! :)

								//Sys_Printf("newtop: %f. top: %f. bottom: %f. mins: %f. maxs: %f.\n", newtop, top, bottom, mins[2], maxs[2]);

								if (mins[2] > newtop || mins[2] > bottom + 512.0) // 512 is > JKA max jump height...
								{
									//Sys_Printf("CULLED: %f > %f.\n", maxs[2], newtop);
									numHeightCulledSurfs++;
									continue;
								}
							}

							VectorSubtract(maxs, mins, size);
							//sz = VectorLength(size);
							sz = maxs[0] - mins[0];
							if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
							if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

							if (sz < 36)
							{
								//Sys_Printf("CULLED: %f < 30. (%f %f %f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
								numSizeCulledSurfs++;
								continue;
							}
						}
						else //if (cullSmallSolids || si->isTreeSolid)
						{// Only cull stuff too small to fall through...
							vec3_t size;
							float sz;

							VectorSubtract(maxs, mins, size);
							//sz = VectorLength(size);
							sz = maxs[0] - mins[0];
							if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
							if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

							if (sz <= 24)//16)
							{
								//Sys_Printf("CULLED: %f < 30. (%f %f %f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
								numSizeCulledSurfs++;
								continue;
							}
						}

						//#define __FORCE_TREE_META__
#if defined(__FORCE_TREE_META__)
						if (meta) si->forceMeta = qtrue; // much slower...
#endif

#pragma omp ordered
						{
#pragma omp critical
							{

#if 1
								/* build a brush */ // -- UQ1: Moved - Why allocate when its not needed...
								buildBrush = AllocBrush( 24/*48*/ ); // UQ1: 48 seems to be more then is used... Wasting memory...
								buildBrush->entityNum = mapEntityNum;
								buildBrush->mapEntityNum = mapEntityNum;
								buildBrush->original = buildBrush;
								buildBrush->contentShader = si;
								buildBrush->compileFlags = si->compileFlags;
								buildBrush->contentFlags = si->contentFlags;

								if (si->isTreeSolid || si->isMapObjectSolid || (si->compileFlags & C_DETAIL))
								{
									buildBrush->detail = qtrue;
								}
								else if (si->compileFlags & C_STRUCTURAL) // allow forced structural brushes here
								{
									buildBrush->detail = qfalse;

									// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
									if(normalEpsilon > 0)
										normalEpsilon = 0;
									if(distanceEpsilon > 0)
										distanceEpsilon = 0;
								}
								else
								{
									buildBrush->detail = qtrue;
								}

								/* set up brush sides */
								buildBrush->numsides = 5;
								buildBrush->sides[ 0 ].shaderInfo = si;

								for( j = 1; j < buildBrush->numsides; j++ )
								{
									buildBrush->sides[ j ].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
									buildBrush->sides[ j ].culled = qtrue;
								}

								buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
								buildBrush->sides[ 1 ].planenum = FindFloatPlane( pa, pa[ 3 ], 2, &points[ 1 ] ); // pa contains points[1] and points[2]
								buildBrush->sides[ 2 ].planenum = FindFloatPlane( pb, pb[ 3 ], 2, &points[ 0 ] ); // pb contains points[0] and points[1]
								buildBrush->sides[ 3 ].planenum = FindFloatPlane( pc, pc[ 3 ], 2, &points[ 2 ] ); // pc contains points[2] and points[0] (copied to points[3]
								buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 3, backs );

								/* add to entity */
								if( CreateBrushWindings( buildBrush ) )
								{
									int numsides;

									AddBrushBevels();
									//%	EmitBrushes( buildBrush, NULL, NULL );

									numsides = buildBrush->numsides;

#if 0
									/* copy sides */
									if (buildBrush->numsides > 5)
									{
										for (z = 5; z < buildBrush->numsides; z++)
										{
											if (buildBrush->sides[z - 5].winding != NULL)
												if (*(unsigned *)buildBrush->sides[z - 5].winding != 0xdeaddead)
													FreeWinding(buildBrush->sides[z - 5].winding);

											//buildBrush->sides[z - 5] = buildBrush->sides[z];
											memcpy(&buildBrush->sides[z - 5], &buildBrush->sides[z], sizeof(side_t));

											if (buildBrush->sides[z].winding != NULL)
												if (*(unsigned *)buildBrush->sides[z].winding != 0xdeaddead)
													buildBrush->sides[z - 5].winding = CopyWinding(buildBrush->sides[z].winding);

											if (buildBrush->sides[z].winding != NULL)
												if (*(unsigned *)buildBrush->sides[z].winding != 0xdeaddead)
													FreeWinding(buildBrush->sides[z].winding);

											memset(&buildBrush->sides[z], 0, sizeof(side_t));
										}

										//Sys_Printf("orig numsides %i.", buildBrush->numsides);

										buildBrush->numsides -= 5;

										//Sys_Printf(" final numsides %i.\n", buildBrush->numsides);
									}
#endif

									if (!RemoveDuplicateBrushPlanes( buildBrush ))
									{// UQ1: Testing - This would create a mirrored plane... free it...
										FreeBrush(buildBrush);
										//Sys_Printf("Removed a mirrored plane\n");
									}
									else
									{
										//int c = (int) &(((brush_t*)0)->sides[buildBrush->numsides]);
										//buildBrush = (brush_t *)realloc(buildBrush, c);

										//if (buildBrush->numsides < numsides) Sys_Printf("numsides reduced from %i to %i.\n", numsides, buildBrush->numsides);
										//Sys_Printf("numsides %i.\n", buildBrush->numsides);

										//brush_t *buildBrush2 = AllocBrush(buildBrush->numsides);
										//memcpy(buildBrush2, buildBrush, c);
										//FreeBrush(buildBrush);
										//buildBrush = buildBrush2;

										//brush_t *buildBrush2 = CopyBrush(buildBrush);
										//FreeBrush(buildBrush);
										//buildBrush = buildBrush2;


										for (j = 1; j < buildBrush->numsides; j++)
										{
											buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
											buildBrush->sides[j].culled = qtrue;
										}

										buildBrush->next = entities[ mapEntityNum ].brushes;
										entities[ mapEntityNum ].brushes = buildBrush;
										entities[ mapEntityNum ].numBrushes++;
										if (added_brushes != NULL)
											*added_brushes += 1;
									}
								}
								else
								{
									FreeBrush(buildBrush);
								}
#else
								for (z = 0; z < 4; z++)
								{
									if (backs[z][0] < mins[0]) mins[0] = backs[z][0];
									if (backs[z][1] < mins[1]) mins[1] = backs[z][1];
									if (backs[z][2] < mins[2]) mins[2] = backs[z][2];

									if (backs[z][0] > maxs[0]) maxs[0] = backs[z][0];
									if (backs[z][1] > maxs[1]) maxs[1] = backs[z][1];
									if (backs[z][2] > maxs[2]) maxs[2] = backs[z][2];
								}

								buildBrush = BrushFromBounds(mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], si);

								/*for (j = 1; j < buildBrush->numsides; j++)
								{
									buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
									buildBrush->sides[j].culled = qtrue;
								}*/

								buildBrush->next = entities[mapEntityNum].brushes;
								entities[mapEntityNum].brushes = buildBrush;
								entities[mapEntityNum].numBrushes++;
								if (added_brushes != NULL)
									*added_brushes += 1;
#endif
							} // #pragma omp critical
						} // #pragma omp ordered
					}
					else
					{
						continue;
					}

					normalEpsilon = normalEpsilon_save;
					distanceEpsilon = distanceEpsilon_save;
				}
			}
#endif //__USE_CULL_BOX_SYSTEM__
		}
	}

#ifdef __USE_CULL_BOX_SYSTEM__
	if (isTreeSolid && TREE_MINS[0] < 999999.9 && TREE_MINS[1] < 999999.9 && TREE_MINS[2] < 999999.9)
	{
		int i;
		unsigned int numIndexes = 0;
		unsigned int indexes[64] = { 0 };
		unsigned int numVerts = 0;
		vec3_t xyz[64] = { 0 };
		vec4_t plane, reverse, pa, pb, pc;

		//Sys_Printf("TREE_MINS %f %f %f. TREE_MAXS %f %f %f.\n", TREE_MINS[0], TREE_MINS[1], TREE_MINS[2], TREE_MAXS[0], TREE_MAXS[1], TREE_MAXS[2]);

		if (TREE_MAXS[0] - TREE_MINS[0] > 156.0 && TREE_MAXS[1] - TREE_MINS[1] > 156.0)
		{// Reduce the box size a bit, so that the corners don't stick out so far...
			TREE_MINS[0] += 64.0;
			TREE_MINS[1] += 64.0;
			TREE_MAXS[0] -= 64.0;
			TREE_MAXS[1] -= 64.0;
		}

#ifdef __USE_CULL_BOX_SYSTEM2__
#pragma omp ordered
		{
#pragma omp critical
			{
				buildBrush = BrushFromBounds(TREE_MINS[0], TREE_MINS[1], TREE_MINS[2], TREE_MAXS[0], TREE_MAXS[1], TREE_MAXS[2], solidSi);

				/*buildBrush->entityNum = mapEntityNum;
				buildBrush->mapEntityNum = mapEntityNum;
				buildBrush->original = buildBrush;
				buildBrush->contentShader = solidSi;
				buildBrush->compileFlags = solidSi->compileFlags;
				buildBrush->contentFlags = solidSi->contentFlags;*/

				AddBrushBevels();

				buildBrush->detail = qtrue;

				/* set up brush sides */
				buildBrush->numsides = 5;
				buildBrush->sides[0].shaderInfo = solidSi;

				for (i = 1; i < buildBrush->numsides; i++)
				{
					buildBrush->sides[i].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
					buildBrush->sides[i].culled = qtrue;
				}

				buildBrush->next = entities[mapEntityNum].brushes;
				entities[mapEntityNum].brushes = buildBrush;
				entities[mapEntityNum].numBrushes++;
				if (added_brushes != NULL)
					*added_brushes += 1;
			}
		}
#else //!__USE_CULL_BOX_SYSTEM2__
		// Create a new cube for this mins/maxs...
		AddCube(TREE_MINS, TREE_MAXS, &numIndexes, indexes, &numVerts, xyz);

		/* create solidity from this new cube */
		for (i = 0; i < numIndexes; i += 3)
		{
			int					j;
			vec3_t				points[4], backs[3];

			/* overflow hack */
			if ((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
			{
				Sys_Warning(mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name);
				break;
			}

			/* make points and back points */
			for (j = 0; j < 3; j++)
			{
				/* copy xyz */
				VectorCopy(xyz[indexes[i + j]], points[j]);
				VectorCopy(xyz[indexes[i + j]], backs[j]);
			}

			VectorCopy(points[0], points[3]); // for cyclic usage

			/* make plane for triangle */
			// div0: add some extra spawnflags:
			//   0: snap normals to axial planes for extrusion
			//   8: extrude with the original normals
			//  16: extrude only with up/down normals (ideal for terrain)
			//  24: extrude by distance zero (may need engine changes)
			if (PlaneFromPoints(plane, points[0], points[1], points[2]))
			{
				double				normalEpsilon_save;
				double				distanceEpsilon_save;

				vec3_t bestNormal;
				float backPlaneDistance = 2;

				if (spawnFlags & 8) // use a DOWN normal
				{
					if (spawnFlags & 16)
					{
						// 24: normal as is, and zero width (broken)
						VectorCopy(plane, bestNormal);
					}
					else
					{
						// 8: normal as is
						VectorCopy(plane, bestNormal);
					}
				}
				else
				{
					if (spawnFlags & 16)
					{
						// 16: UP/DOWN normal
						VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
					}
					else
					{
						// 0: axial normal
						if (fabs(plane[0]) > fabs(plane[1])) // x>y
						if (fabs(plane[1]) > fabs(plane[2])) // x>y, y>z
							VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
						else // x>y, z>=y
						if (fabs(plane[0]) > fabs(plane[2])) // x>z, z>=y
							VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
						else // z>=x, x>y
							VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
						else // y>=x
						if (fabs(plane[1]) > fabs(plane[2])) // y>z, y>=x
							VectorSet(bestNormal, 0, (plane[1] >= 0 ? 1 : -1), 0);
						else // z>=y, y>=x
							VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
					}
				}

				/* regenerate back points */
				for (j = 0; j < 3; j++)
				{
					// shift by some units
					VectorMA(xyz[indexes[i + j]], -64.0f, bestNormal, backs[j]); // 64 prevents roundoff errors a bit
				}

				/* make back plane */
				VectorScale(plane, -1.0f, reverse);
				reverse[3] = -plane[3];
				if ((spawnFlags & 24) != 24)
					reverse[3] += DotProduct(bestNormal, plane) * backPlaneDistance;
				// that's at least sqrt(1/3) backPlaneDistance, unless in DOWN mode; in DOWN mode, we are screwed anyway if we encounter a plane that's perpendicular to the xy plane)

				normalEpsilon_save = normalEpsilon;
				distanceEpsilon_save = distanceEpsilon;

				if (PlaneFromPoints(pa, points[2], points[1], backs[1]) &&
					PlaneFromPoints(pb, points[1], points[0], backs[0]) &&
					PlaneFromPoints(pc, points[0], points[2], backs[2]))
				{
					//Sys_Printf("top: %f. bottom: %f.\n", top, bottom);

					numSolidSurfs++;

					//#define __FORCE_TREE_META__
#if defined(__FORCE_TREE_META__)
					if (meta) si->forceMeta = qtrue; // much slower...
#endif

#pragma omp ordered
					{
#pragma omp critical
						{

							/* build a brush */ // -- UQ1: Moved - Why allocate when its not needed...
							buildBrush = AllocBrush(24/*48*/); // UQ1: 48 seems to be more then is used... Wasting memory...
							buildBrush->entityNum = mapEntityNum;
							buildBrush->mapEntityNum = mapEntityNum;
							buildBrush->original = buildBrush;
							buildBrush->contentShader = solidSi;
							buildBrush->compileFlags = solidSi->compileFlags;
							buildBrush->contentFlags = solidSi->contentFlags;

							buildBrush->detail = qtrue;

							/* set up brush sides */
							buildBrush->numsides = 5;
							buildBrush->sides[0].shaderInfo = solidSi;

							for (j = 1; j < buildBrush->numsides; j++)
							{
								buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
								buildBrush->sides[j].culled = qtrue;
							}

							buildBrush->sides[0].planenum = FindFloatPlane(plane, plane[3], 3, points);
							buildBrush->sides[1].planenum = FindFloatPlane(pa, pa[3], 2, &points[1]); // pa contains points[1] and points[2]
							buildBrush->sides[2].planenum = FindFloatPlane(pb, pb[3], 2, &points[0]); // pb contains points[0] and points[1]
							buildBrush->sides[3].planenum = FindFloatPlane(pc, pc[3], 2, &points[2]); // pc contains points[2] and points[0] (copied to points[3]
							buildBrush->sides[4].planenum = FindFloatPlane(reverse, reverse[3], 3, backs);

							/* add to entity */
							if (CreateBrushWindings(buildBrush))
							{
								int numsides;

								AddBrushBevels();
								//%	EmitBrushes( buildBrush, NULL, NULL );

								numsides = buildBrush->numsides;

								if (!RemoveDuplicateBrushPlanes(buildBrush))
								{// UQ1: Testing - This would create a mirrored plane... free it...
									free(buildBrush);
									//Sys_Printf("Removed a mirrored plane\n");
								}
								else
								{
									//if (buildBrush->numsides < numsides) Sys_Printf("numsides reduced from %i to %i.\n", numsides, buildBrush->numsides);

									buildBrush->next = entities[mapEntityNum].brushes;
									entities[mapEntityNum].brushes = buildBrush;
									entities[mapEntityNum].numBrushes++;
									if (added_brushes != NULL)
										*added_brushes += 1;
								}
							}
							else
							{
								free(buildBrush);
							}
						} // #pragma omp critical
					} // #pragma omp ordered
				}
				else
				{
					continue;
				}

				normalEpsilon = normalEpsilon_save;
				distanceEpsilon = distanceEpsilon_save;
			}
		}
#endif //__USE_CULL_BOX_SYSTEM2__
	}
#elif defined(__MODEL_SIMPLIFICATION__)
	if (haveLodModel)
	{
		model = lodModel;

		/* hack: Stable-1_2 and trunk have differing row/column major matrix order
		this transpose is necessary with Stable-1_2
		uncomment the following line with old m4x4_t (non 1.3/spog_branch) code */
		//%	m4x4_transpose( transform );

		/* create transform matrix for normals */
		memcpy( nTransform, transform, sizeof( m4x4_t ) );
		if( m4x4_invert( nTransform ) )
			Sys_Warning( mapEntityNum, "Can't invert model transform matrix, using transpose instead" );
		m4x4_transpose( nTransform );

		/* each surface on the model will become a new map drawsurface */
		numSurfaces = PicoGetModelNumSurfaces( model );

#pragma omp parallel for ordered num_threads((numSurfaces < numthreads) ? numSurfaces : numthreads)
		for( s = 0; s < numSurfaces; s++ )
		{
			int					i;
			char				*picoShaderName;
			char				shaderName[ MAX_QPATH ];
			remap_t				*rm, *glob;
			shaderInfo_t		*si;
			mapDrawSurface_t	*ds;
			picoSurface_t		*surface;
			picoIndex_t			*indexes;

			/* get surface */
			surface = PicoGetModelSurface( model, s );
			if( surface == NULL )
				continue;

			/* only handle triangle surfaces initially (fixme: support patches) */
			if( PicoGetSurfaceType( surface ) != PICO_TRIANGLES )
				continue;

#pragma omp critical
			{
				/* allocate a surface (ydnar: gs mods) */
				ds = AllocDrawSurface( SURFACE_TRIANGLES );
			}
			ds->entityNum = entityNum;
			ds->mapEntityNum = mapEntityNum;
			ds->castShadows = castShadows;
			ds->recvShadows = recvShadows;
			ds->noAlphaFix = noAlphaFix;
			ds->skybox = skybox;
			if (added_surfaces != NULL)
				*added_surfaces += 1;

			/* get shader name */
			/* vortex: support .skin files */
			picoShaderName = PicoGetSurfaceShaderNameForSkin( surface, skin );

			/* handle shader remapping */
			glob = NULL;
			for( rm = remap; rm != NULL; rm = rm->next )
			{
				if( rm->from[ 0 ] == '*' && rm->from[ 1 ] == '\0' )
					glob = rm;
				else if( !Q_stricmp( picoShaderName, rm->from ) )
				{
					Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", picoShaderName, rm->to );
					picoShaderName = rm->to;
					glob = NULL;
					break;
				}
			}

			if( glob != NULL )
			{
				Sys_FPrintf( SYS_VRB, "Globbing %s to %s\n", picoShaderName, glob->to );
				picoShaderName = glob->to;
			}

			/* shader renaming for sof2 */
			if( renameModelShaders )
			{
				strcpy( shaderName, picoShaderName );
				StripExtension( shaderName );
				if( spawnFlags & 1 )
					strcat( shaderName, "_RMG_BSP" );
				else
					strcat( shaderName, "_BSP" );
				si = ShaderInfoForShader( shaderName );
			}
			else
				si = ShaderInfoForShader( picoShaderName );

			LoadShaderImages( si );
			if (si)
			{
				si->clipModel = qtrue;
				si->isMapObjectSolid = qtrue;
				si->skipSolidCull = qtrue;
				si->compileFlags |= C_NODRAW;
			}

			/* warn for missing shader */
			if( si->warnNoShaderImage == qtrue )
			{
				if( mapEntityNum >= 0 )
					Sys_Warning( mapEntityNum, "Failed to load shader image '%s'", si->shader );
				else
				{
					/* external entity, just show single warning */
					Sys_Warning( "Failed to load shader image '%s' for model '%s'", si->shader, PicoGetModelFileName( model ) );
					si->warnNoShaderImage = qfalse;
				}
			}

			/* set shader */
			ds->shaderInfo = si;

			/* set shading angle */
			ds->smoothNormals = shadeAngle;

			/* force to meta? */
			if( (si != NULL && si->forceMeta) || (spawnFlags & 4) )	/* 3rd bit */
				ds->type = SURFACE_FORCED_META;

			/* fix the surface's normals (jal: conditioned by shader info) */
			if( !(spawnFlags & 64) && ( shadeAngle <= 0.0f || ds->type != SURFACE_FORCED_META ) )
				PicoFixSurfaceNormals( surface );

			/* set sample size */
			if( lightmapSampleSize > 0.0f )
				ds->sampleSize = lightmapSampleSize;

			/* set lightmap scale */
			if( lightmapScale > 0.0f )
				ds->lightmapScale = lightmapScale;

			/* set lightmap axis */
			if (lightmapAxis != NULL)
				VectorCopy( lightmapAxis, ds->lightmapAxis );

			/* set minlight/ambient/colormod */
			if (minlight != NULL)
			{
				VectorCopy( minlight, ds->minlight );
				VectorCopy( minlight, ds->minvertexlight );
			}
			if (minvertexlight != NULL)
				VectorCopy( minvertexlight, ds->minvertexlight );
			if (ambient != NULL)
				VectorCopy( ambient, ds->ambient );
			if (colormod != NULL)
				VectorCopy( colormod, ds->colormod );

			/* set vertical texture projection */
			if ( vertTexProj > 0 )
				ds->vertTexProj = vertTexProj;

			/* set particulars */
			ds->numVerts = PicoGetSurfaceNumVertexes( surface );
#pragma omp critical
			{
				ds->verts = (bspDrawVert_t *)safe_malloc( ds->numVerts * sizeof( ds->verts[ 0 ] ) );
				memset( ds->verts, 0, ds->numVerts * sizeof( ds->verts[ 0 ] ) );
			}

			if (added_verts != NULL)
				*added_verts += ds->numVerts;

			ds->numIndexes = PicoGetSurfaceNumIndexes( surface );
#pragma omp critical
			{
				ds->indexes = (int *)safe_malloc( ds->numIndexes * sizeof( ds->indexes[ 0 ] ) );
				memset( ds->indexes, 0, ds->numIndexes * sizeof( ds->indexes[ 0 ] ) );
			}

			if (added_triangles != NULL)
				*added_triangles += (ds->numIndexes / 3);

			/* copy vertexes */
			for( i = 0; i < ds->numVerts; i++ )
			{
				int					j;
				bspDrawVert_t		*dv;
				vec3_t				forceVecs[ 2 ];
				picoVec_t			*xyz, *normal, *st;
				picoByte_t			*color;

				/* get vertex */
				dv = &ds->verts[ i ];

				/* vortex: create forced tcGen vecs for vertical texture projection */
				if (ds->vertTexProj > 0)
				{
					forceVecs[ 0 ][ 0 ] = 1.0 / ds->vertTexProj;
					forceVecs[ 0 ][ 1 ] = 0.0;
					forceVecs[ 0 ][ 2 ] = 0.0;
					forceVecs[ 1 ][ 0 ] = 0.0;
					forceVecs[ 1 ][ 1 ] = 1.0 / ds->vertTexProj;
					forceVecs[ 1 ][ 2 ] = 0.0;
				}

				/* xyz and normal */
				xyz = PicoGetSurfaceXYZ( surface, i );
				VectorCopy( xyz, dv->xyz );
				m4x4_transform_point( transform, dv->xyz );

				normal = PicoGetSurfaceNormal( surface, i );
				VectorCopy( normal, dv->normal );
				m4x4_transform_normal( nTransform, dv->normal );
				VectorNormalize( dv->normal, dv->normal );

				/* ydnar: tek-fu celshading support for flat shaded shit */
				if( flat )
				{
					dv->st[ 0 ] = si->stFlat[ 0 ];
					dv->st[ 1 ] = si->stFlat[ 1 ];
				}

				/* vortex: entity-set _vp/_vtcproj will force drawsurface to "tcGen ivector ( value 0 0 ) ( 0 value 0 )"  */
				else if( ds->vertTexProj > 0 )
				{
					/* project the texture */
					dv->st[ 0 ] = DotProduct( forceVecs[ 0 ], dv->xyz );
					dv->st[ 1 ] = DotProduct( forceVecs[ 1 ], dv->xyz );
				}

				/* ydnar: gs mods: added support for explicit shader texcoord generation */
				else if( si->tcGen )
				{
					/* project the texture */
					dv->st[ 0 ] = DotProduct( si->vecs[ 0 ], dv->xyz );
					dv->st[ 1 ] = DotProduct( si->vecs[ 1 ], dv->xyz );
				}

				/* normal texture coordinates */
				else
				{
					st = PicoGetSurfaceST( surface, 0, i );
					dv->st[ 0 ] = st[ 0 ];
					dv->st[ 1 ] = st[ 1 ];
				}

				/* scale UV by external key */
				dv->st[ 0 ] *= uvScale;
				dv->st[ 1 ] *= uvScale;

				/* set lightmap/color bits */
				color = PicoGetSurfaceColor( surface, 0, i );

				for( j = 0; j < MAX_LIGHTMAPS; j++ )
				{
					dv->lightmap[ j ][ 0 ] = 0.0f;
					dv->lightmap[ j ][ 1 ] = 0.0f;
					if(spawnFlags & 32) // spawnflag 32: model color -> alpha hack
					{
						dv->color[ j ][ 0 ] = 255.0f;
						dv->color[ j ][ 1 ] = 255.0f;
						dv->color[ j ][ 2 ] = 255.0f;
						dv->color[ j ][ 3 ] = color[ 0 ] * 0.3f + color[ 1 ] * 0.59f + color[ 2 ] * 0.11f;
					}
					else
					{
						dv->color[ j ][ 0 ] = color[ 0 ];
						dv->color[ j ][ 1 ] = color[ 1 ];
						dv->color[ j ][ 2 ] = color[ 2 ];
						dv->color[ j ][ 3 ] = color[ 3 ];
					}
				}
			}

			/* copy indexes */
			indexes = PicoGetSurfaceIndexes( surface, 0 );

			for( i = 0; i < ds->numIndexes; i++ )
				ds->indexes[ i ] = indexes[ i ];

			/* deform vertexes */
			DeformVertexes( ds, pushVertexes);

			/* set cel shader */
			ds->celShader = celShader;

			/* walk triangle list */
			for( i = 0; i < ds->numIndexes; i += 3 )
			{
				bspDrawVert_t		*dv;
				int					j;

				vec3_t points[ 4 ];

				/* make points and back points */
				for( j = 0; j < 3; j++ )
				{
					/* get vertex */
					dv = &ds->verts[ ds->indexes[ i + j ] ];

					/* copy xyz */
					VectorCopy( dv->xyz, points[ j ] );
				}
			}

			/* ydnar: giant hack land: generate clipping brushes for model triangles */
			if( !noclipmodel )	/* 2nd bit */
			{
				vec3_t points[ 4 ], backs[ 3 ];
				vec4_t plane, reverse, pa, pb, pc;

				/* overflow check */
				if( (nummapplanes + 64) >= (MAX_MAP_PLANES >> 1) )
				{
					continue;
				}

				/* walk triangle list */
				for( i = 0; i < ds->numIndexes; i += 3 )
				{
					int					j;

					/* overflow hack */
					if( (nummapplanes + 64) >= (MAX_MAP_PLANES >> 1) )
					{
						Sys_Warning( mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name );
						break;
					}

					/* make points and back points */
					for( j = 0; j < 3; j++ )
					{
						bspDrawVert_t		*dv;
						int					k;

						/* get vertex */
						dv = &ds->verts[ ds->indexes[ i + j ] ];

						/* copy xyz */
						VectorCopy( dv->xyz, points[ j ] );
						VectorCopy( dv->xyz, backs[ j ] );

						/* find nearest axial to normal and push back points opposite */
						/* note: this doesn't work as well as simply using the plane of the triangle, below */
						for( k = 0; k < 3; k++ )
						{
							if( fabs( dv->normal[ k ] ) >= fabs( dv->normal[ (k + 1) % 3 ] ) &&
								fabs( dv->normal[ k ] ) >= fabs( dv->normal[ (k + 2) % 3 ] ) )
							{
								backs[ j ][ k ] += dv->normal[ k ] < 0.0f ? 64.0f : -64.0f;
								break;
							}
						}
					}

					VectorCopy( points[0], points[3] ); // for cyclic usage

					/* make plane for triangle */
					// div0: add some extra spawnflags:
					//   0: snap normals to axial planes for extrusion
					//   8: extrude with the original normals
					//  16: extrude only with up/down normals (ideal for terrain)
					//  24: extrude by distance zero (may need engine changes)
					if( PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] ) )
					{
						double				normalEpsilon_save;
						double				distanceEpsilon_save;

						vec3_t bestNormal;
						float backPlaneDistance = 2;

						if(spawnFlags & 8) // use a DOWN normal
						{
							if(spawnFlags & 16)
							{
								// 24: normal as is, and zero width (broken)
								VectorCopy(plane, bestNormal);
							}
							else
							{
								// 8: normal as is
								VectorCopy(plane, bestNormal);
							}
						}
						else
						{
							if(spawnFlags & 16)
							{
								// 16: UP/DOWN normal
								VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
							}
							else
							{
								// 0: axial normal
								if(fabs(plane[0]) > fabs(plane[1])) // x>y
								if(fabs(plane[1]) > fabs(plane[2])) // x>y, y>z
									VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
								else // x>y, z>=y
								if(fabs(plane[0]) > fabs(plane[2])) // x>z, z>=y
									VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
								else // z>=x, x>y
									VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
								else // y>=x
								if(fabs(plane[1]) > fabs(plane[2])) // y>z, y>=x
									VectorSet(bestNormal, 0, (plane[1] >= 0 ? 1 : -1), 0);
								else // z>=y, y>=x
									VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
							}
						}

						/* regenerate back points */
						for( j = 0; j < 3; j++ )
						{
							bspDrawVert_t		*dv;

							/* get vertex */
							dv = &ds->verts[ ds->indexes[ i + j ] ];

							// shift by some units
							VectorMA(dv->xyz, -64.0f, bestNormal, backs[j]); // 64 prevents roundoff errors a bit
						}

						/* make back plane */
						VectorScale( plane, -1.0f, reverse );
						reverse[ 3 ] = -plane[ 3 ];
						if((spawnFlags & 24) != 24)
							reverse[3] += DotProduct(bestNormal, plane) * backPlaneDistance;
						// that's at least sqrt(1/3) backPlaneDistance, unless in DOWN mode; in DOWN mode, we are screwed anyway if we encounter a plane that's perpendicular to the xy plane)

						normalEpsilon_save = normalEpsilon;
						distanceEpsilon_save = distanceEpsilon;

						if( PlaneFromPoints( pa, points[ 2 ], points[ 1 ], backs[ 1 ] ) &&
							PlaneFromPoints( pb, points[ 1 ], points[ 0 ], backs[ 0 ] ) &&
							PlaneFromPoints( pc, points[ 0 ], points[ 2 ], backs[ 2 ] ) )
						{
							//Sys_Printf("top: %f. bottom: %f.\n", top, bottom);

							numSolidSurfs++;

							if ((cullSmallSolids || si->isTreeSolid) && !(si->skipSolidCull || si->isMapObjectSolid))
							{
								vec3_t mins, maxs;
								vec3_t size;
								float sz;
								int z;

								VectorSet(mins, 999999, 999999, 999999);
								VectorSet(maxs, -999999, -999999, -999999);

								for (z = 0; z < 4; z++)
								{
									if (points[z][0] < mins[0]) mins[0] = points[z][0];
									if (points[z][1] < mins[1]) mins[1] = points[z][1];
									if (points[z][2] < mins[2]) mins[2] = points[z][2];

									if (points[z][0] > maxs[0]) maxs[0] = points[z][0];
									if (points[z][1] > maxs[1]) maxs[1] = points[z][1];
									if (points[z][2] > maxs[2]) maxs[2] = points[z][2];
								}

								if (top != -999999 && bottom != -999999)
								{
									float s = top - bottom;
									float newtop = bottom + (s / 2.0);

									//Sys_Printf("newtop: %f. top: %f. bottom: %f. mins: %f. maxs: %f.\n", newtop, top, bottom, mins[2], maxs[2]);

									if (mins[2] > newtop)
									{
										//Sys_Printf("CULLED: %f > %f.\n", maxs[2], newtop);
										numHeightCulledSurfs++;
										continue;
									}
								}

								VectorSubtract(maxs, mins, size);
								//sz = VectorLength(size);
								sz = maxs[0] - mins[0];
								if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
								if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

								if (sz < 36)
								{
									//Sys_Printf("CULLED: %f < 30. (%f %f %f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
									numSizeCulledSurfs++;
									continue;
								}
							}

							//#define __FORCE_TREE_META__
#if defined(__FORCE_TREE_META__)
							if (meta) si->forceMeta = qtrue; // much slower...
#endif

#pragma omp ordered
							{
#pragma omp critical
								{

									/* build a brush */ // -- UQ1: Moved - Why allocate when its not needed...
									buildBrush = AllocBrush( 24/*48*/ ); // UQ1: 48 seems to be more then is used... Wasting memory...
									buildBrush->entityNum = mapEntityNum;
									buildBrush->mapEntityNum = mapEntityNum;
									buildBrush->original = buildBrush;
									buildBrush->contentShader = si;
									buildBrush->compileFlags = si->compileFlags;
									buildBrush->contentFlags = si->contentFlags;

									if (si->isTreeSolid || si->isMapObjectSolid || (si->compileFlags & C_DETAIL))
									{
										buildBrush->detail = qtrue;
									}
									else if (si->compileFlags & C_STRUCTURAL) // allow forced structural brushes here
									{
										buildBrush->detail = qfalse;

										// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
										if(normalEpsilon > 0)
											normalEpsilon = 0;
										if(distanceEpsilon > 0)
											distanceEpsilon = 0;
									}
									else
									{
										buildBrush->detail = qtrue;
									}

									/* set up brush sides */
									buildBrush->numsides = 5;
									buildBrush->sides[ 0 ].shaderInfo = si;

									for (j = 1; j < buildBrush->numsides; j++)
									{
										buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
										buildBrush->sides[j].culled = qtrue;
									}

									buildBrush->sides[0].planenum = FindFloatPlane(plane, plane[3], 3, points);
									buildBrush->sides[1].planenum = FindFloatPlane(pa, pa[3], 2, &points[1]); // pa contains points[1] and points[2]
									buildBrush->sides[2].planenum = FindFloatPlane(pb, pb[3], 2, &points[0]); // pb contains points[0] and points[1]
									buildBrush->sides[3].planenum = FindFloatPlane(pc, pc[3], 2, &points[2]); // pc contains points[2] and points[0] (copied to points[3]
									buildBrush->sides[4].planenum = FindFloatPlane(reverse, reverse[3], 3, backs);

									/* add to entity */
									if (CreateBrushWindings(buildBrush))
									{
										int numsides;

										AddBrushBevels();
										//%	EmitBrushes( buildBrush, NULL, NULL );

										numsides = buildBrush->numsides;

										if (!RemoveDuplicateBrushPlanes(buildBrush))
										{// UQ1: Testing - This would create a mirrored plane... free it...
											free(buildBrush);
											//Sys_Printf("Removed a mirrored plane\n");
										}
										else
										{
											//if (buildBrush->numsides < numsides) Sys_Printf("numsides reduced from %i to %i.\n", numsides, buildBrush->numsides);

											buildBrush->next = entities[mapEntityNum].brushes;
											entities[mapEntityNum].brushes = buildBrush;
											entities[mapEntityNum].numBrushes++;
											if (added_brushes != NULL)
												*added_brushes += 1;
										}
									}
									else
									{
										free(buildBrush);
									}
								} // #pragma omp critical
							} // #pragma omp ordered
						}
						else
						{
							continue;
						}

						normalEpsilon = normalEpsilon_save;
						distanceEpsilon = distanceEpsilon_save;
					}
				}
			}
		}
	}
#endif //__MODEL_SIMPLIFICATION__
}


/*
LoadTriangleModels()
preload triangle models map is using
*/

void LoadTriangleModels(void)
{
	int num, frame, start, numLoadedModels;
	picoModel_t *picoModel;
	qboolean loaded;
	const char *model;
	entity_t *e;

	numLoadedModels = 0;

	/* note it */
	Sys_PrintHeading("--- LoadTriangleModels ---\n");

	/* load */
	start = I_FloatTime();
	for (num = 1; num < numEntities; num++)
	{
		//printLabelledProgress("LoadTriangleModels", num, numEntities);

		/* get ent */
		e = &entities[num];

		/* convert misc_models into raw geometry  */
		if (Q_stricmp("misc_model", ValueForKey(e, "classname")) && Q_stricmp("misc_gamemodel", ValueForKey(e, "classname")))
			continue;

		/* get model name */
		/* vortex: add _model synonim */
		model = ValueForKey(e, "_model");
		if (model[0] == '\0')
			model = ValueForKey(e, "model");
		if (model[0] == '\0')
			continue;

		/* get model frame */
		if (KeyExists(e, "_frame"))
			frame = IntForKey(e, "_frame");
		else if (KeyExists(e, "frame"))
			frame = IntForKey(e, "frame");
		else
			frame = 0;

		/* load the model */
		loaded = PreloadModel((char*)model, frame);
		if (loaded)
			numLoadedModels++;

		/* warn about missing models */
		picoModel = FindModel((char*)model, frame);
		if (!picoModel || picoModel->numSurfaces == 0)
			Sys_Warning(e->mapEntityNum, "Failed to load model '%s' frame %i", model, frame);

		/* debug */
		//if( loaded && picoModel && picoModel->numSurfaces != 0  )
		//	Sys_Printf("loaded %s: %i vertexes %i triangles\n", PicoGetModelFileName( picoModel ), PicoGetModelTotalVertexes( picoModel ), PicoGetModelTotalIndexes( picoModel ) / 3 );


#ifdef __MODEL_SIMPLIFICATION__
		//
		// Count the verts... Skip lod for anything too big for memory...
		//

		int	numVerts = 0;
		int numIndexes = 0;
		int numSurfaces = PicoGetModelNumSurfaces( picoModel );

		for( int s = 0; s < numSurfaces; s++ )
		{
			int					i;
			picoVec_t			*xyz;
			picoSurface_t		*surface;

			/* get surface */
			surface = PicoGetModelSurface( picoModel, s );

			if( surface == NULL )
				continue;

			/* only handle triangle surfaces initially (fixme: support patches) */
			if( PicoGetSurfaceType( surface ) != PICO_TRIANGLES )
				continue;

			char				*picoShaderName = PicoGetSurfaceShaderNameForSkin( surface, 0 );

			shaderInfo_t		*si = ShaderInfoForShader( picoShaderName );

			LoadShaderImages( si );

			if(!si->clipModel)
			{
				continue;
			}

			if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
			{
				continue;
			}

			if( !si->clipModel
				&& ((si->compileFlags & C_TRANSLUCENT) || !(si->compileFlags & C_SOLID)) )
			{
				continue;
			}

			numVerts += PicoGetSurfaceNumVertexes(surface);
			numIndexes += PicoGetSurfaceNumIndexes(surface);
		}

		// if (FindModel( name, frame ))

		if (picoModel && picoModel->numSurfaces > 0 /*&& numVerts < 50000*/)
		{// UQ1: Testing... Mesh decimation for collision planes...
			picoModel_t *picoModel2 = NULL;
			qboolean loaded2 = qfalse;
			char fileNameIn[128] = { 0 };
			char fileNameOut[128] = { 0 };
			char tempfileNameOut[128] = { 0 };

			strcpy(tempfileNameOut, model);
			StripExtension(tempfileNameOut);
			sprintf(fileNameOut, "%s/base/%s_lod.obj", basePaths[0], tempfileNameOut);
			sprintf(fileNameIn, "%s_lod.obj", tempfileNameOut);

			//Sys_Printf("File path is %s.\n", fileNameOut);

			if (FileExists(fileNameOut))
			{// Try to load _lod version...
				//Sys_Printf("Exists: %s.\n", fileNameOut);

				loaded2 = PreloadModel((char*)fileNameIn, 0);
				if (loaded2)
					numLoadedModels++;

				picoModel2 = FindModel((char*)fileNameIn, 0);
			}

			if (!picoModel2 || picoModel2->numSurfaces == 0)
			{// No _lod version found to load, or it failed to load... Generate a new one...
				Sys_Printf("Simplifying model %s. %i surfaces. %i verts. %i indexes.\n", model, PicoGetModelNumSurfaces(picoModel), numVerts, numIndexes);

				Decimate(picoModel, fileNameOut);

				loaded2 = PreloadModel((char*)fileNameIn, 0);
				if (loaded2) {
					//Sys_Printf("Loaded model %s.\n", fileNameOut);
					numLoadedModels++;
				}

				picoModel2 = FindModel((char*)fileNameIn, 0);
				//if (picoModel2) Sys_Printf("Found model %s.\n", fileNameOut);
			}
			else
			{// All good... _lod loaded and is OK!
				//Sys_Printf("Already have lod %s for model %s.\n", fileNameIn, model);
			}

		}
#endif //__MODEL_SIMPLIFICATION__
	}

	/* print overall time */
	//Sys_Printf (" (%d)\n", (int) (I_FloatTime() - start) );

	/* emit stats */
	Sys_Printf("%9i unique model/frame combinations\n", numLoadedModels);
}

/*
AddTriangleModels()
adds misc_model surfaces to the bsp
*/

void AddTriangleModels(int entityNum, qboolean quiet, qboolean cullSmallSolids)
{
	int				added_surfaces = 0, added_triangles = 0, added_verts = 0, added_brushes = 0;
	int				total_added_surfaces = 0, total_added_triangles = 0, total_added_verts = 0, total_added_brushes = 0;
	int				num, frame, skin, spawnFlags;
	char			castShadows, recvShadows;
	entity_t		*e, *e2;
	const char		*targetName;
	const char		*target, *model, *value;
	char			shader[MAX_QPATH];
	shaderInfo_t	*celShader;
	float			temp, baseLightmapScale, lightmapScale, uvScale, pushVertexes;
	int				baseSmoothNormals, smoothNormals;
	int				baseVertTexProj, vertTexProj;
	qboolean        noAlphaFix, skybox;
	vec3_t			origin, scale, angles, baseLightmapAxis, lightmapAxis, baseMinlight, baseMinvertexlight, baseAmbient, baseColormod, minlight, minvertexlight, ambient, colormod;
	m4x4_t			transform;
	epair_t			*ep;
	remap_t			*remap, *remap2;
	char			*split;

	/* note it */
	if (!quiet) Sys_PrintHeadingVerbose("--- AddTriangleModels ---\n");

	/* get current brush entity targetname */
	e = &entities[entityNum];
	if (e == entities)
		targetName = "";
	else
	{
		targetName = ValueForKey(e, "targetname");

		/* misc_model entities target non-worldspawn brush model entities */
		if (targetName[0] == '\0')
		{
			//Sys_Printf( "Failed Targetname\n" );
			return;
		}
	}

	/* vortex: get lightmap scaling value for this entity */
	GetEntityLightmapScale(e, &baseLightmapScale, 0);

	/* vortex: get lightmap axis for this entity */
	GetEntityLightmapAxis(e, baseLightmapAxis, NULL);

	/* vortex: per-entity normal smoothing */
	GetEntityNormalSmoothing(e, &baseSmoothNormals, 0);

	/* vortex: per-entity _minlight, _minvertexlight, _ambient, _color, _colormod */
	GetEntityMinlightAmbientColor(e, NULL, baseMinlight, baseMinvertexlight, baseAmbient, baseColormod, qtrue);

	/* vortex: vertical texture projection */
	baseVertTexProj = IntForKey(e, "_vtcproj");
	if (baseVertTexProj <= 0)
		baseVertTexProj = IntForKey(e, "_vp");
	if (baseVertTexProj <= 0)
		baseVertTexProj = 0;

	/* walk the entity list */
	for (num = 1; num < numEntities; num++)
	{
		if (!quiet) printLabelledProgress("AddTriangleModels", num, numEntities);

		/* get e2 */
		e2 = &entities[num];

		/* convert misc_models into raw geometry  */
		if (Q_stricmp("misc_model", ValueForKey(e2, "classname")))
		{
			//Sys_Printf( "Failed Classname\n" );
			continue;
		}

		/* ydnar: added support for md3 models on non-worldspawn models */
		target = ValueForKey(e2, "target");
		if (strcmp(target, targetName))
		{
			//Sys_Printf( "Failed Target\n" );
			continue;
		}

		/* get model name */
		/* vortex: add _model synonim */
		model = ValueForKey(e2, "_model");
		if (model[0] == '\0')
			model = ValueForKey(e2, "model");
		if (model[0] == '\0')
		{
			Sys_Warning(e2->mapEntityNum, "misc_model at %i %i %i without a model key", (int)origin[0], (int)origin[1], (int)origin[2]);
			//Sys_Printf( "Failed Model\n" );
			continue;
		}

		/* get model frame */
		frame = 0;
		if (KeyExists(e2, "frame"))
			frame = IntForKey(e2, "frame");
		if (KeyExists(e2, "_frame"))
			frame = IntForKey(e2, "_frame");

		/* get model skin */
		skin = 0;
		if (KeyExists(e2, "skin"))
			skin = IntForKey(e2, "skin");
		if (KeyExists(e2, "_skin"))
			skin = IntForKey(e2, "_skin");

		/* get explicit shadow flags */
		GetEntityShadowFlags(e2, e, &castShadows, &recvShadows, (e == entities) ? qtrue : qfalse);

		/* get spawnflags */
		spawnFlags = IntForKey(e2, "spawnflags");

		/* get origin */
		GetVectorForKey(e2, "origin", origin);
		VectorSubtract(origin, e->origin, origin);	/* offset by parent */

		/* get scale */
		scale[0] = scale[1] = scale[2] = 1.0f;
		temp = FloatForKey(e2, "modelscale");
		if (temp != 0.0f)
			scale[0] = scale[1] = scale[2] = temp;
		value = ValueForKey(e2, "modelscale_vec");
		if (value[0] != '\0')
			sscanf(value, "%f %f %f", &scale[0], &scale[1], &scale[2]);

		/* VorteX: get UV scale */
		uvScale = 1.0;
		temp = FloatForKey(e2, "uvscale");
		if (temp != 0.0f)
			uvScale = temp;
		else
		{
			temp = FloatForKey(e2, "_uvs");
			if (temp != 0.0f)
				uvScale = temp;
		}

		/* VorteX: UV autoscale */
		if (IntForKey(e2, "uvautoscale") || IntForKey(e2, "_uvas"))
			uvScale = uvScale * (scale[0] + scale[1] + scale[2]) / 3.0f;

		/* get "angle" (yaw) or "angles" (pitch yaw roll) */
		angles[0] = angles[1] = angles[2] = 0.0f;
		angles[2] = FloatForKey(e2, "angle");
		value = ValueForKey(e2, "angles");
		if (value[0] != '\0')
			sscanf(value, "%f %f %f", &angles[1], &angles[2], &angles[0]);

		/* set transform matrix (thanks spog) */
		m4x4_identity(transform);
		m4x4_pivoted_transform_by_vec3(transform, origin, angles, eXYZ, scale, vec3_origin);

		/* get shader remappings */
		remap = NULL;
		for (ep = e2->epairs; ep != NULL; ep = ep->next)
		{
			/* look for keys prefixed with "_remap" */
			if (ep->key != NULL && ep->value != NULL &&
				ep->key[0] != '\0' && ep->value[0] != '\0' &&
				!Q_strncasecmp(ep->key, "_remap", 6))
			{
				/* create new remapping */
				remap2 = remap;
				remap = (remap_t *)safe_malloc(sizeof(*remap));
				remap->next = remap2;
				strcpy(remap->from, ep->value);

				/* split the string */
				split = strchr(remap->from, ';');
				if (split == NULL)
				{
					Sys_Warning(e2->mapEntityNum, "Shader _remap key found in misc_model without a ; character");
					free(remap);
					remap = remap2;
					Sys_Printf("Failed Remapping\n");
					continue;
				}

				/* store the split */
				*split = '\0';
				strcpy(remap->to, (split + 1));

				/* note it */
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", remap->from, remap->to );
			}
		}

		/* ydnar: cel shader support */
		value = ValueForKey(e2, "_celshader");
		if (value[0] == '\0')
			value = ValueForKey(&entities[0], "_celshader");
		if (value[0] != '\0')
		{
			sprintf(shader, "textures/%s", value);
			celShader = ShaderInfoForShader(shader);
		}
		else
			celShader = NULL;

		/* vortex: get lightmap scaling value for this entity */
		GetEntityLightmapScale(e2, &lightmapScale, baseLightmapScale);

		/* vortex: get lightmap axis for this entity */
		GetEntityLightmapAxis(e2, lightmapAxis, baseLightmapAxis);

		/* vortex: per-entity normal smoothing */
		GetEntityNormalSmoothing(e2, &smoothNormals, baseSmoothNormals);

		/* vortex: per-entity _minlight, _ambient, _color, _colormod */
		VectorCopy(baseMinlight, minlight);
		VectorCopy(baseMinvertexlight, minvertexlight);
		VectorCopy(baseAmbient, ambient);
		VectorCopy(baseColormod, colormod);
		GetEntityMinlightAmbientColor(e2, NULL, minlight, minvertexlight, ambient, colormod, qfalse);

		/* vortex: vertical texture projection */
		vertTexProj = IntForKey(e2, "_vp");
		if (vertTexProj <= 0)
			vertTexProj = baseVertTexProj;

		/* vortex: prevent alpha-fix stage on entity */
		noAlphaFix = (IntForKey(e2, "_noalphafix") > 0) ? qtrue : qfalse;
		skybox = (IntForKey(e2, "_skybox") > 0) ? qtrue : qfalse;

		/* vortex: push vertexes among their normals (fatboy) */
		pushVertexes = FloatForKey(e2, "_pushvertexes");
		if (pushVertexes == 0)
			pushVertexes = FloatForKey(e2, "_pv");
		pushVertexes += FloatForKey(e2, "_pv2"); // vortex: set by decorator

		/* insert the model */
		InsertModel((char*)model, frame, skin, transform, uvScale, remap, celShader, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, cullSmallSolids);

		//Sys_Printf( "insert model: %s. added_surfaces: %i. added_triangles: %i. added_verts: %i. added_brushes: %i.\n", model, added_surfaces, added_triangles, added_verts, added_brushes );

		total_added_surfaces += added_surfaces;
		total_added_triangles += added_triangles;
		total_added_verts += added_verts;
		total_added_brushes += added_brushes;

		added_surfaces = added_triangles = added_verts = added_brushes = 0;

		/* free shader remappings */
		while (remap != NULL)
		{
			remap2 = remap->next;
			free(remap);
			remap = remap2;
		}
	}

	if (!quiet)
	{
		//int totalExpCulled = numExperimentalCulled;
		int totalHeightCulled = numHeightCulledSurfs;
		int totalSizeCulled = numSizeCulledSurfs;
		int totalTotalCulled = numExperimentalCulled + numHeightCulledSurfs + numSizeCulledSurfs;
		int percentExpCulled = 0;
		int percentHeightCulled = 0;
		int percentSizeCulled = 0;
		int percentTotalCulled = 0;

		//if (totalExpCulled > 0 && numSolidSurfs > 0)
		//	percentExpCulled = (int)(((float)totalExpCulled / (float)numSolidSurfs) * 100.0);

		if (totalHeightCulled > 0 && numSolidSurfs > 0)
			percentHeightCulled = (int)(((float)totalHeightCulled / (float)numSolidSurfs) * 100.0);

		if (totalSizeCulled > 0 && numSolidSurfs > 0)
			percentSizeCulled = (int)(((float)totalSizeCulled / (float)numSolidSurfs) * 100.0);

		if (totalTotalCulled > 0 && numSolidSurfs > 0)
			percentTotalCulled = (int)(((float)totalTotalCulled / (float)numSolidSurfs) * 100.0);

		/* emit some stats */
		Sys_Printf("%9d surfaces added\n", total_added_surfaces);
		Sys_Printf("%9d triangles added\n", total_added_triangles);
		Sys_Printf("%9d vertexes added\n", total_added_verts);
		Sys_Printf("%9d brushes added\n", total_added_brushes);
		//Sys_Printf("%9i of %i solid surfaces culled by experimental culling (%i percent).\n", totalExpCulled, numSolidSurfs, percentExpCulled);
		Sys_Printf("%9i of %i solid surfaces culled for height (%i percent).\n", totalHeightCulled, numSolidSurfs, percentHeightCulled);
		Sys_Printf("%9i of %i solid surfaces culled for tiny size (%i percent).\n", totalSizeCulled, numSolidSurfs, percentSizeCulled);
		Sys_Printf("%9i of %i total solid surfaces culled (%i percent).\n", totalTotalCulled, numSolidSurfs, percentTotalCulled);
	}

	//g_numHiddenFaces = 0;
	//g_numCoinFaces = 0;
	//CullSides( &entities[ mapEntityNum ] );
	//CullSidesStats();
}
