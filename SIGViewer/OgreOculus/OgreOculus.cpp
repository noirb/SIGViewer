#include "OgreOculus.h"
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <RenderSystems\GL\OgreGLRenderTexture.h>
#include <RenderSystems\GL\OgreGLFBORenderTexture.h>
#include <RenderSystems\GL\OgreGLFrameBufferObject.h>
#include "OgreSceneManager.h"
#include "OgreRenderWindow.h"
#include <OgreViewport.h>
#include <OgreSharedPtr.h>
#include <OgreManualObject.h>
#include <OgreRoot.h>

#include <math.h>
#include <vector>

#include "..\WindowUtils.h"

/*
	Oculus Setup & Update code based on Kojack's work from here: http://www.ogre3d.org/forums/viewtopic.php?f=5&t=81627&start=100#p525592
*/

namespace
{
    const float g_defaultNearClip = 0.01f;
    const float g_defaultFarClip = 100000.0f;
    const float g_defaultIPD = 0.064f;
    const Ogre::ColourValue g_defaultViewportColour(97/255.0f, 200/255.0f, 200/255.0f);
}


Oculus::Oculus(void):m_window(0), m_sceneManager(0), m_cameraNode(0), m_waist(0), m_neck(0)
{
    for(int i=0;i<2;++i)
    {
        m_cameras[i] = 0;
        m_viewports[i] = 0;
    }
}

Oculus::~Oculus(void)
{
    shutDownOgre();
    //ovr_Shutdown();
}


void Oculus::shutDownOgre()
{
    for(int i=0;i<2;++i)
    {
        if(m_viewports[i])
        {
            m_window->removeViewport(i);
            m_viewports[i] = 0;
        }
        if(m_cameras[i])
        {
            m_cameras[i]->getParentSceneNode()->detachObject(m_cameras[i]);
            m_sceneManager->destroyCamera(m_cameras[i]);
            m_cameras[i] = 0;
        }
    }
    if(m_cameraNode)
    {
        m_cameraNode->getParentSceneNode()->removeChild(m_cameraNode);
        m_sceneManager->destroySceneNode(m_cameraNode);
        m_cameraNode = 0;
    }
    m_window = 0;
    m_sceneManager = 0;
}

void Oculus::setupOgreOculus( Ogre::SceneManager *sm, Ogre::RenderWindow* win, Ogre::Root* root )
{
    m_sceneManager->setAmbientLight( Ogre::ColourValue( 0.5, 0.5, 0.5 ) );
    m_sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);

    m_cameras[0] = m_sceneManager->createCamera("Oculus Left Eye");
    m_cameras[1] = m_sceneManager->createCamera("Oculus Right Eye");
    
    m_cameraNode->setOrientation(Ogre::Quaternion::IDENTITY);
    m_cameraNode->setPosition(0, 0, 0);

    Ogre::Matrix4 proj;
    for (size_t i = 0; i < 2; i++)
    {
        const Ogre::Vector3 camPos(g_defaultIPD * ((float)i - 0.5f), 0, 0);
        m_cameras[i]->setPosition(camPos);
        m_cameras[i]->setNearClipDistance(g_defaultNearClip);
        m_cameras[i]->setFarClipDistance(g_defaultFarClip);
        m_cameras[i]->setAutoAspectRatio(true);
        m_cameras[i]->detachFromParent();
        m_cameraNode->attachObject(m_cameras[i]);
        m_cameras[i]->setDirection(0, 0, -1);
        
        ovrMatrix4f ovrm = ovrMatrix4f_Projection(
                            mHmdDesc.DefaultEyeFov[i],
                            g_defaultNearClip,
                            g_defaultFarClip,
                            ovrProjection_None
                          );
        for (size_t y = 0; y < 4; y++)
        {
            for (size_t x = 0; x < 4; x++)
            {
                proj[y][x] = ovrm.M[y][x];
            }
        }
        m_cameras[i]->setCustomProjectionMatrix(true, proj);
    }

    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOculus: Setup oculus cameras complete");
}


