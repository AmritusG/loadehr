#pragma once
#include "ModelData.h"
#include <atomic>
#include <string>

namespace mehr
{
// Loads .fbx / .obj (ufbx) or .gltf / .glb (cgltf) into a ModelData. Runs on a worker thread.
// Returns nullptr on failure (the reason is logged and written to `error`).
// `cancel` is polled between stages; a cancelled load returns nullptr.
// `maxTexture` > 0 downsamples every texture whose larger side exceeds it (by whole mip levels).
std::unique_ptr< ModelData > LoadModel( const std::string& path, int maxTexture, const std::atomic< bool >& cancel, std::string& error );

bool IsSupportedModelFile( const std::string& path );

// W21: a generated mesh (text / logo extrusion) through the same finishing steps as a loaded file (dedupe, tangents,
// per-part ranges and boxes, auto-fit). triPart indexes `parts` (at most kMaxParts, slot = index); triGroup = the
// letter / shape each triangle belongs to (the shards and the per-letter animation use it).
struct GenCorner
{
	float p[ 3 ], n[ 3 ], uv[ 2 ];
};
struct GenMesh
{
	std::vector< GenCorner > corners; // 3 per triangle
	std::vector< uint32_t > triPart, triGroup;
	std::vector< Part > parts;
	int groupCount = 0;
};
std::unique_ptr< ModelData > ModelFromMesh( GenMesh& mesh, const std::string& path );
void BuildImageMips( Image& img ); // the loader's CPU mip chain (sRGB-aware)
}
