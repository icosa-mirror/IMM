//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include "piRenderer.h"

#if defined(IMM_IOS)
#include "metal/piMetal_Renderer.h"
#elif defined(ANDROID)
#include "opengles/piGLES_Renderer.h"
#include "vulkan/piVulkan_Renderer.h"
#elif defined(WINDOWS)
#include "opengl4x/piGL4X_Renderer.h"
#include "directx11/piDX11_Renderer.h"
#include "vulkan/piVulkan_Renderer.h"
#elif defined(__APPLE__)
#include "opengl4x/piGL4X_Renderer.h"
#include "metal/piMetal_Renderer.h"
#else
#include "opengl4x/piGL4X_Renderer.h"
#endif


namespace ImmCore {


piRenderer *piRenderer::Create( const API type )
{
#if defined(IMM_IOS)
	if( type==API::Metal ) return new piRendererMetal();
#elif defined(ANDROID)
	if( type==API::GLES ) return new piRendererGLES();
	if( type==API::Vulkan ) return new piRendererVulkan();
#elif defined(WINDOWS)
	if( type==API::GL ) return new piRendererGL4X();
	if( type==API::DX ) return new piRendererDX11();
	if( type==API::Vulkan ) return new piRendererVulkan();
#elif defined(__APPLE__)
	if( type==API::GL ) return new piRendererGL4X();
	if( type==API::Metal ) return new piRendererMetal();
#else
	if( type==API::GL ) return new piRendererGL4X();
#endif

	return 0;
}

}
