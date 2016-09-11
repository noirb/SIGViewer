#include "OgreOpenVR.h"


bool OgreOpenVR::initOpenVR(Ogre::SceneManager *sm, Ogre::RenderWindow *rw, Ogre::SceneNode* parent)
{
    // Loading OpenVR
    vr::EVRInitError eError = vr::VRInitError_None;
    m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);
    if (eError != vr::VRInitError_None)
    {
        Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: VR_Init error");
        m_pHMD = NULL;
        return false;
    }

    m_sceneManager = sm;
    m_window = rw;
    m_frameIndex = 0;


    if (parent)
    {
        m_cameraNode = parent->createChildSceneNode("CameraNode");
    }
    else
    {
        m_cameraNode = sm->getRootSceneNode()->createChildSceneNode("CameraNode");
    }

    m_strDriver = "No Driver";
    m_strDisplay = "No Display";
    m_strDriver = OgreOpenVR::getTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
    m_strDisplay = OgreOpenVR::getTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

    m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);

    // update() gets called before the first updateHMDPos() so we need to make sure this is initialized to something
    m_mat4HMDPose = Ogre::Matrix4::IDENTITY; 

    glewInit();

    if (!initOpenVRCompositor())
    {
        Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: Failed to initialise OpenVR Compositor");
        return false;
    }

    // Configure 'seated' mode so orientation resets will happen for us
    vr::VRCompositor()->SetTrackingSpace(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);

    if( !initOgreTextures())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOpenVR: Failed to initialise Ogre textures");
        return false;
    }

    if (!initOgreCameras())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOpenVR: Failed to initialise Ogre Cameras");
        return false;
    }

    if (!initOgreViewports())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOpenVR: Failed to initialise Ogre viewports");
        return false;
    }

    return true;
}

bool OgreOpenVR::initOgre(Ogre::SceneManager *sm)
{
    return true;
}

bool OgreOpenVR::initOpenVRCompositor()
{
    if (!vr::VRCompositor())
        return false;

    return true;
}

// ----------------------------------------------------------------
// creates the ogre cameras and sets the projection matrices
// ----------------------------------------------------------------
bool OgreOpenVR::initOgreCameras()
{
    m_sceneManager->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));
    m_sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);

    m_cameras[0] = m_sceneManager->createCamera("OpenVR Left Eye");
    m_cameras[1] = m_sceneManager->createCamera("OpenVR Right Eye");

    m_cameraNode->setOrientation(Ogre::Quaternion::IDENTITY);
    m_cameraNode->setPosition(0, 0, 0);

    Ogre::Matrix4 proj;
    for (int i = 0; i < 2; ++i)
    {
        Ogre::Matrix4 camMat4 = getHMDMatrixPoseEye(static_cast<vr::Hmd_Eye>(i));

        const Ogre::Vector3 camPos(m_IPD * (i - 0.5f), 0, 0);
        Ogre::Quaternion camOrientation = camMat4.extractQuaternion();
            
        m_cameras[i]->setPosition(camPos);
        m_cameras[i]->setNearClipDistance(m_fNearClip);
        m_cameras[i]->setFarClipDistance(m_fFarClip);
        m_cameras[i]->setAutoAspectRatio(true);
        m_cameras[i]->detachFromParent();
        m_cameraNode->attachObject(m_cameras[i]);
        m_cameras[i]->setDirection(0, 0, -1);

        // get and set the projection matrix for this camera
        proj = getHMDMatrixProjectionEye( static_cast<vr::Hmd_Eye>(i) );
        m_cameras[i]->setCustomProjectionMatrix(true, proj);
    }


    return true;
}

// ----------------------------------------------------------------
// gets the projection matrix for the specified eye
// ----------------------------------------------------------------
Ogre::Matrix4 OgreOpenVR::getHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
{
    if (!m_pHMD)
        return Ogre::Matrix4();

    // there may be a bug in openvr where this is returning a directx style projection matrix regardless of the api specified
    vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(nEye, m_fNearClip, m_fFarClip, vr::API_OpenGL);
                
    // convert to Ogre::Matrix4
    return Ogre::Matrix4(
        mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
        mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
        mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
        mat.m[3][0], mat.m[3][1], mat.m[3][2], mat.m[3][3]
    );
}

