/*
	MD3 (Quake III Arena) model loader for Aleph One.

	Binary format reference: id Software MD3 spec.
	Struct layout and vertex decompression adapted from
	QuestZDoom / GZDoom models_md3.cpp (LGPL).

	Coordinate system: loaded as-is (MD3 native). Use the MML
	x_rot / y_rot / z_rot / x_shift / y_shift / z_shift attributes
	to orient the model to Aleph One's coordinate frame.
*/

#include "cseries.h"
#include "Logging.h"

#ifdef HAVE_OPENGL

#include "MD3_Loader.h"
#include <cmath>
#include <cstring>
#include <vector>

// ---- Binary structures (little-endian on disk) --------------------------

#pragma pack(push, 1)

static const uint32 MD3_MAGIC   = 0x33504449; // "IDP3"
static const uint32 MD3_VERSION = 15;
static const int    MD3_MAX_QPATH = 64;

struct md3_header_t
{
	uint32 magic;
	uint32 version;
	char   name[MD3_MAX_QPATH];
	uint32 flags;
	uint32 num_frames;
	uint32 num_tags;
	uint32 num_surfaces;
	uint32 num_skins;
	uint32 ofs_frames;
	uint32 ofs_tags;
	uint32 ofs_surfaces;
	uint32 ofs_eof;
};

struct md3_frame_t
{
	float  min_bounds[3];
	float  max_bounds[3];
	float  local_origin[3];
	float  radius;
	char   name[16];
};

struct md3_surface_t
{
	uint32 magic;
	char   name[MD3_MAX_QPATH];
	uint32 flags;
	uint32 num_frames;
	uint32 num_shaders;
	uint32 num_verts;
	uint32 num_triangles;
	uint32 ofs_triangles;
	uint32 ofs_shaders;
	uint32 ofs_st;
	uint32 ofs_xyznormal;
	uint32 ofs_end;
};

struct md3_triangle_t
{
	uint32 idx[3];
};

struct md3_st_t
{
	float s, t;
};

struct md3_vertex_t
{
	int16 x, y, z;
	uint16 normal; // packed lat/lng
};

#pragma pack(pop)

// ---- Normal decompression -----------------------------------------------

static void UnpackNormal(uint16 packed, GLfloat& nx, GLfloat& ny, GLfloat& nz)
{
	double lat = double((packed >> 8) & 0xff) * (M_PI / 128.0);
	double lng = double(packed & 0xff)         * (M_PI / 128.0);
	nx = (GLfloat)(cos(lat) * sin(lng));
	ny = (GLfloat)(sin(lat) * sin(lng));
	nz = (GLfloat)(cos(lng));
}

// ---- Loader -------------------------------------------------------------

