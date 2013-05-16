#pragma once

#include <vector>

#include <generic\pointer.h>
#include <system\locks\spinlock.h>
#include <math\matrix4.h>
#include "light\light.h"

namespace R {

    class   Effect;
    class   Mesh;
    struct  Material;

    typedef GEN::Pointer<Mesh, GEN::PointerImpl::LockedRefCount<SYS::SpinLock> > meshPtr_t;

    struct RenderJob {
        Effect*     fx;
        meshPtr_t   mesh;
        Material*   material;
        M::Matrix4  transform;

        // handled by renderer
        RenderJob* next;
    };

	class Renderer {
    private:
        std::vector<RenderJob>  _renderJobs;
        std::vector<Light>      _lights;

        void DrawFrame(const M::Matrix4& worldMat, const M::Matrix4& projectionMat);
	public:
        void Init(void); // requires gl context

        void Resize(int width, int height);

        void Add(const Light& light);

        void BeginFrame(void);
        void Add(const RenderJob& renderJob);
        void EndFrame(const M::Matrix4& worldMat, const M::Matrix4& projectionMat);
	};

} // namespace R