// ----------------------------------------------------------------
// gets the location of the specified eye
// ----------------------------------------------------------------
Ogre::Matrix4 OgreOpenVR::getHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{
    if (!m_pHMD)
        return Ogre::Matrix4();

    vr::HmdMatrix34_t matEye = m_pHMD->GetEyeToHeadTransform(nEye);    
    Ogre::Matrix4 eyeTransform = convertSteamVRMatrixToOgreMatrix4(matEye);

    return eyeTransform.inverse();
}

// ----------------------------------------------------------------
// creates the ogre textures for each camera to render to
// ----------------------------------------------------------------
bool OgreOpenVR::initOgreTextures()
{
    m_ogreRenderTexture = Ogre::TextureManager::getSingleton().createManual("OpenVR Render Texture", "General", 
        Ogre::TextureType::TEX_TYPE_2D,
        m_nRenderWidth*2, m_nRenderHeight,
        0,
        Ogre::PixelFormat::PF_R8G8B8A8,
        Ogre::TextureUsage::TU_RENDERTARGET);
    m_ogreResolveTexture = Ogre::TextureManager::getSingleton().createManual("OpenVR Resolve Texture", "General",
        Ogre::TextureType::TEX_TYPE_2D,
        m_nRenderWidth*2, m_nRenderHeight,
        0,
        Ogre::PixelFormat::PF_R8G8B8A8,
        Ogre::TextureUsage::TU_DEFAULT);

    return true;
}

// ----------------------------------------------------------------
// sets up the ogre viewports and associates the appropriate 
// camera & render texture to each
// ----------------------------------------------------------------
bool OgreOpenVR::initOgreViewports()
{
    Ogre::RenderTexture* renderTex = m_ogreRenderTexture->getBuffer()->getRenderTarget();

    // configure the two viewports to each render to half of the render targer
    // each half corresponds to one eye, and we submit both in one big texture to OpenVR
    m_viewports[0] = renderTex->addViewport(m_cameras[0], 0, 0.0f, 0.0f, 0.5f, 1.0f);
    m_viewports[1] = renderTex->addViewport(m_cameras[1], 1, 0.5f, 0.0f, 0.5f, 1.0f);

    renderTex->setAutoUpdated(true);

    m_viewports[0]->setBackgroundColour(Ogre::ColourValue(97 / 255.0f, 200 / 255.0f, 200 / 255.0f));
    m_viewports[0]->setOverlaysEnabled(true);
    m_viewports[0]->setAutoUpdated(true);
    m_viewports[1]->setBackgroundColour(Ogre::ColourValue(97 / 255.0f, 200 / 255.0f, 200 / 255.0f));
    m_viewports[1]->setOverlaysEnabled(true);
    m_viewports[1]->setAutoUpdated(true);

    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOpenVR: Setup RTT viewports for ogre OpenVR textures");

    return true;
}

