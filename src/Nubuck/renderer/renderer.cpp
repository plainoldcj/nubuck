#include <assert.h>

#include <functional>
#include <algorithm>

#include <common\common.h>
#include <common\config\config.h>
#include <math\matrix3.h>
#include <math\matrix4.h>
#include <renderer\glew\glew.h>
#include <renderer\glcall.h>
#include <renderer\effects\effect.h>
#include <renderer\effects\effectmgr.h>
#include <renderer\effects\pass.h>
#include <renderer\mesh\mesh.h>
#include <renderer\mesh\meshmgr.h>
#include <renderer\mesh\sphere\sphere.h>
#include <renderer\mesh\cylinder\cylinder.h>
#include <renderer\material\material.h>
#include <renderer\metrics\metrics.h>
#include <renderer\giantbuffer\giant_buffer.h>
#include "renderer.h"
#include <world\world.h>

namespace {

enum {
    IN_POSITION     = 0,
    IN_NORMAL       = 1,
    IN_COLOR        = 2,
    IN_TEXCOORDS    = 3,

    // encodes 3x4 matrix
    IN_A0           = 4,
    IN_A1           = 5,
    IN_A2           = 6,
    IN_A3           = 7,

    IN_HALF_HEIGHT_SQ   = 8,
    IN_RADIUS_SQ        = 9
};

void BindVertices(void) {
    GL_CALL(glVertexAttribPointer(IN_POSITION,
        3, GL_FLOAT, GL_FALSE, sizeof(R::Mesh::Vertex),
        (void*)offsetof(R::Mesh::Vertex, position)));
    GL_CALL(glEnableVertexAttribArray(IN_POSITION));

    GL_CALL(glVertexAttribPointer(IN_NORMAL,
        3, GL_FLOAT, GL_FALSE, sizeof(R::Mesh::Vertex),
        (void*)offsetof(R::Mesh::Vertex, normal)));
    GL_CALL(glEnableVertexAttribArray(IN_NORMAL));

    GL_CALL(glVertexAttribPointer(IN_COLOR,
        4, GL_FLOAT, GL_FALSE, sizeof(R::Mesh::Vertex),
        (void*)offsetof(R::Mesh::Vertex, color)));
    GL_CALL(glEnableVertexAttribArray(IN_COLOR));

    GL_CALL(glVertexAttribPointer(IN_TEXCOORDS,
        2, GL_FLOAT, GL_FALSE, sizeof(R::Mesh::Vertex),
        (void*)offsetof(R::Mesh::Vertex, texCoords)));
    GL_CALL(glEnableVertexAttribArray(IN_TEXCOORDS));
}

template<typename T> struct ToGLEnum { };
template<> struct ToGLEnum<unsigned>    { enum { ENUM = GL_UNSIGNED_INT }; };
template<> struct ToGLEnum<int>         { enum { ENUM = GL_INT }; };

} // unnamed namespace

COM::Config::Variable<float>	cvar_r_nodeSize("r_nodeSize", 0.2f);
COM::Config::Variable<float>    cvar_r_edgeRadius("r_edgeRadius", 0.1f);

namespace R {

RenderList g_renderLists[2];
SYS::Semaphore g_rendererSem(0);
static int rlIdx = 0;

static MeshMgr::meshPtr_t nodeMesh;

static State curState;

enum UniformBindingIndices {
    BINDING_INDEX_HOT       = 0,
    BINDING_INDEX_LIGHTS    = 1,
    BINDING_INDEX_SKELETON  = 2
};

struct UniformsHot {
    M::Matrix4 uProjection;
    M::Matrix4 uTransform;
    M::Matrix4 uInvTransform;
    M::Matrix4 uNormalMat; // use mat4 to avoid padding issues
};

struct UniformsLights {
    M::Vector3  uLightVec0;
    float       padding0;
    M::Vector3  uLightVec1;
    float       padding1;
    M::Vector3  uLightVec2;
    float       padding2;
    Color       uLightDiffuseColor0;
    Color 		uLightDiffuseColor1;
    Color 		uLightDiffuseColor2;
    float       uShininess;
};

struct UniformsSkeleton {
    Color       uColor;
    float       uNodeSize;
};

static UniformsHot                  uniformsHot;
static UniformsLights               uniformsLights;
static UniformsSkeleton             uniformsSkeleton;
static GEN::Pointer<StaticBuffer>   uniformsHotBuffer;
static GEN::Pointer<StaticBuffer>   uniformsLightsBuffer;
static GEN::Pointer<StaticBuffer>   uniformsSkeletonBuffer;

static void Uniforms_InitBuffers(void) {
    uniformsHotBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_UNIFORM_BUFFER, NULL, sizeof(UniformsHot)));
    uniformsLightsBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_UNIFORM_BUFFER, NULL, sizeof(UniformsLights)));
    uniformsSkeletonBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_UNIFORM_BUFFER, NULL, sizeof(UniformsSkeleton)));
}

