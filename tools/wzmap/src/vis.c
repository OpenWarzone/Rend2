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
#define VIS_C



/* dependencies */
#include "q3map2.h"




void PlaneFromWinding (fixedWinding_t *w, visPlane_t *plane)
{
	vec3_t		v1, v2;

// calc plane
	VectorSubtract (w->points[2], w->points[1], v1);
	VectorSubtract (w->points[0], w->points[1], v2);
	CrossProduct (v2, v1, plane->normal);
	VectorNormalize (plane->normal, plane->normal);
	plane->dist = DotProduct (w->points[0], plane->normal);
}


/*
NewFixedWinding()
returns a new fixed winding
ydnar: altered this a bit to reconcile multiply-defined winding_t
*/

fixedWinding_t *NewFixedWinding( uint32_t points )
{
	fixedWinding_t	*w;
	uint32_t			size;
	
	if (points > MAX_POINTS_ON_WINDING)
		Error ("NewWinding: %i points", points);
	
	size = (int)((fixedWinding_t *)0)->points[points];
	w = (fixedWinding_t *)safe_malloc (size);
	memset (w, 0, size);
	
	return w;
}



void prl(leaf_t *l)
{
	uint32_t			i;
	vportal_t	*p;
	visPlane_t	pl;
	
	for (i=0 ; i<l->numportals ; i++)
	{
		p = l->portals[i];
		pl = p->plane;
		Sys_Printf ("portal %4i to leaf %4i : %7.1f : (%4.1f, %4.1f, %4.1f)\n",(int)(p-portals),p->leaf,pl.dist, pl.normal[0], pl.normal[1], pl.normal[2]);
	}
}


//=============================================================================

/*
=============
SortPortals

Sorts the portals from the least complex, so the later ones can reuse
the earlier information.
=============
*/
int PComp (const void *a, const void *b)
{
	if ( (*(vportal_t **)a)->nummightsee == (*(vportal_t **)b)->nummightsee)
		return 0;
	if ( (*(vportal_t **)a)->nummightsee < (*(vportal_t **)b)->nummightsee)
		return -1;
	return 1;
}
void SortPortals (void)
{
	uint32_t		i;
	
	for (i=0 ; i<numportals*2 ; i++)
		sorted_portals[i] = &portals[i];

	if (nosort)
		return;
	qsort (sorted_portals, numportals*2, sizeof(sorted_portals[0]), PComp);
}


/*
==============
LeafVectorFromPortalVector
==============
*/
int LeafVectorFromPortalVector (byte *portalbits, byte *leafbits)
{
	uint32_t			i, j, leafnum;
	vportal_t	*p;
	uint32_t			c_leafs;


	for (i=0 ; i<numportals*2 ; i++)
	{
		if (portalbits[i>>3] & (1<<(i&7)) )
		{
			p = portals+i;
			leafbits[p->leaf>>3] |= (1<<(p->leaf&7));
		}
	}

	for (j = 0; j < portalclusters; j++)
	{
		leafnum = j;
		while (leafs[leafnum].merged >= 0)
			leafnum = leafs[leafnum].merged;
		//if the merged leaf is visible then the original leaf is visible
		if (leafbits[leafnum>>3] & (1<<(leafnum&7)))
		{
			leafbits[j>>3] |= (1<<(j&7));
		}
	}

	c_leafs = CountBits (leafbits, portalclusters);

	return c_leafs;
}


/*
===============
ClusterMerge

Merges the portal visibility for a leaf
===============
*/
void ClusterMerge (uint32_t leafnum)
{
	leaf_t		*leaf;
	byte		portalvector[MAX_PORTALS/8];
	byte		uncompressed[MAX_MAP_LEAFS/8];
	uint32_t			i, j;
	uint32_t			numvis, mergedleafnum;
	vportal_t	*p;
	uint32_t			pnum;

	// OR together all the portalvis bits

	mergedleafnum = leafnum;
	while(leafs[mergedleafnum].merged >= 0)
		mergedleafnum = leafs[mergedleafnum].merged;

	memset (portalvector, 0, portalbytes);
	leaf = &leafs[mergedleafnum];
	for (i = 0; i < leaf->numportals; i++)
	{
		p = leaf->portals[i];
		if (p->removed)
			continue;

		if (p->status != stat_done)
			Error ("portal not done");
		for (j=0 ; j<portallongs ; j++)
			((long *)portalvector)[j] |= ((long *)p->portalvis)[j];
		pnum = p - portals;
		portalvector[pnum>>3] |= 1<<(pnum&7);
	}

	memset (uncompressed, 0, leafbytes);

	uncompressed[mergedleafnum>>3] |= (1<<(mergedleafnum&7));
	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector (portalvector, uncompressed);

//	if (uncompressed[leafnum>>3] & (1<<(leafnum&7)))
//		Sys_Warning ("Leaf portals saw into leaf");
		
//	uncompressed[leafnum>>3] |= (1<<(leafnum&7));

	numvis++;		// count the leaf itself

	totalvis += numvis;

	//Sys_Printf ("cluster %4i : %4i visible\n", leafnum, numvis);

	memcpy (bspVisBytes + VIS_HEADER_SIZE + leafnum*leafbytes, uncompressed, leafbytes);
}

