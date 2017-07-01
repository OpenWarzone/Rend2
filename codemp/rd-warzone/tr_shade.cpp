/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_shade.c

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

qboolean MATRIX_UPDATE = qtrue;
qboolean CLOSE_LIGHTS_UPDATE = qtrue;

extern qboolean WATER_ENABLED;

color4ub_t	styleColors[MAX_LIGHT_STYLES];

extern void RB_DrawSurfaceSprites( shaderStage_t *stage, shaderCommands_t *input);

extern qboolean RB_CheckOcclusion(matrix_t MVP, shaderCommands_t *input);

/*
==================
R_DrawElements

==================
*/

void TesselatedGlDrawElements( int numIndexes, glIndex_t firstIndex, glIndex_t minIndex, glIndex_t maxIndex )
{// Would really suck if this is the only way... I hate opengl...
	GLint MaxPatchVertices = 0;
	qglGetIntegerv(GL_MAX_PATCH_VERTICES, &MaxPatchVertices);

	for (int i = 0; i < maxIndex-minIndex; i++)
	{
		qglPatchParameteri(GL_PATCH_VERTICES, 3);
		qglDrawElements(GL_PATCHES, 3, GL_INDEX_TYPE, BUFFER_OFFSET((firstIndex+i) * sizeof(glIndex_t)));
	}
}

void R_DrawElementsVBO( int numIndexes, glIndex_t firstIndex, glIndex_t minIndex, glIndex_t maxIndex, glIndex_t numVerts, qboolean tesselation )
{
	if (r_tesselation->integer && tesselation)
	{
		GLint MaxPatchVertices = 0;
		qglGetIntegerv(GL_MAX_PATCH_VERTICES, &MaxPatchVertices);
		//printf("Max supported patch vertices %d\n", MaxPatchVertices);
		qglPatchParameteri(GL_PATCH_VERTICES, 3);
		qglDrawRangeElements(GL_PATCHES, minIndex, maxIndex, numIndexes, GL_INDEX_TYPE, BUFFER_OFFSET(firstIndex * sizeof(glIndex_t)));

		//TesselatedGlDrawElements( numIndexes, firstIndex, minIndex, maxIndex );
	}
	else
	{
		qglDrawRangeElements(GL_TRIANGLES, minIndex, maxIndex, numIndexes, GL_INDEX_TYPE, BUFFER_OFFSET(firstIndex * sizeof(glIndex_t)));
	}
}

void TesselatedGlMultiDrawElements( GLenum mode, GLsizei *count, GLenum type, const GLvoid **indices, GLsizei primcount )
{// Would really suck if this is the only way... I hate opengl...
	GLint MaxPatchVertices = 0;
	qglGetIntegerv(GL_MAX_PATCH_VERTICES, &MaxPatchVertices);

	for (int i = 0; i < primcount; i++)
	{
		if (count[i] > 0)
		{
			qglPatchParameteri(GL_PATCH_VERTICES, 3);
			qglDrawElements(mode, count[i], type, indices[i]);
		}
	}
}

void R_DrawMultiElementsVBO( int multiDrawPrimitives, glIndex_t *multiDrawMinIndex, glIndex_t *multiDrawMaxIndex,
	GLsizei *multiDrawNumIndexes, glIndex_t **multiDrawFirstIndex, glIndex_t numVerts, qboolean tesselation)
{
	if (r_tesselation->integer && tesselation)
	{
		//TesselatedGlMultiDrawElements( GL_PATCHES, multiDrawNumIndexes, GL_INDEX_TYPE, (const GLvoid **)multiDrawFirstIndex, multiDrawPrimitives );
		GLint MaxPatchVertices = 0;
		qglGetIntegerv(GL_MAX_PATCH_VERTICES, &MaxPatchVertices);
		qglPatchParameteri(GL_PATCH_VERTICES, 3);
		qglMultiDrawElements(GL_PATCHES, multiDrawNumIndexes, GL_INDEX_TYPE, (const GLvoid **)multiDrawFirstIndex, multiDrawPrimitives);
	}
	else
	{
		qglMultiDrawElements(GL_TRIANGLES, multiDrawNumIndexes, GL_INDEX_TYPE, (const GLvoid **)multiDrawFirstIndex, multiDrawPrimitives);
	}
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;


/*
=================
R_BindAnimatedImageToTMU

=================
*/
void R_BindAnimatedImageToTMU( textureBundle_t *bundle, int tmu ) {
	int		index;

	if ( bundle->isVideoMap ) {
		int oldtmu = glState.currenttmu;
		GL_SelectTexture(tmu);
		ri->CIN_RunCinematic(bundle->videoMapHandle);
		ri->CIN_UploadCinematic(bundle->videoMapHandle);
		GL_SelectTexture(oldtmu);
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_BindToTMU( bundle->image[0], tmu);
		return;
	}

	if (backEnd.currentEntity->e.renderfx & RF_SETANIMINDEX )
	{
		index = backEnd.currentEntity->e.skinNum;
	}
	else
	{
		// it is necessary to do this messy calc to make sure animations line up
		// exactly with waveforms of the same frequency
		index = Q_ftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
		index >>= FUNCTABLE_SIZE2;

		if ( index < 0 ) {
			index = 0;	// may happen with shader time offsets
		}
	}

	if ( bundle->oneShotAnimMap )
	{
		if ( index >= bundle->numImageAnimations )
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	GL_BindToTMU( bundle->image[ index ], tmu );
}


/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input) {
	GL_Bind( tr.whiteImage );

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	{
		shaderProgram_t *sp = &tr.textureColorShader;
		vec4_t color;

		GLSL_VertexAttribsState(ATTR_POSITION);
		GLSL_BindProgram(sp);

		GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
		VectorSet4(color, 1, 1, 1, 1);
		GLSL_SetUniformVec4(sp, UNIFORM_COLOR, color);

		if (input->multiDrawPrimitives)
		{
			R_DrawMultiElementsVBO(input->multiDrawPrimitives, input->multiDrawMinIndex, input->multiDrawMaxIndex, input->multiDrawNumIndexes, input->multiDrawFirstIndex, input->numVertexes, qfalse);
		}
		else
		{
			R_DrawElementsVBO(input->numIndexes, input->firstIndex, input->minIndex, input->maxIndex, input->numVertexes, qfalse);
		}
	}

	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	//FIXME: implement this
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/

#ifdef __PLAYER_BASED_CUBEMAPS__
extern int			currentPlayerCubemap;
extern vec4_t		currentPlayerCubemapVec;
extern float		currentPlayerCubemapDistance;
#endif //__PLAYER_BASED_CUBEMAPS__

void RB_BeginSurface( shader_t *shader, int fogNum, int cubemapIndex ) {

	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.firstIndex = 0;
	tess.numVertexes = 0;
	tess.multiDrawPrimitives = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
#ifdef __PLAYER_BASED_CUBEMAPS__
	tess.cubemapIndex = currentPlayerCubemap;
#else //!__PLAYER_BASED_CUBEMAPS__
	tess.cubemapIndex = cubemapIndex;
#endif //__PLAYER_BASED_CUBEMAPS__
	//tess.dlightBits = 0;		// will be OR'd in by surface functions
#ifdef __PSHADOWS__
	tess.pshadowBits = 0;       // will be OR'd in by surface functions
#endif
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;
	tess.useInternalVBO = qtrue;

	if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
		tess.cubemapIndex = cubemapIndex = 0;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}

	if (backEnd.viewParms.flags & VPF_SHADOWMAP)
	{
		tess.currentStageIteratorFunc = RB_StageIteratorGeneric;
	}
}



extern float EvalWaveForm( const waveForm_t *wf );
extern float EvalWaveFormClamped( const waveForm_t *wf );


static void ComputeTexMods( shaderStage_t *pStage, int bundleNum, float *outMatrix, float *outOffTurb, float *outScale)
{
	int tm;
	float matrix[6], currentmatrix[6];
	textureBundle_t *bundle = &pStage->bundle[bundleNum];

	outScale[0] = outScale[1] = 1.0;

	matrix[0] = 1.0f; matrix[2] = 0.0f; matrix[4] = 0.0f;
	matrix[1] = 0.0f; matrix[3] = 1.0f; matrix[5] = 0.0f;

	currentmatrix[0] = 1.0f; currentmatrix[2] = 0.0f; currentmatrix[4] = 0.0f;
	currentmatrix[1] = 0.0f; currentmatrix[3] = 1.0f; currentmatrix[5] = 0.0f;

	outMatrix[0] = 1.0f; outMatrix[2] = 0.0f;
	outMatrix[1] = 0.0f; outMatrix[3] = 1.0f;

	outOffTurb[0] = 0.0f; outOffTurb[1] = 0.0f; outOffTurb[2] = 0.0f; outOffTurb[3] = 0.0f;

	for (tm = 0; tm < bundle->numTexMods; tm++) {
		switch (bundle->texMods[tm].type)
		{

		case TMOD_NONE:
			tm = TR_MAX_TEXMODS;		// break out of for loop
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentFactors(&bundle->texMods[tm].wave, &outOffTurb[2], &outOffTurb[3]);
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexMatrix(backEnd.currentEntity->e.shaderTexCoord, matrix);
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexMatrix(bundle->texMods[tm].scroll,
				matrix);
			break;

		case TMOD_SCALE:
			//RB_CalcScaleTexMatrix(bundle->texMods[tm].scale,
			//	matrix);
			outScale[0] = bundle->texMods[tm].scale[0];
			outScale[1] = bundle->texMods[tm].scale[1];
			break;

		case TMOD_STRETCH:
			RB_CalcStretchTexMatrix(&bundle->texMods[tm].wave,
				matrix);
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexMatrix(&bundle->texMods[tm],
				matrix);
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexMatrix(bundle->texMods[tm].rotateSpeed,
				matrix);
			break;

		default:
			ri->Error(ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", bundle->texMods[tm].type, tess.shader->name);
			break;
		}

		switch (bundle->texMods[tm].type)
		{
		case TMOD_NONE:
		case TMOD_TURBULENT:
		default:
			break;

		case TMOD_SCALE:
			break;

		case TMOD_ENTITY_TRANSLATE:
		case TMOD_SCROLL:
		case TMOD_STRETCH:
		case TMOD_TRANSFORM:
		case TMOD_ROTATE:
			outMatrix[0] = matrix[0] * currentmatrix[0] + matrix[2] * currentmatrix[1];
			outMatrix[1] = matrix[1] * currentmatrix[0] + matrix[3] * currentmatrix[1];

			outMatrix[2] = matrix[0] * currentmatrix[2] + matrix[2] * currentmatrix[3];
			outMatrix[3] = matrix[1] * currentmatrix[2] + matrix[3] * currentmatrix[3];

			outOffTurb[0] = matrix[0] * currentmatrix[4] + matrix[2] * currentmatrix[5] + matrix[4];
			outOffTurb[1] = matrix[1] * currentmatrix[4] + matrix[3] * currentmatrix[5] + matrix[5];

			currentmatrix[0] = outMatrix[0];
			currentmatrix[1] = outMatrix[1];
			currentmatrix[2] = outMatrix[2];
			currentmatrix[3] = outMatrix[3];
			currentmatrix[4] = outOffTurb[0];
			currentmatrix[5] = outOffTurb[1];
			break;
		}
	}
}


static void ComputeDeformValues(int *deformGen, vec5_t deformParams)
{
	// u_DeformGen
	*deformGen = DGEN_NONE;
	if(!ShaderRequiresCPUDeforms(tess.shader))
	{
		deformStage_t  *ds;

		// only support the first one
		ds = &tess.shader->deforms[0];

		switch (ds->deformation)
		{
			case DEFORM_WAVE:
				*deformGen = ds->deformationWave.func;

				deformParams[0] = ds->deformationWave.base;
				deformParams[1] = ds->deformationWave.amplitude;
				deformParams[2] = ds->deformationWave.phase;
				deformParams[3] = ds->deformationWave.frequency;
				deformParams[4] = ds->deformationSpread;
				break;

			case DEFORM_BULGE:
				*deformGen = DGEN_BULGE;

				deformParams[0] = 0;
				deformParams[1] = ds->bulgeHeight; // amplitude
				deformParams[2] = ds->bulgeWidth;  // phase
				deformParams[3] = ds->bulgeSpeed;  // frequency
				deformParams[4] = 0;
				break;

			default:
				break;
		}
	}
}

static void ComputeShaderColors( shaderStage_t *pStage, vec4_t baseColor, vec4_t vertColor, int blend, colorGen_t *forceRGBGen, alphaGen_t *forceAlphaGen )
{
	colorGen_t rgbGen = pStage->rgbGen;
	alphaGen_t alphaGen = pStage->alphaGen;

	baseColor[0] =
   	baseColor[1] =
   	baseColor[2] =
   	baseColor[3] = 1.0f;

   	vertColor[0] =
   	vertColor[1] =
   	vertColor[2] =
   	vertColor[3] = 0.0f;

	if ( forceRGBGen != NULL && *forceRGBGen != CGEN_BAD )
	{
		rgbGen = *forceRGBGen;
	}

	if ( forceAlphaGen != NULL && *forceAlphaGen != AGEN_IDENTITY )
	{
		alphaGen = *forceAlphaGen;
	}

	switch ( rgbGen )
	{
		case CGEN_IDENTITY_LIGHTING:
			baseColor[0] =
			baseColor[1] =
			baseColor[2] = tr.identityLight;
			break;
		case CGEN_EXACT_VERTEX:
		case CGEN_EXACT_VERTEX_LIT:
			baseColor[0] =
			baseColor[1] =
			baseColor[2] =
			baseColor[3] = 0.0f;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] =
			vertColor[3] = 1.0f;
			break;
		case CGEN_CONST:
			baseColor[0] = pStage->constantColor[0] / 255.0f;
			baseColor[1] = pStage->constantColor[1] / 255.0f;
			baseColor[2] = pStage->constantColor[2] / 255.0f;
			baseColor[3] = pStage->constantColor[3] / 255.0f;
			break;
		case CGEN_VERTEX:
			baseColor[0] =
			baseColor[1] =
			baseColor[2] =
			baseColor[3] = 0.0f;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] = tr.identityLight;
			vertColor[3] = 1.0f;
			break;
		case CGEN_VERTEX_LIT:
			baseColor[0] =
			baseColor[1] =
			baseColor[2] =
			baseColor[3] = 0.0f;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] =
			vertColor[3] = tr.identityLight;
			break;
		case CGEN_ONE_MINUS_VERTEX:
			baseColor[0] =
			baseColor[1] =
			baseColor[2] = tr.identityLight;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] = -tr.identityLight;
			break;
		case CGEN_FOG:
			{
				if (!r_fog->integer)
					break;

				fog_t		*fog;

				fog = tr.world->fogs + tess.fogNum;

				baseColor[0] = ((unsigned char *)(&fog->colorInt))[0] / 255.0f;
				baseColor[1] = ((unsigned char *)(&fog->colorInt))[1] / 255.0f;
				baseColor[2] = ((unsigned char *)(&fog->colorInt))[2] / 255.0f;
				baseColor[3] = ((unsigned char *)(&fog->colorInt))[3] / 255.0f;
			}
			break;
		case CGEN_WAVEFORM:
			baseColor[0] =
			baseColor[1] =
			baseColor[2] = RB_CalcWaveColorSingle( &pStage->rgbWave );
			break;
		case CGEN_ENTITY:
		case CGEN_LIGHTING_DIFFUSE_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[0] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[0] / 255.0f;
				baseColor[1] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[1] / 255.0f;
				baseColor[2] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[2] / 255.0f;
				baseColor[3] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;

				if ( alphaGen == AGEN_IDENTITY &&
					backEnd.currentEntity->e.shaderRGBA[3] == 255 )
				{
					alphaGen = AGEN_SKIP;
				}
			}
			break;
		case CGEN_ONE_MINUS_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[0] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[0] / 255.0f;
				baseColor[1] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[1] / 255.0f;
				baseColor[2] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[2] / 255.0f;
				baseColor[3] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;
			}
			break;
		case CGEN_LIGHTMAPSTYLE:
			VectorScale4 (styleColors[pStage->lightmapStyle], 1.0f / 255.0f, baseColor);
			break;
		case CGEN_IDENTITY:
		case CGEN_LIGHTING_DIFFUSE:
		case CGEN_BAD:
			break;
	}

	//
	// alphaGen
	//
	switch ( alphaGen )
	{
		case AGEN_SKIP:
			break;
		case AGEN_CONST:
			if ( rgbGen != CGEN_CONST ) {
				baseColor[3] = pStage->constantColor[3] / 255.0f;
				vertColor[3] = 0.0f;
			}
			break;
		case AGEN_WAVEFORM:
			baseColor[3] = RB_CalcWaveAlphaSingle( &pStage->alphaWave );
			vertColor[3] = 0.0f;
			break;
		case AGEN_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[3] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;
			}
			vertColor[3] = 0.0f;
			break;
		case AGEN_ONE_MINUS_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[3] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;
			}
			vertColor[3] = 0.0f;
			break;
		case AGEN_VERTEX:
			if ( rgbGen != CGEN_VERTEX ) {
				baseColor[3] = 0.0f;
				vertColor[3] = 1.0f;
			}
			break;
		case AGEN_ONE_MINUS_VERTEX:
			baseColor[3] = 1.0f;
			vertColor[3] = -1.0f;
			break;
		case AGEN_IDENTITY:
		case AGEN_LIGHTING_SPECULAR:
		case AGEN_PORTAL:
			// Done entirely in vertex program
			baseColor[3] = 1.0f;
			vertColor[3] = 0.0f;
			break;
	}

	if ( forceAlphaGen != NULL )
	{
		*forceAlphaGen = alphaGen;
	}

	if ( forceRGBGen != NULL )
	{
		*forceRGBGen = rgbGen;
	}

	// multiply color by overbrightbits if this isn't a blend
	if (tr.overbrightBits
	 && !((blend & GLS_SRCBLEND_BITS) == GLS_SRCBLEND_DST_COLOR)
	 && !((blend & GLS_SRCBLEND_BITS) == GLS_SRCBLEND_ONE_MINUS_DST_COLOR)
	 && !((blend & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_SRC_COLOR)
	 && !((blend & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR))
	{
		float scale = 1 << tr.overbrightBits;

		baseColor[0] *= scale;
		baseColor[1] *= scale;
		baseColor[2] *= scale;
		vertColor[0] *= scale;
		vertColor[1] *= scale;
		vertColor[2] *= scale;
	}

	// FIXME: find some way to implement this.
#if 0
	// if in greyscale rendering mode turn all color values into greyscale.
	if(r_greyscale->integer)
	{
		int scale;

		for(i = 0; i < tess.numVertexes; i++)
		{
			scale = (tess.svars.colors[i][0] + tess.svars.colors[i][1] + tess.svars.colors[i][2]) / 3;
			tess.svars.colors[i][0] = tess.svars.colors[i][1] = tess.svars.colors[i][2] = scale;
		}
	}
#endif
}


static void ComputeFogValues(vec4_t fogDistanceVector, vec4_t fogDepthVector, float *eyeT)
{
	// from RB_CalcFogTexCoords()
	fog_t  *fog;
	vec3_t  local;

	if (!tess.fogNum)
		return;

	if (!r_fog->integer)
		return;

	fog = tr.world->fogs + tess.fogNum;

	VectorSubtract( backEnd.ori.origin, backEnd.viewParms.ori.origin, local );
	fogDistanceVector[0] = -backEnd.ori.modelMatrix[2];
	fogDistanceVector[1] = -backEnd.ori.modelMatrix[6];
	fogDistanceVector[2] = -backEnd.ori.modelMatrix[10];
	fogDistanceVector[3] = DotProduct( local, backEnd.viewParms.ori.axis[0] );

	// scale the fog vectors based on the fog's thickness
	VectorScale4(fogDistanceVector, fog->tcScale, fogDistanceVector);

	// rotate the gradient vector for this orientation
	if ( fog->hasSurface ) {
		fogDepthVector[0] = fog->surface[0] * backEnd.ori.axis[0][0] +
			fog->surface[1] * backEnd.ori.axis[0][1] + fog->surface[2] * backEnd.ori.axis[0][2];
		fogDepthVector[1] = fog->surface[0] * backEnd.ori.axis[1][0] +
			fog->surface[1] * backEnd.ori.axis[1][1] + fog->surface[2] * backEnd.ori.axis[1][2];
		fogDepthVector[2] = fog->surface[0] * backEnd.ori.axis[2][0] +
			fog->surface[1] * backEnd.ori.axis[2][1] + fog->surface[2] * backEnd.ori.axis[2][2];
		fogDepthVector[3] = -fog->surface[3] + DotProduct( backEnd.ori.origin, fog->surface );

		*eyeT = DotProduct( backEnd.ori.viewOrigin, fogDepthVector ) + fogDepthVector[3];
	} else {
		*eyeT = 1;	// non-surface fog always has eye inside
	}
}


static void ComputeFogColorMask( shaderStage_t *pStage, vec4_t fogColorMask )
{
	if (r_fog->integer)
	{
		switch(pStage->adjustColorsForFog)
		{
		case ACFF_MODULATE_RGB:
			fogColorMask[0] =
				fogColorMask[1] =
				fogColorMask[2] = 1.0f;
			fogColorMask[3] = 0.0f;
			break;
		case ACFF_MODULATE_ALPHA:
			fogColorMask[0] =
				fogColorMask[1] =
				fogColorMask[2] = 0.0f;
			fogColorMask[3] = 1.0f;
			break;
		case ACFF_MODULATE_RGBA:
			fogColorMask[0] =
				fogColorMask[1] =
				fogColorMask[2] =
				fogColorMask[3] = 1.0f;
			break;
		default:
			fogColorMask[0] =
				fogColorMask[1] =
				fogColorMask[2] =
				fogColorMask[3] = 0.0f;
			break;
		}
	}
	else
	{
		fogColorMask[0] =
			fogColorMask[1] =
			fogColorMask[2] =
			fogColorMask[3] = 0.0f;
	}
}

static void ProjectPshadowVBOGLSL( void ) {
#ifdef __PSHADOWS__
	int		l;
	vec3_t	origin;
	float	radius;

	int deformGen;
	vec5_t deformParams;

	shaderCommands_t *input = &tess;

	if ( !backEnd.refdef.num_pshadows ) {
		return;
	}

	ComputeDeformValues(&deformGen, deformParams);

	for ( l = 0 ; l < backEnd.refdef.num_pshadows ; l++ ) {
		pshadow_t	*ps;
		shaderProgram_t *sp;
		vec4_t vector;

		//if ( !( tess.pshadowBits & ( 1 << l ) ) ) {
		//	continue;	// this surface definately doesn't have any of this shadow
		//}

		ps = &backEnd.refdef.pshadows[l];
		VectorCopy( ps->lightOrigin, origin );
		radius = ps->lightRadius;

		sp = &tr.pshadowShader;

		GLSL_BindProgram(sp);

		GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

		VectorCopy(origin, vector);
		vector[3] = 1.0f;
		GLSL_SetUniformVec4(sp, UNIFORM_LIGHTORIGIN, vector);

		VectorScale(ps->lightViewAxis[0], 1.0f / ps->viewRadius, vector);
		GLSL_SetUniformVec3(sp, UNIFORM_LIGHTFORWARD, vector);

		VectorScale(ps->lightViewAxis[1], 1.0f / ps->viewRadius, vector);
		GLSL_SetUniformVec3(sp, UNIFORM_LIGHTRIGHT, vector);

		VectorScale(ps->lightViewAxis[2], 1.0f / ps->viewRadius, vector);
		GLSL_SetUniformVec3(sp, UNIFORM_LIGHTUP, vector);

		GLSL_SetUniformFloat(sp, UNIFORM_LIGHTRADIUS, radius);

		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );

		GL_BindToTMU( tr.pshadowMaps[l], TB_DIFFUSEMAP );

		//
		// draw
		//

		if (input->multiDrawPrimitives)
		{
			R_DrawMultiElementsVBO(input->multiDrawPrimitives, input->multiDrawMinIndex, input->multiDrawMaxIndex, input->multiDrawNumIndexes, input->multiDrawFirstIndex, input->numVertexes, qfalse);
		}
		else
		{
			R_DrawElementsVBO(input->numIndexes, input->firstIndex, input->minIndex, input->maxIndex, input->numVertexes, qfalse);
		}

		backEnd.pc.c_totalIndexes += tess.numIndexes;
		//backEnd.pc.c_dlightIndexes += tess.numIndexes;
	}
#endif
}