static void Uniforms_BindBuffers(void) {
    // somehow these bindings break on intel gpus.
    GL_CALL(glBindBufferBase(GL_UNIFORM_BUFFER, BINDING_INDEX_HOT, uniformsHotBuffer->GetID()));
    GL_CALL(glBindBufferBase(GL_UNIFORM_BUFFER, BINDING_INDEX_LIGHTS, uniformsLightsBuffer->GetID()));
    GL_CALL(glBindBufferBase(GL_UNIFORM_BUFFER, BINDING_INDEX_SKELETON, uniformsSkeletonBuffer->GetID()));
}

static void Uniforms_BindBlocks(const Program& prog) {
    GLuint idx = 0;
    
    idx = glGetUniformBlockIndex(prog.GetID(), "UniformsHot");
    if(GL_INVALID_INDEX != idx) GL_CALL(glUniformBlockBinding(prog.GetID(), idx, BINDING_INDEX_HOT));

    idx = glGetUniformBlockIndex(prog.GetID(), "UniformsLights");
    if(GL_INVALID_INDEX != idx) GL_CALL(glUniformBlockBinding(prog.GetID(), idx, BINDING_INDEX_LIGHTS));

    idx = glGetUniformBlockIndex(prog.GetID(), "UniformsSkeleton");
    if(GL_INVALID_INDEX != idx) GL_CALL(glUniformBlockBinding(prog.GetID(), idx, BINDING_INDEX_SKELETON));
}

static void Uniforms_Update(
    const M::Matrix4& projectionMat, 
    const M::Matrix4& worldMat,
    const DirectionalLight dirLights[])
{
    uniformsHot.uProjection = projectionMat;
    uniformsHot.uTransform = worldMat;
    M::TryInvert(worldMat, uniformsHot.uInvTransform);
    uniformsHot.uNormalMat = M::Mat4::FromRigidTransform(M::Transpose(M::Inverse(M::RotationOf(worldMat))), M::Vector3::Zero);
    uniformsHotBuffer->Update_Mapped(0, sizeof(UniformsHot), &uniformsHot);

    uniformsLights.uLightVec0 = -dirLights[0].direction;
    uniformsLights.uLightVec1 = -dirLights[1].direction;
    uniformsLights.uLightVec2 = -dirLights[2].direction;
    uniformsLights.uLightDiffuseColor0 = dirLights[0].diffuseColor;
    uniformsLights.uLightDiffuseColor1 = dirLights[1].diffuseColor;
    uniformsLights.uLightDiffuseColor2 = dirLights[2].diffuseColor;
    uniformsLights.uShininess = 200.0f;
    uniformsLightsBuffer->Update_Mapped(0, sizeof(UniformsLights), &uniformsLights);

    uniformsSkeleton.uColor = Color(0.4f, 0.4f, 0.4f, 1.0f);
    uniformsSkeleton.uNodeSize = cvar_r_nodeSize;
    uniformsSkeletonBuffer->Update_Mapped(0, sizeof(UniformsSkeleton), &uniformsSkeleton);
}


struct BillboardHotVertex {
    M::Vector3  position;
    Color       color;
};

struct BillboardHot {
    BillboardHotVertex verts[4];
};

struct BillboardColdVertex {
    M::Vector2 texCoords; // used for clipping
};

struct BillboardCold {
    BillboardColdVertex verts[4];
};

static std::vector<M::Vector3>      billboardPositions;
static std::vector<Color>           billboardColors;
static std::vector<Mesh::Index>     billboardIndices;
static std::vector<BillboardHot>    billboardsHot;
static std::vector<BillboardCold>   billboardsCold;
static GEN::Pointer<StaticBuffer>   billboardHotVertexBuffer;
static GEN::Pointer<StaticBuffer>   billboardColdVertexBuffer;
static GEN::Pointer<StaticBuffer>   billboardIndexBuffer;

static float RandFloat(float min, float max) {
    return min + (rand() % 1000 / 1000.0f) * (max - min);
}

