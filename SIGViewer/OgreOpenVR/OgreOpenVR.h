#ifndef OGRE_OPEN_VR_H_
#define OGRE_OPEN_VR_H_

#undef GL_VERSION_1_1
#undef GL_VERSION_3_0
#undef GL_VERSION_3_1
#undef GL_VERSION_3_2
#undef GL_VERSION_3_3
#undef GL_VERSION_4_0
#undef GL_VERSION_4_1
#undef GL_VERSION_4_2
#undef GL_VERSION_4_3
#undef GL_VERSION_4_4

#include <RenderSystems\GL\OgreGLTextureManager.h>
#include <RenderSystems\GL\OgreGLRenderSystem.h>
#include <RenderSystems\GL\OgreGLTexture.h>
#include <RenderSystems\GL\OgreGLFrameBufferObject.h>
#include <RenderSystems\GL\OgreGLFBORenderTexture.h>
#include <RenderSystems\GL\OgreGLRenderTexture.h>
#include "OgreSceneManager.h"
#include "OgreRenderWindow.h"
#include "OgreViewport.h"
#include "OgreSharedPtr.h"

// Ogre
#include "ogrecamera.h"
#include "ogrescenemanager.h"
#include "ogretexturemanager.h"
#include "ogretexture.h"
#include "ogrelogmanager.h"
#include "OgreMesh.h"
#include "OgreMeshManager.h"
#include "ogrehardwarepixelbuffer.h"
#include "ogreroot.h"
#include "ogrerendertarget.h"
#include "ogrerendertexture.h"
#include "ogrerenderwindow.h"

#include <openvr.h>


class OgreOpenVR
{
public:
    struct FramebufferDesc
    {
        uint32_t g_nDepthBufferId;
        uint32_t g_nRenderTextureId;
        uint32_t g_nRenderFramebufferId;
        uint32_t g_nResolveTextureId;
        uint32_t g_nResolveFramebufferId;
    };

    bool initOpenVR(Ogre::SceneManager *sm, Ogre::RenderWindow *rw, Ogre::SceneNode* parent = nullptr);
    bool initOpenVRCompositor();

    bool initOgre(Ogre::SceneManager *sm);
    bool initOgreCameras();
    bool initOgreTextures();
    bool initOgreViewports();

    bool handleInput();
    void updateHMDPos();
    void update();

    const unsigned int          LeftEye = 0;
    const unsigned int          RightEye = 1;
    const unsigned int          EyeCount = 2;

    //void setPosition(Ogre::Vector3 pos);
    Ogre::Vector3 getPosition();
    Ogre::Quaternion getOrientation();
    Ogre::Matrix4 getHMDMat4();

    void resetOrientation();

    Ogre::Camera*               m_cameras[2] = { nullptr, nullptr };
    Ogre::SceneNode*            m_cameraNode = nullptr;
    Ogre::Viewport*             m_viewports[2] = { nullptr, nullptr };

private:


    // Ogre
    Ogre::RenderWindow*         m_window = nullptr;
    Ogre::SceneManager*         m_sceneManager = nullptr;
    Ogre::TexturePtr            m_ogreRenderTexture;
    Ogre::TexturePtr            m_ogreResolveTexture;

    // helper functions
    Ogre::Matrix4 convertSteamVRMatrixToOgreMatrix4(const vr::HmdMatrix34_t &matPose);
    std::string getTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL);
    Ogre::Matrix4 getHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
    Ogre::Matrix4 getHMDMatrixPoseEye(vr::Hmd_Eye nEye);

    // General
    Ogre::Vector3               m_position;
    Ogre::Quaternion            m_orientation;
    Ogre::Vector3               m_poseNeutralPosition = Ogre::Vector3::ZERO;
    Ogre::Quaternion            m_poseNeutralOrientation = Ogre::Quaternion::IDENTITY;
    float                       m_heightStanding = 1.7f;
    float                       m_heightSitting = 1.0;
    bool                        m_isStanding = true;
    float                       m_IPD = 0.07f;
    float                       m_fNearClip = 0.01f;
    float                       m_fFarClip = 1000.0f;
    long long                   m_frameIndex = 0;

    // openvr
    vr::IVRSystem*              m_pHMD;
    vr::IVRRenderModels*        m_pRenderModels;
    std::string                 m_strDriver;
    std::string                 m_strDisplay;
    vr::TrackedDevicePose_t     m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
    Ogre::Matrix4               m_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];
    Ogre::Matrix4               m_mat4HMDPose;
    uint32_t                    m_nRenderWidth;
    uint32_t                    m_nRenderHeight;
    int                         m_iValidPoseCount;
    int                         m_iValidPoseCount_Last;
    std::string                 m_strPoseClasses;
    char                        m_rDevClassChar[vr::k_unMaxTrackedDeviceCount];

    OgreOpenVR::FramebufferDesc leftEyeDesc;
    OgreOpenVR::FramebufferDesc rightEyeDesc;
};

#endif // OGRE_OPEN_VR_H_