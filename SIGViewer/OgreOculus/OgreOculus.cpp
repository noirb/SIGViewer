#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include "OgreOculus.h"
#include "OgreSceneManager.h"
#include "OgreRenderWindow.h"
#include <OgreSharedPtr.h>
#include <OgreManualObject.h>
#include <OgreRoot.h>


#include <math.h>
#include <vector>

namespace
{
    const float g_defaultNearClip = 0.01f;
    const float g_defaultFarClip = 100000.0f;
    const float g_defaultIPD = 0.064f;
    const Ogre::ColourValue g_defaultViewportColour(97/255.0f, 97/255.0f, 200/255.0f);
}


Oculus::Oculus(void):m_window(0), m_sceneManager(0), m_cameraNode(0)
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
    ovr_Shutdown();
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
    mSceneMgr = root->createSceneManager(Ogre::ST_GENERIC);
    mSceneMgr->setAmbientLight( Ogre::ColourValue( 0.5, 0.5, 0.5 ) );
    mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);


    Ogre::SceneNode* meshNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();

    m_cameras[0] = mSceneMgr->createCamera("Lcam");
    m_cameras[0]->setAutoAspectRatio(true);
    m_cameras[1] = mSceneMgr->createCamera("Rcam");
    m_cameras[1]->setAutoAspectRatio(true);

    texSizeL = ovr_GetFovTextureSize(mSession, ovrEye_Left, mHmdDesc.MaxEyeFov[0], 1.0f);
    texSizeR = ovr_GetFovTextureSize(mSession, ovrEye_Right, mHmdDesc.MaxEyeFov[1], 1.0f);
    bufferSize.w = texSizeL.w + texSizeR.w;
    bufferSize.h = std::max(texSizeL.h, texSizeR.h);

    ovrResult res = ovr_CreateSwapTextureSetGL(mSession, 0x8C43, bufferSize.w, bufferSize.h, &mTextureSet);
    if (OVR_FAILURE(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Creation of Oculus SwapTextureSet failed.");
        return;
    }

    //Ogre::GLTextureManager* textureManager(static_cast<Ogre::GLTextureManager*>(Ogre::GLTextureManager::getSingletonPtr()));
    Ogre::TexturePtr rtt_texture(Ogre::TextureManager::getSingletonPtr()->createManual("RttTex", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D, bufferSize.w, bufferSize.h, 0, Ogre::PF_R8G8B8, Ogre::TU_RENDERTARGET));
    Ogre::RenderTexture* rttEyes = rtt_texture->getBuffer(0, 0)->getRenderTarget();

    //Ogre::GLTexture* gltex = static_cast<Ogre::GLTexture*>(textureManager->getByName("RttTex").getPointer());
    //mRenderTextureID = gltex->getGLID();

    m_viewports[0] = rttEyes->addViewport(m_cameras[0], 0, 0,    0, 0.5f);
    m_viewports[1] = rttEyes->addViewport(m_cameras[1], 1, 0.5f, 0, 0.5f);

    eyeRenderDesc[0] = ovr_GetRenderDesc(mSession, ovrEye_Left, mHmdDesc.MaxEyeFov[0]);
    eyeRenderDesc[1] = ovr_GetRenderDesc(mSession, ovrEye_Right, mHmdDesc.MaxEyeFov[1]);
    offset[0] = eyeRenderDesc[0].HmdToEyeViewOffset;
    offset[1] = eyeRenderDesc[1].HmdToEyeViewOffset;

    mLayer.Header.Type     = ovrLayerType_EyeFov;
    mLayer.Header.Flags    = 0;
    mLayer.ColorTexture[0] = mTextureSet;
    mLayer.ColorTexture[1] = mTextureSet;
    mLayer.Fov[0]          = eyeRenderDesc[0].Fov;
    mLayer.Fov[1]          = eyeRenderDesc[1].Fov;
    mLayer.Viewport[0].Pos.x  = 0;
    mLayer.Viewport[0].Pos.y  = 0;
    mLayer.Viewport[0].Size.w = bufferSize.w / 2;
    mLayer.Viewport[0].Size.h = bufferSize.h;
    mLayer.Viewport[1].Pos.x  = bufferSize.w / 2;
    mLayer.Viewport[1].Pos.y  = 0;
    mLayer.Viewport[1].Size.w = bufferSize.w / 2;
    mLayer.Viewport[1].Size.h = bufferSize.h;

    // Create a camera in the (new, external) scene so the mesh can be rendered onto it:
    mCamera = mSceneMgr->createCamera("OculusRiftExternalCamera");
    mCamera->setFarClipDistance(1000000.0f);
    mCamera->setNearClipDistance( 0.1f );
    mCamera->setProjectionType( Ogre::PT_ORTHOGRAPHIC );
    mCamera->setOrthoWindow(2 , 2);
    mCamera->lookAt( 0, 0, -1 );


    mSceneMgr->getRootSceneNode()->attachObject( mCamera );

    meshNode->setPosition( 0, 0, -1 );
    meshNode->setScale( 1, 1, -0.1f );

    mViewport = m_window->addViewport( mCamera);
    mViewport->setBackgroundColour(Ogre::ColourValue::Black);
    mViewport->setOverlaysEnabled(true);
}