static void ReserveBillboards(unsigned numBillboards) {
    if(numBillboards <= billboardsHot.size()) return;

    unsigned numBillboardIndices = 5 * numBillboards - 1;

    billboardsHot.clear();
    billboardsHot.resize(numBillboards);
    if(billboardHotVertexBuffer.IsValid()) billboardHotVertexBuffer->Destroy();
    billboardHotVertexBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_ARRAY_BUFFER, NULL, sizeof(BillboardHot) * numBillboards));

    static const BillboardColdVertex coldVertices[4] = {
        { M::Vector2(-1.0f, -1.0f) },
        { M::Vector2( 1.0f, -1.0f) },
        { M::Vector2( 1.0f,  1.0f) },
        { M::Vector2(-1.0f,  1.0f) }
    };
    billboardsCold.clear();
    billboardsCold.resize(numBillboards);
    for(unsigned i = 0; i < numBillboards; ++i) {
        memcpy(billboardsCold[i].verts, coldVertices, sizeof(coldVertices));
    }
    if(billboardColdVertexBuffer.IsValid()) billboardColdVertexBuffer->Destroy();
    billboardColdVertexBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_ARRAY_BUFFER, &billboardsCold[0], sizeof(BillboardCold) * numBillboards));

    billboardIndices.clear();
    billboardIndices.reserve(numBillboardIndices);
    for(unsigned i = 0; i < 4 * numBillboards; ++i) {
        if(0 < i && 0 == i % 4) billboardIndices.push_back(Mesh::RESTART_INDEX);
        billboardIndices.push_back(i);
    }
    assert(numBillboardIndices == billboardIndices.size());
    if(billboardIndexBuffer.IsValid()) billboardIndexBuffer->Destroy();
    billboardIndexBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_ELEMENT_ARRAY_BUFFER, &billboardIndices[0], sizeof(Mesh::Index) * numBillboardIndices));
}

static void InitBillboards(const std::vector<M::Vector3>& positions, const std::vector<Color>& colors) {
    unsigned numBillboards = positions.size();
    unsigned numBillboardIndices = 5 * numBillboards - 1;
    if(numBillboards) {
        billboardPositions = positions;
        billboardColors = colors;
        ReserveBillboards(numBillboards);
    }
}

// camera position at 0
static M::Matrix4 Billboard_FaceCamera(const M::Matrix4& worldMat, const M::Vector3& billboardPos) {
    const M::Vector3 p = M::Transform(worldMat, billboardPos);
    const M::Vector3 v2 = M::Normalize(-M::Vector3(p.x, 0.0f, p.z));
    const M::Vector3 v1 = M::Vector3(0.0f, 1.0f, 0.0f);
    const M::Vector3 v0 = M::Normalize(M::Cross(v1, v2));
    const M::Matrix3 M = M::Mat3::FromColumns(v0, v1, v2);
    const M::Vector3 w2 = M::Normalize(-M::Vector3(0.0f, p.y, p.z));
    const M::Vector3 w0 = M::Vector3(1.0f, 0.0f, 0.0f);
    const M::Vector3 w1 = M::Normalize(M::Cross(w0, w2));
    const M::Matrix3 N = M::Mat3::FromColumns(w0, w1, w2);
    return M::Mat4::FromRigidTransform(M * N, p);
}

// omits scale
static M::Matrix4 Billboard_FaceViewingPlane(const M::Matrix4& worldMat, const M::Vector3& billboardPos) {
    const M::Vector3 p = M::Transform(worldMat, billboardPos);
    return M::Matrix4(
        1.0f, 0.0f, 0.0f, worldMat.m03 + p.x,
        0.0f, 1.0f, 0.0f, worldMat.m13 + p.y,
        0.0f, 0.0f, 1.0f, worldMat.m23 + p.z,
        0.0f, 0.0f, 0.0f, 1.0f);
}

static void BuildBillboards(const M::Matrix4& worldMat) {
	float nodeSize = cvar_r_nodeSize;
    const BillboardHotVertex hotVertices[4] = {
        { M::Vector3(-nodeSize, -nodeSize, 0.0f) },
        { M::Vector3( nodeSize, -nodeSize, 0.0f) },
        { M::Vector3( nodeSize,  nodeSize, 0.0f) },
        { M::Vector3(-nodeSize,  nodeSize, 0.0f) }
    };
    unsigned numBillboards = billboardPositions.size();
    for(unsigned i = 0; i < numBillboards; ++i) {
        for(unsigned k = 0; k < 4; ++k) {
            billboardsHot[i].verts[k].position = M::Transform(worldMat, billboardPositions[i]) + hotVertices[k].position;
            billboardsHot[i].verts[k].color = billboardColors[i];
        }
    }
}

static void BindHotBillboardVertices(void) {
    GL_CALL(glVertexAttribPointer(IN_POSITION,
        3, GL_FLOAT, GL_FALSE, sizeof(BillboardHotVertex),
        (void*)offsetof(BillboardHotVertex, position)));
    GL_CALL(glEnableVertexAttribArray(IN_POSITION));

    GL_CALL(glVertexAttribPointer(IN_COLOR,
        4, GL_FLOAT, GL_FALSE, sizeof(BillboardHotVertex),
        (void*)offsetof(BillboardHotVertex, color)));
    GL_CALL(glEnableVertexAttribArray(IN_COLOR));
}

static void BindColdBillboardVertices(void) {
    GL_CALL(glVertexAttribPointer(IN_TEXCOORDS,
        2, GL_FLOAT, GL_FALSE, sizeof(BillboardColdVertex),
        (void*)offsetof(BillboardColdVertex, texCoords)));
    GL_CALL(glEnableVertexAttribArray(IN_TEXCOORDS));
}

void SetState(const State& state);