// ----------------------------------------------------------------
// gets the current position of all tracked devices
// ----------------------------------------------------------------
void OgreOpenVR::updateHMDPos()
{
    if (!m_pHMD)
        return;

    vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    m_iValidPoseCount = 0;
    m_strPoseClasses = "";
    for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
    {
        if (m_rTrackedDevicePose[nDevice].bPoseIsValid)
        {
            m_iValidPoseCount++;
                
            m_rmat4DevicePose[nDevice] = convertSteamVRMatrixToOgreMatrix4(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
            if (m_rDevClassChar[nDevice] == 0)
            {
                switch (m_pHMD->GetTrackedDeviceClass(nDevice))
                {
                case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
                case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
                case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
                case vr::TrackedDeviceClass_Other:             m_rDevClassChar[nDevice] = 'O'; break;
                case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
                default:                                       m_rDevClassChar[nDevice] = '?'; break;
                }
            }
            m_strPoseClasses += m_rDevClassChar[nDevice];
        }
    }

    if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
    {
        m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
    }

}

bool OgreOpenVR::handleInput()
{
    bool bRet = false;
        
    // Process SteamVR events
    vr::VREvent_t event;
    while (m_pHMD->PollNextEvent(&event, sizeof(event)))
    {
        switch (event.eventType)
        {
            case vr::VREvent_TrackedDeviceActivated:
                
                //SetupRenderModelForTrackedDevice(event.trackedDeviceIndex);
                //dprintf("Device %u attached. Setting up render model.\n", event.trackedDeviceIndex);
                break;

            case vr::VREvent_TrackedDeviceDeactivated:
                //dprintf("Device %u detached.\n", event.trackedDeviceIndex);
                break;

            case vr::VREvent_TrackedDeviceUpdated:
                //dprintf("Device %u updated.\n", event.trackedDeviceIndex);
                break;
        }
        
    }

    // Process SteamVR controller state
    for (vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++)
    {
        vr::VRControllerState_t state;
        if (m_pHMD->GetControllerState(unDevice, &state))
        {
            // handle controller updates here
        }
    }

    return bRet;
}

// ----------------------------------------------------------------
//  renders the frame
// ----------------------------------------------------------------
void OgreOpenVR::update()
{
    // update the parent camera node's orientation and position with the new tracking data
    Ogre::Quaternion q = m_orientation;
    Ogre::Quaternion q_hack; q_hack.FromAngleAxis(Ogre::Radian(Ogre::Degree(180)), Ogre::Vector3::UNIT_Y);

    if (!lockToCamera && m_waist != NULL)
    {
        q = q_hack * m_mat4HMDPose.extractQuaternion() * m_waist->getOrientation();
    }
    else if (!lockToCamera)
    {
        q = q * m_mat4HMDPose.extractQuaternion();
    }

    m_cameraNode->setOrientation(q);
        
    //m_position = m_mat4HMDPose.getTrans();
    m_cameraNode->setPosition(m_position); // -m_poseNeutralPosition);

    Ogre::Root::getSingleton().renderOneFrame();

    Ogre::GLTexture* gt = ((Ogre::GLTexture*)m_ogreRenderTexture.get());
    GLuint srcid = gt->getGLID();

    // In OpenGL, the render texture comes out topside-bottomwards.
    //                                       u1    v1    u2    v2
    const vr::VRTextureBounds_t lbounds = { 0.0f, 1.0f, 0.5f, 0.0f };
    const vr::VRTextureBounds_t rbounds = { 0.5f, 1.0f, 1.0f, 0.0f };

    vr::Texture_t stereoTexture = { (void*)srcid, vr::API_OpenGL, vr::ColorSpace_Gamma };

    vr::VRCompositor()->Submit(vr::Eye_Left, &stereoTexture, &lbounds);
    vr::VRCompositor()->Submit(vr::Eye_Right, &stereoTexture, &rbounds);

    glFlush();
    glFinish();

    m_frameIndex++;

    // update the tracked device positions
    updateHMDPos();
}

// ----------------------------------------------------------------
// converts an OpenVR 3x4 matrix to Ogre::Matrix4
// ----------------------------------------------------------------
Ogre::Matrix4 OgreOpenVR::convertSteamVRMatrixToOgreMatrix4(const vr::HmdMatrix34_t &matPose)
{
    return Ogre::Matrix4(
        matPose.m[0][0], matPose.m[0][1], matPose.m[0][2], matPose.m[0][3],
        matPose.m[1][0], matPose.m[1][1], matPose.m[1][2], matPose.m[1][3],
        matPose.m[2][0], matPose.m[2][1], matPose.m[2][2], matPose.m[2][3],
                    0.0f,            0.0f,            0.0f,            1.0f
    );
}

// ----------------------------------------------------------------
// returns a string describing a tracked device
// ----------------------------------------------------------------
std::string OgreOpenVR::getTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError)
{
    uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
    if (unRequiredBufferLen == 0)
        return "";

    char *pchBuffer = new char[unRequiredBufferLen];
    unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
    std::string sResult = pchBuffer;
    delete[] pchBuffer;
    return sResult;
}

void OgreOpenVR::resetOrientation()
{
    m_pHMD->ResetSeatedZeroPose();
    m_poseNeutralPosition = m_position;
    m_poseNeutralOrientation = m_orientation;
}

void OgreOpenVR::SetPosition(Ogre::Vector3 pos)
{
    m_position = pos;
}

Ogre::Vector3 OgreOpenVR::GetPosition()
{
    return m_position;
}

void OgreOpenVR::SetOrientation(Ogre::Quaternion rot)
{
    m_orientation = rot;
}

Ogre::Quaternion OgreOpenVR::GetOrientation()
{
    return m_orientation;
}

Ogre::Matrix4 OgreOpenVR::getHMDMat4()
{
    return m_mat4HMDPose;
}