/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	fog_t		*fog;
	vec4_t  color;
	vec4_t	fogDistanceVector, fogDepthVector = {0, 0, 0, 0};
	float	eyeT = 0;
	shaderProgram_t *sp;

	int deformGen;
	vec5_t deformParams;

	ComputeDeformValues(&deformGen, deformParams);

	{
		int index = 0;

		if (deformGen != DGEN_NONE)
			index |= FOGDEF_USE_DEFORM_VERTEXES;

		if (glState.vertexAnimation)
			index |= FOGDEF_USE_VERTEX_ANIMATION;

		if (glState.skeletalAnimation)
			index |= FOGDEF_USE_SKELETAL_ANIMATION;

		sp = &tr.fogShader[index];
	}

	backEnd.pc.c_fogDraws++;

	GLSL_BindProgram(sp);

	fog = tr.world->fogs + tess.fogNum;

	GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	GLSL_SetUniformMatrix16(sp, UNIFORM_BONE_MATRICES, &glState.boneMatrices[0][0], glState.numBones);
	GLSL_SetUniformFloat(sp, UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);

	GLSL_SetUniformInt(sp, UNIFORM_DEFORMGEN, deformGen);
	if (deformGen != DGEN_NONE)
	{
		GLSL_SetUniformFloat5(sp, UNIFORM_DEFORMPARAMS, deformParams);
		GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);
	}

	color[0] = ((unsigned char *)(&fog->colorInt))[0] / 255.0f;
	color[1] = ((unsigned char *)(&fog->colorInt))[1] / 255.0f;
	color[2] = ((unsigned char *)(&fog->colorInt))[2] / 255.0f;
	color[3] = ((unsigned char *)(&fog->colorInt))[3] / 255.0f;
	GLSL_SetUniformVec4(sp, UNIFORM_COLOR, color);

	ComputeFogValues(fogDistanceVector, fogDepthVector, &eyeT);

	GLSL_SetUniformVec4(sp, UNIFORM_FOGDISTANCE, fogDistanceVector);
	GLSL_SetUniformVec4(sp, UNIFORM_FOGDEPTH, fogDepthVector);
	GLSL_SetUniformFloat(sp, UNIFORM_FOGEYET, eyeT);

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	if (tess.multiDrawPrimitives)
	{
		shaderCommands_t *input = &tess;
		R_DrawMultiElementsVBO(input->multiDrawPrimitives, input->multiDrawMinIndex, input->multiDrawMaxIndex, input->multiDrawNumIndexes, input->multiDrawFirstIndex, input->numVertexes, qfalse);
	}
	else
	{
		R_DrawElementsVBO(tess.numIndexes, tess.firstIndex, tess.minIndex, tess.maxIndex, tess.numVertexes, qfalse);
	}
}


static unsigned int RB_CalcShaderVertexAttribs( const shader_t *shader )
{
	unsigned int vertexAttribs = shader->vertexAttribs;

	if(glState.vertexAnimation)
	{
		//vertexAttribs &= ~ATTR_COLOR;
		vertexAttribs |= ATTR_POSITION2;
		if (vertexAttribs & ATTR_NORMAL)
		{
			vertexAttribs |= ATTR_NORMAL2;
			vertexAttribs |= ATTR_TANGENT2;
		}
	}

	if (glState.skeletalAnimation)
	{
		vertexAttribs |= ATTR_BONE_WEIGHTS;
		vertexAttribs |= ATTR_BONE_INDEXES;
	}

	return vertexAttribs;
}

static void UpdateTexCoords ( const shaderStage_t *stage )
{
	uint32_t updateAttribs = 0;
	if ( stage->bundle[0].image[0] != NULL )
	{
		switch (stage->bundle[0].tcGen)
		{
			case TCGEN_LIGHTMAP:
			case TCGEN_LIGHTMAP1:
			case TCGEN_LIGHTMAP2:
			case TCGEN_LIGHTMAP3:
			{
				int newLightmapIndex = stage->bundle[0].tcGen - TCGEN_LIGHTMAP + 1;
				if (newLightmapIndex != glState.vertexAttribsTexCoordOffset[0])
				{
					glState.vertexAttribsTexCoordOffset[0] = newLightmapIndex;
					updateAttribs |= ATTR_TEXCOORD0;
				}

				break;
			}

			case TCGEN_TEXTURE:
				if (glState.vertexAttribsTexCoordOffset[0] != 0)
				{
					glState.vertexAttribsTexCoordOffset[0] = 0;
					updateAttribs |= ATTR_TEXCOORD0;
				}
				break;

			default:
				break;
		}
	}

	if ( stage->bundle[TB_LIGHTMAP].image[0] != NULL )
	{
		switch (stage->bundle[TB_LIGHTMAP].tcGen)
		{
			case TCGEN_LIGHTMAP:
			case TCGEN_LIGHTMAP1:
			case TCGEN_LIGHTMAP2:
			case TCGEN_LIGHTMAP3:
			{
				int newLightmapIndex = stage->bundle[TB_LIGHTMAP].tcGen - TCGEN_LIGHTMAP + 1;
				if (newLightmapIndex != glState.vertexAttribsTexCoordOffset[1])
				{
					glState.vertexAttribsTexCoordOffset[1] = newLightmapIndex;
					updateAttribs |= ATTR_TEXCOORD1;
				}

				break;
			}

			case TCGEN_TEXTURE:
				if (glState.vertexAttribsTexCoordOffset[1] != 0)
				{
					glState.vertexAttribsTexCoordOffset[1] = 0;
					updateAttribs |= ATTR_TEXCOORD1;
				}
				break;

			default:
				break;
		}
	}

	if ( updateAttribs != 0 )
	{
		GLSL_UpdateTexCoordVertexAttribPointers (updateAttribs);
	}
}


int			overlaySwayTime = 0;
qboolean	overlaySwayDown = qfalse;
float		overlaySway = 0.0;

void RB_AdvanceOverlaySway ( void )
{
	if (overlaySwayTime > ri->Milliseconds())
		return;

	if (overlaySwayDown)
	{
		overlaySway -= 0.00016;

		if (overlaySway < 0.0)
		{
			overlaySway += 0.00032;
			overlaySwayDown = qfalse;
		}
	}
	else
	{
		overlaySway += 0.00016;

		if (overlaySway > 0.0016)
		{
			overlaySway -= 0.00032;
			overlaySwayDown = qtrue;
		}
	}

	overlaySwayTime = ri->Milliseconds() + 50;
}

extern float MAP_WATER_LEVEL;
extern float MAP_INFO_MAXSIZE;
extern vec3_t  MAP_INFO_MINS;
extern vec3_t  MAP_INFO_MAXS;