static void DrawBillboards(const M::Matrix4& worldMat, const M::Matrix4& projectionMat) {
    unsigned numBillboards = billboardPositions.size();
    unsigned numBillboardIndices = 5 * numBillboards - 1;

    GEN::Pointer<Effect> fx = effectMgr.GetEffect("NodeBillboard");
    fx->Compile();
    Pass* pass = fx->GetPass(0);
    pass->Use();

    Program& prog = pass->GetProgram();
    Uniforms_BindBlocks(prog);
    Uniforms_BindBuffers();
    SetState(pass->GetDesc().state);

    billboardHotVertexBuffer->Bind();
    BindHotBillboardVertices();

    billboardColdVertexBuffer->Bind();
    BindColdBillboardVertices();

    billboardIndexBuffer->Bind();

    glDrawElements(GL_TRIANGLE_FAN, numBillboardIndices, GL_UNSIGNED_INT, NULL);
}

// rotates the coordinate space such that the new z axis coincides with the vector d.
// example. AlignZ(d) * d = Length(d) * (0, 0, 1)
static M::Matrix4 AlignZ(const M::Vector3& d) {
    const float len_yz_sq = d.y * d.y + d.z * d.z;
    const float len = d.Length();
    assert(0.0f < len);
    if(0.0f == len_yz_sq) { // case d, x collinear => rot_x = 0
        return M::Matrix4(
            0.0f, 0.0f, -d.x / len, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            d.x / len, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    } else {
        const float len_yz = sqrt(len_yz_sq);
        return M::Matrix4(
            len_yz * len_yz, -d.x * d.y, -d.x * d.z, 0.0f,
            0.0f, len * d.z, -len * d.y, 0.0f,
            len_yz * d.x, len_yz * d.y, len_yz * d.z, 0.0f,
            0.0f, 0.0f, 0.0f, len_yz * len
        ) / (len_yz * len);
    }
}

struct EdgeBBoxVertex {
    M::Vector3  position;
    Color4ub    color;
    M::Vector3  A[4];
    float       halfHeightSq;
    float       radiusSq;
};

static std::vector<EdgeBBoxVertex>  edgeBBoxVertices;
static std::vector<Mesh::Index>     edgeBBoxIndices;
static GEN::Pointer<StaticBuffer>   edgeBBoxVertexBuffer;
static GEN::Pointer<StaticBuffer>   edgeBBoxIndexBuffer;

static void BindEdgeBBoxVertices(void) {
    GL_CALL(glVertexAttribPointer(IN_POSITION,
        3, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)offsetof(EdgeBBoxVertex, position)));
    GL_CALL(glEnableVertexAttribArray(IN_POSITION));

    GL_CALL(glVertexAttribPointer(IN_COLOR,
        4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(EdgeBBoxVertex),
        (void*)offsetof(EdgeBBoxVertex, color)));
    GL_CALL(glEnableVertexAttribArray(IN_COLOR));

    GL_CALL(glVertexAttribPointer(IN_A0,
        3, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)(offsetof(EdgeBBoxVertex, A) + sizeof(M::Vector3) * 0)));
    GL_CALL(glEnableVertexAttribArray(IN_A0));

    GL_CALL(glVertexAttribPointer(IN_A1,
        3, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)(offsetof(EdgeBBoxVertex, A) + sizeof(M::Vector3) * 1)));
    GL_CALL(glEnableVertexAttribArray(IN_A1));

    GL_CALL(glVertexAttribPointer(IN_A2,
        3, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)(offsetof(EdgeBBoxVertex, A) + sizeof(M::Vector3) * 2)));
    GL_CALL(glEnableVertexAttribArray(IN_A2));

    GL_CALL(glVertexAttribPointer(IN_A3,
        3, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)(offsetof(EdgeBBoxVertex, A) + sizeof(M::Vector3) * 3)));
    GL_CALL(glEnableVertexAttribArray(IN_A3));

    GL_CALL(glVertexAttribPointer(IN_HALF_HEIGHT_SQ,
        1, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)(offsetof(EdgeBBoxVertex, halfHeightSq))));
    GL_CALL(glEnableVertexAttribArray(IN_HALF_HEIGHT_SQ));

    GL_CALL(glVertexAttribPointer(IN_RADIUS_SQ,
        1, GL_FLOAT, GL_FALSE, sizeof(EdgeBBoxVertex),
        (void*)(offsetof(EdgeBBoxVertex, radiusSq))));
    GL_CALL(glEnableVertexAttribArray(IN_RADIUS_SQ));
}

// remove edges with zero length
static void RemoveDegeneratedEdges(std::vector<Edge>& edges) {
    unsigned i = 0;
    while(edges.size() > i) {
        if(M::AlmostEqual(0.0f, M::Distance(edges[i].p0, edges[i].p1))) {
            std::swap(edges[i], edges.back());
            edges.pop_back();
        } else ++i;
    }
}