bool Oculus::InitOculusVR()
{
    ovrInitParams params;
    params.Flags = ovrInit_Debug;
    params.ConnectionTimeoutMS = 0;
    params.RequestedMinorVersion = 6;
    params.UserData = 0;
    params.LogCallback = 0;
    ovrResult res = ovr_Initialize(&params);
    if (!OVR_SUCCESS(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Initalization for LibOVR is failed.");
        return false;
    }

    res = ovr_Create(&mSession, &mLuid);
    if (!OVR_SUCCESS(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Creation of OVR session failed.");
        return false;
    }

    mHmdDesc = ovr_GetHmdDesc(mSession);

    ovrSizei recommendedTex0Size = ovr_GetFovTextureSize(mSession, ovrEye_Left, mHmdDesc.DefaultEyeFov[0], 1.0f);
    ovrSizei recommendedTex1Size = ovr_GetFovTextureSize(mSession, ovrEye_Right, mHmdDesc.DefaultEyeFov[1], 1.0f);
    mIdealTextureSize.w = recommendedTex0Size.w + recommendedTex0Size.w;
    mIdealTextureSize.h = std::max(recommendedTex0Size.h, recommendedTex1Size.h);

    mEyeRenderDesc[0] = ovr_GetRenderDesc(mSession, ovrEye_Left, mHmdDesc.DefaultEyeFov[0]);
    mEyeRenderDesc[1] = ovr_GetRenderDesc(mSession, ovrEye_Right, mHmdDesc.DefaultEyeFov[1]);

    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOculus: OVR Session Initialization complete");
    return true;
}

bool Oculus::InitOculusTextures()
{
    ovrResult res;

    ovrTextureSwapChainDesc desc = {};
    desc.Type = ovrTexture_2D;
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.ArraySize = 1;
    desc.Width = mIdealTextureSize.w;
    desc.Height = mIdealTextureSize.h;
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.StaticImage = ovrFalse;

    res = ovr_CreateTextureSwapChainGL(mSession, &desc, &mTextureSwapChain);

    if (!OVR_SUCCESS(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Creation of OVR texture swapchain failed.");
        return false;
    }

	Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOculus: Created OVR texture swapchain");
    return true;
}

bool Oculus::InitOgreTextures()
{
    glGenFramebuffers(1, &readFBO);
    glGenFramebuffers(1, &writeFBO);

    if (readFBO < 1 || writeFBO < 1)
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to generate GL Framebuffers for OVR texture swap!");
        return false;
    }

    mOgreRenderTexture = Ogre::TextureManager::getSingleton().createManual(
                                "Oculus Eye Texture",
                                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                                Ogre::TextureType::TEX_TYPE_2D,
                                mIdealTextureSize.w, mIdealTextureSize.h,
                                0,
                                Ogre::PixelFormat::PF_R8G8B8A8,
                                Ogre::TextureUsage::TU_RENDERTARGET
                            );
    return true;
}

bool Oculus::InitOculusLayers()
{
    mLayer.Header.Type = ovrLayerType_EyeFov;
    mLayer.Header.Flags = 0;
    for (size_t eye = 0; eye < 2; ++eye)
    {
        mLayer.ColorTexture[eye] = mTextureSwapChain;
        mLayer.Viewport[eye].Pos.x = (mIdealTextureSize.w / 2)*eye;
        mLayer.Viewport[eye].Pos.y = 0;
        mLayer.Viewport[eye].Size.w = mIdealTextureSize.w / 2;
        mLayer.Viewport[eye].Size.h = mIdealTextureSize.h;
        mLayer.Fov[eye] = mHmdDesc.DefaultEyeFov[eye];
    }

	Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOculus: Oculus layers setup complete");
    return true;
}

bool Oculus::InitOgreViewports()
{
    Ogre::RenderTexture* renderTex = mOgreRenderTexture->getBuffer()->getRenderTarget();

    // configure the two viewports to each render to half of the render target
    // each half corresponds to one eye, and we submit both in one big texture to OVR
    m_viewports[0] = renderTex->addViewport(m_cameras[0], 0, 0.0f, 0.0f, 0.5f, 1.0f);
    m_viewports[1] = renderTex->addViewport(m_cameras[1], 1, 0.5f, 0.0f, 0.5f, 1.0f);

    renderTex->setAutoUpdated(true);
    
    m_viewports[0]->setBackgroundColour(g_defaultViewportColour);
    m_viewports[0]->setOverlaysEnabled(true);
    m_viewports[0]->setAutoUpdated(true);
    m_viewports[1]->setBackgroundColour(g_defaultViewportColour);
    m_viewports[1]->setOverlaysEnabled(true);
    m_viewports[1]->setAutoUpdated(true);

    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOculus: Setup RTT viewports for ogre oculus textures");
    return true;
}

bool Oculus::setupOgre(Ogre::SceneManager *sm, Ogre::RenderWindow *win,Ogre::Root *mRoot, Ogre::SceneNode *parent)
{
    glewInit();
    m_window = win;
    m_sceneManager = sm;

    if (parent)
    {
        m_cameraNode = parent->createChildSceneNode("CameraNode");
    }
    else
    {
        m_cameraNode = sm->getRootSceneNode()->createChildSceneNode("CameraNode");
    }

    if (!InitOculusVR())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to intialize HMD!");
        return false;
    }

    if (!InitOculusTextures())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to intialize Oculus Textures!");
        return false;
    }

    if (!InitOgreTextures())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to intialize Ogre Textures!");
        return false;
    }

    if (!InitOculusLayers())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to intialize Oculus Layers!");
        return false;
    }

    setupOgreOculus( m_sceneManager, m_window,mRoot );

    if (!InitOgreViewports())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to initialize RTT viewports for ogre oculus textures");
    }


    Ogre::LogManager::getSingleton().logMessage("OgreOculus: Oculus setup completed successfully");
    return true;
}

