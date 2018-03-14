#include "dock_models.h"

#include "../imgui_docks/dock_console.h"
#include "../imgui_docks_openjk/dock_mdxm.h"
//#include <renderergl2/tr_model_kung.h>
#include "rd-warzone/tr_glsl.h"
#include "../tr_local.h"
#include "../compose_models.h"

DockModels::DockModels() {}

const char *DockModels::label() {
	return "Models";
}

char *modeltype2string(modtype_t type) {
	switch (type) {
		case MOD_BAD   : return "MOD_BAD   "; break; 
		case MOD_BRUSH : return "MOD_BRUSH "; break; 
		case MOD_MESH  : return "MOD_MESH  "; break; 
		case MOD_MDR   : return "MOD_MDR   "; break; 
		case MOD_IQM   : return "MOD_IQM   "; break; 
		case MOD_MDXM  : return "MOD_MDXM"  ; break; 
		case MOD_MDXA  : return "MOD_MDXA"  ; break;
	}
	return "Unknown modtype_t";
}



void imgui_mdxm_list_surfhierarchy(mdxmHeader_t *header) {
	mdxmSurfHierarchy_t *surfHierarchy = firstSurfHierarchy(header);
 	for (int surface_id=0 ; surface_id<header->numSurfaces; surface_id++) {
		/*
			char		name[MAX_QPATH];
			unsigned int flags;
			char		shader[MAX_QPATH];
			int			shaderIndex;		// for in-game use (carcass defaults to 0)
			int			parentIndex;		// this points to the index in the file of the parent surface. -1 if null/root
			int			numChildren;		// number of surfaces which are children of this one
			int			childIndexes[1];	// [mdxmSurfHierarch_t->numChildren] (variable sized)		
		*/
		char tmp[512];
		snprintf(tmp, sizeof(tmp), "mdxmSurfHierarchy[%d] name=%s flags=%d shader=%s shaderIndex=%d parentIndex=%d numChildren=%d childIndexes[0]=%d",
			surface_id,
			surfHierarchy->name,
			surfHierarchy->flags,
			surfHierarchy->shader,
			surfHierarchy->shaderIndex,
			surfHierarchy->parentIndex,
			surfHierarchy->numChildren,
			surfHierarchy->childIndexes[0]
		);
		if (ImGui::CollapsingHeader(tmp)) {
		}
		
		surfHierarchy = next(surfHierarchy);
	}
}


const char *surfacetypeToString(surfaceType_t t) {
	switch (t) {
		case SF_BAD:			return "SF_BAD";
		case SF_SKIP:			return "SF_SKIP";
		case SF_FACE:			return "SF_FACE";
		case SF_GRID:			return "SF_GRID";
		case SF_TRIANGLES:		return "SF_TRIANGLES";
		case SF_POLY:			return "SF_POLY";
		case SF_MDV:			return "SF_MDV";
		case SF_MDR:			return "SF_MDR";
		case SF_IQM:			return "SF_IQM";
		case SF_MDX:			return "SF_MDX";
		case SF_FLARE:			return "SF_FLARE";
		case SF_ENTITY:			return "SF_ENTITY";
		case SF_DISPLAY_LIST:	return "SF_DISPLAY_LIST";
		case SF_VBO_MESH:		return "SF_VBO_MESH";
		case SF_VBO_MDVMESH:	return "SF_VBO_MDVMESH";
	}
	return "missing surfacetype";
}



void imgui_mdxm_surface_vertices(mdxmHeader_t *header, mdxmSurface_t *surf) {
	mdxmVertex_t *vert = firstVertex(surf);

	//ImGui::Text("verts=%p type=%s numVerts=%d numTriangles=%d numBoneReferences=%d", 
	//	vert->,
	//	surfacetypeToString((surfaceType_t)surf->ident),
	//	surf->numVerts,
	//	surf->numTriangles,
	//	surf->numBoneReferences
	//);

	for (int vert_id=0; vert_id<surf->numVerts; vert_id++) {
		char dragString[128];

		snprintf(dragString, sizeof(dragString), "verts[%i]", vert_id);
		ImGui::DragFloat3(dragString, vert->vertCoords);
		vert++;
	}



}

qboolean model_upload_mdxm_to_gpu(model_t *mod);

model_t *currentModel = NULL;

