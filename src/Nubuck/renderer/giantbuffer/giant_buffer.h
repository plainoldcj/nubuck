#pragma once

#include <limits.h>
#include <renderer\mesh\mesh.h>

namespace R {

#define GB_INVALID_HANDLE UINT_MAX

typedef unsigned gbHandle_t;

gbHandle_t  GB_AllocMemItem(Mesh::Vertex* const vertices, unsigned numVertices);
void        GB_FreeMemItem(gbHandle_t handle);
void        GB_Touch(gbHandle_t handle); 
void        GB_Cache(gbHandle_t handle);
unsigned    GB_GetOffset(gbHandle_t handle);
void        GB_Invalidate(gbHandle_t handle);
void        GB_Bind(void);
void        GB_CacheAll(void);
bool        GB_IsCached(gbHandle_t handle);

} // namespace R