bool LoadModel_MD3(FileSpecifier& Spec, Model3D& Model)
{
	const char* path = Spec.GetPath();
	logNote("Loading MD3 model: %s", path);

	OpenedFile f;
	if (!Spec.Open(f))
	{
		logError("MD3: could not open %s", path);
		return false;
	}

	int32 fileLen = 0;
	if (!f.GetLength(fileLen) || fileLen < (int32)sizeof(md3_header_t))
	{
		logError("MD3: file too small: %s", path);
		return false;
	}

	std::vector<uint8> buf(fileLen);
	if (!f.Read(fileLen, buf.data()))
	{
		logError("MD3: read error: %s", path);
		return false;
	}

	const uint8* base = buf.data();

	// Validate header
	const md3_header_t* hdr = reinterpret_cast<const md3_header_t*>(base);
	if (hdr->magic != MD3_MAGIC)
	{
		logError("MD3: bad magic in %s", path);
		return false;
	}
	if (hdr->version != MD3_VERSION)
	{
		logError("MD3: unsupported version %u in %s", hdr->version, path);
		return false;
	}

	const uint32 numFrames   = hdr->num_frames;
	const uint32 numSurfaces = hdr->num_surfaces;

	if (numFrames == 0 || numSurfaces == 0)
	{
		logError("MD3: no frames or surfaces in %s", path);
		return false;
	}

	Model.Clear();

	// Walk surfaces and accumulate into Model3D flat arrays.
	// All surfaces share the same frame count (required by the MD3 spec).
	// We concatenate surfaces: globalVtxBase tracks the running vertex count
	// so triangle indices from surface N are offset correctly.

	uint32 globalVtxBase = 0;
	uint32 totalVerts    = 0;

	// First pass: count total vertices across all surfaces
	{
		const uint8* surfPtr = base + hdr->ofs_surfaces;
		for (uint32 s = 0; s < numSurfaces; ++s)
		{
			const md3_surface_t* surf = reinterpret_cast<const md3_surface_t*>(surfPtr);
			totalVerts += surf->num_verts;
			surfPtr    += surf->ofs_end;
		}
	}

	// Allocate per-frame storage (frame-major: frame0 verts, frame1 verts, ...)
	Model.MD3NumFrames = (int)numFrames;
	Model.MD3Positions.resize(numFrames * totalVerts * 3, 0.f);
	Model.MD3Normals.resize(numFrames * totalVerts * 3, 0.f);

	// Second pass: fill geometry
	const uint8* surfPtr = base + hdr->ofs_surfaces;
	for (uint32 s = 0; s < numSurfaces; ++s)
	{
		const md3_surface_t* surf = reinterpret_cast<const md3_surface_t*>(surfPtr);
		const uint32 nv  = surf->num_verts;
		const uint32 ntri = surf->num_triangles;

		// Texture coordinates (per-vertex, constant across frames)
		const md3_st_t* stArr = reinterpret_cast<const md3_st_t*>(
			reinterpret_cast<const uint8*>(surf) + surf->ofs_st);

		// Triangle indices (offset by globalVtxBase to make them global)
		const md3_triangle_t* triArr = reinterpret_cast<const md3_triangle_t*>(
			reinterpret_cast<const uint8*>(surf) + surf->ofs_triangles);

		// Per-frame vertex data (all frames × nv verts, frame-major)
		const md3_vertex_t* vtxArr = reinterpret_cast<const md3_vertex_t*>(
			reinterpret_cast<const uint8*>(surf) + surf->ofs_xyznormal);

		// Append UVs
		for (uint32 v = 0; v < nv; ++v)
		{
			Model.TxtrCoords.push_back(stArr[v].s);
			Model.TxtrCoords.push_back(stArr[v].t);
		}

		// Append triangle indices (with global offset)
		for (uint32 t = 0; t < ntri; ++t)
		{
			for (int c = 0; c < 3; ++c)
			{
				uint32 localIdx = triArr[t].idx[c];
				Model.VertIndices.push_back((GLushort)(globalVtxBase + localIdx));
			}
		}

		// Decompress per-frame vertex positions + normals into MD3Positions/MD3Normals
		for (uint32 fr = 0; fr < numFrames; ++fr)
		{
			const md3_vertex_t* frameVtx = vtxArr + fr * nv;
			GLfloat* posOut  = Model.MD3Positions.data() + (fr * totalVerts + globalVtxBase) * 3;
			GLfloat* normOut = Model.MD3Normals.data()   + (fr * totalVerts + globalVtxBase) * 3;

			for (uint32 v = 0; v < nv; ++v)
			{
				posOut[0] = frameVtx[v].x / 64.f;
				posOut[1] = frameVtx[v].y / 64.f;
				posOut[2] = frameVtx[v].z / 64.f;
				posOut   += 3;

				UnpackNormal(frameVtx[v].normal, normOut[0], normOut[1], normOut[2]);
				normOut += 3;
			}
		}

		globalVtxBase += nv;
		surfPtr        += surf->ofs_end;
	}

	if (Model.VertIndices.empty() || Model.TxtrCoords.empty())
	{
		logError("MD3: no usable geometry in %s", path);
		return false;
	}

	// Aleph One uses CW front faces; MD3 uses CCW. Reverse winding to match,
	// same as LoadModel_Wavefront_RightHand does for OBJ.
	for (uint32 i = 0; i < Model.VertIndices.size(); i += 3)
	{
		GLushort tmp = Model.VertIndices[i];
		Model.VertIndices[i]   = Model.VertIndices[i+1];
		Model.VertIndices[i+1] = tmp;
	}

	// Seed Positions / Normals from frame 0 so the bounding box and
	// normal processing in OGL_ModelData::Load() have data to work with.
	const uint32 nv0 = totalVerts;
	Model.Positions.resize(nv0 * 3);
	Model.Normals.resize(nv0 * 3);
	memcpy(Model.Positions.data(), Model.MD3Positions.data(), nv0 * 3 * sizeof(GLfloat));
	memcpy(Model.Normals.data(),   Model.MD3Normals.data(),   nv0 * 3 * sizeof(GLfloat));

	logNote("MD3: loaded %u surfaces, %u verts, %u tris, %u frames from %s",
		numSurfaces, totalVerts, (uint32)(Model.VertIndices.size() / 3),
		numFrames, path);

	return true;
}

#endif // HAVE_OPENGL