/*
==================
CalcPortalVis
==================
*/
void CalcPortalVis (void)
{
#ifdef MREDEBUG
	Sys_Printf("%6d portals out of %d", 0, numportals*2);
	//get rid of the counter
	RunThreadsOnIndividual ("CalcPortalVis", numportals*2, qfalse, PortalFlow);
#else
	RunThreadsOnIndividual ("CalcPortalVis", numportals*2, qtrue, PortalFlow);
#endif

}

/*
==================
CalcPassageVis
==================
*/
void CalcPassageVis(void)
{
	PassageMemory();

#ifdef MREDEBUG
	Sys_Printf("%6d portals out of %d", 0, numportals*2);
	RunThreadsOnIndividual ("CreatePassages", numportals*2, qfalse, CreatePassages);
	Sys_Printf("\n");
	Sys_Printf("%6d portals out of %d", 0, numportals*2);
	RunThreadsOnIndividual ("PassageFlow", numportals*2, qfalse, PassageFlow);
	Sys_Printf("\n");
#else
	Sys_PrintHeading ( "--- CreatePassages ---\n" );
	RunThreadsOnIndividual( "CreatePassages", numportals*2, qtrue, CreatePassages );

	Sys_PrintHeading ( "--- PassageFlow ---\n" );
	RunThreadsOnIndividual( "PassageFlow", numportals * 2, qtrue, PassageFlow );
#endif
}

/*
==================
CalcPassagePortalVis
==================
*/

void CalcPassagePortalVis(void)
{
	PassageMemory();

#ifdef MREDEBUG
	Sys_Printf("%6d portals out of %d", 0, numportals*2);
	RunThreadsOnIndividual ("CreatePassages", numportals*2, qfalse, CreatePassages);
	Sys_Printf("\n");
	Sys_Printf("%6d portals out of %d", 0, numportals*2);
	RunThreadsOnIndividual ("PassagePortalFlow", numportals*2, qfalse, PassagePortalFlow);
	Sys_Printf("\n");
#else
	Sys_PrintHeading ( "--- CreatePassages ---\n" );
	RunThreadsOnIndividual( "CreatePassages", numportals * 2, qtrue, CreatePassages);
	
	Sys_PrintHeading ( "--- PassagePortalFlow  ---\n" );
	RunThreadsOnIndividual( "PassagePortalFlow", numportals * 2, qtrue, PassagePortalFlow );
#endif
}

/*
==================
CalcFastVis
==================
*/
void CalcFastVis(void)
{
	uint32_t		i;

	// fastvis just uses mightsee for a very loose bound
	for (i=0 ; i<numportals*2 ; i++)
	{
		portals[i].portalvis = portals[i].portalflood;
		portals[i].status = stat_done;
	}
}

