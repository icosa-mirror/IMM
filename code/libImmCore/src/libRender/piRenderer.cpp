//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include "piRenderer.h"

#if defined(ANDROID)
#include "opengles/piGLES_Renderer.h"
#elif defined(WINDOWS)
#include "opengl4x/piGL4X_Renderer.h"
#include "directx11/piDX11_Renderer.h"
#elif defined(__APPLE__)
#include "opengl4x/piGL4X_Renderer.h"
#include "metal/piMetal_Renderer.h"
#else
#include "opengl4x/piGL4X_Renderer.h"
#endif


namespace ImmCore {


piRenderer *piRenderer::Create( const API type )
{
#if defined(ANDROID)
	if( type==API::GLES ) return new piRendererGLES();
#elif defined(WINDOWS)
	if( type==API::GL ) return new piRendererGL4X();
	if( type==API::DX ) return new piRendererDX11();
#elif defined(__APPLE__)
	if( type==API::GL ) return new piRendererGL4X();
	if( type==API::Metal ) return new piRendererMetal();
#else
	if( type==API::GL ) return new piRendererGL4X();
#endif

	return 0;
}

}
