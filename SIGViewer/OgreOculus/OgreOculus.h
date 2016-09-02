#pragma once
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

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
#define GL_GLEXT_PROTOTYPES 1
#include <Ogre.h>
#include <OgreRectangle2D.h>

#include <RenderSystems\GL\OgreGLTextureManager.h>
#include <RenderSystems\GL\OgreGLRenderSystem.h>
#include <RenderSystems\GL\OgreGLTexture.h>

namespace Ogre
{
    class SceneManager;
    class RenderWindow;
    class Camera;
    class SceneNode;
    class Viewport;
}


class Oculus
{
public:
    Oculus(void);
    ~Oculus(void);
    bool setupOgre(Ogre::SceneManager *sm, Ogre::RenderWindow *win, Ogre::Root *mRoot, Ogre::SceneNode *parent = 0);
    void shutDownOgre();
    bool InitOculusVR();
    bool InitOculusTextures();
    bool InitOgreTextures();
	bool InitOgreViewports();
    bool InitOculusLayers();
    void setupOgreOculus( Ogre::SceneManager *sm, Ogre::RenderWindow* win,Ogre::Root* root);
    double updateHMDState();
    void Update();

    Ogre::Vector3 GetPosition()
    {
        return m_headPosition;
    }

    void SetPosition(Ogre::Vector3 newPos)
    {
        m_headPosition = newPos;
    }

    Ogre::Quaternion GetOrientation()
    {
        return m_headOrientation;
    }

    void SetOrientation(Ogre::Quaternion newOrientation)
    {
        m_headOrientation = newOrientation;
    }

    /// Reset orientation of the sensor.
    void resetOrientation();
    /// Retrieve the SceneNode that contains the two cameras used for stereo rendering.
    Ogre::SceneNode *getCameraNode();

    /// Retrieve the current orientation of the Oculus HMD.
    //Ogre::Quaternion getOrientation() const;


    /// Retrieve either of the two cameras.
    Ogre::Camera *getCamera(unsigned int i);

    /// Retrieve either of the two viewports.
    Ogre::Viewport *getViewport(unsigned int i);


    void setTexture( std::string tex );

    Ogre::Viewport *m_viewports[2];
    ovrVector2f mUVScaleOffset[2][2];

    Ogre::Camera* mCamera;
    Ogre::Viewport* mViewport;

    // Ogre stuff needed for VR
    Ogre::Camera    *m_cameras[2];
    Ogre::TexturePtr mOgreRenderTexture;

    // Oculus stuff
    ovrSession          mSession;
    ovrGraphicsLuid     mLuid;
    ovrHmdDesc          mHmdDesc;
    ovrSizei            mIdealTextureSize;
    ovrTextureSwapChain mTextureSwapChain;
    ovrEyeRenderDesc    mEyeRenderDesc[2]; // two eyes
    ovrViewScaleDesc    mViewScaleDesc;
    ovrLayerEyeFov      mLayer;
    ovrPosef            mEyeRenderPose[2];
    ovrTrackingState    mHMDState;
    long long           mFrameIndex = 0;
    GLuint              readFBO     = 0;
    GLuint              writeFBO    = 0;

    bool                lockToCamera = true; // if true, head orientation is fixed to camera orientation, not HMD sensor orientation

protected:
    Ogre::SceneManager *m_sceneManager;
    Ogre::RenderWindow *m_window;
    Ogre::SceneNode    *m_cameraNode;
    Ogre::Vector3       m_headPosition;
    Ogre::Quaternion    m_headOrientation;

    Ogre::Vector3 convertVector3(const ovrVector3f &v)
    {
        return Ogre::Vector3(v.x, v.y, v.z);
    }

    ovrVector3f convertVector3(const Ogre::Vector3 &v)
    {
        ovrVector3f ov;
        ov.x = v.x;
        ov.y = v.y;
        ov.z = v.z;
        return ov;
    }

    Ogre::Quaternion convertQuaternion(const ovrQuatf &q)
    {
        return Ogre::Quaternion(q.w, q.x, q.y, q.z);
    }

    ovrQuatf convertQuaternion(const Ogre::Quaternion &q)
    {
        ovrQuatf oq;
        oq.w = q.w;
        oq.x = q.x;
        oq.y = q.y;
        oq.z = q.z;
        return oq;
    }
};