/*
==================
CalcVis
==================
*/
void CalcVis (void)
{
	uint32_t			i;
	const char	*value;
	
	/* ydnar: rr2do2's farplane code */
	farPlaneDist = 0.0f;
	value = ValueForKey( &entities[ 0 ], "_farplanedist" );		/* proper '_' prefixed key */
	if( value[ 0 ] == '\0' )
		value = ValueForKey( &entities[ 0 ], "fogclip" );		/* wolf compatibility */
	if( value[ 0 ] == '\0' )
		value = ValueForKey( &entities[ 0 ], "distancecull" );	/* sof2 compatibility */
	if( value[ 0 ] != '\0' )
	{
		farPlaneDist = atof( value );
		if( farPlaneDist > 0.0f )
			Sys_Printf( "farplane distance = %.1f\n", farPlaneDist );
		else
			farPlaneDist = 0.0f;
	}
	
	/* base portal vis */
	Sys_PrintHeading ( "--- BasePortalVis ---\n" );
	RunThreadsOnIndividual( "BasePortalVis", numportals * 2, qtrue, BasePortalVis );

	portals = (vportal_t*)realloc(portals, sizeof(vportal_t)*numportals * 2);
	//sorted_portals = (vportal_t*)realloc(sorted_portals, sizeof(vportal_t)*numportals*2);

	/* fast/passage vis */
	SortPortals ();

	if (fastvis)
		CalcFastVis();
	else if ( noPassageVis )
		CalcPortalVis();
	else if ( passageVisOnly )
		CalcPassageVis();
	else
		CalcPassagePortalVis();

	/* assemble the leaf vis lists by oring and compressing the portal lists */
	Sys_PrintHeading ( "--- CreateLeafVis ---\n" );
	for( i = 0; i < portalclusters; i++ )
		ClusterMerge( i );

	/* emit some stats */
	Sys_Printf( "%9d clusters\n", portalclusters );
	Sys_Printf( "%9d total visible clusters\n", totalvis );
	Sys_Printf( "%9d average clusters visible\n", totalvis / portalclusters );
}

/*
==================
SetPortalSphere
==================
*/
void SetPortalSphere (vportal_t *p)
{
	uint32_t		i;
	vec3_t	total, dist;
	fixedWinding_t	*w;
	float	r, bestr;

	w = p->winding;
	VectorCopy (vec3_origin, total);
	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorAdd (total, w->points[i], total);
	}
	
	for (i=0 ; i<3 ; i++)
		total[i] /= w->numpoints;

	bestr = 0;		
	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorSubtract (w->points[i], total, dist);
		r = VectorLength (dist);
		if (r > bestr)
			bestr = r;
	}
	VectorCopy (total, p->origin);
	p->radius = bestr;
}

/*
=============
Winding_PlanesConcave
=============
*/
#define WCONVEX_EPSILON		0.2

int Winding_PlanesConcave(fixedWinding_t *w1, fixedWinding_t *w2,
							 vec3_t normal1, vec3_t normal2,
							 float dist1, float dist2)
{
	uint32_t i;

	if (!w1 || !w2) return qfalse;

	// check if one of the points of winding 1 is at the front of the plane of winding 2
	for (i = 0; i < w1->numpoints; i++)
	{
		if (DotProduct(normal2, w1->points[i]) - dist2 > WCONVEX_EPSILON) return qtrue;
	}
	// check if one of the points of winding 2 is at the front of the plane of winding 1
	for (i = 0; i < w2->numpoints; i++)
	{
		if (DotProduct(normal1, w2->points[i]) - dist1 > WCONVEX_EPSILON) return qtrue;
	}

	return qfalse;
}

/*
============
TryMergeLeaves
============
*/
int TryMergeLeaves(uint32_t l1num, uint32_t l2num)
{
	uint32_t i, j, k, n, numportals;
	visPlane_t plane1, plane2;
	leaf_t *l1, *l2;
	vportal_t *p1, *p2;
	vportal_t *portals[MAX_PORTALS_ON_LEAF];

	for (k = 0; k < 2; k++)
	{
		if (k) l1 = &leafs[l1num];
		else l1 = &faceleafs[l1num];
		for (i = 0; i < l1->numportals; i++)
		{
			p1 = l1->portals[i];
			if (p1->leaf == l2num) continue;
			for (n = 0; n < 2; n++)
			{
				if (n) l2 = &leafs[l2num];
				else l2 = &faceleafs[l2num];
				for (j = 0; j < l2->numportals; j++)
				{
					p2 = l2->portals[j];
					if (p2->leaf == l1num) continue;
					//
					plane1 = p1->plane;
					plane2 = p2->plane;
					if (Winding_PlanesConcave(p1->winding, p2->winding, plane1.normal, plane2.normal, plane1.dist, plane2.dist))
						return qfalse;
				}
			}
		}
	}
	
	for (k = 0; k < 2; k++)
	{
		if (k)
		{
			l1 = &leafs[l1num];
			l2 = &leafs[l2num];
		}
		else
		{
			l1 = &faceleafs[l1num];
			l2 = &faceleafs[l2num];
		}
		numportals = 0;
		//the leaves can be merged now
		for (i = 0; i < l1->numportals; i++)
		{
			p1 = l1->portals[i];
			if (p1->leaf == l2num)
			{
				p1->removed = qtrue;
				continue;
			}
			portals[numportals++] = p1;
		}
		for (j = 0; j < l2->numportals; j++)
		{
			p2 = l2->portals[j];

			if (p2->leaf == l1num)
			{
				p2->removed = qtrue;
				continue;
			}

			if (numportals +1 > MAX_PORTALS_ON_LEAF) 
			{
				Error("Portal %i > MAX_PORTALS_ON_LEAF\n", numportals+1); 
			}

			portals[numportals++] = p2;
		}
		for (i = 0; i < numportals; i++)
		{
			l2->portals[i] = portals[i];
		}
		l2->numportals = numportals;
		l1->merged = l2num;
	}
	
	return qtrue;
}