void RB_SetMaterialBasedProperties(shaderProgram_t *sp, shaderStage_t *pStage, int stageNum)
{
	vec4_t	local1, local3, local4, local5;
	float	specularScale = 1.0;
	float	materialType = 0.0;
	float   parallaxScale = 1.0;
	float	cubemapScale = 0.0;
	float	isMetalic = 0.0;
	float	hasOverlay = 0.0;
	float	doSway = 0.0;
	float	phongFactor = r_blinnPhong->value;
	float	hasSteepMap = 0;
	float	hasSteepMap2 = 0;
	float	hasSplatMap1 = 0;
	float	hasSplatMap2 = 0;
	float	hasSplatMap3 = 0;
	float	hasSplatMap4 = 0;
	float	hasNormalMap = 0;

	if (!backEnd.depthFill && !(tr.viewParms.flags & VPF_SHADOWPASS))
	{
		if (r_normalMapping->integer >= 2
			&& pStage->bundle[TB_NORMALMAP].image[0]
			&& pStage->bundle[TB_NORMALMAP].image[0] != tr.whiteImage
			&& pStage->bundle[TB_NORMALMAP].image[0] != tr.blackImage)
		{
			hasNormalMap = 1.0;
		}

		if (pStage->bundle[TB_OVERLAYMAP].image[0]
			&& pStage->bundle[TB_OVERLAYMAP].image[0] != tr.whiteImage)
		{
			hasOverlay = 1.0;
		}

		if (pStage->bundle[TB_STEEPMAP].image[0]
			&& pStage->bundle[TB_STEEPMAP].image[0] != tr.whiteImage)
		{
			hasSteepMap = 1.0;
		}

		if (pStage->bundle[TB_STEEPMAP2].image[0]
			&& pStage->bundle[TB_STEEPMAP2].image[0] != tr.whiteImage)
		{
			hasSteepMap2 = 1.0;
		}

		if ((pStage->bundle[TB_SPLATMAP1].image[0]
			&& pStage->bundle[TB_SPLATMAP1].image[0] != tr.whiteImage))
		{
			hasSplatMap1 = 1;
		}

		if ((pStage->bundle[TB_SPLATMAP2].image[0]
			&& pStage->bundle[TB_SPLATMAP2].image[0] != tr.whiteImage))
		{
			hasSplatMap2 = 1;
		}

		if ((pStage->bundle[TB_SPLATMAP3].image[0]
			&& pStage->bundle[TB_SPLATMAP3].image[0] != tr.whiteImage))
		{
			hasSplatMap3 = 1;
		}

		/*if ((pStage->bundle[TB_SPLATMAP4].image[0]
			&& pStage->bundle[TB_SPLATMAP4].image[0] != tr.whiteImage))
		{
			hasSplatMap4 = 1;
		}*/

		if (pStage->isWater && r_glslWater->integer && WATER_ENABLED)
		{
			specularScale = 1.5;
			cubemapScale = 1.0;
			materialType = (float)MATERIAL_WATER;
			parallaxScale = 2.0;
		}
		else
		{
			switch( tess.shader->surfaceFlags & MATERIAL_MASK )
			{
			case MATERIAL_WATER:			// 13			// light covering of water on a surface
				specularScale = 1.0;
				cubemapScale = 1.0;
				materialType = (float)MATERIAL_WATER;
				parallaxScale = 1.5;
				break;
			case MATERIAL_SHORTGRASS:		// 5			// manicured lawn
				specularScale = 0.75;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_SHORTGRASS;
				parallaxScale = 1.5;
				//phongFactor = -phongFactor; // no blinn phong on grassy terrains (to stop the joins being so obvious)
				break;
			case MATERIAL_LONGGRASS:		// 6			// long jungle grass
				specularScale = 0.75;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_LONGGRASS;
				parallaxScale = 1.5;
				//phongFactor = -phongFactor; // no blinn phong on grassy terrains (to stop the joins being so obvious)
				break;
			case MATERIAL_SAND:				// 8			// sandy beach
				specularScale = 0.65;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_SAND;
				parallaxScale = 1.5;
				break;
			case MATERIAL_CARPET:			// 27			// lush carpet
				specularScale = 0.25;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_CARPET;
				parallaxScale = 1.5;
				break;
			case MATERIAL_GRAVEL:			// 9			// lots of small stones
				specularScale = 0.30;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_GRAVEL;
				parallaxScale = 1.5;
				break;
			case MATERIAL_ROCK:				// 23			//
				specularScale = 0.22;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_ROCK;
				parallaxScale = 1.5;
				break;
			case MATERIAL_TILES:			// 26			// tiled floor
				specularScale = 0.56;
				cubemapScale = 0.25;
				materialType = (float)MATERIAL_TILES;
				parallaxScale = 1.5;
				break;
			case MATERIAL_SOLIDWOOD:		// 1			// freshly cut timber
				specularScale = 0.40;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_SOLIDWOOD;
				parallaxScale = 1.5;
				break;
			case MATERIAL_HOLLOWWOOD:		// 2			// termite infested creaky wood
				specularScale = 0.35;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_HOLLOWWOOD;
				parallaxScale = 1.5;
				break;
			case MATERIAL_SOLIDMETAL:		// 3			// solid girders
				specularScale = 0.98;
				cubemapScale = 0.98;
				materialType = (float)MATERIAL_SOLIDMETAL;
				parallaxScale = 1.5;
				isMetalic = 1.0;
				break;
			case MATERIAL_HOLLOWMETAL:		// 4			// hollow metal machines -- UQ1: Used for weapons to force lower parallax and high reflection...
				specularScale = 1.0;
				cubemapScale = 1.0;
				materialType = (float)MATERIAL_HOLLOWMETAL;
				parallaxScale = 1.5;
				isMetalic = 1.0;
				break;
			case MATERIAL_DRYLEAVES:		// 19			// dried up leaves on the floor
				specularScale = 0.35;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_DRYLEAVES;
				parallaxScale = 0.0;
				break;
			case MATERIAL_GREENLEAVES:		// 20			// fresh leaves still on a tree
				specularScale = 0.85;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_GREENLEAVES;
				parallaxScale = 0.0; // GreenLeaves should NEVER be parallaxed.. It's used for surfaces with an alpha channel and parallax screws it up...
				break;
			case MATERIAL_FABRIC:			// 21			// Cotton sheets
				specularScale = 0.45;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_FABRIC;
				parallaxScale = 1.5;
				break;
			case MATERIAL_CANVAS:			// 22			// tent material
				specularScale = 0.35;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_CANVAS;
				parallaxScale = 1.5;
				break;
			case MATERIAL_MARBLE:			// 12			// marble floors
				specularScale = 0.65;
				cubemapScale = 0.6;
				materialType = (float)MATERIAL_MARBLE;
				parallaxScale = 1.5;
				break;
			case MATERIAL_SNOW:				// 14			// freshly laid snow
				specularScale = 0.75;
				cubemapScale = 0.35;
				materialType = (float)MATERIAL_SNOW;
				parallaxScale = 1.5;
				break;
			case MATERIAL_MUD:				// 17			// wet soil
				specularScale = 0.25;
				cubemapScale = 0.1;
				materialType = (float)MATERIAL_MUD;
				parallaxScale = 1.5;
				break;
			case MATERIAL_DIRT:				// 7			// hard mud
				specularScale = 0.15;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_DIRT;
				parallaxScale = 1.5;
				break;
			case MATERIAL_CONCRETE:			// 11			// hardened concrete pavement
				specularScale = 0.375;
				cubemapScale = 0.1;
				materialType = (float)MATERIAL_CONCRETE;
				parallaxScale = 1.5;
				break;
			case MATERIAL_FLESH:			// 16			// hung meat, corpses in the world
				specularScale = 0.25;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_FLESH;
				parallaxScale = 1.5;
				break;
			case MATERIAL_RUBBER:			// 24			// hard tire like rubber
				specularScale = 0.25;
				cubemapScale = 0.0;
				materialType = (float)MATERIAL_RUBBER;
				parallaxScale = 1.5;
				break;
			case MATERIAL_PLASTIC:			// 25			//
				specularScale = 0.58;
				cubemapScale = 0.4;
				materialType = (float)MATERIAL_PLASTIC;
				parallaxScale = 1.5;
				break;
			case MATERIAL_PLASTER:			// 28			// drywall style plaster
				specularScale = 0.3;
				cubemapScale = 0.1;
				materialType = (float)MATERIAL_PLASTER;
				parallaxScale = 1.5;
				break;
			case MATERIAL_SHATTERGLASS:		// 29			// glass with the Crisis Zone style shattering
				specularScale = 0.93;
				cubemapScale = 0.7;
				materialType = (float)MATERIAL_SHATTERGLASS;
				parallaxScale = 1.0;
				break;
			case MATERIAL_ARMOR:			// 30			// body armor
				specularScale = 0.5;
				cubemapScale = 0.6;
				materialType = (float)MATERIAL_ARMOR;
				parallaxScale = 1.5;
				isMetalic = 1.0;
				break;
			case MATERIAL_ICE:				// 15			// packed snow/solid ice
				specularScale = 0.75;
				cubemapScale = 0.7;
				materialType = (float)MATERIAL_ICE;
				parallaxScale = 2.0;
				break;
			case MATERIAL_GLASS:			// 10			//
				specularScale = 0.95;
				cubemapScale = 0.7;
				materialType = (float)MATERIAL_GLASS;
				parallaxScale = 1.0;
				break;
			case MATERIAL_BPGLASS:			// 18			// bulletproof glass
				specularScale = 0.93;
				cubemapScale = 0.73;
				materialType = (float)MATERIAL_BPGLASS;
				parallaxScale = 1.0;
				break;
			case MATERIAL_COMPUTER:			// 31			// computers/electronic equipment
				specularScale = 0.92;
				cubemapScale = 0.92;
				materialType = (float)MATERIAL_COMPUTER;
				parallaxScale = 1.5;
				break;
			default:
				specularScale = 0.15;
				cubemapScale = 0.0;
				materialType = (float)0.0;
				parallaxScale = 1.0;
				break;
			}
		}

		// Shader overrides material...
		if (pStage->cubeMapScale > 0.0)
		{
			cubemapScale = pStage->cubeMapScale;
		}

		if (pStage->isFoliage)
		{
			doSway = 0.7;
		}

		if (tess.shader == tr.sunShader)
		{// SPECIAL MATERIAL TYPE FOR SUN
			materialType = 1025.0;
		}

		VectorSet4(local1, parallaxScale*r_parallaxScale->value, (float)pStage->hasSpecular, specularScale, materialType);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL1, local1);
		VectorSet4(local3, 0.0, 0.0, r_cubemapCullRange->value, cubemapScale);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL3, local3);
		VectorSet4(local4, hasNormalMap, isMetalic, 0.0/*(float)pStage->hasRealSubsurfaceMap*/, doSway);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL4, local4);
		VectorSet4(local5, hasOverlay, overlaySway, phongFactor, hasSteepMap);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL5, local5);

		vec4_t local6;
		VectorSet4(local6, r_sunlightSpecular->value, hasSteepMap2, MAP_INFO_MAXSIZE, MAP_WATER_LEVEL);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL6,  local6);

		vec4_t local7;
		VectorSet4(local7, hasSplatMap1, hasSplatMap2, hasSplatMap3, hasSplatMap4);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL7,  local7);

		vec4_t local8;
		VectorSet4(local8, (float)stageNum, r_glowStrength->value, MAP_INFO_MAXS[2], r_showsplat->value);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL8, local8);
	}
	else
	{// Don't waste time on unneeded stuff... Absolute minimum shader complexity...
		specularScale = 0.0;
		cubemapScale = 0.0;
		materialType = (float)0.0;
		parallaxScale = 0.0;

		if (tess.shader == tr.sunShader)
		{// SPECIAL MATERIAL TYPE FOR SUN
			materialType = 1025.0;
		}
		else
		{
			materialType = ( tess.shader->surfaceFlags & MATERIAL_MASK );
		}

		if (pStage->isFoliage)
		{
			doSway = 0.7;
		}

		VectorSet4(local1, 0.0, 0.0, 0.0, materialType);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL1, local1);
		//GLSL_SetUniformVec4(sp, UNIFORM_LOCAL2, pStage->subsurfaceExtinctionCoefficient);
		VectorSet4(local3, 0.0, 0.0, 0.0, 0.0);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL3, local3);
		VectorSet4(local4, 0.0, 0.0, 0.0, doSway);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL4, local4);
		VectorSet4(local5, 0.0, overlaySway, 0.0, 0.0);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL5, local5);

		vec4_t local6;
		VectorSet4(local6, 0.0, 0.0, MAP_INFO_MAXSIZE, MAP_WATER_LEVEL);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL6,  local6);

		vec4_t local7;
		VectorSet4(local7, 0.0, 0.0, 0.0, 0.0);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL7,  local7);

		vec4_t local8;
		VectorSet4(local8, (float)stageNum, r_glowStrength->value, 0.0, 0.0);
		GLSL_SetUniformVec4(sp, UNIFORM_LOCAL8, local8);
	}

	vec4_t specMult;

	if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
	{// Don't waste time on speculars...
		VectorSet4(specMult, 0.0, 0.0, 0.0, 0.0);
		GLSL_SetUniformVec4(sp, UNIFORM_SPECULARSCALE, specMult);
	}
	else if (pStage->specularScale[0] + pStage->specularScale[1] + pStage->specularScale[2] + pStage->specularScale[3] != 0.0)
	{// Shader Specified...
		GLSL_SetUniformVec4(sp, UNIFORM_SPECULARSCALE, pStage->specularScale);
	}
	else // Material Defaults...
	{
		VectorSet4(specMult, specularScale, specularScale, specularScale, 1.0);

		if (( tess.shader->surfaceFlags & MATERIAL_MASK ) == MATERIAL_ARMOR /* ARMOR */
			|| ( tess.shader->surfaceFlags & MATERIAL_MASK ) == MATERIAL_PLASTIC /* PLASTIC */
			|| ( tess.shader->surfaceFlags & MATERIAL_MASK ) == MATERIAL_MARBLE /* MARBLE */)
		{// Armor, plastic, and marble should remain somewhat shiny...
			specMult[0] = 0.333;
			specMult[1] = 0.333;
			specMult[2] = 0.333;
			GLSL_SetUniformVec4(sp, UNIFORM_SPECULARSCALE, specMult);
		}
		else if ( !(isMetalic > 0.0)
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_SOLIDMETAL
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_HOLLOWMETAL
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_GLASS /* GLASS */
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_SHATTERGLASS /* SHATTERGLASS */
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_BPGLASS /* BPGLASS */
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_COMPUTER /* COMPUTER */
			&& ( tess.shader->surfaceFlags & MATERIAL_MASK ) != MATERIAL_ICE /* ICE */)
		{// Only if not metalic... Metals should remain nice and shiny...
			specMult[0] *= 0.04;
			specMult[1] *= 0.04;
			specMult[2] *= 0.04;
			GLSL_SetUniformVec4(sp, UNIFORM_SPECULARSCALE, specMult);
		}
		else
		{
			GLSL_SetUniformVec4(sp, UNIFORM_SPECULARSCALE, specMult);
		}
	}

	GLSL_SetUniformFloat(sp, UNIFORM_TIME, backEnd.refdef.floatTime);
}

void RB_SetStageImageDimensions(shaderProgram_t *sp, shaderStage_t *pStage)
{
	vec2_t dimensions = { 0.0 };

	if (pStage->bundle[TB_DIFFUSEMAP].image[0])
	{
		dimensions[0] = pStage->bundle[TB_DIFFUSEMAP].image[0]->width;
		dimensions[1] = pStage->bundle[TB_DIFFUSEMAP].image[0]->height;
	}

	GLSL_SetUniformVec2(sp, UNIFORM_DIMENSIONS, dimensions);
}

qboolean RB_ShouldUseTesselation ( shaderCommands_t *input )
{
	if (/*backEnd.currentEntity != &tr.worldEntity &&*/ backEnd.currentEntity != &backEnd.entity2D)
		return qtrue;

	/*
	int materialType = tess.shader->surfaceFlags & MATERIAL_MASK);

	if ( materialType == MATERIAL_SHORTGRASS
		|| materialType == MATERIAL_LONGGRASS
		|| materialType == MATERIAL_SAND
		//|| materialType == MATERIAL_ROCK
		|| materialType == MATERIAL_ICE)
		return qtrue;
	*/

	for (int stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t *pStage = input->xstages[stage];

		if (!pStage) break;

		if (pStage->bundle[TB_STEEPMAP].image[0]
			|| pStage->bundle[TB_STEEPMAP2].image[0]
			|| pStage->bundle[TB_SPLATMAP1].image[0]
			|| pStage->bundle[TB_SPLATMAP2].image[0]
			|| pStage->bundle[TB_SPLATMAP3].image[0])
		{
			return qtrue;
		}
	}

	return qfalse;
}

float RB_GetTesselationAlphaLevel ( int materialType )
{
	float tessAlphaLevel = r_tesselationAlpha->value;
	/*
	switch( materialType )
	{
	case MATERIAL_SHORTGRASS:
	case MATERIAL_LONGGRASS:
	case MATERIAL_SAND:
	//case MATERIAL_ROCK:
	case MATERIAL_ICE:
		tessAlphaLevel = 10.0 * r_tesselationAlpha->value;
		break;
	default:
		tessAlphaLevel = 0.001;
		break;
	}
	*/
	tessAlphaLevel = 10.0 * r_tesselationAlpha->value;

	return tessAlphaLevel;
}