static void ReserveEdgeBBoxBuffers(unsigned numEdges) {
    // numbers per bbox
    const unsigned numVertices = 8;
    const unsigned numIndices = 14 + 1; // including restart index

    unsigned vbSize = sizeof(EdgeBBoxVertex) * numVertices * numEdges;
    unsigned ibSize = sizeof(Mesh::Index) * numIndices * numEdges;

    if(edgeBBoxVertexBuffer.IsValid() && edgeBBoxVertexBuffer->GetSize() >= vbSize) return;

    if(edgeBBoxVertexBuffer.IsValid()) edgeBBoxVertexBuffer->Destroy();
    edgeBBoxVertexBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_ARRAY_BUFFER, NULL, vbSize));
    if(edgeBBoxIndexBuffer.IsValid()) edgeBBoxIndexBuffer->Destroy();
    edgeBBoxIndexBuffer = GEN::Pointer<StaticBuffer>(new StaticBuffer(GL_ELEMENT_ARRAY_BUFFER, NULL, ibSize));
}

static void CreateEdges(const std::vector<Edge>& edges) {
    if(edges.empty()) return;

    edgeBBoxVertices.clear();
    edgeBBoxIndices.clear();
    unsigned baseIdx = 0;
    for(unsigned i = 0; i < edges.size(); ++i) {
        const Edge& edge = edges[i];
        const M::Vector3 center = 0.5f * (edge.p1 + edge.p0);
        M::Matrix4 R = AlignZ(edge.p1 - edge.p0);
        M::Matrix4 objectToWorld = M::Mat4::Translate(center) * M::Transpose(R);

        const float h = 0.5f * M::Length(edge.p1 - edge.p0);
        const float r = edge.radius;
        M::Vector3 bboxVertexPositions[] = {
            M::Vector3( r,  r, -h),
            M::Vector3(-r,  r, -h),
            M::Vector3( r,  r,  h),
            M::Vector3(-r,  r,  h),
            M::Vector3( r, -r, -h),
            M::Vector3(-r, -r, -h),
            M::Vector3(-r, -r,  h),
            M::Vector3( r, -r,  h)
        };
        const unsigned numVertices = 8;
        Mesh::Index bboxIndices[] = { 3, 2, 6, 7, 4, 2, 0, 3, 1, 6, 5, 4, 1, 0 };
        const unsigned numIndices = 14;
        EdgeBBoxVertex vertex;
        vertex.color = ColorTo4ub(edge.color);
        vertex.A[0] = M::Vector3(objectToWorld.m00, objectToWorld.m10, objectToWorld.m20);
        vertex.A[1] = M::Vector3(objectToWorld.m01, objectToWorld.m11, objectToWorld.m21);
        vertex.A[2] = M::Vector3(objectToWorld.m02, objectToWorld.m12, objectToWorld.m22);
        vertex.A[3] = M::Vector3(objectToWorld.m03, objectToWorld.m13, objectToWorld.m23);
        vertex.halfHeightSq = h * h;
        vertex.radiusSq = edge.radius * edge.radius;
        for(unsigned i = 0; i < numVertices; ++i) {
            vertex.position = M::Transform(objectToWorld, bboxVertexPositions[i]);
            edgeBBoxVertices.push_back(vertex);
        }
        for(unsigned i = 0; i < numIndices; ++i) edgeBBoxIndices.push_back(baseIdx + bboxIndices[i]);
        edgeBBoxIndices.push_back(Mesh::RESTART_INDEX);
        baseIdx += numVertices;
    } // for all edges

    edgeBBoxVertexBuffer->Update_Mapped(0, sizeof(EdgeBBoxVertex) * edgeBBoxVertices.size(), &edgeBBoxVertices[0]);
    edgeBBoxIndexBuffer->Update_Mapped(0, sizeof(Mesh::Index) * edgeBBoxIndices.size(), &edgeBBoxIndices[0]);
}

static void DrawEdges(const M::Matrix4& projectionMat, const M::Matrix4& worldMat) {
    GEN::Pointer<Effect> fx = effectMgr.GetEffect("EdgeBillboard");
    // GEN::Pointer<Effect> fx = effectMgr.GetEffect("GenericWireframe");
    fx->Compile();
    Pass* pass = fx->GetPass(0);
    pass->Use();

    Program& prog = pass->GetProgram();
    Uniforms_BindBlocks(prog);
    Uniforms_BindBuffers();
    SetState(pass->GetDesc().state);

    edgeBBoxVertexBuffer->Bind();
    BindEdgeBBoxVertices();

    edgeBBoxIndexBuffer->Bind();

    glDrawElements(GL_TRIANGLE_STRIP, edgeBBoxIndices.size(), GL_UNSIGNED_INT, NULL);
}

void InitDebugOutput(void);

void SetEnabled(GLenum state, GLboolean enabled) {
    if(enabled) GL_CALL(glEnable(state));
    else        GL_CALL(glDisable(state));
}