/*
============
UpdatePortals
============
*/
void UpdatePortals(void)
{
	uint32_t i;
	vportal_t *p;

	for (i = 0; i < numportals * 2; i++)
	{
		p = &portals[i];
		if (p->removed)
			continue;
		while(leafs[p->leaf].merged >= 0)
			p->leaf = leafs[p->leaf].merged;
	}
}

/*
============
MergeLeaves

try to merge leaves but don't merge through hint splitters
============
*/
void MergeLeaves(void)
{
	uint32_t i, j, nummerges, totalnummerges;
	leaf_t *leaf;
	vportal_t *p;

	totalnummerges = 0;

	do
	{
		nummerges = 0;

		for (i = 0; i < portalclusters; i++)
		{
			leaf = &leafs[i];
			//if this leaf is merged already

			/* ydnar: vmods: merge all non-hint portals */
			if( leaf->merged >= 0 && hint == qfalse )
				continue;

			//Sys_Printf("Leaf %i. numportals %i.\n", i, leaf->numportals);

			for (j = 0; j < leaf->numportals; j++)
			{
				p = leaf->portals[j];
				
				if (p->removed)
					continue;

				//never merge through hint portals
				if (p->hint)
					continue;

				if (TryMergeLeaves(i, p->leaf))
				{
					UpdatePortals();
					nummerges++;
					break;
				}
			}
		}

		totalnummerges += nummerges;
	} while (nummerges);

	Sys_Printf("%6d leaves merged\n", totalnummerges);
}

/*
============
TryMergeWinding
============
*/
#define	CONTINUOUS_EPSILON	0.005

fixedWinding_t *TryMergeWinding (fixedWinding_t *f1, fixedWinding_t *f2, vec3_t planenormal)
{
	vec_t		*p1, *p2, *p3, *p4, *back;
	fixedWinding_t	*newf;
	uint32_t			i, j, k, l;
	vec3_t		normal, delta;
	vec_t		dot;
	qboolean	keep1, keep2;
	

	//
	// find a common edge
	//	
	p1 = p2 = NULL;	// stop compiler warning
	j = 0;			// 
	
	for (i = 0; i < f1->numpoints; i++)
	{
		p1 = f1->points[i];
		p2 = f1->points[(i+1) % f1->numpoints];
		for (j = 0; j < f2->numpoints; j++)
		{
			p3 = f2->points[j];
			p4 = f2->points[(j+1) % f2->numpoints];
			for (k = 0; k < 3; k++)
			{
				if (fabs(p1[k] - p4[k]) > 0.1)//EQUAL_EPSILON) //ME
					break;
				if (fabs(p2[k] - p3[k]) > 0.1)//EQUAL_EPSILON) //ME
					break;
			} //end for
			if (k==3)
				break;
		} //end for
		if (j < f2->numpoints)
			break;
	} //end for
	
	if (i == f1->numpoints)
		return NULL;			// no matching edges

	//
	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	//
	back = f1->points[(i+f1->numpoints-1)%f1->numpoints];
	VectorSubtract (p1, back, delta);
	CrossProduct (planenormal, delta, normal);
	VectorNormalize (normal, normal);
	
	back = f2->points[(j+2)%f2->numpoints];
	VectorSubtract (back, p1, delta);
	dot = DotProduct (delta, normal);
	if (dot > CONTINUOUS_EPSILON)
		return NULL;			// not a convex polygon
	keep1 = (qboolean)(dot < -CONTINUOUS_EPSILON);
	
	back = f1->points[(i+2)%f1->numpoints];
	VectorSubtract (back, p2, delta);
	CrossProduct (planenormal, delta, normal);
	VectorNormalize (normal, normal);

	back = f2->points[(j+f2->numpoints-1)%f2->numpoints];
	VectorSubtract (back, p2, delta);
	dot = DotProduct (delta, normal);
	if (dot > CONTINUOUS_EPSILON)
		return NULL;			// not a convex polygon
	keep2 = (qboolean)(dot < -CONTINUOUS_EPSILON);

	//
	// build the new polygon
	//
	newf = NewFixedWinding (f1->numpoints + f2->numpoints);
	
	// copy first polygon
	for (k=(i+1)%f1->numpoints ; k != i ; k=(k+1)%f1->numpoints)
	{
		if (k==(i+1)%f1->numpoints && !keep2)
			continue;
		
		VectorCopy (f1->points[k], newf->points[newf->numpoints]);
		newf->numpoints++;
	}
	
	// copy second polygon
	for (l= (j+1)%f2->numpoints ; l != j ; l=(l+1)%f2->numpoints)
	{
		if (l==(j+1)%f2->numpoints && !keep1)
			continue;
		VectorCopy (f2->points[l], newf->points[newf->numpoints]);
		newf->numpoints++;
	}

	return newf;
}