void Oculus::setCameras()
{
    Ogre::Camera* camLeft = m_cameras[0] ;
    Ogre::Camera* camRight = m_cameras[1] ; 
    Ogre::RenderTexture* renderTexLeft = mLeftEyeRenderTexture->getBuffer()->getRenderTarget();
    std::cout << "[Rift] Adding viewport to left texture" << std::endl;
    m_viewports[0] = renderTexLeft->addViewport(m_cameras[0]);
    renderTexLeft->getViewport(0)->setOverlaysEnabled(true);

    std::cout << "[Rift] Adding viewport to right texture" << std::endl;
    Ogre::RenderTexture* renderTexRight = mRightEyeRenderTexture->getBuffer()->getRenderTarget();
    m_viewports[1] = renderTexRight->addViewport(m_cameras[1]);

    renderTexRight->getViewport(0)->setOverlaysEnabled(true);

    m_viewports[0]->setBackgroundColour(g_defaultViewportColour);
    m_viewports[1]->setBackgroundColour(g_defaultViewportColour);


    ovrFovPort fovLeft = eyeRenderDesc[0].Fov;
    ovrFovPort fovRight = eyeRenderDesc[1].Fov;

    float combinedTanHalfFovHorizontal = std::max( fovLeft.LeftTan, fovLeft.RightTan );
    float combinedTanHalfFovVertical = std::max( fovLeft.UpTan, fovLeft.DownTan );

    float aspectRatio = combinedTanHalfFovHorizontal / combinedTanHalfFovVertical;

    m_cameras[0]->setAspectRatio( aspectRatio );
    m_cameras[1]->setAspectRatio( aspectRatio );

    // Get projection matrices for each eye
    for (size_t eyeIndex(0); eyeIndex < ovrEye_Count; eyeIndex++)
    {
        // get projection matrix for each eye from oculus
        ovrMatrix4f proj = ovrMatrix4f_Projection(eyeRenderDesc[eyeIndex].Fov,
            g_defaultNearClip,
            g_defaultFarClip,
            true);

        // convert to Ogre matrix
        Ogre::Matrix4 OgreProj;
        for (size_t x = 0; x < 4; x++)
        {
            for (size_t y = 0; y < 4; y++)
            {
                OgreProj[x][y] = proj.M[x][y];
            }
        }
        // set projection on each corresponding camera
        m_cameras[eyeIndex]->setCustomProjectionMatrix(true, OgreProj);
    }
}



bool Oculus::InitOculusVR()
{
    ovrResult res = ovr_Initialize(NULL);
    if (OVR_FAILURE(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Initalization for LibOVR is failed.");
        return false;
    }

    res = ovr_Create(&mSession, &mLuid);
    if (OVR_FAILURE(res))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Creation of OVR session failed.");
        return false;
    }

    mHmdDesc = ovr_GetHmdDesc(mSession);

    return true;
}


bool Oculus::setupOgre(Ogre::SceneManager *sm, Ogre::RenderWindow *win,Ogre::Root *mRoot, Ogre::SceneNode *parent)
{
    InitOculusVR();

    m_window = win;
    m_sceneManager = sm;

    setupOgreOculus( m_sceneManager, m_window,mRoot);

    if(parent)
    {
        m_cameraNode = parent->createChildSceneNode("CameraNode");
    }
    else
    {
        m_cameraNode = sm->getRootSceneNode()->createChildSceneNode("CameraNode");
    }
    m_cameras[0] = sm->createCamera("Camera1");
    m_cameras[1] = sm->createCamera("Camera2");


    for(int i=0;i<2;++i)
    {
        m_cameraNode->attachObject(m_cameras[i]);
        m_cameras[i]->setNearClipDistance(g_defaultNearClip);
        m_cameras[i]->setFarClipDistance(g_defaultFarClip);			
        m_cameras[i]->setPosition((i * 2 - 1) * mIPD * 0.5f, 0, 0);
    }


    setCameras( );

    Ogre::LogManager::getSingleton().logMessage("Oculus: Oculus setup completed successfully");
    return true;
}



Ogre::SceneNode *Oculus::getCameraNode()
{
	return m_cameraNode;
}


