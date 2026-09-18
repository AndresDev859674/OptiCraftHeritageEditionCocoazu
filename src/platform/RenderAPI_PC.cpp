#include "platform/RenderAPI.h"

#if PLATFORM_PC

#include "pc/render/PcRenderBackend.h"
#include "pc/render/PcRenderBackendApi.h"

#if defined(MC_WIN32)
#define PC_RENDER_TRY_D3D9(name, callArgs) \
    if (pcRenderBackendIsDirect3D9()) \
        return PcD3D9RenderBackend::name callArgs;
#else
#define PC_RENDER_TRY_D3D9(name, callArgs)
#endif

#define PC_RENDER_DISPATCH(returnType, name, signature, callArgs) \
    returnType name signature \
    { \
        PC_RENDER_TRY_D3D9(name, callArgs) \
        return PcOpenGlRenderBackend::name callArgs; \
    }

PC_RENDER_BACKEND_FUNCTIONS(PC_RENDER_DISPATCH)

#undef PC_RENDER_DISPATCH
#undef PC_RENDER_TRY_D3D9

#endif