/*
============
MergeLeafPortals
============
*/
void MergeLeafPortals(void)
{
	uint32_t i, j, k, nummerges, hintsmerged;
	leaf_t *leaf;
	vportal_t *p1, *p2;
	fixedWinding_t *w;

	nummerges = 0;
	hintsmerged = 0;
	for (i = 0; i < portalclusters; i++)
	{
		leaf = &leafs[i];
		if (leaf->merged >= 0) continue;
		for (j = 0; j < leaf->numportals; j++)
		{
			p1 = leaf->portals[j];
			if (p1->removed)
				continue;
			for (k = j+1; k < leaf->numportals; k++)
			{
				p2 = leaf->portals[k];
				if (p2->removed)
					continue;
				if (p1->leaf == p2->leaf)
				{
					w = TryMergeWinding(p1->winding, p2->winding, p1->plane.normal);
					if (w)
					{
						free( p1->winding );	//% FreeWinding(p1->winding);
						p1->winding = w;
						if (p1->hint && p2->hint)
							hintsmerged++;
						if (p2->hint)
							p1->hint = qtrue; // p1->hint |= p2->hint;
						SetPortalSphere(p1);
						p2->removed = qtrue;
						nummerges++;
						i--;
						break;
					}
				}
			}
			if (k < leaf->numportals)
				break;
		}
	}
	Sys_Printf("%6d portals merged\n", nummerges);
	Sys_Printf("%6d hint portals merged\n", hintsmerged);
}


/*
============
WritePortals
============
*/
int CountActivePortals(void)
{
	uint32_t num, hints, j;
	vportal_t *p;

	num = 0;
	hints = 0;
	for (j = 0; j < numportals * 2; j++)
	{
		p = portals + j;
		if (p->removed)
			continue;
		if (p->hint)
			hints++;
		num++;
	}
	Sys_Printf("%9d active portals\n", num);
	Sys_Printf("%9d hint portals\n", hints);
	return num;
}

/*
============
WritePortals
============
*/
void WriteFloat (FILE *f, vec_t v);

void WritePortals(char *filename)
{
	uint32_t i, j, num;
	FILE *pf;
	vportal_t *p;
	fixedWinding_t *w;

	// write the file
	pf = fopen (filename, "w");
	if (!pf)
		Error ("Error opening %s", filename);

	num = 0;
	for (j = 0; j < numportals * 2; j++)
	{
		p = portals + j;
		if (p->removed)
			continue;
//		if (!p->hint)
//			continue;
		num++;
	}

	fprintf (pf, "%s\n", PORTALFILE);
	fprintf (pf, "%i\n", 0);
	fprintf (pf, "%i\n", num);// + numfaces);
	fprintf (pf, "%i\n", 0);

	for (j = 0; j < numportals * 2; j++)
	{
		p = portals + j;
		if (p->removed)
			continue;
//		if (!p->hint)
//			continue;
		w = p->winding;
		fprintf (pf,"%i %i %i ",w->numpoints, 0, 0);
		fprintf (pf, "%d ", p->hint);
		for (i=0 ; i<w->numpoints ; i++)
		{
			fprintf (pf,"(");
			WriteFloat (pf, w->points[i][0]);
			WriteFloat (pf, w->points[i][1]);
			WriteFloat (pf, w->points[i][2]);
			fprintf (pf,") ");
		}
		fprintf (pf,"\n");
	}

	/*
	for (j = 0; j < numfaces; j++)
	{
		p = faces + j;
		w = p->winding;
		fprintf (pf,"%i %i %i ",w->numpoints, 0, 0);
		fprintf (pf, "0 ");
		for (i=0 ; i<w->numpoints ; i++)
		{
			fprintf (pf,"(");
			WriteFloat (pf, w->points[i][0]);
			WriteFloat (pf, w->points[i][1]);
			WriteFloat (pf, w->points[i][2]);
			fprintf (pf,") ");
		}
		fprintf (pf,"\n");
	}*/

	fclose (pf);
}