void SetState(const State& state) {
    bool c0, c1, c2, c3;

    if(curState.culling.hw.enabled != state.culling.hw.enabled) {
        SetEnabled(GL_CULL_FACE, state.culling.hw.enabled);
        curState.culling.hw.enabled = state.culling.hw.enabled;
    }
    if(curState.culling.hw.enabled && curState.culling.hw.cullFace != state.culling.hw.cullFace) {
        GL_CALL(glCullFace(state.culling.hw.cullFace));
        curState.culling.hw.cullFace = state.culling.hw.cullFace;
    }
    curState.culling.sw = state.culling.sw;

    if(curState.blend.enabled != state.blend.enabled) {
        SetEnabled(GL_BLEND, state.blend.enabled);
        curState.blend.enabled = state.blend.enabled;
    }
    if(curState.blend.srcFactor != state.blend.srcFactor || curState.blend.dstFactor != state.blend.dstFactor) {
        glBlendFunc(state.blend.srcFactor, state.blend.dstFactor);
        curState.blend.srcFactor = state.blend.srcFactor;
        curState.blend.dstFactor = state.blend.dstFactor;
    }

    // SetEnabled(GL_DEPTH_TEST, state.depth.enabled);
    if(curState.depth.maskEnabled != state.depth.maskEnabled) {
        GL_CALL(glDepthMask(state.depth.maskEnabled));
        curState.depth.maskEnabled = state.depth.maskEnabled;
    }
    if(curState.depth.func != state.depth.func) {
        GL_CALL(glDepthFunc(state.depth.func));
        curState.depth.func = state.depth.func;
    }

    // SetEnabled(GL_STENCIL_TEST, state.stencil.enabled);
    c0 = curState.stencil.func.func != state.stencil.func.func;
    c1 = curState.stencil.func.ref  != state.stencil.func.ref;
    c2 = curState.stencil.func.mask != state.stencil.func.mask;
    if(c0 || c1 || c2) {
        GL_CALL(glStencilFunc(state.stencil.func.func, state.stencil.func.ref, state.stencil.func.mask));
        curState.stencil.func.func  = state.stencil.func.func;
        curState.stencil.func.ref   = state.stencil.func.ref;
        curState.stencil.func.mask  = state.stencil.func.mask;
    }
    c0 = curState.stencil.op.front.fail   != state.stencil.op.front.fail;
    c1 = curState.stencil.op.front.zfail  != state.stencil.op.front.zfail;
    c2 = curState.stencil.op.front.zpass  != state.stencil.op.front.zpass;
    if(c0 || c1 || c2) {
        GL_CALL(glStencilOpSeparate(GL_FRONT, state.stencil.op.front.fail, state.stencil.op.front.zfail, state.stencil.op.front.zpass));
        curState.stencil.op.front.fail    = state.stencil.op.front.fail;
        curState.stencil.op.front.zfail   = state.stencil.op.front.zfail;
        curState.stencil.op.front.zpass   = state.stencil.op.front.zpass;
    }
    c0 = curState.stencil.op.back.fail   != state.stencil.op.back.fail;
    c1 = curState.stencil.op.back.zfail  != state.stencil.op.back.zfail;
    c2 = curState.stencil.op.back.zpass  != state.stencil.op.back.zpass;
    if(c0 || c1 || c2) {
        GL_CALL(glStencilOpSeparate(GL_BACK, state.stencil.op.back.fail, state.stencil.op.back.zfail, state.stencil.op.back.zpass));
        curState.stencil.op.back.fail    = state.stencil.op.back.fail;
        curState.stencil.op.back.zfail   = state.stencil.op.back.zfail;
        curState.stencil.op.back.zpass   = state.stencil.op.back.zpass;
    }

    if(curState.raster.lineWidth != state.raster.lineWidth) {
        GL_CALL(glLineWidth(state.raster.lineWidth));
        curState.raster.lineWidth = state.raster.lineWidth;
    }

    c0 = curState.color.maskEnabled.red != state.color.maskEnabled.red;
    c1 = curState.color.maskEnabled.green != state.color.maskEnabled.green;
    c2 = curState.color.maskEnabled.blue != state.color.maskEnabled.blue;
    c3 = curState.color.maskEnabled.alpha != state.color.maskEnabled.alpha;
    if(c0 || c1 || c2 || c3) {
        GL_CALL(glColorMask(state.color.maskEnabled.red, state.color.maskEnabled.green, state.color.maskEnabled.blue, state.color.maskEnabled.alpha));
        curState.color.maskEnabled = state.color.maskEnabled;
    }
}

Renderer::Renderer(void) : _time(0.0f) { }