float RB_GetTesselationInnerLevel ( int materialType )
{
	return r_tesselationLevel->value;

#if 0
	float tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);

	switch( materialType )
	{
	case MATERIAL_SHORTGRASS:		// 5			// manicured lawn
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_LONGGRASS:		// 6			// long jungle grass
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_SAND:				// 8			// sandy beach
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.1, 2.25);
		break;
	/*case MATERIAL_CARPET:			// 27			// lush carpet
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.3, 2.25);
		break;
	case MATERIAL_GRAVEL:			// 9			// lots of small stones
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_ROCK:				// 23			//
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.5, 2.25);
		break;
	case MATERIAL_TILES:			// 26			// tiled floor
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.3, 2.25);
		break;
	case MATERIAL_SOLIDWOOD:		// 1			// freshly cut timber
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, r_tesselationLevel->value);
		break;
	case MATERIAL_HOLLOWWOOD:		// 2			// termite infested creaky wood
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, r_tesselationLevel->value);
		break;
	case MATERIAL_SOLIDMETAL:		// 3			// solid girders
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.5, 2.25);
		break;
	case MATERIAL_HOLLOWMETAL:		// 4			// hollow metal machines -- UQ1: Used for weapons to force lower parallax and high reflection...
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.3, 2.25);
		break;
	case MATERIAL_DRYLEAVES:		// 19			// dried up leaves on the floor
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_GREENLEAVES:		// 20			// fresh leaves still on a tree
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_FABRIC:			// 21			// Cotton sheets
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, r_tesselationLevel->value);
		break;
	case MATERIAL_CANVAS:			// 22			// tent material
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_MARBLE:			// 12			// marble floors
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.5, 2.25);
		break;
	case MATERIAL_SNOW:				// 14			// freshly laid snow
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.1, 2.25);
		break;
	case MATERIAL_MUD:				// 17			// wet soil
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.1, 2.25);
		break;
	case MATERIAL_DIRT:				// 7			// hard mud
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.1, 2.25);
		break;
	case MATERIAL_CONCRETE:			// 11			// hardened concrete pavement
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.3, 2.25);
		break;
	case MATERIAL_FLESH:			// 16			// hung meat, corpses in the world
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, r_tesselationLevel->value);
		break;
	case MATERIAL_RUBBER:			// 24			// hard tire like rubber
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);
		break;
	case MATERIAL_PLASTIC:			// 25			//
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.5, 2.25);
		break;
	case MATERIAL_PLASTER:			// 28			// drywall style plaster
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.3, 2.25);
		break;
	case MATERIAL_ARMOR:			// 30			// body armor
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, r_tesselationLevel->value);
		break;*/
	case MATERIAL_ICE:				// 15			// packed snow/solid ice
		tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value * 0.3, r_tesselationLevel->value);
		break;
	case MATERIAL_WATER:			// 13			// light covering of water on a surface
	case MATERIAL_SHATTERGLASS:		// 29			// glass with the Crisis Zone style shattering
	case MATERIAL_GLASS:			// 10			//
	case MATERIAL_BPGLASS:			// 18			// bulletproof glass
	case MATERIAL_COMPUTER:			// 31			// computers/electronic equipment
	default:
		tessInnerLevel = 1.0;
		break;
	}
#endif

	float tessInnerLevel = Q_clamp(1.0, r_tesselationLevel->value, 2.25);

	return tessInnerLevel;
}


extern qboolean R_SurfaceIsAllowedFoliage( int materialType );

qboolean RB_ShouldUseGeometryGrass (int materialType )
{
	if ( materialType <= MATERIAL_NONE )
	{
		return qfalse;
	}

	if ( materialType == MATERIAL_SHORTGRASS
		|| materialType == MATERIAL_LONGGRASS )
	{
		return qtrue;
	}

	if ( R_SurfaceIsAllowedFoliage( materialType ) )
	{// *sigh* due to surfaceFlags mixing materials with other flags, we need to do it this way...
		return qtrue;
	}

	return qfalse;
}

qboolean RB_ShouldUseGeometryPebbles(int materialType)
{
	if (materialType <= MATERIAL_NONE)
	{
		return qfalse;
	}

	if (materialType == MATERIAL_SAND || materialType == MATERIAL_DIRT || materialType == MATERIAL_GRAVEL || materialType == MATERIAL_MUD)
	{
		return qtrue;
	}

	return qfalse;
}

bool theOriginalGluInvertMatrix(const float m[16], float invOut[16])
{
	double inv[16], det;
	int i;

	inv[0] = m[5] * m[10] * m[15] -
		m[5] * m[11] * m[14] -
		m[9] * m[6] * m[15] +
		m[9] * m[7] * m[14] +
		m[13] * m[6] * m[11] -
		m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] +
		m[4] * m[11] * m[14] +
		m[8] * m[6] * m[15] -
		m[8] * m[7] * m[14] -
		m[12] * m[6] * m[11] +
		m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] -
		m[4] * m[11] * m[13] -
		m[8] * m[5] * m[15] +
		m[8] * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] +
		m[4] * m[10] * m[13] +
		m[8] * m[5] * m[14] -
		m[8] * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] +
		m[1] * m[11] * m[14] +
		m[9] * m[2] * m[15] -
		m[9] * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
		m[0] * m[11] * m[14] -
		m[8] * m[2] * m[15] +
		m[8] * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] +
		m[0] * m[11] * m[13] +
		m[8] * m[1] * m[15] -
		m[8] * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
		m[0] * m[10] * m[13] -
		m[8] * m[1] * m[14] +
		m[8] * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] -
		m[1] * m[7] * m[14] -
		m[5] * m[2] * m[15] +
		m[5] * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] +
		m[0] * m[7] * m[14] +
		m[4] * m[2] * m[15] -
		m[4] * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] -
		m[0] * m[7] * m[13] -
		m[4] * m[1] * m[15] +
		m[4] * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] +
		m[0] * m[6] * m[13] +
		m[4] * m[1] * m[14] -
		m[4] * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0)
	{
		for (i = 0; i < 16; i++)
			invOut[i] = 0;

		return false;
	}

	det = 1.0 / det;

	for (i = 0; i < 16; i++)
		invOut[i] = inv[i] * det;

	return true;
}

matrix_t MATRIX_TRANS, MATRIX_MODEL, MATRIX_MVP, MATRIX_INVTRANS, MATRIX_NORMAL, MATRIX_VP, MATRIX_INVMV;

void RB_UpdateMatrixes ( void )
{
	if (!MATRIX_UPDATE) return;

	//theOriginalGluInvertMatrix((const float *)glState.modelviewProjection, (float *)MATRIX_INVTRANS);
	Matrix16SimpleInverse(backEnd.viewParms.projectionMatrix, MATRIX_NORMAL);

	MATRIX_UPDATE = qfalse;
}

int			NUM_CLOSE_LIGHTS = 0;
int			CLOSEST_LIGHTS[MAX_DEFERRED_LIGHTS] = {0};
vec3_t		CLOSEST_LIGHTS_POSITIONS[MAX_DEFERRED_LIGHTS] = {0};
vec2_t		CLOSEST_LIGHTS_SCREEN_POSITIONS[MAX_DEFERRED_LIGHTS];
float		CLOSEST_LIGHTS_DISTANCES[MAX_DEFERRED_LIGHTS] = {0};
float		CLOSEST_LIGHTS_HEIGHTSCALES[MAX_DEFERRED_LIGHTS] = { 0 };
vec3_t		CLOSEST_LIGHTS_COLORS[MAX_DEFERRED_LIGHTS] = {0};

extern void WorldCoordToScreenCoord(vec3_t origin, float *x, float *y);
extern qboolean Volumetric_Visible(vec3_t from, vec3_t to, qboolean isSun);
extern void Volumetric_Trace(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const int passEntityNum, const int contentmask);

qboolean Light_Visible(vec3_t from, vec3_t to, qboolean isSun, float radius)
{

	if (!R_inPVS(tr.refdef.vieworg, to, tr.refdef.areamask))
	{// Not in PVS, don't add it...
		return qfalse;
	}

	//if (isSun)
		return qtrue;

	float scanRad = radius;
	if (scanRad < 0) scanRad = -scanRad;
	scanRad *= 4.0;

	trace_t trace;

	Volumetric_Trace(&trace, from, NULL, NULL, to, -1, (CONTENTS_SOLID | CONTENTS_TERRAIN));

	if (!(trace.fraction != 1.0 && Distance(trace.endpos, to) > scanRad))
	{
		return qtrue;
	}

	vec3_t to2;
	VectorCopy(to, to2);
	to2[0] += scanRad;
	Volumetric_Trace(&trace, from, NULL, NULL, to2, -1, (CONTENTS_SOLID | CONTENTS_TERRAIN));

	if (!(trace.fraction != 1.0 && Distance(trace.endpos, to2) > scanRad))
	{
		return qtrue;
	}

	VectorCopy(to, to2);
	to2[0] -= scanRad;
	Volumetric_Trace(&trace, from, NULL, NULL, to2, -1, (CONTENTS_SOLID | CONTENTS_TERRAIN));

	if (!(trace.fraction != 1.0 && Distance(trace.endpos, to2) > scanRad))
	{
		return qtrue;
	}

	VectorCopy(to, to2);
	to2[1] += scanRad;
	Volumetric_Trace(&trace, from, NULL, NULL, to2, -1, (CONTENTS_SOLID | CONTENTS_TERRAIN));

	if (!(trace.fraction != 1.0 && Distance(trace.endpos, to2) > scanRad))
	{
		return qtrue;
	}

	VectorCopy(to, to2);
	to2[1] -= scanRad;
	Volumetric_Trace(&trace, from, NULL, NULL, to2, -1, (CONTENTS_SOLID | CONTENTS_TERRAIN));

	if (!(trace.fraction != 1.0 && Distance(trace.endpos, to2) > scanRad))
	{
		return qtrue;
	}

	return qfalse;
}

void RB_UpdateCloseLights ( void )
{
	if (!CLOSE_LIGHTS_UPDATE) return; // Already done for this frame...

	NUM_CLOSE_LIGHTS = 0;

	for ( int l = 0 ; l < backEnd.refdef.num_dlights ; l++ )
	{
		dlight_t	*dl = &backEnd.refdef.dlights[l];

		if (dl->color[0] < 0.0 && dl->color[1] < 0.0 && dl->color[2] < 0.0)
		{// Surface glow light... But has no color assigned...
			continue;
		}

		float distance = Distance(tr.refdef.vieworg, dl->origin);

		if (distance > 4096.0) continue; // Don't even check at this range. Traces are costly!

		float x, y;
		WorldCoordToScreenCoord(dl->origin, &x, &y);

		if (NUM_CLOSE_LIGHTS < MAX_DEFERRED_LIGHTS)
		{// Have free light slots for a new light...
			vec3_t from;
			VectorCopy(tr.refdef.vieworg, from);
			from[2] += 64.0;
			if (!Light_Visible(tr.refdef.vieworg, dl->origin, qfalse, dl->radius))
			{
				continue;
			}

			CLOSEST_LIGHTS[NUM_CLOSE_LIGHTS] = l;
			VectorCopy(dl->origin, CLOSEST_LIGHTS_POSITIONS[NUM_CLOSE_LIGHTS]);
			CLOSEST_LIGHTS_DISTANCES[NUM_CLOSE_LIGHTS] = dl->radius;
			CLOSEST_LIGHTS_HEIGHTSCALES[NUM_CLOSE_LIGHTS] = dl->heightScale;
			CLOSEST_LIGHTS_COLORS[NUM_CLOSE_LIGHTS][0] = dl->color[0];
			CLOSEST_LIGHTS_COLORS[NUM_CLOSE_LIGHTS][1] = dl->color[1];
			CLOSEST_LIGHTS_COLORS[NUM_CLOSE_LIGHTS][2] = dl->color[2];
			CLOSEST_LIGHTS_SCREEN_POSITIONS[NUM_CLOSE_LIGHTS][0] = x;
			CLOSEST_LIGHTS_SCREEN_POSITIONS[NUM_CLOSE_LIGHTS][1] = y;
			NUM_CLOSE_LIGHTS++;
			continue;
		}
		else
		{// See if this is closer then one of our other lights...
			int		farthest_light = 0;
			float	farthest_distance = 0.0;

			for (int i = 0; i < NUM_CLOSE_LIGHTS; i++)
			{// Find the most distance light in our current list to replace, if this new option is closer...
				dlight_t	*thisLight = &backEnd.refdef.dlights[CLOSEST_LIGHTS[i]];
				float		dist = Distance(thisLight->origin, tr.refdef.vieworg);

				if (dist > farthest_distance)
				{// This one is further!
					farthest_light = i;
					farthest_distance = dist;
					//break;
				}
			}

			if (distance < farthest_distance)
			{// This light is closer. Replace this one in our array of closest lights...
				vec3_t from;
				VectorCopy(tr.refdef.vieworg, from);
				from[2] += 64.0;
				if (!Light_Visible(tr.refdef.vieworg, dl->origin, qfalse, dl->radius))
				{
					continue;
				}

				CLOSEST_LIGHTS[farthest_light] = l;
				VectorCopy(dl->origin, CLOSEST_LIGHTS_POSITIONS[farthest_light]);
				CLOSEST_LIGHTS_DISTANCES[farthest_light] = dl->radius;
				CLOSEST_LIGHTS_HEIGHTSCALES[farthest_light] = dl->heightScale;
				CLOSEST_LIGHTS_COLORS[farthest_light][0] = dl->color[0];
				CLOSEST_LIGHTS_COLORS[farthest_light][1] = dl->color[1];
				CLOSEST_LIGHTS_COLORS[farthest_light][2] = dl->color[2];
				CLOSEST_LIGHTS_SCREEN_POSITIONS[farthest_light][0] = x;
				CLOSEST_LIGHTS_SCREEN_POSITIONS[farthest_light][1] = y;
			}
		}
	}

	for (int i = 0; i < NUM_CLOSE_LIGHTS; i++)
	{
		if (CLOSEST_LIGHTS_DISTANCES[i] < 0.0)
		{// Remove volume light markers...
			CLOSEST_LIGHTS_DISTANCES[i] = -CLOSEST_LIGHTS_DISTANCES[i];
		}

		// Double the range on all lights...
		CLOSEST_LIGHTS_DISTANCES[i] *= 4.0;
	}

	//ri->Printf(PRINT_ALL, "Found %i close lights this frame.\n", NUM_CLOSE_LIGHTS);

	CLOSE_LIGHTS_UPDATE = qfalse;
}

extern image_t *skyImage;

extern vec3_t		SUN_COLOR_MAIN;
extern vec3_t		SUN_COLOR_SECONDARY;
extern vec3_t		SUN_COLOR_TERTIARY;
extern vec3_t		MAP_AMBIENT_COLOR;

float waveTime = 0.5;
float waveFreq = 0.1;

extern void GLSL_AttachTextures( void );
extern void GLSL_AttachGenericTextures( void );
extern void GLSL_AttachGlowTextures( void );
extern void GLSL_AttachWaterTextures( void );
//extern void GLSL_AttachWaterTextures2( void );

extern world_t				s_worldData;
extern qboolean ALLOW_GL_400;

