/**
  \file G3D/constants.h

  \maintainer Morgan McGuire, http://graphics.cs.williams.edu
  \created 2009-05-20
  \edited  2015-01-02
*/
#ifndef G3D_constants_h
#define G3D_constants_h

#include "G3D/platform.h"
#include "G3D/enumclass.h"
#include "G3D/Any.h"

namespace G3D {

/** These are defined to have the same value as the equivalent OpenGL
    constant. */
class PrimitiveType {
public:
    enum Value {
        POINTS         = 0x0000,
        LINES          = 0x0001,
        LINE_STRIP     = 0x0003, 
        TRIANGLES      = 0x0004, 
        TRIANGLE_STRIP = 0x0005,
        TRIANGLE_FAN   = 0x0006,
        QUADS          = 0x0007, 
        QUAD_STRIP     = 0x0008,
        PATCHES        = 0x000E
    };

private:
    
    static const char* toString(int i, Value& v);

    Value value;

public:

    G3D_DECLARE_ENUM_CLASS_METHODS(PrimitiveType);
};


/** Values for UniversalSurface::GPUGeom::refractionHint. */
G3D_DECLARE_ENUM_CLASS(RefractionHint,
        /** No refraction; a translucent object will appear as if it had the same index of refraction
            as the surrounding medium and objects will be undistorted in the background. */
        NONE, 

        /** Use a static environment map (cube or paraboloid) for computing transmissivity.*/
        //STATIC_PROBE, 

        /** Use a dynamically rendered 2D environment map; distort the background.  This looks good for many scenes
            but avoids the cost of rendering a cube map for DYNAMIC_ENV. UniversalSurface considers this hint
            to mean that blending is not required, so G3D::Renderer::cullAndSort will categorize
            UniversalSurface with this hint as opaque and a G3D::Renderer will send them to the RenderPassType::UNBLENDED_SCREEN_SPACE_REFRACTION_SAMPLES pass.
            */
        DYNAMIC_FLAT,

        /** Combine DYNAMIC_FLAT mode with order-independent transparency. UniversalSurface considers this hint to
            require blending, so G3D::Renderer::cullAndSort will categorize
            send UniversalSurface with this hint as transparent and a Renderer
            will then send them to the RenderPassType::SINGLE_PASS_UNORDERED_BLENDED_SAMPLES pass. 
            \sa DefaultRenderer::setOrderIndependentTransparency
            \sa DefaultRenderer::cullAndSort
        */
        DYNAMIC_FLAT_OIT,

        /** Use a dynamically rendered 2D environment map that is re-captured per transparent object.  This works well
            for transparent objects that are separated by a significant camera space z distance but overlap in screen space.*/
        //DYNAMIC_FLAT_MULTILAYER,

        /** Render a dynamic environment map */
        //DYNAMIC_PROBE, 

        /** True ray tracing. */
        RAY_TRACE);


/** Values for UniversalSurface::GPUGeom::mirrorHint. */
G3D_DECLARE_ENUM_CLASS(MirrorQuality,
        /** Reflections are black */
        NONE, 
        
        /** Use a static environment map.  This is what most games use */
        STATIC_PROBE,
        
        /** Use a screen-space hack to approximate reflection */
        SCREEN_SPACE,

        /** Render a dynamic environment map. */
        DYNAMIC_PROBE, 
        
        /** True ray tracing. */
        RAY_TRACE);


/** \brief How the alpha value should be interpreted for partial coverage. 

    This must be kept in sync with AlphaFilter.glsl
    \sa UniversalMaterial */
G3D_DECLARE_ENUM_CLASS(AlphaFilter,
        /** Used by UniversalMaterial to specify a default behavior:
        
            - use BLEND for surfaces with some 0.0 < alpha < 1.0
            - use ONE for surfaces with all alpha = 1.0. 
            - use BINARY for surfaces with both alpha = 0 and alpha = 1 at locations, but no intermediate values
        */
        DETECT,

        /** Treat the surface as completely covered, forcing alpha = 1.0 everywhere */
        ONE,

        /** Alpha is rounded per sample before being applied. Alpha >= 0.5 is rendered, alpha < 0.5 is discarded. 
            This enables faster deferred shading and more accurate post-processing because the surface can be exactly represented in the 
            GBuffer. It creates some aliasing on surface edges, however. */
        BINARY,

        /** Convert alpha to coverage values using
           <code>glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB)</code>.
           Requires a MSAA framebuffer to be bound.

           Not currently supported by UniversalSurface.
           
           See http://www.dhpoware.com/samples/glMultiSampleAntiAliasing.html for an example.*/
        COVERAGE_MASK,
        
        /** Render surfaces with partial coverage from back to front,
            using Porter and Duff&apos;s OVER operator.  This leaves the
            depth buffer inconsistent with the color buffer and
            requires a sort, but often gives the best appearance. 
            
            Even surfaces with a binary alpha cutout can benefit from
            blending because the transition under minification or magnification
            between alpha = 0 and alpha = 1 will create fractional values.*/
        BLEND
    );

} // namespace G3D

G3D_DECLARE_ENUM_CLASS_HASHCODE(G3D::PrimitiveType)
G3D_DECLARE_ENUM_CLASS_HASHCODE(G3D::RefractionHint)
G3D_DECLARE_ENUM_CLASS_HASHCODE(G3D::MirrorQuality)
G3D_DECLARE_ENUM_CLASS_HASHCODE(G3D::AlphaFilter)

#endif

