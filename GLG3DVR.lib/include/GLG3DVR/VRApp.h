/**
\file VRApp.h

    G3D Innovation Engine
    Copyright 2000-2016, Morgan McGuire.
    All rights reserved.

*/
#pragma once

#include "GLG3D/GApp.h"
#include "GLG3D/Texture.h"
#include <openvr/openvr.h>

#ifdef _MSC_VER
#   pragma comment(lib, "openvr_api")
#   ifdef G3D_DEBUG
#       pragma comment(lib, "GLG3DVR_x64d.lib")
#   else
#       pragma comment(lib, "GLG3DVR_x64.lib")
#   endif
#endif

namespace G3D {

class MarkerEntity;

/** \brief Application framework for HMD Virtual Reality programs on HTC Vive, Oculus Rift, and Oculus DK2.

    Use the tab key to toggle seeing the GUI in the HMD.
    
    For many programs, simply changing from inheriting your App from GApp to VRApp will immediately
    add HMD support. You must have the OpenVR Runtime (AKA SteamVR) installed to use VRApp. OpenVR is 
    a free download as part of (Steam)[https://steamcdn-a.akamaihd.net/client/installer/SteamSetup.exe], which
    is also free download for multiple platforms.
    
    You do *not* need to install the Oculus, SteamVR, or OpenVR SDKs--G3D includes the files that you need.

    There are several MarkerEntity and reference frames at play for VR in order to attempt to model
    the degrees of freedom for a tracked HMD while still minimizing the amount of code needed to make
    a non-VR GApp subclass run as a VRGApp subclass. This API is in a very early state and subject to 
    continous change.

    The reference frames are:

    - The "body" is the player's **feet**, or rather, the center of the floor in the tracked space. GApp::activeCamera() _before_ GApp::onGraphics() runs. This is the 
      center of the tracked space. For seated and standing VR such as Oculus Rift, this probably is the actual
      player's body. For nomad (walking) VR, this is effectively the center of the room. To
      "teleport", move the body camera.
    - VRApp::m_vrControllerArray Array of MarkerEntitys for tracked controllers ("wands") in VR. 
      Their names are of the form "VR Controller %d". The first one has index 0 and is always the right hand.
    - VRApp::m_eyeFrame transforms from body space to the camera used for rendering each eye.
    - VRApp::m_vrHead is a child of the body. This is the tracked MarkerEntity for the player's HMD.
    - VRApp::m_vrEyeCamera is the full camera for rendering each eye. It is a child of the m_vrHead.

    \beta
*/
class VRApp : public GApp {
public:

    // See documentation on setSubmitToDisplayMode
    // Note that this is also used for the VR API
    G3D_DECLARE_ENUM_CLASS(DebugMirrorMode,
        NONE,
        
        /** Both eyes without HMD distortion to correct for chromatic abberation. This is the output of onGraphics3D */
        PRE_DISTORTION);

    class Settings : public GApp::Settings {
    public:

        class VR {
        public:

            /** Defaults to false. Cannot be changed once VRApp is initialized. */
            DebugMirrorMode     debugMirrorMode;

            /** If no HMD is present, should the system provide a virtual HMD for development
                and debugging? If false and there
                is no HMD, the system will throw an error. */
            bool                emulateHMDIfMissing;

            /** If no physical VR controller is present, should the system provide a
                virtual one that is locked relative to the HMD? This is useful for
                both development and deployment.
                
                If false and there is no physical controller, then there is no error--
                tracking will simply report nothing. */
            bool                emulateControllerIfMissing;

            /** Use pitch control from the HMD instead of from the m_cameraManipulator. Defaults to true.
            For walking simulators, we recommend m_trackingOverridesPitch = true.
            For driving or flight simulators, we recommend m_trackingOverridesPitch = false.

            Yaw control is not overriden in order to allow typical first-person strafing movement and rotation.
            (We may provide an option to do so, or at least to compose them, in the future). Beware that this can be confusing
            to the user unless some kind of body avatar is rendered.

            Can be changed at runtime, although some inconsistency may occur for a few frames after the change.
            */
            bool                trackingOverridesPitch;

            /** If this is true, after too many frames have rendered below the target frame rate
            post-processing effects will be selectively disabled on the active camera. Defaults
            to true.
            */
            bool                disablePostEffectsIfTooSlow;

            /**
             Force motionBlurSettings on VR eye cameras at render time.
            */
            bool                overrideMotionBlur;
            
            /** Defaults to 100% camera motion, 15% exposure time, enabled. 
                \sa overrideMotionBlur */
            MotionBlurSettings  motionBlurSettings;

            /**
             Force depthOfFieldSettings on VR eye cameras at render time.
            */
            bool                overrideDepthOfField;

            /** Disabled */
            DepthOfFieldSettings depthOfFieldSettings;

            /** Must be CAMERA (player head), OBJECT (player body, the default), or WORLD (fixed at the origin) */
            FrameName           hudSpace;

            VR(bool debugMirrorMode = DebugMirrorMode::NONE) : 
                emulateHMDIfMissing(true),
                emulateControllerIfMissing(true),
                trackingOverridesPitch(true),
                disablePostEffectsIfTooSlow(true),
                debugMirrorMode(debugMirrorMode), 
                overrideMotionBlur(true),
                overrideDepthOfField(true),
                hudSpace(FrameName::OBJECT) {

                motionBlurSettings.setCameraMotionInfluence(1.00f);
                motionBlurSettings.setExposureFraction(0.15f);
                motionBlurSettings.setEnabled(true);

                depthOfFieldSettings.setEnabled(false);
            }
        } vr;