static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	vec4_t	fogDistanceVector, fogDepthVector = {0, 0, 0, 0};
	float	eyeT = 0;
	int		deformGen;
	vec5_t	deformParams;

	ComputeDeformValues(&deformGen, deformParams);

	ComputeFogValues(fogDistanceVector, fogDepthVector, &eyeT);

	RB_UpdateMatrixes();

	//
	// UQ1: I think we only need to do all these once, not per stage... Waste of FPS!
	//

	qboolean ADD_CUBEMAP_INDEX = qfalse;
	qboolean useTesselation = qfalse;
	qboolean isWater = qfalse;
	qboolean isGrass = qfalse;
	qboolean isPebbles = qfalse;

	float tessInner = 0.0;
	float tessOuter = 0.0;
	float tessAlpha = 0.0;

	int cubeMapNum = 0;
	vec4_t cubeMapVec;
	float cubeMapRadius;

	if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
	{
		if (r_foliage->integer
			&& r_sunlightMode->integer >= 2
			&& r_foliageShadows->integer
			&& RB_ShouldUseGeometryGrass(tess.shader->surfaceFlags & MATERIAL_MASK))
		{
			isGrass = qtrue;
		}
		else if (r_pebbles->integer
			&& r_sunlightMode->integer >= 2
			&& r_foliageShadows->integer
			&& RB_ShouldUseGeometryPebbles(tess.shader->surfaceFlags & MATERIAL_MASK))
		{
			isPebbles = qtrue;
		}
	}
	else
	{
		if (r_foliage->integer
			&& RB_ShouldUseGeometryGrass(tess.shader->surfaceFlags & MATERIAL_MASK))
		{
			isGrass = qtrue;
		}
		else if (r_pebbles->integer
			&& RB_ShouldUseGeometryPebbles(tess.shader->surfaceFlags & MATERIAL_MASK))
		{
			isPebbles = qtrue;
		}
	}

	if (r_tesselation->integer 
		&& RB_ShouldUseTesselation(input))
	{
		useTesselation = qtrue;

		tessInner = RB_GetTesselationInnerLevel(tess.shader->surfaceFlags & MATERIAL_MASK);
		tessOuter = tessInner;
		tessAlpha = RB_GetTesselationAlphaLevel(tess.shader->surfaceFlags & MATERIAL_MASK);
	}

	if (!(tr.viewParms.flags & VPF_NOCUBEMAPS) && input->cubemapIndex && r_cubeMapping->integer >= 1)
	{
#ifdef __PLAYER_BASED_CUBEMAPS__
		cubeMapNum = currentPlayerCubemap;
		cubeMapVec[0] = currentPlayerCubemapVec[0];// -backEnd.viewParms.ori.origin[0];
		cubeMapVec[1] = currentPlayerCubemapVec[1];// - backEnd.viewParms.ori.origin[1];
		cubeMapVec[2] = currentPlayerCubemapVec[2];// - backEnd.viewParms.ori.origin[2];
		cubeMapVec[3] = 1.0f;
		cubeMapRadius = tr.cubemapRadius[currentPlayerCubemap];
		ADD_CUBEMAP_INDEX = qtrue;
#else //!__PLAYER_BASED_CUBEMAPS__
		cubeMapNum = input->cubemapIndex - 1;
		cubeMapVec[0] = tr.cubemapOrigins[input->cubemapIndex - 1][0] - backEnd.viewParms.ori.origin[0];
		cubeMapVec[1] = tr.cubemapOrigins[input->cubemapIndex - 1][1] - backEnd.viewParms.ori.origin[1];
		cubeMapVec[2] = tr.cubemapOrigins[input->cubemapIndex - 1][2] - backEnd.viewParms.ori.origin[2];
		cubeMapVec[3] = 1.0f;
		cubeMapRadius = tr.cubemapRadius[input->cubemapIndex - 1];
		ADD_CUBEMAP_INDEX = qtrue;
#endif //__PLAYER_BASED_CUBEMAPS__
	}

	qboolean usingDeforms = ShaderRequiresCPUDeforms(tess.shader);
	qboolean didNonDetail = qfalse;

	for ( int stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = input->xstages[stage];
		shaderProgram_t *sp = NULL, *sp2 = NULL, *sp3 = NULL;
		vec4_t texMatrix;
		vec4_t texOffTurb;
		int stateBits;
		colorGen_t forceRGBGen = CGEN_BAD;
		alphaGen_t forceAlphaGen = AGEN_IDENTITY;
		qboolean multiPass = qtrue;
		qboolean isGlowStage = qfalse;
		qboolean isUsingRegions = qfalse;

		int passNum = 0, passMax = 0;

		if ( !pStage )
		{// How does this happen???
			break;
		}

		if ( !pStage->active )
		{// Shouldn't this be here, just in case???
			continue;
		}

		int index = pStage->glslShaderIndex;

		if ( pStage->isSurfaceSprite )
		{
#ifdef __SURFACESPRITES__
			if (!r_surfaceSprites->integer)
#endif //__SURFACESPRITES__
			{
				continue;
			}
		}

#ifdef __DEFERRED_IMAGE_LOADING__
		if (pStage->bundle[TB_DIFFUSEMAP].image[0]
			&& pStage->bundle[TB_DIFFUSEMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_DIFFUSEMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_DIFFUSEMAP].image[0]);
		}

		if (pStage->bundle[TB_NORMALMAP].image[0]
			&& pStage->bundle[TB_NORMALMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_NORMALMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_NORMALMAP].image[0]);
		}

		if (pStage->bundle[TB_SPECULARMAP].image[0]
			&& pStage->bundle[TB_SPECULARMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_SPECULARMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_SPECULARMAP].image[0]);
		}

		if (pStage->bundle[TB_OVERLAYMAP].image[0]
			&& pStage->bundle[TB_OVERLAYMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_OVERLAYMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_OVERLAYMAP].image[0]);
		}

		if (pStage->bundle[TB_STEEPMAP].image[0]
			&& pStage->bundle[TB_STEEPMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_STEEPMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_STEEPMAP].image[0]);
		}

		if (pStage->bundle[TB_STEEPMAP2].image[0]
			&& pStage->bundle[TB_STEEPMAP2].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_STEEPMAP2].image[0] = R_LoadDeferredImage(pStage->bundle[TB_STEEPMAP2].image[0]);
		}

		if (pStage->bundle[TB_SPLATCONTROLMAP].image[0]
			&& pStage->bundle[TB_SPLATCONTROLMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_SPLATCONTROLMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_SPLATCONTROLMAP].image[0]);
		}

		if (pStage->bundle[TB_SPLATMAP1].image[0]
			&& pStage->bundle[TB_SPLATMAP1].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_SPLATMAP1].image[0] = R_LoadDeferredImage(pStage->bundle[TB_SPLATMAP1].image[0]);
		}

		if (pStage->bundle[TB_SPLATMAP2].image[0]
			&& pStage->bundle[TB_SPLATMAP2].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_SPLATMAP2].image[0] = R_LoadDeferredImage(pStage->bundle[TB_SPLATMAP2].image[0]);
		}

		if (pStage->bundle[TB_SPLATMAP3].image[0]
			&& pStage->bundle[TB_SPLATMAP3].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_SPLATMAP3].image[0] = R_LoadDeferredImage(pStage->bundle[TB_SPLATMAP3].image[0]);
		}

		/*if (pStage->bundle[TB_SPLATMAP4].image[0]
			&& pStage->bundle[TB_SPLATMAP4].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_SPLATMAP4].image[0] = R_LoadDeferredImage(pStage->bundle[TB_SPLATMAP4].image[0]);
		}*/

		if (pStage->bundle[TB_DETAILMAP].image[0]
			&& pStage->bundle[TB_DETAILMAP].image[0]->deferredLoad)
		{// Load the actual image file...
			pStage->bundle[TB_DETAILMAP].image[0] = R_LoadDeferredImage(pStage->bundle[TB_DETAILMAP].image[0]);
		}
#endif //__DEFERRED_IMAGE_LOADING__

		float useTC = 0.0;
		float useDeform = 0.0;
		float useRGBA = 0.0;
		float useFog = 0.0;

		float useVertexAnim = 0.0;
		float useSkeletalAnim = 0.0;

//#define __USE_ALPHA_TEST__ // This interferes with the ability for the depth prepass to optimize out fragments causing FPS hit.
#define __USE_DETAIL_DEPTH_SKIP__
#define __LIGHTMAP_IS_DETAIL__
#define __USE_GLOW_DETAIL_BUFFERS__

		if (pStage->isWater && r_glslWater->integer && WATER_ENABLED && MAP_WATER_LEVEL > -131072.0)
		{
			if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
			{
				break;
			}
			else if (stage <= 0)
			{
				sp = &tr.waterShader;
				pStage->glslShaderGroup = &tr.waterShader;
				isWater = qtrue;
				isGrass = qfalse;
				isPebbles = qfalse;
				multiPass = qfalse;
			}
			else
			{// Only do one stage on GLSL water...
				break;
			}

			GLSL_BindProgram(sp);
		}
		else if (r_proceduralSun->integer && tess.shader == tr.sunShader)
		{// Special case for procedural sun...
			sp = &tr.sunPassShader;
			GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);
			isGrass = qfalse;
			isPebbles = qfalse;
			multiPass = qfalse;

			GLSL_BindProgram(sp);
		}
		else if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
		{// testing - force lightall
#ifdef __USE_DETAIL_DEPTH_SKIP__
			if (!(pStage->type == ST_COLORMAP || pStage->type == ST_GLSL)
				&& pStage->bundle[0].tcGen >= TCGEN_LIGHTMAP
				&& pStage->bundle[0].tcGen <= TCGEN_LIGHTMAP3)
			{// No point at all in doing this stage...
				continue;
			}

			if (stage > 0 && didNonDetail && !pStage->glow)
			{
				continue;
			}

			if (pStage->isDetail && !pStage->glow)
			{// Don't waste the time...
				continue;
			}

			if (pStage->noScreenMap)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}

			if (pStage->bundle[0].isLightmap)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}

			/*if (backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity)
			{
				break;
			}*/

			if (pStage->type != ST_COLORMAP && pStage->type != ST_GLSL && !pStage->glow)
			{// Don't output these to position and normal map...
				index |= LIGHTDEF_IS_DETAIL;
			}

			/*if ((stateBits & GLS_SRCBLEND_SRC_ALPHA)
			&& (stateBits & GLS_DSTBLEND_ONE))
			{// Don't output these to position and normal map...
			index |= LIGHTDEF_IS_DETAIL;
			}*/

			if ((!pStage->bundle[TB_DIFFUSEMAP].image[0] || pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_COLORALPHA) && !pStage->glow)
			{// Don't output these to position and normal map...
				index |= LIGHTDEF_IS_DETAIL;
			}

			//GLS_DEPTHTEST_DISABLE
			if (backEnd.currentEntity == &backEnd.entity2D)// || (pStage->stateBits & GLS_DEPTHTEST_DISABLE))
			{
				index |= LIGHTDEF_IS_DETAIL;
			}

			/*if (pStage->stateBits & GLS_ATEST_BITS)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}*/

			if (!(index & LIGHTDEF_IS_DETAIL) && !pStage->glow)
			{
				didNonDetail = qtrue;
			}
			else
			{
				continue;
			}
#endif //__USE_DETAIL_DEPTH_SKIP__


			//if (tr.currentEntity && tr.currentEntity != &tr.worldEntity)
			{
				if (glState.vertexAnimation)
				{
					useVertexAnim = 1.0;
				}

				if (glState.skeletalAnimation)
				{
					useSkeletalAnim = 1.0;
				}
			}

			if (pStage->bundle[0].tcGen != TCGEN_TEXTURE || pStage->bundle[0].numTexMods)
			{
				useTC = 1.0;
			}

			if (tess.shader->numDeforms || usingDeforms)
			{
				useDeform = 1.0;
			}

			//pStage->glslShaderGroup = tr.lightallShader;

			if (useTesselation)
			{
				index |= LIGHTDEF_USE_TESSELLATION;

				sp = &tr.lightallMergedShader;

				backEnd.pc.c_lightallDraws++;
			}
			else
			{
				sp = &tr.shadowPassShader;
				//pStage->glslShaderGroup = &tr.shadowPassShader;

				backEnd.pc.c_shadowPassDraws++;
			}

			GLSL_BindProgram(sp);
		}
		else
		{
			if ((s_worldData.lightGridArray == NULL && (index & LIGHTDEF_USE_LIGHTMAP))
				|| (backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity))
			{// Bsp has no lightmap data, disable lightmaps in any shaders that would try to use one...
				if (!(pStage->type == ST_COLORMAP || pStage->type == ST_GLSL)
					&& pStage->bundle[0].tcGen >= TCGEN_LIGHTMAP 
					&& pStage->bundle[0].tcGen <= TCGEN_LIGHTMAP3)
				{// No point at all in doing this stage...
					continue;
				}

				index &= ~LIGHTDEF_USE_LIGHTMAP;
			}
			
			//if (backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity)
			{
				if (glState.vertexAnimation)
				{
					useVertexAnim = 1.0;
				}

				if (glState.skeletalAnimation)
				{
					useSkeletalAnim = 1.0;
				}
			}


			if (useTesselation)
			{
				index |= LIGHTDEF_USE_TESSELLATION;
			}

			//
			// testing cube map
			//
			if (ADD_CUBEMAP_INDEX)
			{
				if (backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity)
				{
					if (glState.vertexAnimation)
					{
						if (r_cubeMapping->integer >= 2)
						{
							index |= LIGHTDEF_USE_CUBEMAP;
						}
					}
					else if (glState.skeletalAnimation)
					{
						if (r_cubeMapping->integer >= 1)
						{
							index |= LIGHTDEF_USE_CUBEMAP;
						}
					}
				}
				else
				{
					index |= LIGHTDEF_USE_CUBEMAP;
				}
			}

			if ((tess.shader->surfaceFlags & MATERIAL_MASK) == MATERIAL_ROCK
				&& (pStage->bundle[TB_STEEPMAP].image[0]
					|| pStage->bundle[TB_STEEPMAP2].image[0]
					|| pStage->bundle[TB_SPLATMAP1].image[0]
					|| pStage->bundle[TB_SPLATMAP2].image[0]
					|| pStage->bundle[TB_SPLATMAP3].image[0]))
			{
				isUsingRegions = qtrue;
				index |= LIGHTDEF_USE_REGIONS;
			}
			else if ((pStage->bundle[TB_STEEPMAP].image[0]
				|| pStage->bundle[TB_STEEPMAP2].image[0]
				|| pStage->bundle[TB_SPLATMAP1].image[0]
				|| pStage->bundle[TB_SPLATMAP2].image[0]
				|| pStage->bundle[TB_SPLATMAP3].image[0]))
			{
				index |= LIGHTDEF_USE_TRIPLANAR;
			}

			if ((index & LIGHTDEF_USE_GLOW_BUFFER) || pStage->glow)
			{
				isGlowStage = qtrue;
			}

			switch (pStage->rgbGen)
			{
			case CGEN_LIGHTING_DIFFUSE:
				useRGBA = 1.0;
				break;
			default:
				break;
			}

			switch (pStage->alphaGen)
			{
			case AGEN_LIGHTING_SPECULAR:
			case AGEN_PORTAL:
				useRGBA = 1.0;
				break;
			default:
				break;
			}

			if (pStage->bundle[0].tcGen != TCGEN_TEXTURE || pStage->bundle[0].numTexMods)
			{
				useTC = 1.0;
			}

			if (tess.shader->numDeforms || usingDeforms)
			{
				useDeform = 1.0;
			}

			if (input->fogNum)
			{
				useFog = 1.0;
			}
			
			if (stage > 0 && didNonDetail)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}

			if (pStage->isDetail)
			{// Don't output these to position and normal map...
				index |= LIGHTDEF_IS_DETAIL;
			}

			if (pStage->noScreenMap)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}

#ifdef __LIGHTMAP_IS_DETAIL__
			if (pStage->bundle[0].isLightmap)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}
#endif //__LIGHTMAP_IS_DETAIL__

			if (pStage->type != ST_COLORMAP && pStage->type != ST_GLSL)
			{// Don't output these to position and normal map...
				index |= LIGHTDEF_IS_DETAIL;
			}

			//GLS_DEPTHTEST_DISABLE
			if (backEnd.currentEntity == &backEnd.entity2D)// || (pStage->stateBits & GLS_DEPTHTEST_DISABLE))
			{
				index |= LIGHTDEF_IS_DETAIL;
			}

			if (!pStage->bundle[TB_DIFFUSEMAP].image[0]
				|| pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_COLORALPHA)
			{// Don't output these to position and normal map...
				index |= LIGHTDEF_IS_DETAIL;
			}

#ifdef __LIGHTMAP_IS_DETAIL__
			if (!(pStage->type == ST_COLORMAP || pStage->type == ST_GLSL)
				&& pStage->bundle[0].tcGen >= TCGEN_LIGHTMAP
				&& pStage->bundle[0].tcGen <= TCGEN_LIGHTMAP3)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}