/*
============
LoadPortals
============
*/
void LoadPortals (char *name)
{
	uint32_t	i, j, hint;
	vportal_t	*p;
	leaf_t		*l;
	char		magic[80];
	FILE		*f;
	uint32_t	numpoints;
	fixedWinding_t	*w;
	uint32_t	leafnums[2];
	visPlane_t	plane;
	
	if (!strcmp(name,"-"))
		f = stdin;
	else
	{
		f = fopen(name, "r");
		if (!f)
			Error ("LoadPortals: couldn't read %s\n",name);
	}

	if (fscanf (f,"%79s\n%i\n%i\n%i\n",magic, &portalclusters, &numportals, &numfaces) != 4)
		Error ("LoadPortals: failed to read header");
	if (strcmp(magic,PORTALFILE))
		Error ("LoadPortals: not a portal file");

	Sys_Printf ("%9d portalclusters\n", portalclusters);
	Sys_Printf ("%9d numportals\n", numportals);
	Sys_Printf ("%9d numfaces\n", numfaces);

#if 0
	{
		Sys_Printf("Freeing memory.\n");
		//if (mapplanes) { free(mapplanes); mapplanes = NULL; }
		if (bspLeafs) { free(bspLeafs); bspLeafs = NULL; }
		if (bspPlanes) { free(bspPlanes); bspPlanes = NULL; }
		if (bspBrushes) { free(bspBrushes); bspBrushes = NULL; }
		if (bspBrushSides) { free(bspBrushSides); bspBrushSides = NULL; }
		if (bspNodes) { free(bspNodes); bspNodes = NULL; }
		if (bspLeafSurfaces) { free(bspLeafSurfaces); bspLeafSurfaces = NULL; }
		if (bspLeafBrushes) { free(bspLeafBrushes); bspLeafBrushes = NULL; }
	}
#endif
	
	
	// these counts should take advantage of 64 bit systems automatically
	leafbytes = ((portalclusters+63)&~63)>>3;
	leaflongs = leafbytes/sizeof(long);
	
	portalbytes = ((numportals*2+63)&~63)>>3;
	portallongs = portalbytes/sizeof(long);

	// each file portal is split into two memory portals
	portals = (vportal_t *)safe_malloc(2*numportals*sizeof(vportal_t));
	memset (portals, 0, 2*numportals*sizeof(vportal_t));

	//Sys_Printf("portals: %i MB.\n", (2*numportals*sizeof(vportal_t)) / (1024 * 1024));

#define UQ1_TESTING
#ifdef UQ1_TESTING
	leafs = (leaf_t *)safe_malloc(2*portalclusters*sizeof(leaf_t));
	memset (leafs, 0, portalclusters*sizeof(leaf_t));
#else
	leafs = (leaf_t *)safe_malloc(2*portalclusters*sizeof(leaf_t));
	memset (leafs, 0, 2*portalclusters*sizeof(leaf_t));
#endif

	//Sys_Printf("leafs: %i MB.\n", (2*portalclusters*sizeof(leaf_t)) / (1024 * 1024));

	for (i = 0; i < portalclusters; i++)
		leafs[i].merged = -1;

	numBSPVisBytes = VIS_HEADER_SIZE + portalclusters*leafbytes;

	if (numBSPVisBytes > MAX_MAP_VISIBILITY)
	  Error("MAX_MAP_VISIBILITY exceeded");

	((int *)bspVisBytes)[0] = portalclusters;
	((int *)bspVisBytes)[1] = leafbytes;
		
	for (i=0, p=portals ; i<numportals ; i++)
	{
		if (fscanf (f, "%i %i %i ", &numpoints, &leafnums[0], &leafnums[1]) != 3)
			Error ("LoadPortals: reading portal %i", i);
		if (numpoints > MAX_POINTS_ON_WINDING)
			Error ("LoadPortals: portal %i has too many points", i);
		if ( (unsigned)leafnums[0] > portalclusters
		|| (unsigned)leafnums[1] > portalclusters)
			Error ("LoadPortals: reading portal %i", i);
		if (fscanf (f, "%i ", &hint) != 1)
			Error ("LoadPortals: reading hint state");
		
		w = p->winding = NewFixedWinding (numpoints);
		w->numpoints = numpoints;
		
		for (j=0 ; j<numpoints ; j++)
		{
			double	v[3];
			int		k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			if (fscanf (f, "(%lf %lf %lf ) "
			, &v[0], &v[1], &v[2]) != 3)
				Error ("LoadPortals: reading portal %i", i);
			for (k=0 ; k<3 ; k++)
				w->points[j][k] = v[k];
		}
		fscanf (f, "\n");
		
		// calc plane
		PlaneFromWinding (w, &plane);

		// create forward portal
		l = &leafs[leafnums[0]];
		if (l->numportals == MAX_PORTALS_ON_LEAF)
			Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->num = i+1;
		p->hint = hint ? qtrue : qfalse;
		p->winding = w;
		VectorSubtract (vec3_origin, plane.normal, p->plane.normal);
		p->plane.dist = -plane.dist;
		p->leaf = leafnums[1];
		SetPortalSphere (p);
		p++;
		
		// create backwards portal
		l = &leafs[leafnums[1]];
		if (l->numportals == MAX_PORTALS_ON_LEAF)
			Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->num = i+1;
		p->hint = hint ? qtrue : qfalse;
		p->winding = NewFixedWinding(w->numpoints);
		p->winding->numpoints = w->numpoints;
		for (j=0 ; j<w->numpoints ; j++)
		{
			VectorCopy (w->points[w->numpoints-1-j], p->winding->points[j]);
		}

		p->plane = plane;
		p->leaf = leafnums[0];
		SetPortalSphere (p);
		p++;

	}

	faces = (vportal_t *)safe_malloc(2*numfaces*sizeof(vportal_t));
	memset (faces, 0, 2*numfaces*sizeof(vportal_t));

	faceleafs = (leaf_t *)safe_malloc(2*portalclusters*sizeof(leaf_t));
	memset(faceleafs, 0, portalclusters*sizeof(leaf_t));

	//Sys_Printf("faces: %i MB.\n", (2*numfaces*sizeof(vportal_t)) / (1024 * 1024));
	//Sys_Printf("faceleafs: %i MB.\n", (2*portalclusters*sizeof(leaf_t)) / (1024 * 1024));

	for (i = 0, p = faces; i < numfaces; i++)
	{
		if (fscanf (f, "%i %i ", &numpoints, &leafnums[0]) != 2)
			Error ("LoadPortals: reading portal %i", i);

		w = p->winding = NewFixedWinding (numpoints);
		w->numpoints = numpoints;
		
		for (j=0 ; j<numpoints ; j++)
		{
			double	v[3];
			int		k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			if (fscanf (f, "(%lf %lf %lf ) "
			, &v[0], &v[1], &v[2]) != 3)
				Error ("LoadPortals: reading portal %i", i);
			for (k=0 ; k<3 ; k++)
				w->points[j][k] = v[k];
		}
		fscanf (f, "\n");
		
		// calc plane
		PlaneFromWinding (w, &plane);

		l = &faceleafs[leafnums[0]];
		l->merged = -1;
		if (l->numportals == MAX_PORTALS_ON_LEAF)
			Error ("Leaf with too many faces");
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->num = i+1;
		p->winding = w;
		// normal pointing out of the leaf
		VectorSubtract (vec3_origin, plane.normal, p->plane.normal);
		p->plane.dist = -plane.dist;
		p->leaf = -1;
		SetPortalSphere (p);
		p++;
	}
	
	fclose (f);

	mapplanes = (plane_t*)realloc(mapplanes, sizeof(plane_t)*nummapplanes);
}