void Oculus::resetOrientation()
{
    ovrResult res = ovr_RecenterTrackingOrigin(mSession);
    if (!OVR_SUCCESS(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "ERROR: Could not recenter HMD Pose!");
    }
    else
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "Reset Oculus Orientation");
        HWND oculus_svc = WindowHacks::FindWindowByName("\\OculusRiftCV1_vs2015.sig");
        if (oculus_svc != NULL)
        {
            if (WindowHacks::SendKeyStroke(oculus_svc, 'r'))
            {
                Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "Sent reset key to Oculus Service window!");
            }
            else
            {
                Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "Failed to send reset key to oculus service window :(");
            }
        }
        else
        {
            Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "Could not find Oculus Service window. Not sending reset key command.");
        }
    }

}

Ogre::SceneNode *Oculus::getCameraNode()
{
	return m_cameraNode;
}

double Oculus::updateHMDState()
{
    double ftiming = ovr_GetPredictedDisplayTime(mSession, mFrameIndex);

    double sensorSampleTime = ovr_GetTimeInSeconds();
    
    // update eye states
    mEyeRenderDesc[0] = ovr_GetRenderDesc(mSession, ovrEye_Left, mHmdDesc.DefaultEyeFov[0]);
    mEyeRenderDesc[1] = ovr_GetRenderDesc(mSession, ovrEye_Right, mHmdDesc.DefaultEyeFov[1]);

    mHMDState = ovr_GetTrackingState(mSession, ftiming, ovrTrue);
    ovrVector3f viewOffset[2] = { mEyeRenderDesc[0].HmdToEyeOffset, mEyeRenderDesc[1].HmdToEyeOffset };
    ovr_CalcEyePoses(mHMDState.HeadPose.ThePose, viewOffset, mEyeRenderPose);

    return sensorSampleTime;
}

void Oculus::Update()
{
    double sensorSampleTime = updateHMDState();

    ovrSessionStatus sessionStatus;
    ovrResult res = ovr_GetSessionStatus(mSession, &sessionStatus);
    if (!OVR_SUCCESS(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Could not retrieve OVR Session Status!");
    }

    if (sessionStatus.ShouldRecenter)
    {
        resetOrientation();
    }

    Ogre::Quaternion q = m_headOrientation;

    // include HMD orientation in new perspective if we're not locked to camera
    if (!lockToCamera && m_waist != NULL && m_neck != NULL)
    {
        Ogre::Quaternion q_hack; q_hack.FromAngleAxis(Ogre::Radian::Radian(Ogre::Degree(180)), Ogre::Vector3::UNIT_Y); // waist is backwards?
        q =  q_hack * convertQuaternion(mHMDState.HeadPose.ThePose.Orientation) * m_waist->getOrientation();
    }
    else if (!lockToCamera)
    {
        q = q * convertQuaternion(mHMDState.HeadPose.ThePose.Orientation);
    }

    m_cameraNode->setOrientation(q);
    m_cameraNode->setPosition(m_headPosition);
    m_cameraNode->setPosition(convertVector3(mHMDState.HeadPose.ThePose.Position) + m_headPosition);

    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, "OgreOculus: HeadPos:  " + std::to_string(m_cameraNode->_getDerivedPosition().x) + "," + std::to_string(m_cameraNode->_getDerivedPosition().y) + "," + std::to_string(m_cameraNode->_getDerivedPosition().z));

    for (size_t i = 0; i < 2; ++i)
    {
        mLayer.RenderPose[i] = mEyeRenderPose[i];
        mLayer.SensorSampleTime = sensorSampleTime;
    }

    // RENDER!
    Ogre::Root::getSingleton().renderOneFrame();

    int index = 0;
    ovr_GetTextureSwapChainCurrentIndex(mSession, mTextureSwapChain, &index);

    Ogre::GLTexture* gt = ((Ogre::GLTexture*)mOgreRenderTexture.get());
    GLuint srcid = gt->getGLID();

    unsigned int dstid;// = ((ovrGLTexture *)g_textureSet[0]->Textures)[g_textureSet[0]->CurrentIndex].OGL.TexId;
    ovr_GetTextureSwapChainBufferGL(mSession, mTextureSwapChain, index, &dstid);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcid, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstid, 0);
    glBlitFramebuffer(0, 0, mIdealTextureSize.w - 1, mIdealTextureSize.h - 1, 0, 0, mIdealTextureSize.w - 1, mIdealTextureSize.h - 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    ovr_CommitTextureSwapChain(mSession, mTextureSwapChain);

    ovrLayerHeader* layers = &mLayer.Header;
    ovrResult result = ovr_SubmitFrame(mSession, mFrameIndex, 0, &layers, 1);

    if (!OVR_SUCCESS(result))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "OgreOculus: Failed to submit frame " + std::to_string(mFrameIndex));
    }

    mFrameIndex++;
}