#endif //__LIGHTMAP_IS_DETAIL__
			
			/*if (pStage->stateBits & GLS_ATEST_BITS)
			{
				index |= LIGHTDEF_IS_DETAIL;
			}*/

			if (!(index & LIGHTDEF_IS_DETAIL))
			{
				didNonDetail = qtrue;
			}
			
			//pStage->glslShaderGroup = tr.lightallShader;
			sp = &tr.lightallMergedShader;

			backEnd.pc.c_lightallDraws++;

			GLSL_BindProgram(sp);


			if ((r_foliage->integer || r_pebbles->integer) && (isGrass || isPebbles))
			{// Special extra pass stuff for grass or pebbles...
				if (isGrass && r_foliage->integer)
				{
					sp2 = &tr.grass2Shader;
					multiPass = qtrue;
					passMax = r_foliagePasses->integer;

					//if (ALLOW_GL_400) passMax = 2; // uses hardware invocations instead

					if (isPebbles && r_pebbles->integer)
					{
						sp3 = &tr.pebblesShader;
						passMax = r_foliagePasses->integer + r_pebblesPasses->integer;
					}
				}
				else if (isPebbles && r_pebbles->integer)
				{
					sp2 = &tr.pebblesShader;
					passMax = r_pebblesPasses->integer;
				}
			}
		}


		RB_SetMaterialBasedProperties(sp, pStage, stage);

		stateBits = pStage->stateBits;

		if ( backEnd.currentEntity )
		{
			assert(backEnd.currentEntity->e.renderfx >= 0);

			if ( backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1 )
			{
				// we want to be able to rip a hole in the thing being disintegrated, and by doing the depth-testing it avoids some kinds of artefacts, but will probably introduce others?
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_192;
			}

			if ( backEnd.currentEntity->e.renderfx & RF_RGB_TINT )
			{//want to use RGBGen from ent
				forceRGBGen = CGEN_ENTITY;
			}

#ifndef __USE_ALPHA_TEST__
			if ( backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA )
			{
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				if ( backEnd.currentEntity->e.renderfx & RF_ALPHA_DEPTH )
				{ //depth write, so faces through the model will be stomped over by nearer ones. this works because
					//we draw RF_FORCE_ENT_ALPHA stuff after everything else, including standard alpha surfs.
					stateBits |= GLS_DEPTHMASK_TRUE;
				}
			}
#endif //__USE_ALPHA_TEST__
		}
		else
		{// UQ: - FPS TESTING - This may cause issues, we will need to keep an eye on things...
			if (!(tr.refdef.rdflags & RDF_NOWORLDMODEL) && !(tr.viewParms.flags & VPF_SHADOWPASS))
				stateBits |= GLS_DEPTHMASK_TRUE | GLS_DEPTHFUNC_LESS | GLS_DEPTHFUNC_EQUAL;
		}

		{// Set up basic shader settings... This way we can avoid the bind bloat of dumb vert shader #ifdefs...
			vec4_t vec;

			if (r_debugMapAmbientR->value + r_debugMapAmbientG->value + r_debugMapAmbientB->value > 0.0)
				VectorSet4(vec, r_debugMapAmbientR->value, r_debugMapAmbientG->value, r_debugMapAmbientB->value, 0.0);
			else
				VectorSet4(vec, MAP_AMBIENT_COLOR[0], MAP_AMBIENT_COLOR[1], MAP_AMBIENT_COLOR[2], 0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_MAP_AMBIENT, vec);

#ifdef __USE_ALPHA_TEST__
			vec2_t atest = { 0 };

			if (stateBits & GLS_ATEST_BITS)
			{
				useTC = 1.0;

				switch (stateBits & GLS_ATEST_BITS)
				{
				case GLS_ATEST_GT_0:
					atest[0] = ATEST_GT;
					atest[1] = 0.0;
					break;
				case GLS_ATEST_LT_128:
					atest[0] = ATEST_LT;
					atest[1] = 0.5;
					break;
				case GLS_ATEST_GE_128:
					atest[0] = ATEST_GE;
					atest[1] = 0.5;
					break;
				case GLS_ATEST_GE_192:
					atest[0] = ATEST_GE;
					atest[1] = 0.75;
					break;
				default:
					atest[0] = ATEST_GT;
					atest[1] = 0.0;
					break;
				}
			}
			else
			{
				atest[0] = ATEST_GT;
				atest[1] = 0.0;
			}

			GLSL_SetUniformVec2(sp, UNIFORM_ALPHATEST, atest);
#else //!__USE_ALPHA_TEST__
			//vec2_t atest = { 0 };
			//GLSL_SetUniformVec2(sp, UNIFORM_ALPHATEST, atest);
#endif //__USE_ALPHA_TEST__

#if 0
			uniform vec4				u_Settings0; // useTC, useDeform, useRGBA, isTextureClamped
			uniform vec4				u_Settings1; // useVertexAnim, useSkeletalAnim, useFog, is2D
			uniform vec4				u_Settings2; // LIGHTDEF_USE_LIGHTMAP, LIGHTDEF_USE_GLOW_BUFFER, LIGHTDEF_USE_CUBEMAP, LIGHTDEF_USE_TRIPLANAR
			uniform vec4				u_Settings3; // LIGHTDEF_USE_REGIONS, LIGHTDEF_IS_DETAIL
#endif

			VectorSet4(vec, 
				useTC, 
				useDeform, 
				useRGBA, 
				(pStage->bundle[TB_DIFFUSEMAP].image[0] && (pStage->bundle[TB_DIFFUSEMAP].image[0]->flags & IMGFLAG_CLAMPTOEDGE)) ? 1.0 : 0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_SETTINGS0, vec);

			VectorSet4(vec, 
				useVertexAnim, 
				useSkeletalAnim, 
				useFog, 
				(backEnd.currentEntity == &backEnd.entity2D || (pStage->stateBits & GLS_DEPTHTEST_DISABLE)) ? 1.0 : 0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_SETTINGS1, vec);

			VectorSet4(vec, 
				(index & LIGHTDEF_USE_LIGHTMAP) ? 1.0 : 0.0, 
				(index & LIGHTDEF_USE_GLOW_BUFFER) ? 1.0 : 0.0, 
				(!(tr.viewParms.flags & VPF_NOCUBEMAPS) && tr.cubemaps && cubeMapNum && ADD_CUBEMAP_INDEX && r_cubeMapping->integer >= 1) ? 1.0 : 0.0,
				(index & LIGHTDEF_USE_TRIPLANAR) ? 1.0 : 0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_SETTINGS2, vec);

			VectorSet4(vec, 
				(index & LIGHTDEF_USE_REGIONS) ? 1.0 : 0.0, 
				(index & LIGHTDEF_IS_DETAIL) ? 1.0 : 0.0, 
				tess.shader->detailMapFromTC ? 1.0 : tess.shader->detailMapFromWorld ? 2.0 : 0.0,
				0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_SETTINGS3, vec);
		}

		// UQ1: Used by both generic and lightall...
		RB_SetStageImageDimensions(sp, pStage);

		GLSL_SetUniformMatrix16(sp, UNIFORM_MODELMATRIX, backEnd.ori.transformMatrix);
		//GLSL_SetUniformMatrix16(sp, UNIFORM_MODELMATRIX, backEnd.ori.modelMatrix);
		GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
		GLSL_SetUniformMatrix16(sp, UNIFORM_NORMALMATRIX, MATRIX_NORMAL);

		GLSL_SetUniformVec3(sp, UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);
		GLSL_SetUniformFloat(sp, UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);

		GLSL_SetUniformVec3(sp, UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);

		if (pStage->normalScale[0] == 0 && pStage->normalScale[1] == 0 && pStage->normalScale[2] == 0)
		{
			vec4_t normalScale;
			VectorSet4(normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
			GLSL_SetUniformVec4(sp, UNIFORM_NORMALSCALE, normalScale);
		}
		else
		{
			GLSL_SetUniformVec4(sp, UNIFORM_NORMALSCALE, pStage->normalScale);
		}

		if (glState.skeletalAnimation)
		{
			GLSL_SetUniformMatrix16(sp, UNIFORM_BONE_MATRICES, &glState.boneMatrices[0][0], glState.numBones);
		}

		GLSL_SetUniformInt(sp, UNIFORM_DEFORMGEN, deformGen);
		if (deformGen != DGEN_NONE)
		{
			GLSL_SetUniformFloat5(sp, UNIFORM_DEFORMPARAMS, deformParams);
			GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);
		}

		GLSL_SetUniformInt(sp, UNIFORM_TCGEN0, pStage->bundle[0].tcGen);
		if (pStage->bundle[0].tcGen == TCGEN_VECTOR)
		{
			vec3_t vec;

			VectorCopy(pStage->bundle[0].tcGenVectors[0], vec);
			GLSL_SetUniformVec3(sp, UNIFORM_TCGEN0VECTOR0, vec);
			VectorCopy(pStage->bundle[0].tcGenVectors[1], vec);
			GLSL_SetUniformVec3(sp, UNIFORM_TCGEN0VECTOR1, vec);
		}


		vec2_t scale;
		ComputeTexMods(pStage, TB_DIFFUSEMAP, texMatrix, texOffTurb, scale);
		GLSL_SetUniformVec4(sp, UNIFORM_DIFFUSETEXMATRIX, texMatrix);
		GLSL_SetUniformVec4(sp, UNIFORM_DIFFUSETEXOFFTURB, texOffTurb);
		GLSL_SetUniformVec2(sp, UNIFORM_TEXTURESCALE, scale);

		if (!(backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS)))
		{
			//if (r_fog->integer) //useFog
			if (useFog)
			{
				if (input->fogNum)
				{
					vec4_t fogColorMask;
					GLSL_SetUniformVec4(sp, UNIFORM_FOGDISTANCE, fogDistanceVector);
					GLSL_SetUniformVec4(sp, UNIFORM_FOGDEPTH, fogDepthVector);
					GLSL_SetUniformFloat(sp, UNIFORM_FOGEYET, eyeT);

					ComputeFogColorMask(pStage, fogColorMask);
					GLSL_SetUniformVec4(sp, UNIFORM_FOGCOLORMASK, fogColorMask);
				}
			}
		}

		//GLSL_SetUniformFloat(sp, UNIFORM_MAPLIGHTSCALE, backEnd.refdef.mapLightScale);

		//
		// testing cube map
		//
		if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
		{
			GL_BindToTMU(tr.blackImage, TB_CUBEMAP);
			GLSL_SetUniformFloat(sp, UNIFORM_CUBEMAPSTRENGTH, 0.0);
			VectorSet4(cubeMapVec, 0.0, 0.0, 0.0, 0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_CUBEMAPINFO, cubeMapVec);
		}
		else if (!(tr.viewParms.flags & VPF_NOCUBEMAPS) && tr.cubemaps && cubeMapNum && ADD_CUBEMAP_INDEX && r_cubeMapping->integer >= 1)
		{
			//ri->Printf(PRINT_ALL, "%s stage %i is using cubemap (correct lightall: %s)\n", input->shader->name, stage, (index & LIGHTDEF_USE_CUBEMAP) ? "true" : "false");
			GL_BindToTMU(tr.cubemaps[cubeMapNum], TB_CUBEMAP);
			GLSL_SetUniformFloat(sp, UNIFORM_CUBEMAPSTRENGTH, r_cubemapStrength->value);
			VectorScale4(cubeMapVec, 1.0f / cubeMapRadius/*1000.0f*/, cubeMapVec);
			GLSL_SetUniformVec4(sp, UNIFORM_CUBEMAPINFO, cubeMapVec);
		}
		else
		{
			GL_BindToTMU(tr.blackImage, TB_CUBEMAP);
			GLSL_SetUniformFloat(sp, UNIFORM_CUBEMAPSTRENGTH, 0.0);
			VectorSet4(cubeMapVec, 0.0, 0.0, 0.0, 0.0);
			GLSL_SetUniformVec4(sp, UNIFORM_CUBEMAPINFO, cubeMapVec);
		}

		//
		//
		//

		if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
		{
			vec4_t baseColor;
			vec4_t vertColor;

			//if (pStage->rgbGen || pStage->alphaGen)
			{
				ComputeShaderColors(pStage, baseColor, vertColor, stateBits, &forceRGBGen, &forceAlphaGen);

				if ((backEnd.refdef.colorScale != 1.0f) && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
				{
					// use VectorScale to only scale first three values, not alpha
					VectorScale(baseColor, backEnd.refdef.colorScale, baseColor);
					VectorScale(vertColor, backEnd.refdef.colorScale, vertColor);
				}

				if (backEnd.currentEntity != NULL &&
					(backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA))
				{
					vertColor[3] = backEnd.currentEntity->e.shaderRGBA[3] / 255.0f;
				}
			}

			GLSL_SetUniformVec4(sp, UNIFORM_BASECOLOR, baseColor);
			GLSL_SetUniformVec4(sp, UNIFORM_VERTCOLOR, vertColor);

			if (pStage->alphaGen == AGEN_PORTAL)
			{
				GLSL_SetUniformFloat(sp, UNIFORM_PORTALRANGE, tess.shader->portalRange);
			}

			GLSL_SetUniformInt(sp, UNIFORM_COLORGEN, forceRGBGen);
			GLSL_SetUniformInt(sp, UNIFORM_ALPHAGEN, forceAlphaGen);

			if (r_sunlightMode->integer && (r_sunlightSpecular->integer || (backEnd.viewParms.flags & VPF_USESUNLIGHT)))
			{
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTCOLOR, backEnd.refdef.sunCol);
				GLSL_SetUniformVec4(sp, UNIFORM_PRIMARYLIGHTORIGIN, backEnd.refdef.sunDir);

				GLSL_SetUniformInt(sp, UNIFORM_LIGHTCOUNT, 0);
			}
		}
		else
		{
			vec4_t baseColor;
			vec4_t vertColor;

			//if (pStage->rgbGen || pStage->alphaGen)
			{
				ComputeShaderColors(pStage, baseColor, vertColor, stateBits, &forceRGBGen, &forceAlphaGen);

				if ((backEnd.refdef.colorScale != 1.0f) && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
				{
					// use VectorScale to only scale first three values, not alpha
					VectorScale(baseColor, backEnd.refdef.colorScale, baseColor);
					VectorScale(vertColor, backEnd.refdef.colorScale, vertColor);
				}

				if (backEnd.currentEntity != NULL &&
					(backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA))
				{
					vertColor[3] = backEnd.currentEntity->e.shaderRGBA[3] / 255.0f;
				}
			}

			GLSL_SetUniformVec4(sp, UNIFORM_BASECOLOR, baseColor);
			GLSL_SetUniformVec4(sp, UNIFORM_VERTCOLOR, vertColor);

			if (pStage->rgbGen == CGEN_LIGHTING_DIFFUSE || pStage->rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY)
			{
				vec4_t vec;

				VectorScale(backEnd.currentEntity->ambientLight, 1.0f / 255.0f, vec);
				GLSL_SetUniformVec3(sp, UNIFORM_AMBIENTLIGHT, vec);

				VectorScale(backEnd.currentEntity->directedLight, 1.0f / 255.0f, vec);
				GLSL_SetUniformVec3(sp, UNIFORM_DIRECTEDLIGHT, vec);

				VectorCopy(backEnd.currentEntity->lightDir, vec);
				vec[3] = 0.0f;
				GLSL_SetUniformVec4(sp, UNIFORM_LIGHTORIGIN, vec);
				GLSL_SetUniformVec3(sp, UNIFORM_MODELLIGHTDIR, backEnd.currentEntity->modelLightDir);
				GLSL_SetUniformFloat(sp, UNIFORM_LIGHTRADIUS, 0.0f);
			}

			if (pStage->alphaGen == AGEN_PORTAL)
			{
				GLSL_SetUniformFloat(sp, UNIFORM_PORTALRANGE, tess.shader->portalRange);
			}

			GLSL_SetUniformInt(sp, UNIFORM_COLORGEN, forceRGBGen);
			GLSL_SetUniformInt(sp, UNIFORM_ALPHAGEN, forceAlphaGen);


			if (pStage->bundle[TB_DETAILMAP].image[0])
			{
				GL_BindToTMU(pStage->bundle[TB_DETAILMAP].image[0], TB_DETAILMAP);
			}
			else
			{
				GL_BindToTMU(tr.defaultDetail, TB_DETAILMAP);
			}

			if (pStage->bundle[TB_STEEPMAP].image[0])
			{
				//ri->Printf(PRINT_WARNING, "Image bound to steep map %i x %i.\n", pStage->bundle[TB_STEEPMAP].image[0]->width, pStage->bundle[TB_STEEPMAP].image[0]->height);
				//R_BindAnimatedImageToTMU( &pStage->bundle[TB_STEEPMAP], TB_STEEPMAP);
				GL_BindToTMU( pStage->bundle[TB_STEEPMAP].image[0], TB_STEEPMAP );
			}
			else
			{
				GL_BindToTMU( tr.whiteImage, TB_STEEPMAP );
			}

			if (pStage->bundle[TB_STEEPMAP2].image[0])
			{
				//ri->Printf(PRINT_WARNING, "Image %s bound to steep map %i x %i.\n", pStage->bundle[TB_STEEPMAP2].image[0]->imgName, pStage->bundle[TB_STEEPMAP2].image[0]->width, pStage->bundle[TB_STEEPMAP2].image[0]->height);
				//R_BindAnimatedImageToTMU( &pStage->bundle[TB_STEEPMAP2], TB_STEEPMAP2);
				GL_BindToTMU( pStage->bundle[TB_STEEPMAP2].image[0], TB_STEEPMAP2 );
			}
			else
			{
				GL_BindToTMU( tr.whiteImage, TB_STEEPMAP2 );
			}

			if (pStage->bundle[TB_SPLATMAP1].image[0] && pStage->bundle[TB_SPLATMAP1].image[0] != tr.whiteImage)
			{
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP1].image[0], TB_SPLATMAP1 );
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP1].image[1], TB_SPLATNORMALMAP1 );
			}
			else
			{
				if (pStage->bundle[TB_DIFFUSEMAP].image[0])
				{
					R_BindAnimatedImageToTMU( &pStage->bundle[TB_DIFFUSEMAP], TB_SPLATMAP1);
					GL_BindToTMU( pStage->bundle[TB_NORMALMAP].image[0], TB_SPLATNORMALMAP1 );
				}
				else // will never get used anyway
				{
					GL_BindToTMU( tr.whiteImage, TB_SPLATMAP1 );
					GL_BindToTMU( tr.whiteImage, TB_SPLATNORMALMAP1 );
				}
			}

			if (pStage->bundle[TB_SPLATMAP2].image[0] && pStage->bundle[TB_SPLATMAP2].image[0] != tr.whiteImage)
			{
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP2].image[0], TB_SPLATMAP2 );
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP2].image[1], TB_SPLATNORMALMAP2 );
			}
			else
			{
				if (pStage->bundle[TB_DIFFUSEMAP].image[0])
				{
					R_BindAnimatedImageToTMU( &pStage->bundle[TB_DIFFUSEMAP], TB_SPLATMAP2);
					GL_BindToTMU( pStage->bundle[TB_NORMALMAP].image[0], TB_SPLATNORMALMAP2 );
				}
				else // will never get used anyway
				{
					GL_BindToTMU( tr.whiteImage, TB_SPLATMAP2 );
					GL_BindToTMU( tr.whiteImage, TB_SPLATNORMALMAP2 );
				}
			}

			if (pStage->bundle[TB_SPLATMAP3].image[0] && pStage->bundle[TB_SPLATMAP3].image[0] != tr.whiteImage)
			{
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP3].image[0], TB_SPLATMAP3 );
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP3].image[1], TB_SPLATNORMALMAP3 );
			}
			else
			{
				if (pStage->bundle[TB_DIFFUSEMAP].image[0])
				{
					R_BindAnimatedImageToTMU( &pStage->bundle[TB_DIFFUSEMAP], TB_SPLATMAP3);
					GL_BindToTMU( pStage->bundle[TB_NORMALMAP].image[0], TB_SPLATNORMALMAP3 );
				}
				else // will never get used anyway
				{
					GL_BindToTMU( tr.whiteImage, TB_SPLATMAP3 );
					GL_BindToTMU( tr.whiteImage, TB_SPLATNORMALMAP3 );
				}
			}

			/*if (pStage->bundle[TB_SPLATMAP4].image[0] && pStage->bundle[TB_SPLATMAP4].image[0] != tr.whiteImage)
			{
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP4].image[0], TB_SPLATMAP4 );
				GL_BindToTMU( pStage->bundle[TB_SPLATMAP4].image[1], TB_SPLATNORMALMAP4 );
			}
			else
			{
				if (pStage->bundle[TB_DIFFUSEMAP].image[0])
				{
					R_BindAnimatedImageToTMU( &pStage->bundle[TB_DIFFUSEMAP], TB_SPLATMAP4);
					GL_BindToTMU( pStage->bundle[TB_NORMALMAP].image[0], TB_SPLATNORMALMAP4 );
				}
				else // will never get used anyway
				{
					GL_BindToTMU( tr.whiteImage, TB_SPLATMAP4 );
					GL_BindToTMU( tr.whiteImage, TB_SPLATNORMALMAP4 );
				}
			}*/

			/*if (pStage->bundle[TB_SUBSURFACEMAP].image[0])
			{
			R_BindAnimatedImageToTMU( &pStage->bundle[TB_SUBSURFACEMAP], TB_SUBSURFACEMAP);
			}
			else
			{
			GL_BindToTMU( tr.whiteImage, TB_SUBSURFACEMAP );
			}*/

			if (pStage->bundle[TB_OVERLAYMAP].image[0])
			{
				R_BindAnimatedImageToTMU( &pStage->bundle[TB_OVERLAYMAP], TB_OVERLAYMAP);
			}
			else
			{
				GL_BindToTMU( tr.blackImage, TB_OVERLAYMAP );
			}

			if (r_sunlightMode->integer && (r_sunlightSpecular->integer || (backEnd.viewParms.flags & VPF_USESUNLIGHT)))
			{
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTCOLOR,   backEnd.refdef.sunCol);
				GLSL_SetUniformVec4(sp, UNIFORM_PRIMARYLIGHTORIGIN,  backEnd.refdef.sunDir);

				GLSL_SetUniformInt(sp, UNIFORM_LIGHTCOUNT, 0);
			}

			GL_BindToTMU(tr.whiteImage, TB_NORMALMAP);
			GL_BindToTMU( tr.whiteImage, TB_NORMALMAP2 );
			GL_BindToTMU( tr.whiteImage, TB_NORMALMAP3 );
		}

		//
		// do multitexture
		//
		if ((tr.viewParms.flags & VPF_SHADOWPASS))
		{
			if (pStage->bundle[TB_DIFFUSEMAP].image[0])
				R_BindAnimatedImageToTMU(&pStage->bundle[TB_DIFFUSEMAP], TB_DIFFUSEMAP);
			else if (!(pStage->stateBits & GLS_ATEST_BITS))
				GL_BindToTMU(tr.whiteImage, 0);
		}
		else if (backEnd.depthFill)
		{
			if (!(pStage->stateBits & GLS_ATEST_BITS))
				GL_BindToTMU( tr.whiteImage, 0 );
			else if ( pStage->bundle[TB_COLORMAP].image[0] != 0 )
				R_BindAnimatedImageToTMU( &pStage->bundle[TB_COLORMAP], TB_COLORMAP );
		}
		else if ( sp == &tr.lightallMergedShader || sp == &tr.shadowPassShader )
		{
			int i;
			vec4_t enableTextures;

			VectorSet4(enableTextures, 0, 0, 0, 0);
			if ((r_lightmap->integer == 1 || r_lightmap->integer == 2) && pStage->bundle[TB_LIGHTMAP].image[0])
			{
				for (i = 0; i < NUM_TEXTURE_BUNDLES; i++)
				{
					if (i == TB_LIGHTMAP)
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_LIGHTMAP], i);
					else
						GL_BindToTMU( tr.whiteImage, i );
				}
			}
			else if (r_lightmap->integer == 3 && pStage->bundle[TB_DELUXEMAP].image[0])
			{
				for (i = 0; i < NUM_TEXTURE_BUNDLES; i++)
				{
					if (i == TB_LIGHTMAP)
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_DELUXEMAP], i);
					else
						GL_BindToTMU( tr.whiteImage, i );
				}
			}
			else
			{
				if (pStage->bundle[TB_DIFFUSEMAP].image[0])
					R_BindAnimatedImageToTMU( &pStage->bundle[TB_DIFFUSEMAP], TB_DIFFUSEMAP);

				if (pStage->bundle[TB_LIGHTMAP].image[0])
					R_BindAnimatedImageToTMU( &pStage->bundle[TB_LIGHTMAP], TB_LIGHTMAP);
				else
					GL_BindToTMU( tr.whiteImage, TB_LIGHTMAP );

				// bind textures that are sampled and used in the glsl shader, and
				// bind whiteImage to textures that are sampled but zeroed in the glsl shader
				//
				// alternatives:
				//  - use the last bound texture
				//     -> costs more to sample a higher res texture then throw out the result
				//  - disable texture sampling in glsl shader with #ifdefs, as before
				//     -> increases the number of shaders that must be compiled
				//
				{
					if (r_normalMapping->integer >= 2
						&& !input->shader->isPortal
						&& !input->shader->isSky
						&& !pStage->glow
						&& !pStage->bundle[TB_DIFFUSEMAP].normalsLoaded2
						&& (!pStage->bundle[TB_NORMALMAP].image[0] || pStage->bundle[TB_NORMALMAP].image[0] == tr.whiteImage)
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName[0]
					&& pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName[0] != '*'
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName[0] != '$'
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName[0] != '_'
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName[0] != '!'
						&& !(pStage->bundle[TB_DIFFUSEMAP].image[0]->flags & IMGFLAG_CUBEMAP)
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_NORMAL
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_SPECULAR
						/*&& pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_SUBSURFACE*/
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_OVERLAY
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_STEEPMAP
						&& pStage->bundle[TB_DIFFUSEMAP].image[0]->type != IMGTYPE_STEEPMAP2
						// gfx dirs can be exempted I guess...
						&& !(r_disableGfxDirEnhancement->integer && StringContainsWord(pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName, "gfx/")))
					{// How did this happen??? Oh well, generate a normal map now...
						char imgname[68];
						//ri->Printf(PRINT_WARNING, "Realtime generating normal map for %s.\n", pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName);
						sprintf(imgname, "%s_n", pStage->bundle[TB_DIFFUSEMAP].image[0]->imgName);
						pStage->bundle[TB_NORMALMAP].image[0] = R_CreateNormalMapGLSL( imgname, NULL, pStage->bundle[TB_DIFFUSEMAP].image[0]->width, pStage->bundle[TB_DIFFUSEMAP].image[0]->height, pStage->bundle[TB_DIFFUSEMAP].image[0]->flags, pStage->bundle[TB_DIFFUSEMAP].image[0] );

						if (pStage->bundle[TB_NORMALMAP].image[0] && pStage->bundle[TB_NORMALMAP].image[0] != tr.whiteImage)
						{
							pStage->hasRealNormalMap = true;
							RB_SetMaterialBasedProperties(sp, pStage, stage);

							if (pStage->normalScale[0] == 0 && pStage->normalScale[1] == 0 && pStage->normalScale[2] == 0)
							{
								VectorSet4(pStage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
								GLSL_SetUniformVec4(sp, UNIFORM_NORMALSCALE, pStage->normalScale);
							}
						}

						//if (pStage->bundle[TB_NORMALMAP].image[0] != tr.whiteImage)
						pStage->bundle[TB_DIFFUSEMAP].normalsLoaded2 = qtrue;
					}

					if (pStage->bundle[TB_NORMALMAP].image[0])
					{
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_NORMALMAP], TB_NORMALMAP);
						enableTextures[0] = 1.0f;
					}
					else if (r_normalMapping->integer >= 2)
					{
						GL_BindToTMU( tr.whiteImage, TB_NORMALMAP );
					}

					if (pStage->bundle[TB_NORMALMAP2].image[0])
					{
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_NORMALMAP2], TB_NORMALMAP2);
					}
					else if (r_normalMapping->integer >= 2)
					{
						GL_BindToTMU( tr.whiteImage, TB_NORMALMAP2 );
					}

					if (pStage->bundle[TB_NORMALMAP3].image[0])
					{
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_NORMALMAP3], TB_NORMALMAP3);
					}
					else if (r_normalMapping->integer >= 2)
					{
						GL_BindToTMU( tr.whiteImage, TB_NORMALMAP3 );
					}

					if (pStage->bundle[TB_DELUXEMAP].image[0])
					{
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_DELUXEMAP], TB_DELUXEMAP);
						enableTextures[1] = 1.0f;
					}
					else if (r_deluxeMapping->integer)
					{
						GL_BindToTMU( tr.whiteImage, TB_DELUXEMAP );
					}

					if (pStage->bundle[TB_SPECULARMAP].image[0])
					{
						R_BindAnimatedImageToTMU( &pStage->bundle[TB_SPECULARMAP], TB_SPECULARMAP);
						enableTextures[2] = 1.0f;
					}
					else if (r_specularMapping->integer)
					{
						GL_BindToTMU( tr.whiteImage, TB_SPECULARMAP );
					}
				}

				if (backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS))
				{
					enableTextures[3] = 0.0f;
				}
				else
				{
					enableTextures[3] = (r_cubeMapping->integer >= 1 && !(tr.viewParms.flags & VPF_NOCUBEMAPS) && input->cubemapIndex) ? 1.0f : 0.0f;
				}
			}

			GLSL_SetUniformVec4(sp, UNIFORM_ENABLETEXTURES, enableTextures);
		}
		else if ( pStage->bundle[1].image[0] != 0 )
		{
			R_BindAnimatedImageToTMU( &pStage->bundle[0], 0 );
			R_BindAnimatedImageToTMU( &pStage->bundle[1], 1 );
		}
		else
		{
			//
			// set state
			//
			R_BindAnimatedImageToTMU( &pStage->bundle[0], 0 );
		}


		while (1)
		{
			if (!(backEnd.depthFill || (tr.viewParms.flags & VPF_SHADOWPASS)))
			{
				if (pStage->bundle[TB_SPLATCONTROLMAP].image[0] && pStage->bundle[TB_SPLATCONTROLMAP].image[0] != tr.blackImage)
				{
					//ri->Printf(PRINT_WARNING, "Image %s bound to splat control map %i x %i.\n", pStage->bundle[TB_SPLATCONTROLMAP].image[0]->imgName, pStage->bundle[TB_SPLATCONTROLMAP].image[0]->width, pStage->bundle[TB_SPLATCONTROLMAP].image[0]->height);
					GL_BindToTMU(pStage->bundle[TB_SPLATCONTROLMAP].image[0], TB_SPLATCONTROLMAP);
				}
				else
				{
					GL_BindToTMU(tr.defaultSplatControlImage, TB_SPLATCONTROLMAP); // really need to make a blured (possibly also considering heightmap) version of this...
				}
			}

			if (isGrass && passNum == 1 && sp2)
			{// Switch to grass geometry shader, once... Repeats will reuse it...
				sp = sp2;
				sp2 = NULL;

				GLSL_BindProgram(sp);

				stateBits = GLS_DEPTHMASK_TRUE;

				RB_SetMaterialBasedProperties(sp, pStage, stage);

				GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);

				GLSL_SetUniformMatrix16(sp, UNIFORM_MODELMATRIX, backEnd.ori.transformMatrix);
				GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

				GLSL_SetUniformMatrix16(sp, UNIFORM_NORMALMATRIX, MATRIX_NORMAL);

				GLSL_SetUniformVec3(sp, UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);
				GLSL_SetUniformFloat(sp, UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);

				GLSL_SetUniformVec3(sp, UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);

				GL_BindToTMU( tr.grassImage[0], TB_DIFFUSEMAP );
				GL_BindToTMU( tr.grassImage[1], TB_SPLATMAP1 );
				GL_BindToTMU( tr.grassImage[2], TB_SPLATMAP2 );
				GL_BindToTMU( tr.seaGrassImage, TB_OVERLAYMAP );

				vec4_t l10;
				VectorSet4(l10, r_foliageDistance->value, r_foliageDensity->value, MAP_WATER_LEVEL, 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL10, l10);

				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTCOLOR,   backEnd.refdef.sunCol);
				GLSL_SetUniformVec4(sp, UNIFORM_PRIMARYLIGHTORIGIN,  backEnd.refdef.sunDir);

				GL_BindToTMU( tr.defaultGrassMapImage, TB_SPLATCONTROLMAP );

				/*
				if (r_sunlightMode->integer && (r_sunlightSpecular->integer || (backEnd.viewParms.flags & VPF_USESUNLIGHT)))
				{
					if (backEnd.viewParms.flags & VPF_USESUNLIGHT)
					{
						GL_BindToTMU(tr.screenShadowImage, TB_SHADOWMAP);
					}
					else
					{
						GL_BindToTMU(tr.whiteImage, TB_SHADOWMAP);
					}
				}
				*/
			}
			else if (isGrass && passNum > r_foliagePasses->integer && sp3)
			{// Switch to pebbles geometry shader, once... Repeats will reuse it...
				sp = sp3;
				sp3 = NULL;

				GLSL_BindProgram(sp);

				stateBits = GLS_DEPTHMASK_TRUE;

				RB_SetMaterialBasedProperties(sp, pStage, stage);

				GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);

				GLSL_SetUniformMatrix16(sp, UNIFORM_MODELMATRIX, backEnd.ori.transformMatrix);
				GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
				GLSL_SetUniformMatrix16(sp, UNIFORM_NORMALMATRIX, MATRIX_NORMAL);

				GLSL_SetUniformVec3(sp, UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);
				GLSL_SetUniformFloat(sp, UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);

				GLSL_SetUniformVec3(sp, UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);

				GL_BindToTMU(tr.pebblesImage[0], TB_DIFFUSEMAP);
				GL_BindToTMU(tr.pebblesImage[1], TB_SPLATMAP1);
				GL_BindToTMU(tr.pebblesImage[2], TB_SPLATMAP2);
				GL_BindToTMU(tr.pebblesImage[3], TB_OVERLAYMAP );

				vec4_t l10;
				float tessOffset = useTesselation ? 7.5 : 0.0;
				VectorSet4(l10, r_pebblesDistance->value, r_foliageDensity->value, MAP_WATER_LEVEL, tessOffset);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL10, l10);

				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTCOLOR, backEnd.refdef.sunCol);
				GLSL_SetUniformVec4(sp, UNIFORM_PRIMARYLIGHTORIGIN, backEnd.refdef.sunDir);

				GL_BindToTMU(tr.defaultGrassMapImage, TB_SPLATCONTROLMAP);

				/*
				if (r_sunlightMode->integer && (r_sunlightSpecular->integer || (backEnd.viewParms.flags & VPF_USESUNLIGHT)))
				{
					if (backEnd.viewParms.flags & VPF_USESUNLIGHT)
					{
						GL_BindToTMU(tr.screenShadowImage, TB_SHADOWMAP);
					}
					else
					{
						GL_BindToTMU(tr.whiteImage, TB_SHADOWMAP);
					}
				}
				*/
			}
			else if (isPebbles && passNum == 1 && sp2)
			{// Switch to pebbles geometry shader, once... Repeats will reuse it...
				sp = sp2;
				sp2 = NULL;

				GLSL_BindProgram(sp);

				stateBits = GLS_DEPTHMASK_TRUE;

				RB_SetMaterialBasedProperties(sp, pStage, stage);

				GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);

				GLSL_SetUniformMatrix16(sp, UNIFORM_MODELMATRIX, backEnd.ori.transformMatrix);
				GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
				GLSL_SetUniformMatrix16(sp, UNIFORM_NORMALMATRIX, MATRIX_NORMAL);

				GLSL_SetUniformVec3(sp, UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);
				GLSL_SetUniformFloat(sp, UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);

				GLSL_SetUniformVec3(sp, UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);

				GL_BindToTMU(tr.pebblesImage[0], TB_DIFFUSEMAP);
				GL_BindToTMU(tr.pebblesImage[1], TB_SPLATMAP1);
				GL_BindToTMU(tr.pebblesImage[2], TB_SPLATMAP2);
				GL_BindToTMU(tr.pebblesImage[3], TB_OVERLAYMAP);

				vec4_t l10;
				float tessOffset = useTesselation ? 7.5 : 0.0;
				VectorSet4(l10, r_pebblesDistance->value, r_foliageDensity->value, MAP_WATER_LEVEL, tessOffset);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL10, l10);

				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
				GLSL_SetUniformVec3(sp, UNIFORM_PRIMARYLIGHTCOLOR, backEnd.refdef.sunCol);
				GLSL_SetUniformVec4(sp, UNIFORM_PRIMARYLIGHTORIGIN, backEnd.refdef.sunDir);

				GL_BindToTMU(tr.defaultGrassMapImage, TB_SPLATCONTROLMAP);

				/*
				if (r_sunlightMode->integer && (r_sunlightSpecular->integer || (backEnd.viewParms.flags & VPF_USESUNLIGHT)))
				{
					if (backEnd.viewParms.flags & VPF_USESUNLIGHT)
					{
						GL_BindToTMU(tr.screenShadowImage, TB_SHADOWMAP);
					}
					else
					{
						GL_BindToTMU(tr.whiteImage, TB_SHADOWMAP);
					}
				}
				*/
			}

			if (isWater && r_glslWater->integer && WATER_ENABLED && MAP_WATER_LEVEL > -131072.0)
			{// Attach dummy water output textures...
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					stateBits = GLS_DEFAULT | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE;
					tess.shader->cullType = CT_TWO_SIDED; // Always...
					GLSL_AttachWaterTextures();
				}
				else
				{
					break;
				}
			}
