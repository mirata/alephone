/*
	MD3 (Quake III Arena) model loader for Aleph One.

	Loads geometry, UVs, and per-frame morph positions into Model3D.
	Frame animation is stored in Model3D::MD3Positions / MD3Normals and
	retrieved via Model3D::FindPositions_MD3Frame().
*/
#ifndef MD3_LOADER_H
#define MD3_LOADER_H

#include "Model3D.h"
#include "FileHandler.h"

// Load an MD3 file into Model. Returns false on error.
bool LoadModel_MD3(FileSpecifier& Spec, Model3D& Model);

#endif