        /** Also invokes initGLG3D() */
        Settings() {}

        /** Also invokes initGLG3D() */
        Settings(int argc, const char* argv[]) : GApp::Settings(argc, argv) {}
    };

private:

    /** Used to hack the window to a fixed size, as required by the HMD for mirroring and HUD rendering */
    static const GApp::Settings& makeFixedSize(const GApp::Settings& s);

protected:

    /** Intended for subclasses to redefine to themselves. This allows application subclasses
        to invoke `super::onInit`, etc., and easily change their base class without rewriting
        all methods. */
    typedef GApp super;

    /** If nullptr, then VR initialization failed */
    vr::IVRSystem*          m_hmd;
    vr::TrackedDevicePose_t m_trackedDevicePose[vr::k_unMaxTrackedDeviceCount];

    Array<shared_ptr<MarkerEntity>> m_vrControllerArray;

    /**
      The HDR framebuffer used by G3D::Film for the HMD. 
      Comparable to GApp::monitorFramebuffer.
     */
    shared_ptr<Framebuffer> m_hmdHDRFramebuffer[2];

    /** LDR faux-"hardware framebuffer" for the HMD, comparable to GApp::m_osWindowDeviceFramebuffer.
        This is the buffer that 

        The m_framebuffer is still bound during the default onGraphics3D and then
        resolved by Film to the m_hmdDeviceFramebuffer.
        */
    shared_ptr<Framebuffer> m_hmdDeviceFramebuffer[2];

    /** Net eye-to-body transform */
    CFrame                  m_previousEyeFrame[2];

    /** Net eye-to-body transform */
    CFrame                  m_eyeFrame[2];

    SubmitToDisplayMode     m_vrSubmitToDisplayMode;

    /** Automatically turned on when the scene is loaded,
       disabled only if frame rate can't be maintained */
    bool                    m_highQualityWarping;

    /** Set by onGraphics for each onGraphics3D call. */
    int                     m_currentEyeIndex;

    /** The active m_gbuffer is switched between these per eye. That allows
        reprojection between them. */
    shared_ptr<GBuffer>     m_hmdGBuffer[2];

    /** \see m_numSlowFrames */
    static const int        MAX_SLOW_FRAMES = 20;

    /** The number of frames during which the renderer failed to reach the desired frame rate.
        When this count hits MAX_SLOW_FRAMES, some post effects are disabled and m_numSlowFrames
        resets.
    */
    int                     m_numSlowFrames;

    /** If true, onGraphics2D is captured and displayed in the HMD. By default, TAB toggles this. */
    float                   m_hudEnabled;

    /** Position at which onGraphics2D renders on the virtual HUD layer if m_hudEnabled == true. 
    
    \sa VRApp::Settings::hudSpace */
    CFrame                  m_hudFrame;

    /** Width in meters of the HUD layer used to display onGraphics2D content in the HMD. \sa VRApp::Settings::hudSpace */
    float                   m_hudWidth;

    /** Color of the HUD background, which reveals the boundaries of the virtual display */
    Color4                  m_hudBackgroundColor;

    /** Tracks the position of the player's HMD, as determined from the eye cameras. Can be used
        to attach other objects relative to the head. 
        
        \sa m_eyeFrame, m_previousEyeFrame, m_vrEyeCamera
        */
    shared_ptr<MarkerEntity> m_vrHead;
    
    /** Camera not visible to the normal scene graph that is updated every frame based on m_eyeFrame and m_vrHead.
        The GApp::activeCamera() is bound to one of these during GApp::onGraphics. 
        
        \sa m_eyeFrame, m_vrHead */
    shared_ptr<Camera>      m_vrEyeCamera[2];

    Settings::VR            m_vrSettings;

    shared_ptr<Texture>     m_cursorPointerTexture;

    /** if m_cameraManipulator is a FirstPersonManipulator and m_trackingOverridesPitch is true,
     then zero out the pitch and roll in \a source */
    CFrame maybeRemovePitchAndRoll(const CFrame& source) const;

    /** If frame rate is being consistently missed, reduce the effects on activeCamera() */
    void maybeAdjustEffects();
    
    /** Called by maybeAdjustEffects when the frame rate is too low */
    void decreaseEffects();

public:
    
    int numEyes() const {
        return isNull(m_hmd) ? 1 : 2;
    }

    /** The window will be forced to non-resizable */
    VRApp(const GApp::Settings& settings = VRApp::Settings());

    virtual void onInit() override;

    /** Used to override the first person manipulator's pitch and roll using tracked data. */
    virtual void onBeforeSimulation(RealTime& rdt, SimTime& sdt, SimTime& idt) override;

    /** Set up a double-eye call for onGraphics3D */
    virtual void onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) override;

    virtual void onCleanup() override;
    
    /** Latch tracking data for:

        - m_trackedDevicePose
        - m_previousEyeFrame
        - m_eyeFrame
        - m_vrHead
        - m_vrEyeCamera
    */
    virtual void sampleTrackingData();

    /** Like swapBuffers for the m_hmd */
    virtual void submitHMDFrame(RenderDevice* rd);

    /** Intentionally empty so that subclasses don't accidentally swap buffers. Simplifies upgrading existing apps to VRApps */
    virtual void swapBuffers() override;

    /** Resets some state and adds the VRApp::m_vrHead MarkerEntity to the scene. */
    virtual void onAfterLoadScene(const Any &any, const String &sceneName) override;

    /** Support for toggling the HUD using the TAB key */
    virtual bool onEvent(const GEvent& event) override;
};

} // namespace