void imgui_mdxm_surface(mdxmHeader_t *header, mdxmSurface_t *surf) {
	ImGui::PushID(surf);

	char strBoneReferences[128];
	snprintf(strBoneReferences, sizeof(strBoneReferences), "%d bone references", surf->numBoneReferences);
	if (ImGui::CollapsingHeader(strBoneReferences)) {
		
		int *boneRef = (int *) ( (byte *)surf + surf->ofsBoneReferences);
		for (int j=0 ; j<surf->numBoneReferences; j++) {
			char tmpName[128];
			snprintf(tmpName, sizeof(tmpName), "boneReference[%d]", j);
			ImGui::DragInt(tmpName, boneRef + j);
			if (boneRef[j] < 0)
				boneRef[j] = 0;
			if (boneRef[j] >= header->numBones)
				boneRef[j] = header->numBones - 1;
		}
	}
	
	if (ImGui::Button("verts *= 2")) {
		
		mdxmVertex_t *vert = firstVertex(surf);
		for (int vertex_id=0; vertex_id<surf->numVerts; vertex_id++) {
			vert->vertCoords[0] *= 2.0;
			vert->vertCoords[1] *= 2.0;
			vert->vertCoords[2] *= 2.0;
			vert++;
		}

		model_upload_mdxm_to_gpu(currentModel);
	}
	if (ImGui::Button("verts /= 2")) {
		mdxmVertex_t *vert = firstVertex(surf);
		for (int vertex_id=0; vertex_id<surf->numVerts; vertex_id++) {
			vert->vertCoords[0] /= 2.0;
			vert->vertCoords[1] /= 2.0;
			vert->vertCoords[2] /= 2.0;
			vert++;
		}
		model_upload_mdxm_to_gpu(currentModel);
	
	}

	char strVerts[128];
	snprintf(strVerts, sizeof(strVerts), "%d vertices", surf->numVerts);
	if (ImGui::CollapsingHeader(strVerts)) {
		imgui_mdxm_surface_vertices(header, surf);
	}

	ImGui::PopID();
}

void imgui_mdxm_list_lods(mdxmHeader_t *header) {
	mdxmLOD_t *lod = firstLod(header);
	for (int lod_id=0; lod_id<header->numLODs; lod_id++) {
		char tmp[512];
		snprintf(tmp, sizeof(tmp), "mdxmLOD_t[%d] ofsEnd=%d", lod_id, lod->ofsEnd );
		if (ImGui::CollapsingHeader(tmp)) {
			mdxmSurface_t *surf = firstSurface(header, lod);
			for (int i=0; i<header->numSurfaces; i++) {


				char tmp[512];
				snprintf(tmp, sizeof(tmp), "surf id=%d ptr=%p type=%s numVerts=%d numTriangles=%d numBoneReferences=%d", 
					i,
					surf,
					surfacetypeToString((surfaceType_t)surf->ident),
					surf->numVerts,
					surf->numTriangles,
					surf->numBoneReferences
				);
				if (ImGui::CollapsingHeader(tmp)) {
					imgui_mdxm_surface(header, surf);
				}
				surf = next(surf);
			}
		}
		lod = next(lod);
	}
}

void imgui_mdxm(model_t *mod) {
	mdxmData_t *glm = mod->data.glm;
	mdxmHeader_t *header = glm->header;
	ImGui::Text("ident=%d", header->ident);
	ImGui::Text("version=%d", header->version);
	ImGui::Text("name=%s", header->name);
	ImGui::Text("animName=%s", header->animName);
	ImGui::Text("animIndex=%d", header->animIndex);
	ImGui::Text("numBones=%d", header->numBones);
	ImGui::Text("numLODs=%d", header->numLODs);
	ImGui::Text("ofsLODs=%d", header->ofsLODs);
	ImGui::Text("numSurfaces=%d", header->numSurfaces);
	ImGui::Text("ofsSurfHierarchy=%d", header->ofsSurfHierarchy);
	ImGui::Text("ofsEnd=%d", header->ofsEnd);

	
	if (ImGui::Button("Open")) {
		new DockMDXM(mod);
	}

	if (ImGui::CollapsingHeader("mdxmSurfHierarchy_t")) {
		imgui_mdxm_list_surfhierarchy(header);
	}
	if (ImGui::CollapsingHeader("lods")) {
		imgui_mdxm_list_lods(header);
	}


}

void DockModels::imgui() {
	for (int i=0; i<tr.numModels; i++) {
		auto model = tr.models[i];
		
		char buf[512];
		sprintf(buf, "model[%d] name=%s type=%s", i, model->name, modeltype2string(model->type));


		if (ImGui::CollapsingHeader(buf)) {
			ImGui::PushID(model);


			currentModel = model;
			switch (model->type) {
				case MOD_MDXM: imgui_mdxm(model); break;
			}
			ImGui::PopID();
		}



	}
		
}