static void PrintGLInfo(void) {
    common.printf("INFO - GL strings {\n");
    common.printf("\tGL_VENDOR                   = '%s'\n", glGetString(GL_VENDOR));
    common.printf("\tGL_RENDERER                 = '%s'\n", glGetString(GL_RENDERER));
    common.printf("\tGL_VERSION                  = '%s'\n", glGetString(GL_VERSION));
    common.printf("\tGL_SHADING_LANGUAGE_VERSION = '%s'\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    common.printf("} // GL strings\n");
}

void Renderer::Init(void) {
    GL_CHECK_ERROR;

    PrintGLInfo();

    InitDebugOutput();

    /*
    uncomment this when using QGLWidget
    GLenum ret;
    if(GLEW_OK != (ret = glewInit())) {
        common.printf("ERROR - glewInit() failed with code %d, \"%s\"\n",
                ret, glewGetErrorString(ret));
        Crash();
    }
    common.printf("INFO - using glew version %s.\n", glewGetString(GLEW_VERSION));

    const GLubyte* glVersion = glGetString(GL_VERSION);
    common.printf("INFO - supported GL version: '%s'.\n", glVersion);
    */

    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    assert(0 < stencilBits);

    curState.SetDefault();

    GL_CALL(glEnable(GL_PRIMITIVE_RESTART));
    GL_CALL(glPrimitiveRestartIndex(Mesh::RESTART_INDEX));

    float f = 1.0f / 255.0f;
    // glClearColor(f * 176, f * 196, f * 222, 1.0f); // light steel blue
    // glClearColor(f * 70, f * 130, f * 180, 1.0f); // steel blue
    glClearColor(f * 154, f * 206, f * 235, 1.0f); // cornflower blue (crayola)
    glClearDepth(1.0f);
    glClearStencil(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    // glEnable(GL_CULL_FACE);
    
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    Uniforms_InitBuffers();

    _timer.Start();
}

void Renderer::Resize(int width, int height) {
    glViewport(0, 0, width, height);
    _aspect = (float)width / height;
}

static bool Cmp_Dist(const Mesh::Triangle& lhp, const Mesh::Triangle& rhp) {
    return lhp.dist > rhp.dist;
}

namespace {

    struct IsBackFace {
        float alpha;
        IsBackFace(float alpha) : alpha(alpha) { }
        bool operator()(const Mesh::Triangle& tri) const { return alpha > tri.viewAngle; }
    };

    struct IsFrontFace {
        float alpha;
        IsFrontFace(float alpha) : alpha(alpha) { }
        bool operator()(const Mesh::Triangle& tri) const { return alpha < tri.viewAngle; }
    };

    struct IsViewedEdgeOn {
        bool operator()(const Mesh::Triangle& tri) const { return 0.0f <= tri.viewAngle && tri.viewAngle < 0.01f; }
    };

} // unnamed namespace

static RenderJob* DrawMeshList(Program& prog, const State& state, int passType, int passFlags, const M::Matrix4& worldMat, RenderJob* first) {
    typedef std::vector<Mesh::Triangle>::iterator triIt_t;
    M::Matrix4 invWorld;
    M::TryInvert(worldMat, invWorld);
    M::Vector3 localEye = M::Transform(invWorld, M::Vector3::Zero);
    std::vector<Mesh::Triangle> tris;
    RenderJob* meshJob = first;
    while(meshJob && meshJob->fx == first->fx) {
        Mesh& mesh = meshMgr.GetMesh(meshJob->mesh);
        mesh.AppendTriangles(tris, localEye);
        meshJob = meshJob->next;
    }
    triIt_t trisEnd = tris.end();
    if(state.culling.sw.enabled) {
        switch(state.culling.sw.cullFace) {
        case GL_FRONT: trisEnd = std::remove_if(tris.begin(), tris.end(), IsFrontFace(state.culling.sw.alpha)); break;
        case GL_BACK: trisEnd = std::remove_if(tris.begin(), tris.end(), IsBackFace(state.culling.sw.alpha)); break;
        default: assert(false);
        };
    }
    std::sort(tris.begin(), trisEnd, Cmp_Dist);
    std::vector<Mesh::Index> indices;
    for(triIt_t triIt(tris.begin()); trisEnd != triIt; ++triIt) {
        indices.push_back(triIt->bufIndices.indices[0]);
        indices.push_back(triIt->bufIndices.indices[1]);
        indices.push_back(triIt->bufIndices.indices[2]);
    }
    if(!indices.empty()) {
        BindVertices();
        GL_CALL(glDrawElements(GL_TRIANGLES, indices.size(), ToGLEnum<Mesh::Index>::ENUM, &indices[0]));
        metrics.frame.numDrawCalls++;
    }
    return meshJob;
}

static void DrawFrame(RenderList& renderList, const M::Matrix4& projectionMat, float time) {
    if(renderList.jobs.empty()) return;

    GB_Bind();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    RenderJob* cur = &renderList.jobs[0];
    RenderJob* next = NULL;
    while(cur) {
        next = NULL;
        GEN::Pointer<Effect> fx = effectMgr.GetEffect(cur->fx);
        int numPasses = fx->NumPasses();
        for(int i = 0; i < numPasses; ++i) {
            Pass* pass = fx->GetPass(i);
            pass->Use();
            
            Program&        prog = pass->GetProgram();
            const PassDesc& desc = pass->GetDesc();
            Uniforms_BindBlocks(prog);
            Uniforms_BindBuffers();
            SetState(desc.state);

            next = DrawMeshList(prog, desc.state, desc.type, desc.flags, renderList.worldMat, cur);
        }
        cur = next;
    }
}

static bool CompareJobs(const R::RenderJob& lhp, const R::RenderJob& rhp) {
    return effectMgr.GetEffect(lhp.fx)->SortKey() < effectMgr.GetEffect(rhp.fx)->SortKey();
}

static void Link(std::vector<R::RenderJob>& renderJobs) {
    unsigned numJobs = renderJobs.size();
	if(0 < numJobs) {
		for(unsigned i = 0; i < numJobs; ++i) {
			RenderJob& rjob = renderJobs[i];
			rjob.next = &renderJobs[(i + 1) % numJobs];
		}
		renderJobs[numJobs - 1].next = NULL;
	}
}

static void Compile(R::RenderJob& rjob) {
    effectMgr.GetEffect(rjob.fx)->Compile();
    meshMgr.GetMesh(rjob.mesh).R_Compile();
    meshMgr.GetMesh(rjob.mesh).R_Touch();
}

static void Transform(const M::Matrix4& worldMat, R::RenderJob& rjob) {
    Mesh& mesh = meshMgr.GetMesh(rjob.mesh);
    mesh.Transform(rjob.transform);
}

static void CompileAndTransform(const M::Matrix4& worldMat, R::RenderJob& rjob) {
    Compile(rjob);
    Transform(worldMat, rjob);
}

static M::Matrix4 ComputeProjectionMatrix(float aspect, const M::Matrix4& worldMat, const std::vector<R::RenderJob>& renderJobs) {
    /*
    float zMin = 10000.0f, zMax = -10000.0f;
    unsigned numJobs = renderJobs.size();
    for(unsigned i = 0; i < numJobs; ++i) {
        const RenderJob& rjob = renderJobs[i];
        M::Vector3 v = M::Transform(rjob.transform, M::Vector3::Zero);
        float r = rjob.mesh->Radius();
        if(0 > v.z - r) zMin = M::Min(zMin, v.z - r);
        if(0 > v.z + r) zMax = M::Max(zMax, v.z + r);
    }
    return M::Mat4::Perspective(45.0f, aspect, -zMax, -zMin);
    */
    return M::Mat4::Perspective(45.0f, aspect, 0.1f, 100.0f);
}

void Renderer::Render(void) {
    float secsPassed = _timer.Stop();
    _time += secsPassed;
    _timer.Start();

    if(curState.depth.maskEnabled != GL_TRUE) {
        GL_CALL(glDepthMask(GL_TRUE));
        curState.depth.maskEnabled = GL_TRUE;
    }
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    RenderList& renderList = g_renderLists[rlIdx];

    if(!(renderList.jobs.empty() && renderList.nodePositions.empty() && renderList.edges.empty())) { 

        static SYS::Timer frameTime;
        frameTime.Start();

        metrics.frame.numDrawCalls = 0;


        M::Matrix4 projectionMat = ComputeProjectionMatrix(_aspect, renderList.worldMat, renderList.jobs);

        Uniforms_Update(projectionMat, renderList.worldMat, renderList.dirLights);

        InitBillboards(renderList.nodePositions, renderList.nodeColors);
        BuildBillboards(renderList.worldMat);
        unsigned numBillboards = billboardPositions.size();
        if(numBillboards) {
            billboardHotVertexBuffer->Update_Mapped(0, sizeof(BillboardHot) * numBillboards, &billboardsHot[0]);
            DrawBillboards(renderList.worldMat, projectionMat);
            billboardHotVertexBuffer->Discard();
        }

        if(!renderList.edges.empty()) {
            RemoveDegeneratedEdges(renderList.edges);
            ReserveEdgeBBoxBuffers(renderList.edges.size());
            CreateEdges(renderList.edges);
            DrawEdges(projectionMat, renderList.worldMat);
        }

        std::sort(renderList.jobs.begin(), renderList.jobs.end(), CompareJobs);
        Link(renderList.jobs);
        GB_Bind();
        std::for_each(renderList.jobs.begin(), renderList.jobs.end(),
            std::bind(CompileAndTransform, renderList.worldMat, std::placeholders::_1));
        GB_CacheAll();
        DrawFrame(renderList, projectionMat, _time);

        meshMgr.R_Update();

        metrics.frame.time = frameTime.Stop();
    }

    g_rendererSem.Signal();
    W::g_worldSem.Wait();
    rlIdx = 1 - rlIdx;
}


} // namespace R