#ifdef __USE_GLOW_DETAIL_BUFFERS__
			else if (index & LIGHTDEF_USE_GLOW_BUFFER)
			{
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					GLSL_AttachGlowTextures();
				}
			}
			else if (index & LIGHTDEF_IS_DETAIL)
			{
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					GLSL_AttachGenericTextures();
				}
			}
#endif //__USE_GLOW_DETAIL_BUFFERS__

			qboolean tesselation = qfalse;

			if (isGrass && passNum > 0 && r_foliage->integer)
			{// Geometry grass drawing passes...
				vec4_t l8;
				VectorSet4(l8, (float)passNum, 0.0, 0.0, 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL8, l8);

				GL_Cull( CT_TWO_SIDED );
			}
			else if (isGrass && passNum > r_foliagePasses->integer && r_pebbles->integer)
			{// Geometry pebbles drawing passes...
				vec4_t l8;
				VectorSet4(l8, (float)passNum, 0.0, 0.0, 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL8, l8);

				GL_Cull(CT_TWO_SIDED);
			}
			else if (isPebbles && passNum > 0 && r_pebbles->integer)
			{// Geometry pebbles drawing passes...
				vec4_t l8;
				VectorSet4(l8, (float)passNum, 0.0, 0.0, 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL8, l8);

				GL_Cull(CT_TWO_SIDED);
			}
			else if (r_tesselation->integer && sp->tesselation)
			{
				tesselation = qtrue;

				//float tessInner = RB_GetTesselationInnerLevel(tess.shader->surfaceFlags & MATERIAL_MASK);
				//float tessOuter = tessInner;
				//float tessAlpha = RB_GetTesselationAlphaLevel(tess.shader->surfaceFlags & MATERIAL_MASK);

				vec4_t l10;
				VectorSet4(l10, tessAlpha, tessInner, tessOuter, 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL10, l10);
			}

			vec4_t l9;
			VectorSet4(l9, r_testshaderValue1->value, r_testshaderValue2->value, r_testshaderValue3->value, r_testshaderValue4->value);
			GLSL_SetUniformVec4(sp, UNIFORM_LOCAL9, l9);

			if (isWater && r_glslWater->integer && WATER_ENABLED && MAP_WATER_LEVEL > -131072.0)
			{// Attach dummy water output textures...
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					vec4_t passInfo;
					VectorSet4(passInfo, /*passNum*/0.0, r_waterWaveHeight->value, 0.0, 0.0);
					GLSL_SetUniformVec4(sp, UNIFORM_LOCAL10, passInfo);
				}
			}

			if (r_proceduralSun->integer && tess.shader == tr.sunShader)
			{// Procedural sun...
				vec4_t loc;
				VectorSet4(loc, SUN_COLOR_MAIN[0], SUN_COLOR_MAIN[1], SUN_COLOR_MAIN[2], 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL7, loc);

				VectorSet4(loc, SUN_COLOR_SECONDARY[0], SUN_COLOR_SECONDARY[1], SUN_COLOR_SECONDARY[2], 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL8, loc);

				VectorSet4(loc, SUN_COLOR_TERTIARY[0], SUN_COLOR_TERTIARY[1], SUN_COLOR_TERTIARY[2], 0.0);
				GLSL_SetUniformVec4(sp, UNIFORM_LOCAL9, loc);

				GL_Cull(CT_TWO_SIDED);
			}

			UpdateTexCoords (pStage);

			GL_State( stateBits );

			//
			// draw
			//

			if (input->multiDrawPrimitives)
			{
				R_DrawMultiElementsVBO(input->multiDrawPrimitives, input->multiDrawMinIndex, input->multiDrawMaxIndex, input->multiDrawNumIndexes, input->multiDrawFirstIndex, input->numVertexes, tesselation);
			}
			else
			{
				R_DrawElementsVBO(input->numIndexes, input->firstIndex, input->minIndex, input->maxIndex, input->numVertexes, tesselation);
			}

			if (isWater && r_glslWater->integer && WATER_ENABLED && MAP_WATER_LEVEL > -131072.0)
			{// Unattach dummy water output textures...
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					GLSL_AttachTextures();
				}
			}