/*
===========
VisMain
===========
*/
int VisMain (int argc, char **argv)
{
	char		portalfile[MAX_OS_PATH];
	int			i;
	
	
	/* note it */
	Sys_PrintHeading ( "--- Vis ---\n" );
	
	/* process arguments */
	Sys_PrintHeading ( "--- CommandLine ---\n" );
	for (i=1 ; i < (argc - 1) && argv[ i ]; i++)
	{
		if (!strcmp(argv[i], "-fast")) 
		{
			Sys_Printf (" FastVis = true\n");
			fastvis = qtrue;
		} 
		else if (!strcmp(argv[i], "-full")) 
		{
			Sys_Printf (" FastVis = false\n");
			fastvis = qfalse;
		} 
		else if (!strcmp(argv[i], "-merge")) 
		{
			Sys_Printf (" Merge = true\n");
			mergevis = qtrue;
		} 
		else if (!strcmp(argv[i], "-nopassage"))
		{
			Sys_Printf (" NoPassage = true\n");
			noPassageVis = qtrue;
		} 
		else if (!strcmp(argv[i], "-passageOnly"))
		{
			Sys_Printf (" PassageOnly = true\n");
			passageVisOnly = qtrue;
		} 
		else if (!strcmp (argv[i],"-nosort"))
		{
			Sys_Printf (" NoSort = true\n");
			nosort = qtrue;
		} 
		else if (!strcmp (argv[i],"-saveprt"))
		{
			Sys_Printf (" Saveprt = true\n");
			saveprt = qtrue;
		} 
		else if (!strcmp (argv[i],"-tmpin"))
		{
			strcpy (inbase, "/tmp");
		} 
		else if (!strcmp (argv[i],"-tmpout")) 
		{
			strcpy (outbase, "/tmp");
		}
		
	
		/* ydnar: -hint to merge all but hint portals */
		else if( !strcmp( argv[ i ], "-hint" ) )
		{
			Sys_Printf( " Hint = true\n" );
			hint = qtrue;
			mergevis = qtrue;
		}
		
		else
			Sys_Warning( "Unknown option \"%s\"", argv[ i ] );
	}

	if( i != argc - 1 )
		Error( "usage: vis [-threads #] [-level 0-4] [-fast] [-v] bspfile" );
	

	/* load the bsp */
	Sys_PrintHeading ( "--- LoadBSPFile ---\n" );
	sprintf( source, "%s%s", inbase, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	strcat( source, ".bsp" );
	Sys_Printf( "loading %s\n", source );
	LoadBSPFile( source );

	/* load the portal file */
	Sys_PrintHeading ( "--- LoadPRTFile ---\n" );
	sprintf( portalfile, "%s%s", inbase, ExpandArg( argv[ i ] ) );
	StripExtension( portalfile );
	strcat( portalfile, ".prt" );
	Sys_Printf( "loading %s\n", portalfile );
	LoadPortals( portalfile );

	
	/* ydnar: exit if no portals, hence no vis */
	if( numportals == 0 )
	{
		Sys_Printf( "No portals means no vis, exiting.\n" );
		return 0;
	}

	/* UQ1: Realloc all memory usage so we only use what this map needs... */
	mapplanes = (plane_t*)realloc(mapplanes, sizeof(plane_t)*nummapplanes);
	bspPlanes = (bspPlane_t*)realloc(bspPlanes, numBSPPlanes * sizeof( bspPlane_t ));
	bspLeafs = (bspLeaf_t*)realloc(bspLeafs, numBSPLeafs * sizeof( bspLeaf_t ));
	bspNodes = (bspNode_t*)realloc(bspNodes, numBSPNodes * sizeof( bspNode_t ));
	bspLeafSurfaces = (int*)realloc(bspLeafSurfaces, numBSPLeafSurfaces * sizeof( bspLeafSurfaces[ 0 ] ));
	bspLeafBrushes = (int*)realloc(bspLeafBrushes, numBSPLeafBrushes * sizeof( bspLeafBrushes[ 0 ] ));
	bspBrushes = (bspBrush_t*)realloc(bspBrushes, numBSPBrushes * sizeof( bspBrush_t ));
	bspBrushSides = (bspBrushSide_t*)realloc(bspBrushSides, numBSPBrushSides * sizeof( bspBrushSide_t ));
	bspDrawVerts = (bspDrawVert_t*)realloc(bspDrawVerts, numBSPDrawVerts * sizeof( bspDrawVerts[ 0 ] ));
	bspDrawSurfaces = (bspDrawSurface_t*)realloc(bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ));
	bspVisBytes = (byte*)realloc(bspVisBytes, numBSPVisBytes * sizeof( byte ));
	
	/* ydnar: for getting far plane */
	ParseEntities();
	
	if( mergevis )
	{
		MergeLeaves();
		MergeLeafPortals();
	}

	/*if (!mergevis)
	{
		fastvis = qtrue;
	}*/
	
	CountActivePortals();
	/* WritePortals( "maps/hints.prs" );*/
	
	CalcVis();
	
	/* delete the prt file */
	if( !saveprt && portalfile && portalfile[0] )
		remove( portalfile );

	/* write the bsp file */
	Sys_PrintHeading ( "--- WriteBSPFile ---\n" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	return 0;
}