#ifdef __USE_GLOW_DETAIL_BUFFERS__
			else if (index & LIGHTDEF_USE_GLOW_BUFFER)
			{
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					GLSL_AttachTextures();
				}
			}
			else if (index & LIGHTDEF_IS_DETAIL)
			{
				if (glState.currentFBO == tr.renderFbo)
				{// Only attach textures when doing a render pass...
					GLSL_AttachTextures();
				}
			}
#endif //__USE_GLOW_DETAIL_BUFFERS__

			passNum++;

			if (multiPass && passNum > passMax)
			{// Finished all passes...
				multiPass = qfalse;
			}

			if (!multiPass)
			{
				if ((isGrass && r_foliage->integer) || (isPebbles && r_pebbles->integer))
				{// Set cull type back to original... Just in case...
					GL_Cull( input->shader->cullType );
				}

				break;
			}
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap ) )
		{
			break;
		}

		if (backEnd.depthFill)
			break;
	}
}

void RB_ExternalIterateStagesGeneric( shaderCommands_t *input )
{
	RB_IterateStagesGeneric( input );
}


static void RB_RenderShadowmap( shaderCommands_t *input )
{
	int deformGen;
	vec5_t deformParams;

	ComputeDeformValues(&deformGen, deformParams);

	{
		shaderProgram_t *sp = &tr.shadowmapShader;

		vec4_t vector;

		GLSL_BindProgram(sp);

		GLSL_SetUniformMatrix16(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

		GLSL_SetUniformMatrix16(sp, UNIFORM_MODELMATRIX, backEnd.ori.transformMatrix);

		GLSL_SetUniformFloat(sp, UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);

		GLSL_SetUniformInt(sp, UNIFORM_DEFORMGEN, deformGen);
		if (deformGen != DGEN_NONE)
		{
			GLSL_SetUniformFloat5(sp, UNIFORM_DEFORMPARAMS, deformParams);
			GLSL_SetUniformFloat(sp, UNIFORM_TIME, tess.shaderTime);
		}

		VectorCopy(backEnd.viewParms.ori.origin, vector);
		vector[3] = 1.0f;
		GLSL_SetUniformVec4(sp, UNIFORM_LIGHTORIGIN, vector);
		GLSL_SetUniformFloat(sp, UNIFORM_LIGHTRADIUS, backEnd.viewParms.zFar);

		GL_State( 0 );

		//
		// do multitexture
		//
		//if ( pStage->glslShaderGroup )
		{
			//
			// draw
			//

			if (input->multiDrawPrimitives)
			{
				R_DrawMultiElementsVBO(input->multiDrawPrimitives, input->multiDrawMinIndex, input->multiDrawMaxIndex, input->multiDrawNumIndexes, input->multiDrawFirstIndex, input->numVertexes, qfalse);
			}
			else
			{
				R_DrawElementsVBO(input->numIndexes, input->firstIndex, input->minIndex, input->maxIndex, input->numVertexes, qfalse);
			}
		}
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	unsigned int vertexAttribs = 0;

	input = &tess;

	if (!input->numVertexes || !input->numIndexes)
	{
		return;
	}

	if (tess.useInternalVBO)
	{
		RB_DeformTessGeometry();
	}

	vertexAttribs = RB_CalcShaderVertexAttribs( input->shader );

	if (tess.useInternalVBO)
	{
		RB_UpdateVBOs(vertexAttribs);
	}
	else
	{
		backEnd.pc.c_staticVboDraws++;
	}

	//
	// log this call
	//
	if ( r_logFile->integer )
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	if ((backEnd.viewParms.flags & VPF_DEPTHSHADOW))
	{
		//GL_Cull( CT_TWO_SIDED );

		if (input->shader->cullType == CT_TWO_SIDED)
			GL_Cull( CT_TWO_SIDED );
		else if (input->shader->cullType == CT_FRONT_SIDED)
			GL_Cull( CT_BACK_SIDED );
		else
			GL_Cull( CT_FRONT_SIDED );

	}
	else
		GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// Set vertex attribs and pointers
	//
	GLSL_VertexAttribsState(vertexAttribs);

	//
	// UQ1: Set up any special shaders needed for this surface/contents type...
	//

	if ((tess.shader->isWater && r_glslWater->integer && WATER_ENABLED)
		|| (tess.shader->contentFlags & CONTENTS_WATER)
		/*|| (tess.shader->contentFlags & CONTENTS_LAVA)*/
		|| (tess.shader->surfaceFlags & MATERIAL_MASK) == MATERIAL_WATER)
	{
		if (input && input->xstages[0] && input->xstages[0]->isWater == 0 && r_glslWater->integer && WATER_ENABLED) // In case it is already set, no need looping more then once on the same shader...
		{
			int isWater = 1;

			if (tess.shader->contentFlags & CONTENTS_LAVA)
				isWater = 2;

			for ( int stage = 0; stage < MAX_SHADER_STAGES; stage++ )
			{
				if (input->xstages[stage])
				{
					input->xstages[stage]->isWater = isWater;
					//input->xstages[stage]->glslShaderGroup = tr.lightallShader;
				}
			}
		}
	}

	//
	// render depth if in depthfill mode
	//
	if (backEnd.depthFill)
	{
		/*if (tr.currentEntity && tr.currentEntity != &tr.worldEntity)
		{// UQ1: Hack!!! Disabled... These have cull issues...
			if (backEnd.currentEntity->e.ignoreCull)
			{
				//model_t	*model = R_GetModelByHandle(backEnd.currentEntity->e.hModel);
				//if (model)
				//	ri->Printf(PRINT_WARNING, "Cull ignored on model type: %i. name: %s.\n", model->type, model->name);
				//else
				//	ri->Printf(PRINT_WARNING, "Cull ignored on unknown.\n");
				if (input->shader->polygonOffset)
				{
					qglDisable(GL_POLYGON_OFFSET_FILL);
				}

				//if (model->type == MOD_BRUSH || model->data.bmodel || !tr.currentModel)
				return;
			}
			else if (tr.currentEntity->e.reType == RT_MODEL)
			{
				model_t	*model = R_GetModelByHandle(backEnd.currentEntity->e.hModel);
				
				if (model->type == MOD_BRUSH || model->data.bmodel || !tr.currentModel)
					return;
			}
		}*/

		RB_IterateStagesGeneric(input);

		//
		// reset polygon offset
		//
		if ( input->shader->polygonOffset )
		{
			qglDisable( GL_POLYGON_OFFSET_FILL );
		}

		return;
	}

	//
	// render shadowmap if in shadowmap mode
	//
	if (backEnd.viewParms.flags & VPF_SHADOWMAP)
	{
		if ( input->shader->sort == SS_OPAQUE )
		{
			RB_RenderShadowmap( input );
		}
		//
		// reset polygon offset
		//
		if ( input->shader->polygonOffset )
		{
			qglDisable( GL_POLYGON_OFFSET_FILL );
		}

		return;
	}

	//
	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

#ifdef __PSHADOWS__
	ProjectPshadowVBOGLSL();
#endif

	// Now check for surfacesprites.
#if 0
	if (r_surfaceSprites->integer)
	{
		//for ( int stage = 1; stage < MAX_SHADER_STAGES/*tess.shader->numUnfoggedPasses*/; stage++ )
		for ( int stage = 0; stage < MAX_SHADER_STAGES/*tess.shader->numUnfoggedPasses*/; stage++ )
		{
			if (tess.xstages[stage])
			{
				if (tess.xstages[stage]->ss && tess.xstages[stage]->ss->surfaceSpriteType)
				{	// Draw the surfacesprite
					RB_DrawSurfaceSprites( tess.xstages[stage], input);
				}
			}
		}
	}
#endif

	//
	// now do fog
	//
	if ( r_fog->integer && tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0 || input->numVertexes == 0) {
		return;
	}

	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		ri->Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		ri->Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer ) {
		DrawTris (input);
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.firstIndex = 0;
	tess.multiDrawPrimitives = 0;

	GLimp_LogComment( "----------\n" );
}
