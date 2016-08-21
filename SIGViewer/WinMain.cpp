#include "WinMain.h"
#include "SgvLog.h"
#include "SgvEntity.h"
#include "SgvSocket.h"
#include "ViewerService.h"
#include "RenderService.h"
#include "SIGService.h"
#include "binary.h"
#include "SIGVerse.h"

#include <commdlg.h>
#include <tchar.h>
#include <map>
#include <list>
#include <shlwapi.h>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <libssh2.h>
#include <ws2tcpip.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define LIBSSH2_INIT_NO_CRYPTO 0x0001
#pragma comment( lib, "libssh2.lib")

#pragma comment( lib, "shlwapi.lib")

char *getFileName(char *lpszPath)
{
    char *lpszPtr=lpszPath;
    while (*lpszPtr != '\0')
    {
        if (IsDBCSLeadByte(*lpszPtr) == 0)
        {
            if ((*lpszPtr == '\\') || (*lpszPtr == '/') || (*lpszPtr == ':'))
            {
                lpszPath=lpszPtr+1;
            }
        }
        lpszPtr=CharNext(lpszPtr);
    }
    return lpszPath;
}

enum {
    AUTH_NONE = 0,
    AUTH_PASSWORD,
    AUTH_PUBLICKEY
};

static const double R_DX = 0.92;
static const double R_DY = 0.90;

static const int MAX_SUBVIEW = 4;

static const int MAX_SERVICE_SIZE = 9;

static const int MAX_CAMERA_LIST = 20 + 1; // 20 Cameras and PlayerCam

static const int PRIVATE_PROFILE_STRING_SIZE = 1024;
static const int PRIVATE_PROFILE_SECTION_LEN = 1024;

//-------------------------------------------------------------------------------------
SgvMain::SgvMain(void)
: mRenderer(0), 
mConnectServer(false), 
mSimRun(false),
mSock(NULL),
mSubView(true),
mService(NULL),
mSended(false),
m_pX3D(NULL),
m_sshSock(-1),
m_session(NULL),
m_dynamicsView(0),
mOverWrite(false),
mEntityData(NULL),
mAlgEntityPos(2)
{

}
//-------------------------------------------------------------------------------------
SgvMain::~SgvMain(void)
{
	if (mSetupSuccessful)
	{
		CEGUI::OgreRenderer::destroySystem();
		mLog->flush();
	}

    if (mSock != NULL)    delete mSock;
    if (mService != NULL) delete mService;
   
    if (m_pX3D != NULL) {
        delete m_pX3D;
        m_pX3D = NULL;
    }
}

void SgvMain::destroyScene()
{
    BaseApplication::destroyScene();
}
 
//-------------------------------------------------------------------------------------
void SgvMain::createScene(void)
{
    char dir[MAX_STRING_NUM];
    GetCurrentDirectory(MAX_STRING_NUM, dir);
    std::string inipath = std::string(dir) + "/SIGVerse.ini";

    sprintf_s(mSettingPath, MAX_STRING_NUM, inipath.c_str());

    Sgv::LogFactory::setLog(0, Sgv::LogPtr(new Sgv::Log(Sgv::Log::INFO, "main", 
        Sgv::LogFileWriterAutoPtr(new Sgv::DelayFileWriter<Sgv::FileLock>("SIGVerse.log")))) );
    mLog = Sgv::LogFactory::getLog(0);

    TCHAR pathText[MAX_STRING_NUM];

    GetPrivateProfileString("SHAPEFILE","OVERWRITE",'\0', pathText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (strcmp(pathText,"true") == 0)  mOverWrite = true;

    GetPrivateProfileString("ALGORITHM","EntityPosition",'\0', pathText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if      (strcmp(pathText,"1") == 0)  mAlgEntityPos = 1;
    else if (strcmp(pathText,"2") == 0)  mAlgEntityPos = 2;

    mRenderer = &CEGUI::OgreRenderer::bootstrapSystem();

    CEGUI::ImageManager::setImagesetDefaultResourceGroup("Imagesets");
    CEGUI::Font::setDefaultResourceGroup("Fonts");
    CEGUI::Scheme::setDefaultResourceGroup("Schemes");
    CEGUI::WidgetLookManager::setDefaultResourceGroup("LookNFeel");
    CEGUI::WindowManager::setDefaultResourceGroup("Layouts");
    CEGUI::SchemeManager::getSingleton().createFromFile("TaharezLook.scheme");
    CEGUI::SchemeManager::getSingleton().createFromFile("OgreTray.scheme");
    CEGUI::SchemeManager::getSingleton().createFromFile("VanillaSkin.scheme");
    CEGUI::SchemeManager::getSingleton().createFromFile("WindowsLook.scheme");

    CEGUI::System::getSingleton().getDefaultGUIContext().getMouseCursor().setDefaultImage("WindowsLook/MouseArrow");

    if (oculusMode){ mSubView = false; }

    createInitWindow();

    mPlane = mSceneMgr->createManualObject("GroundPlane");

    Ogre::MovablePlane *plane = new Ogre::MovablePlane("myplane");
    plane->normal = Ogre::Vector3::UNIT_Y;
    Ogre::MeshManager::getSingleton().createPlane("MyPlane",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        *plane,
        3000, 3000, 50, 50, true, 1, 5, 5, Ogre::Vector3::UNIT_Z);

    GetPrivateProfileString("PLANE", "PLANE", '\0', pathText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (0 < strlen(pathText)) {
        Ogre::Entity* entGround = mSceneMgr->createEntity("GroundEntity", "MyPlane");
        mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(entGround);
        // (Ex.)"Examples/Rockwall"
        entGround->setMaterialName(pathText);
        entGround->setCastShadows(false);
  
    } else {
        Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("MA_White");
        Ogre::MaterialPtr tmat = tmp->clone("Plane");
        tmat->setAmbient(1.0f, 1.0f, 1.0f);

        int planeSize = 1500;
        int planeStep = 10;

        mPlane->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_LIST);
        Ogre::ColourValue col(0.0f, 0.0f, 0.6f);

        for (int x=-planeSize; x<=planeSize; x+=planeStep) {
            mPlane->position(x, 0.0, -planeSize);
            mPlane->colour(col);

            mPlane->position(x, 0.0, planeSize);
            mPlane->colour(col);
        }

        for (int z=-planeSize; z<=planeSize; z+=planeStep) {
            mPlane->position(-planeSize, 0.0, z);
            mPlane->colour(col);

            mPlane->position(planeSize, 0.0, z);
            mPlane->colour(col);
        }
        mPlane->setQueryFlags(false);
        mPlane->end();
    }

    mPlane->setCastShadows(false);

    mAxis = mSceneMgr->createManualObject("axis");
    mAxis->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_LIST);
    mAxis->position(0.0f, 0.0f, 0.0f);

    mAxis->colour(Ogre::ColourValue(1.0f, 0.0f, 0.0f));
    mAxis->position(10.0f, 0.0f, 0.0f);

    mAxis->colour(Ogre::ColourValue(1.0f, 0.0f, 0.0f));
    mAxis->position(0.0f, 0.0f, 0.0f);

    mAxis->colour(Ogre::ColourValue(1.0f, 1.0f, 0.0f));
    mAxis->position(0.0f, 10.0f, 0.0f);

    mAxis->colour(Ogre::ColourValue(1.0f, 1.0f, 0.0f));
    mAxis->position(0.0f, 0.0f, 0.0f);

    mAxis->colour(Ogre::ColourValue(0.0f, 1.0f, 0.0f));
    mAxis->position(0.0f, 0.0f, 10.0f);

    mAxis->colour(Ogre::ColourValue(0.0f, 1.0f, 0.0f));
    mAxis->position(0.0f, 0.0f, 0.0f);

    mAxis->setQueryFlags(false);
    mAxis->end();

    mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(mPlane);
    mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(mAxis);

    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.8f, 0.8f, 0.8f));

    Ogre::Light *l = mSceneMgr->createLight("MainLight");
    l->setType( Ogre::Light::LT_DIRECTIONAL );
    l->setPosition(1000.0f, 1000.0f, -1000.0f);
    l->setDirection(Ogre::Vector3(0, -1, 0));
    l->setDiffuseColour(Ogre::ColourValue(0.8f, 0.8f, 0.8f));
    l->setSpecularColour(Ogre::ColourValue(0.8f, 0.8f, 0.8f));

    mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_NONE);

    GetPrivateProfileString("SKYPLANE", "SKYPLANE", '\0', pathText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (0 < strlen(pathText)) {
        Ogre::Plane sky_plane;
        sky_plane.d = 3000;
        sky_plane.normal = Ogre::Vector3::NEGATIVE_UNIT_Y; 
        // (Ex.) "Examples/CloudySky", "Examples/SpaceSkyPlane"
        mSceneMgr->setSkyPlane(true, sky_plane, pathText, 3000, 50, true, 1.5f, 150, 150);
    }

    mHeadNode = mSceneMgr->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(0, 0, 0));

    // Position it at 500 in Z direction
    //mCamera->setPosition(Ogre::Vector3(103.4f, 34.3f, 65.9f));
    // Look back along -Z
    //mCamera->lookAt(Ogre::Vector3(-0.5f, -0.2f, -0.8f));
    mCamera->setAutoAspectRatio(true);

    mCamera->setNearClipDistance(Ogre::Real(1.0f));
    mCamera->setFarClipDistance(Ogre::Real(3000.0f));

    mTex = Ogre::TextureManager::getSingleton().createManual(
        "RttTex",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
        320, 240, 0, Ogre::PF_R8G8B8, Ogre::TU_RENDERTARGET);

    mDstTex = Ogre::TextureManager::getSingleton().createManual(
        "DstTex",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
        320, 240, 0, Ogre::PF_B8G8R8, Ogre::TU_RENDERTARGET);

    Ogre::RenderTexture *renderTexture = mTex->getBuffer()->getRenderTarget();
    Ogre::RenderTexture *renderDstTexture = mDstTex->getBuffer()->getRenderTarget();

    mCaptureCamera = mSceneMgr->createCamera("CaptureCamera");
    mCaptureCamera->setNearClipDistance(Ogre::Real(1.0f));
    mCaptureCamera->setFarClipDistance(Ogre::Real(3000.0f));

    Ogre::Viewport *port = renderTexture->addViewport(mCaptureCamera);
    port->setBackgroundColour(mBackGroundColor);
    port->setOverlaysEnabled(false);
    renderTexture->setAutoUpdated(true);

    mDistanceCamera = mSceneMgr->createCamera("DistanceCamera");
    mDistanceCamera->setNearClipDistance(Ogre::Real(0.0001f)); 
    mDistanceCamera->setFarClipDistance(Ogre::Real(3000.0f));
    mDistanceCamera->setProjectionType(PT_PERSPECTIVE);

    mDistanceCamera->setFocalLength(1.0);

    //for detect Entities
    m_VolQuery = mSceneMgr->createPlaneBoundedVolumeQuery(Ogre::PlaneBoundedVolumeList());

    Ogre::Viewport *distport = renderDstTexture->addViewport(mDistanceCamera);
    distport->setBackgroundColour(mBackGroundColor);
    distport->setOverlaysEnabled(false);
    distport->setBackgroundColour(Ogre::ColourValue(1.0f, 1.0f, 1.0f));

    renderDstTexture->setAutoUpdated(true);

	Ogre::LogManager::getSingleton().logMessage("Base Scene Creation Complete!");
}

/*! 
 * @brief
 */
void SgvMain::createInitWindow()
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window *sheet = wmgr.createWindow("DefaultWindow", "MainSheet");

    CEGUI::Window *connect = wmgr.createWindow("OgreTray/FrameWindow", "Connect");
    connect->setText("Connect to Server");
    connect->setSize(CEGUI::USize(CEGUI::UDim(0.5f, 0.0f), CEGUI::UDim(0.5f, 0.0f)));
    connect->setPosition(CEGUI::UVector2(CEGUI::UDim(0.25f, 0.0f), CEGUI::UDim(0.25f, 0.0f)));

    CEGUI::Window *sname = wmgr.createWindow("OgreTray/Editbox", "ServiceName");
    sname->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    sname->setPosition(CEGUI::UVector2(CEGUI::UDim(0.55f, 0.0f), CEGUI::UDim(0.00f, 0.0f)));

    TCHAR strText[MAX_STRING_NUM];
    GetPrivateProfileString("LOGIN","SERVICE_NAME", '\0',strText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);

    if (strText[0] == '\0') sname->setText("SIGViewer");
    else sname->setText(strText);

    CEGUI::Window *serverip = wmgr.createWindow("OgreTray/Editbox", "ServerIP");
    serverip->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    serverip->setPosition(CEGUI::UVector2(CEGUI::UDim(0.55f, 0.0f), CEGUI::UDim(0.15f, 0.0f)));

    GetPrivateProfileString("LOGIN","SERVER_IP",'\0',strText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);

    if (strText[0] == '\0') serverip->setText("localhost");
    else serverip->setText(strText);

    CEGUI::Window *port = wmgr.createWindow("OgreTray/Editbox", "PortNumber");
    port->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    port->setPosition(CEGUI::UVector2(CEGUI::UDim(0.55f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));

    GetPrivateProfileString("LOGIN","SERVER_PORT", '\0',strText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (strText[0] == '\0') port->setText("9000");
    else port->setText(strText);

    CEGUI::ToggleButton *ssh_checkbox = static_cast<CEGUI::ToggleButton*>(wmgr.createWindow("OgreTray/Checkbox","ssh_check"));
    ssh_checkbox->setSize(CEGUI::USize(CEGUI::UDim(0.5f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    ssh_checkbox->setPosition(CEGUI::UVector2(CEGUI::UDim(0.45f, 0.0f), CEGUI::UDim(0.45f, 0.0f)));

    GetPrivateProfileString("LOGIN","SSH_CHECK", '\0',strText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (strcmp(strText, "true") == 0) ssh_checkbox->setSelected(true);

    CEGUI::Window *username = wmgr.createWindow("OgreTray/Editbox", "UserName");
    username->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
    username->setPosition(CEGUI::UVector2(CEGUI::UDim(0.55f, 0.0f), CEGUI::UDim(0.6f, 0.0f)));
    if (!ssh_checkbox->isSelected()) {
        username->disable();
        username->setAlpha(0.4f);
    }

    GetPrivateProfileString("LOGIN","SSH_USERNAME", '\0',strText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (strText[0] != '\0') username->setText(strText);

    CEGUI::Window *passwd = wmgr.createWindow("OgreTray/Editbox", "Password");
    passwd->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
    passwd->setPosition(CEGUI::UVector2(CEGUI::UDim(0.55f, 0.0f), CEGUI::UDim(0.725f, 0.0f)));
    passwd->setProperty("MaskText","True");
    if (!ssh_checkbox->isSelected()) {
        passwd->disable();
        passwd->setAlpha(0.4f);
    }
    CEGUI::Window *cb = wmgr.createWindow("OgreTray/Button", "ConnectToSimServer");
    cb->setSize(CEGUI::USize(CEGUI::UDim(0.5f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
    cb->setPosition(CEGUI::UVector2(CEGUI::UDim(0.45f, 0.0f), CEGUI::UDim(0.875f, 0.0f)));
    cb->setText("Connect");

    ////////////title////////////
    // name
    CEGUI::Window *tname = wmgr.createWindow("Vanilla/StaticText", "tname");
    tname->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    tname->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.00f, 0.0f)));
    tname->setText("Service name ");
    tname->setProperty("FrameEnabled", "false");
    tname->setProperty("BackgroundEnabled", "false");
    tname->setProperty("HorzFormatting","HorzCenterd");

    // ip
    CEGUI::Window *tip = wmgr.createWindow("Vanilla/StaticText", "tip");
    tip->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    tip->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.15f, 0.0f)));
    tip->setProperty("FrameEnabled", "false");
    tip->setProperty("BackgroundEnabled", "false");
    tip->setProperty("HorzFormatting","HorzCenterd");
    tip->setText("Host name (IP)");

    // centre
    CEGUI::Window *tport = wmgr.createWindow("Vanilla/StaticText", "tport");
    tport->setSize(CEGUI::USize(CEGUI::UDim(0.5f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    tport->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
    tport->setProperty("FrameEnabled", "false");
    tport->setProperty("BackgroundEnabled", "false");
    tport->setProperty("HorzFormatting","HorzCenterd");
    tport->setText("Port (or World) number ");

    CEGUI::Window *ch_ssh = wmgr.createWindow("Vanilla/StaticText", "sshcheck");
    ch_ssh->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
    ch_ssh->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.45f, 0.0f)));
    ch_ssh->setProperty("FrameEnabled", "false");
    ch_ssh->setProperty("BackgroundEnabled", "false");
    ch_ssh->setProperty("HorzFormatting","HorzCenterd");
    ch_ssh->setText("Connect by SSH ");

    // username
    CEGUI::Window *tusername = wmgr.createWindow("Vanilla/StaticText", "tusername");
    tusername->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
    tusername->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.6f, 0.0f)));
    tusername->setProperty("FrameEnabled", "false");
    tusername->setProperty("BackgroundEnabled", "false");
    tusername->setProperty("HorzFormatting","HorzCenterd");
    tusername->setText("User name ");
    if (!ssh_checkbox->isSelected()) {
        tusername->disable();
        tusername->setAlpha(0.4f);
    }
    // password
    CEGUI::Window *tpasswd = wmgr.createWindow("Vanilla/StaticText", "tpasswd");
    tpasswd->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
    tpasswd->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.725f, 0.0f)));
    tpasswd->setProperty("FrameEnabled", "false");
    tpasswd->setProperty("BackgroundEnabled", "false");
    tpasswd->setProperty("HorzFormatting","HorzCenterd");
    tpasswd->setText("Passphrase ");
    if (!ssh_checkbox->isSelected()) {
        tpasswd->disable();
        tpasswd->setAlpha(0.4f);
    }
    CEGUI::FrameWindow *rtray = static_cast<CEGUI::FrameWindow*>(wmgr.createWindow("Vanilla/FrameWindow", "RecvMessage"));
    rtray->setText("Message Log");
    rtray->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.20f, 0.0f)));
    rtray->setPosition(CEGUI::UVector2(CEGUI::UDim(0.24f, 0.0f), CEGUI::UDim(0.79f, 0.0f)));
    rtray->setCloseButtonEnabled(true);
    CEGUI::PushButton* closebutton = rtray->getCloseButton();
    closebutton->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::closeRecvMessageTray, this));
    rtray->setAlpha(0.5f);

    mTelop = wmgr.createWindow("Vanilla/Listbox", "Telop");
    mTelop->setSize(CEGUI::USize(CEGUI::UDim(1.0f, 0.0f), CEGUI::UDim(1.0f, 0.0f)));
    mTelop->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
    mTelop->setInheritsAlpha(false);

    rtray->setVisible(false);
    rtray->addChild(mTelop);

    // Menu bar
    CEGUI::Menubar *menu = static_cast<CEGUI::Menubar*>(wmgr.createWindow("TaharezLook/Menubar", "MenuBar"));
    menu->setSize(CEGUI::USize(CEGUI::UDim(0.3f, 0.0f), CEGUI::UDim(0.04f, 0.0f)));
    menu->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
    menu->setAlwaysOnTop(true);

    // Menu item
    CEGUI::MenuItem *display = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "item1"));
    display->setText("Display");
    display->setAlwaysOnTop(true);

    // Menu item
    CEGUI::MenuItem *plugin = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "item2"));
    plugin->setText("Services");
    plugin->setAlwaysOnTop(true);

    CEGUI::MenuItem *option = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "item3"));
    option->setText("Option");
    option->setAlwaysOnTop(true);
    //////////

    CEGUI::PopupMenu *display_menu = static_cast<CEGUI::PopupMenu*>(wmgr.createWindow("TaharezLook/PopupMenu", "display"));
    display_menu->setAlwaysOnTop(true);
    display_menu->setAutoCloseNestedPopups(true);

    // Menu item
    CEGUI::MenuItem *subview = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "subview"));
    subview->setAlwaysOnTop(true);
    if (mSubView)
    {
        subview->setText("* Sub View");
    }
    else
    {
        subview->setText("   Sub View");
    }

    // Menu item
    CEGUI::MenuItem *overwrite = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "overwrite"));
    overwrite->setAlwaysOnTop(true);
    if (!mOverWrite) overwrite->setText("   Overwrite shape file");
    else            overwrite->setText("* Overwrite shape file");

    // Menu item mejirusi 
    CEGUI::MenuItem *dmode = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "dynamicsview"));
    dmode->setText("   Dynamics view");
    dmode->setEnabled(false);
    dmode->setAlwaysOnTop(true);

    // Select Camera
    CEGUI::MenuItem *selectCamera = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "SelectCamera"));
    selectCamera->setText("  Select Camera");
    selectCamera->setEnabled(false);
    selectCamera->setAlwaysOnTop(true);


    CEGUI::PopupMenu *plugin_menu = static_cast<CEGUI::PopupMenu*>(wmgr.createWindow("TaharezLook/PopupMenu", "plugin_menu"));
    plugin_menu->setAlwaysOnTop(true);
    plugin_menu->setAutoCloseNestedPopups(true);

    // add plug-in
    CEGUI::MenuItem *add_plugin = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "add_plugin"));
    add_plugin->setText("  Add");
    add_plugin->setAlwaysOnTop(true);

    // start plug-in
    CEGUI::MenuItem *start_plugin = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "start_plugin"));
    start_plugin->disable();
    start_plugin->setText("  Start");
    start_plugin->setAlwaysOnTop(true);

    CEGUI::PopupMenu *startplugin_menu = static_cast<CEGUI::PopupMenu*>(wmgr.createWindow("TaharezLook/PopupMenu", "startplugin_menu"));
    startplugin_menu->setAlwaysOnTop(true);

        if (!mPSrvs.empty()) mPSrvs.clear();
    int count = 0;
    while (1) {
        TCHAR pathText[MAX_STRING_NUM];

        char tmp[2];
        sprintf_s(tmp, 2, "%d", count);
        std::string pathnum = std::string(tmp);
        std::string key = "PATH" + pathnum;

        GetPrivateProfileString("PLUGIN",key.c_str(),'\0', pathText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);

        if (pathText[0] == '\0') break;

        if (count >= MAX_SERVICE_SIZE)
        {
            char msg[MAX_STRING_NUM];
            sprintf_s(msg, MAX_STRING_NUM, "Exceed the maximum number of Service. Maximum number is %d.", MAX_SERVICE_SIZE);
            MessageBox( NULL, msg, _T("Error"), MB_OK);
            break;
        }

        mPSrvs.push_back(pathText);

        char * filename = getFileName((char*)pathText);
        CEGUI::Window *serviceItemWindow = wmgr.createWindow("TaharezLook/MenuItem", filename);
        serviceItemWindow->setAlwaysOnTop(true);
        serviceItemWindow->setText(filename);
        serviceItemWindow->setUserString("FullPath", pathText);
        startplugin_menu->addChild(serviceItemWindow);

        serviceItemWindow->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::startService, this));

        count++;
    }

    CEGUI::PopupMenu *option_menu = static_cast<CEGUI::PopupMenu*>(wmgr.createWindow("TaharezLook/PopupMenu", "option"));
    option_menu->setAlwaysOnTop(true);
    option_menu->setAutoCloseNestedPopups(true);

    // Entity position
    CEGUI::MenuItem *entity_pos = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "EntityPosition"));
    entity_pos->setText("  Entity Position");
    entity_pos->setAlwaysOnTop(true);

    // EntityPosition
    CEGUI::PopupMenu *epm = static_cast<CEGUI::PopupMenu*>(wmgr.createWindow("TaharezLook/PopupMenu", "EntityPositionMenu"));
    epm->setAlwaysOnTop(true);
    CEGUI::MenuItem *mov = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "MeanOfVertex"));
    mov->setAlwaysOnTop(true);
    CEGUI::MenuItem *cov = static_cast<CEGUI::MenuItem*>(wmgr.createWindow("TaharezLook/MenuItem", "CenterOfVertex"));
    cov->setAlwaysOnTop(true);

    if (mAlgEntityPos == 1) {
        mov->setText("* Average vertices (old)");
        cov->setText("   Center of vertices (default)");
    }
    if (mAlgEntityPos == 2) {
        mov->setText("   Average vertices (old)");
        cov->setText("* Center of vertices (default)");
    }

    epm->addChild(mov);
    epm->addChild(cov);

    entity_pos->addChild(epm);


    subview     ->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::subView,        this));
    overwrite   ->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::overwriteShape, this));
    ssh_checkbox->subscribeEvent(CEGUI::ToggleButton::EventMouseClick, CEGUI::Event::Subscriber(&SgvMain::sshCheckONOFF,  this));
    dmode       ->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::dynamicsView,   this));
    mov         ->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::setMOV,         this));
    cov         ->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::setCOV,         this));
    add_plugin  ->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::editService,    this));

    // editbox
    connect->addChild(sname);
    connect->addChild(serverip);
    connect->addChild(port);
    connect->addChild(cb);
    connect->addChild(ssh_checkbox);
    connect->addChild(username);
    connect->addChild(passwd);

    // title
    connect->addChild(tname);
    connect->addChild(tip);
    connect->addChild(tport);
    connect->addChild(ch_ssh);
    connect->addChild(tusername);
    connect->addChild(tpasswd);

    display->addChild(display_menu);
    plugin  ->addChild(plugin_menu);
    option  ->addChild(option_menu);
    start_plugin->addChild(startplugin_menu);

    display_menu->addChild(subview);
    display_menu->addChild(overwrite);
    display_menu->addChild(dmode);
    display_menu->addChild(selectCamera);

    plugin_menu->addChild(add_plugin);
    plugin_menu->addChild(start_plugin);

    option_menu->addChild(entity_pos);

    menu->addChild(display);
    menu->addChild(plugin);
    menu->addChild(option);

    sheet->addChild(connect);
    sheet->addChild(menu);
    sheet->addChild(rtray);

    CEGUI::System::getSingleton().getDefaultGUIContext().setRootWindow(sheet);

    cb->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::connect, this));

    Ogre::Camera *tmp_cam = mSceneMgr->createCamera("RTTCam1");

    std::vector<CEGUI::Window*> tmp_subwin;

    char tmp_name1[MAX_STRING_NUM], tmp_name2[MAX_STRING_NUM];
    static bool createdSubviews = false;
    if (createdSubviews) { return; }
    for (int i = 0; i < MAX_SUBVIEW; i++) 
    {
        sprintf(tmp_name1, "RTT_%d",i);
        sprintf(tmp_name2, "cam%d", i + 1);

        Ogre::TexturePtr tex = Ogre::TextureManager::getSingleton().createManual(
            tmp_name1,
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TextureType::TEX_TYPE_2D,
            512,
            512,
            0,
            Ogre::PixelFormat::PF_R8G8B8,
            Ogre::TextureUsage::TU_RENDERTARGET
        );

        Ogre::RenderTexture *rtex = tex->getBuffer()->getRenderTarget();
        rtex->setAutoUpdated(true);
        Ogre::Viewport *view  = rtex->addViewport(tmp_cam);

        view->setBackgroundColour(mBackGroundColor);
        view->setOverlaysEnabled(true);
        view->setAutoUpdated(true);
        mViews.push_back(view);

        CEGUI::Texture& guiTex = mRenderer->createTexture(tmp_name1, tex);
        
        CEGUI::BasicImage* rtImage = static_cast<CEGUI::BasicImage*>(&CEGUI::ImageManager::getSingleton().create("BasicImage", "RTTImages/" + std::string(tmp_name1)));
        CEGUI::OgreRenderer* ogreRenderer = static_cast<CEGUI::OgreRenderer*>(CEGUI::System::getSingleton().getRenderer());
        bool isTextureTargetVerticallyFlipped = ogreRenderer->isTexCoordSystemFlipped();
        CEGUI::Rectf imageArea;
        
        if (isTextureTargetVerticallyFlipped)
            imageArea = CEGUI::Rectf(0.0, tex->getHeight(), tex->getWidth(), 0.0f);
        else
            imageArea = CEGUI::Rectf(0.0f, 0.0f, tex->getWidth(), tex->getHeight());
        rtImage->setArea(imageArea);
        rtImage->setAutoScaled(CEGUI::ASM_Disabled);
        rtImage->setTexture(&guiTex);

        CEGUI::Window *si = CEGUI::WindowManager::getSingleton().createWindow("TaharezLook/StaticImage", tmp_name1);
        si->setVisible(false);

        CEGUI::Window *fb = wmgr.createWindow("TaharezLook/Button", tmp_name2);
        fb->setSize(CEGUI::USize(CEGUI::UDim(0.15f, 0.0f), CEGUI::UDim(0.15f, 0.0f)));
        fb->setPosition(CEGUI::UVector2(CEGUI::UDim(0.85f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
        fb->setAlpha(0.50f);
        fb->setUserString("SubViewIndex", std::to_string((long long)i));
        tmp_subwin.push_back(fb);
        
        //si->setProperty("Image", CEGUI::PropertyHelper<CEGUI::Image*>::toString(&imageSet.getImage("RTTImage")));
        
        float rx = (0.24f * (1.0f - (float)R_DX)) / 2.0f;
        float ry = (0.24f * (1.0f - (float)R_DY)) / 2.0f;


        si->setSize(CEGUI::USize(CEGUI::UDim(0.24f * (float)R_DX, 0), CEGUI::UDim(0.24f * (float)R_DY, 0)));
        si->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f + rx, 0), CEGUI::UDim(0.04 + (0.24f * i) + ry, 0)));
        si->setProperty("Image", "RTTImages/" + std::string(tmp_name1));
        si->addChild(fb);
        si->setMaxSize(CEGUI::USize(CEGUI::UDim(1.0f, 0), CEGUI::UDim(1.0f, 0)));
        si->setUserString("SubViewIndex", std::to_string((long long)i));

        mSubViews.push_back(si);

        sheet->addChild(si);
    }

    sheet->subscribeEvent(CEGUI::Window::EventMouseButtonDown, CEGUI::Event::Subscriber(&SgvMain::mouseButtonDownForMainWindow, this));

    for (int i = 0; i < MAX_SUBVIEW; i++) 
    {
        tmp_subwin[i]->subscribeEvent(CEGUI::PushButton::EventClicked,  CEGUI::Event::Subscriber(&SgvMain::agentView, this));

        mSubViews[i]->subscribeEvent(CEGUI::Window::EventMouseButtonDown, CEGUI::Event::Subscriber(&SgvMain::cameraView_Down, this));
        mSubViews[i]->subscribeEvent(CEGUI::Window::EventMouseMove,       CEGUI::Event::Subscriber(&SgvMain::cameraView_Move, this));
        mSubViews[i]->subscribeEvent(CEGUI::Window::EventMouseButtonUp,   CEGUI::Event::Subscriber(&SgvMain::cameraView_Up,   this));
        mSubViews[i]->subscribeEvent(CEGUI::Window::EventMouseLeavesArea, CEGUI::Event::Subscriber(&SgvMain::cameraView_Up,   this));
    }
    createdSubviews = true;
}

bool SgvMain::mouseButtonDownForMainWindow(const CEGUI::EventArgs &eventArgs)
{
    const CEGUI::MouseEventArgs mouseEventArgs = static_cast<const CEGUI::MouseEventArgs&>(eventArgs);
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    if (mouseEventArgs.button == CEGUI::LeftButton)
    {
        // Close Popup menu.
        CEGUI::Menubar *menuBar = static_cast<CEGUI::Menubar*>(root_win->getChildRecursive("MenuBar"));
        
        for (size_t i=0; i < menuBar->getChildCount(); i++)
        {
            CEGUI::MenuItem *popupMenu = static_cast<CEGUI::MenuItem*>(menuBar->getChildAtIdx(i));
            popupMenu->closePopupMenu();
        }

        // Set up the ray scene query
        Ogre::Camera *mouseCamera;
        Ogre::Viewport *mouseViewport;

        if (oculusMode)
        {
            mouseCamera = oculus.m_cameras[0]; //Left eye camera
            mouseViewport = mViewPort;
        }
        else
        {
            mouseViewport = mViewPort;
            mouseCamera = mViewPort->getCamera();
        }

        CEGUI::Vector2f mousePos = CEGUI::System::getSingleton().getDefaultGUIContext().getMouseCursor().getPosition();
        Ray mouseRay = mouseCamera->getCameraToViewportRay(mousePos.d_x / mouseViewport->getActualWidth(), mousePos.d_y / mouseViewport->getActualHeight());
        mRaySceneQuery->setRay(mouseRay);
        mRaySceneQuery->setSortByDistance(true);

        // Execute query
        RaySceneQueryResult &result = mRaySceneQuery->execute();
        RaySceneQueryResult::iterator iter = result.begin();


        bool display = false;

        while (iter != result.end())
        {
            if (iter->movable)
            {
                Ogre::String str = iter->movable->getName();

                if (!str.empty() && str != "GroundPlane" && str != "plane")
                {
                    if (strstr(str.c_str(), "camera") != NULL) {
                        iter++;
                        continue;
                    }

                    if (strstr(str.c_str(), "ODEShape") != NULL) {
                        iter++;
                        continue;
                    }

                    // 
                    int strPos1 = 0;
                    int strPos2;
                    std::string namess;
                    std::string tmpss;

                    strPos2 = str.find("/", strPos1);
                    namess.assign(str, strPos1, strPos2 - strPos1);

                    std::map<std::string, Sgv::SgvEntity*>::iterator it = mAllEntities.find(namess);
                    if (it == mAllEntities.end()) {
                        iter++;
                        continue;
                    }

                    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
                    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
                    CEGUI::Window* ent_win = root_win->getChildRecursive("EntityDataWindow");
                    // entity data window 
                    if (ent_win != NULL && wmgr.isAlive(ent_win) && !display)
                    {
                        CEGUI::Window *main = ent_win;

                        if (main->isVisible()) {

                            if (mEntityData != NULL) {

                                mEntityData->resetTransparency();

                                mEntityData->setPositionMarkVisible(false);

                                if (mEntityData->isRobot()) {
                                    mEntityData->setJointPositionVisible(false);
                                    mEntityData->setSegmentPositionVisible(false);
                                    mEntityData->setCameraArrowVisible(false);
                                    mEntityData->setCameraPositionVisible(false);

                                    if (!mCurrentEntityName.empty()) {
                                        Ogre::Entity *ent = mSceneMgr->getEntity(mCurrentEntityName);
                                        if (ent != NULL)
                                            ent->getParentSceneNode()->showBoundingBox(false);
                                    }
                                }
                                else{
                                    mEntityData->setBoundingBoxVisible(false);
                                }
                            }

                            (*it).second->setTransparency(0.6f);

                            CEGUI::Listbox *eDataList = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive(("EntityDataList")));

                            int dataSize = mEntityDataList.size();
                            for (int i = 0; i < dataSize; i++) {
                                eDataList->removeItem(mEntityDataList[i]);
                            }
                            mEntityDataList.clear();

                            CEGUI::String entName = "Name : " + namess;
                            CEGUI::ListboxTextItem *entname = new CEGUI::ListboxTextItem(entName);

                            Ogre::Vector3 pos = (*it).second->getHeadNode()->getPosition();
                            char pos_tmp[MAX_STRING_NUM];
                            sprintf_s(pos_tmp, MAX_STRING_NUM, "Position : (%.3g, %.3g, %.3g)", pos.x, pos.y, pos.z);
                            CEGUI::ListboxTextItem *entpos = new CEGUI::ListboxTextItem(pos_tmp);
                            entpos->setTextColours(CEGUI::ColourRect(CEGUI::Colour(1.0f, 0.0f, 0.0f)));

                            Ogre::Quaternion qua = (*it).second->getHeadNode()->getOrientation();

                            char qua_tmp[MAX_STRING_NUM];
                            sprintf_s(qua_tmp, MAX_STRING_NUM, "Quaternion : (%.3g, %.3g, %.3g, %.3g)", qua.w, qua.x, qua.y, qua.z);
                            CEGUI::ListboxTextItem *qua_item = new CEGUI::ListboxTextItem(qua_tmp);

                            eDataList->addItem(entname);
                            eDataList->addItem(entpos);
                            eDataList->addItem(qua_item);
                            mEntityDataList.push_back(entname);
                            mEntityDataList.push_back(entpos);
                            mEntityDataList.push_back(qua_item);

                            if (!(*it).second->isRobot()) {
                                Ogre::Vector3 bsize = (*it).second->getBBoxSize();
                                Ogre::Vector3 scale = (*it).second->getScale();
                                char bbox_tmp[MAX_STRING_NUM];
                                sprintf_s(bbox_tmp, MAX_STRING_NUM, "BBoxSize : (%.3g, %.3g, %.3g)", bsize.x*scale.x, bsize.y*scale.y, bsize.z*scale.z);
                                CEGUI::ListboxTextItem *bbox = new CEGUI::ListboxTextItem(bbox_tmp);
                                eDataList->addItem(bbox);
                                mEntityDataList.push_back(bbox);
                                (*it).second->setBoundingBoxVisible(true);
                            }

                            else{
                                (*it).second->setSegmentPositionVisible(true);

                                (*it).second->setJointPositionVisible(true);

                                (*it).second->setCameraPositionVisible(true);

                                (*it).second->setCameraArrowVisible(true);

                                Ogre::SceneNode *partsnode = mSceneMgr->getEntity(str)->getParentSceneNode()->getParentSceneNode();

                                //partsnode->showBoundingBox(true);
                                Ogre::String ptmp = partsnode->getName();

                                mSceneMgr->getEntity(str)->getParentSceneNode()->showBoundingBox(true);

                                //Ogre::Vector3 lpos = partsnode->getPosition();
                                Ogre::Vector3 lpos = mSceneMgr->getEntity(str)->getParentSceneNode()->getPosition();
                                Ogre::Vector3 ppos = partsnode->convertLocalToWorldPosition(lpos);


                                Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(str);
                                Ogre::Pass *p = mat->getTechnique(0)->getPass(0);
                                Ogre::ColourValue color = p->getDiffuse();
                                p->setDiffuse(color.r, color.g, color.b, 0.7f);

                                Ogre::String partsname;
                                strPos1 = ptmp.find("/", 0);
                                partsname.assign(ptmp, strPos1 + 1, ptmp.size() - strPos1 - 1);

                                //Ogre::String jointname;
                                //strPos1 = jtmp.find("/",0);
                                //jointname.assign(jtmp, strPos1+1, jtmp.size() - strPos1 -1);

                                CEGUI::ListboxTextItem *separate1 = new CEGUI::ListboxTextItem("-----------------------------------");
                                CEGUI::ListboxTextItem *separate2 = new CEGUI::ListboxTextItem("-----------------------------------");

                                char parts_name[MAX_STRING_NUM];
                                sprintf_s(parts_name, MAX_STRING_NUM, "Parts name : %s", partsname.c_str());
                                CEGUI::ListboxTextItem *parts_name_item = new CEGUI::ListboxTextItem(parts_name);

                                char parts_pos[MAX_STRING_NUM];
                                sprintf_s(parts_pos, MAX_STRING_NUM, "Parts pos : (%.1f, %.1f, %.1f)", ppos.x, ppos.y, ppos.z);
                                CEGUI::ListboxTextItem *parts_pos_item = new CEGUI::ListboxTextItem(parts_pos);

                                //char joint_name[128];
                                //sprintf_s(joint_name, 128, "Joint name : %s", jointname.c_str());
                                //CEGUI::ListboxTextItem *joint_name_item = new CEGUI::ListboxTextItem(joint_name);

                                parts_name_item->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.5f, 0.0f)));
                                parts_pos_item->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.5f, 0.0f)));
                                //joint_name_item->setTextColours(CEGUI::ColourRect(CEGUI::colour(0.0f, 0.0f, 0.5f)));

                                eDataList->addItem(separate1);
                                eDataList->addItem(parts_name_item);
                                eDataList->addItem(parts_pos_item);
                                eDataList->addItem(separate2);
                                //eDataList->addItem(joint_name_item);

                                mEntityDataList.push_back(separate1);
                                mEntityDataList.push_back(parts_name_item);
                                mEntityDataList.push_back(parts_pos_item);
                                mEntityDataList.push_back(separate2);
                                //mEntityDataList.push_back(joint_name_item);

                                Ogre::Node::ChildNodeIterator it = partsnode->getChildIterator();
                                while (it.current() != it.end()) {
                                    SceneNode *node = static_cast<Ogre::SceneNode*>(it.current()->second);
                                    Ogre::String tmp_name = node->getName();
                                    if (strstr(tmp_name.c_str(), "/camera") != NULL) {

                                        if (mSceneMgr->hasCamera(tmp_name)) {

                                            Ogre::Camera *cam = mSceneMgr->getCamera(tmp_name);

                                            Ogre::String camname;
                                            strPos1 = tmp_name.find("/camera", 0);
                                            camname.assign(tmp_name, strPos1 + 1, tmp_name.size() - strPos1 - 1);

                                            Ogre::Vector3 cam_pos = cam->getRealPosition();

                                            char camera_name[MAX_STRING_NUM];
                                            sprintf_s(camera_name, MAX_STRING_NUM, "%s pos : (%.1f, %.1f, %.1f)", camname.c_str(), cam_pos.x, cam_pos.y, cam_pos.z);
                                            CEGUI::ListboxTextItem *camera_name_item = new CEGUI::ListboxTextItem(camera_name);
                                            camera_name_item->setTextColours(CEGUI::ColourRect(CEGUI::Colour(1.0f, 0.4f, 0.0f)));

                                            eDataList->addItem(camera_name_item);
                                            mEntityDataList.push_back(camera_name_item);
                                        }
                                    }
                                    it.moveNext();
                                }
                            }
                            (*it).second->setPositionMarkVisible(true);

                            mEntityData = (*it).second;
                            display = true;
                            mCurrentEntityName = str;
                        }
                    }

                    //char tmp[64];
                    //sprintf(tmp, "name = %s", namess.c_str());
                    //MessageBox( NULL, tmp, "Error", MB_OK);

                    int camNum = (*it).second->getCameraNum();

                    if (camNum > 0 && mSubView)  {
                        for (int i = 0; i < 4; i++) {
                            std::string cam_name = (*it).second->getCameraName(i + 1);

                            if (cam_name.length() > 0) {
                                Ogre::Camera *ocam = mSceneMgr->getCamera(cam_name);
                                mViews[i]->setCamera(ocam);

                                mSubViews[i]->setVisible(true);
                            }
                            else{
                                if (mSubViews[i]->isVisible()) {
                                    mSubViews[i]->setVisible(false);
                                }
                            }
                        }
                    }
                    
                    if (wmgr.isAlive(root_win->getChildRecursive("EntityList"))) {

                        std::map<CEGUI::String, CEGUI::ListboxTextItem*>::iterator it;
                        it = mMsgList.find(namess);
                        if (it != mMsgList.end()) {
                            CEGUI::Listbox *elist = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("EntityList"));
                            elist->deactivate();

                            elist->clearAllSelections();

                            CEGUI::String str = (*it).second->getText();
                            bool selected = (*it).second->isSelected();
                            if (!selected) {
                                (*it).second->setSelected(true);
                                elist->activate();
                                elist->ensureItemIsVisible((*it).second);
                            }
                        }
                    }
                }
            }
            iter++;
        } //  while (iter != result.end())
    }

    return true;
}

bool SgvMain::cameraView_Down(const CEGUI::EventArgs &eventArgs) 
{
    this->mMovingSubView = true;
 
    return true;
}

bool SgvMain::cameraView_Move(const CEGUI::EventArgs &eventArgs) 
{
    const CEGUI::MouseEventArgs mouseEventArgs = static_cast<const CEGUI::MouseEventArgs&>(eventArgs);

    int index = std::atoi(mouseEventArgs.window->getUserString("SubViewIndex").c_str());

    if (this->mMovingSubView) 
    {
        mSubViews[index]->setPosition(CEGUI::UVector2(
                                      CEGUI::UDim((mouseEventArgs.position.d_x - mSubViews[index]->getPixelSize().d_width /2.0) / mSubViews[index]->getParent()->getPixelSize().d_width,  0.0f ),
                                      CEGUI::UDim((mouseEventArgs.position.d_y - mSubViews[index]->getPixelSize().d_height/2.0) / mSubViews[index]->getParent()->getPixelSize().d_height, 0.0f )));
    }

    return true;
}

bool SgvMain::cameraView_Up(const CEGUI::EventArgs &eventArgs) 
{
    this->mMovingSubView = false;

    return true;
}

void SgvMain::chooseSceneManager(void)
{
    BaseApplication::chooseSceneManager();
}

void SgvMain::createFrameListener(void)
{
    BaseApplication::createFrameListener();

    mCount = 0;

    mRotateSpeed =.1f;
    mMoveXYSpeed =.3f;
    mMoveZSpeed =.3f;

    // Create RaySceneQuery
    mRaySceneQuery = mSceneMgr->createRayQuery(Ogre::Ray());
}


bool SgvMain::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
    BaseApplication::frameRenderingQueued(evt);

    if (oculusMode && oculusCameraFlag) {
        oculus.m_cameras[0]->setPosition(0.0f,0.0f,0.0f);
        oculus.m_cameras[1]->setPosition(0.0f,0.0f,0.0f);
        oculus.getCameraNode()->setPosition(oculusCamera->getRealPosition());
        oculus.m_cameras[0]->setOrientation(oculusCamera->getRealOrientation());
        oculus.m_cameras[1]->setOrientation(oculusCamera->getRealOrientation());
    }



            //static bool sended;

    if (mWindow->isClosed()) {
        return false;
    }



    if (mSended && mConnectServer) {
        if (!recvMoveEntities()) {
            //MessageBox(NULL, _T("failed to recv entities"), _T("Attention"), MB_OKCANCEL); 
        } else {
            mSended = false;
        }
    }

    if (mService != NULL) 
    {
        if (mService->isMsg()) {
            sigverse::RecvMsgEvent *mevt = mService->getRecvMsgEvent();
            CEGUI::String msg = mevt->getMsg();
            CEGUI::String sender = mevt->getSender();
            CEGUI::String Sender =  sender + " : ";

            CEGUI::ListboxTextItem *i_sender = new CEGUI::ListboxTextItem(Sender);
            CEGUI::ListboxTextItem *i_msg = new CEGUI::ListboxTextItem(msg);

            std::map<std::string, Sgv::SgvEntity *>::iterator it;
            it = mAllEntities.find(sender.c_str());
            if (it != mAllEntities.end()) {
                i_sender->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.0f, 0.5f)));
            } else {
                i_sender->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.5f, 0.0f, 0.0f)));
            }

            i_sender->setAutoDeleted(true);
            i_msg->setAutoDeleted(true);

            CEGUI::Listbox *mlist = static_cast<CEGUI::Listbox *>(mTelop);
            mlist->addItem(i_sender);
            mlist->addItem(i_msg);
            mlist->ensureItemIsVisible(i_msg);

            CEGUI::Window *rtray = mTelop->getParent();
            if (!rtray->isVisible()) {
                rtray->setVisible(true);
            }
        }
    }

    if (mConnectServer) { 
        if (mEntityData != NULL) {

            Ogre::Vector3 pos = mEntityData->getHeadNode()->getPosition();
            char tmp[MAX_STRING_NUM];
            sprintf_s(tmp, MAX_STRING_NUM, "position : (%g, %g, %g)", pos.x, pos.y, pos.z);
            mEntityDataList[1]->setText(tmp);

            CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
            CEGUI::Listbox *eDataList = static_cast<CEGUI::Listbox *>(CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow()->getChildRecursive("EntityDataList"));
        }


        if (!mSended) {
            char msg[4];
            char *p = msg;
            //unsigned short tmp = htons(GET_MOVE_ENTITIES);
            BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0004); // TODO: Magic number
            BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0004); 
            //memcpy(msg, (char*)&tmp, 2); 
            SOCKET sock = mSock->getSocket();
            int ret = send(sock, msg, 4, 0);
            if (ret < 0) {
                MessageBox(NULL, _T("failed to send request to get entities."), _T("Attention"), MB_OKCANCEL); 
            } else {
                mSended = true;
            }
        }

        int requestType = mService->getRequestType();

        if (requestType != NULL) 
        {
            switch(requestType) 
            {
                case VIEWER_REQUEST_TYPE_CAPTUREVIEW: 
                {
                    captureView();
                    break;
                }
                case VIEWER_REQUEST_TYPE_DISTANCE_SENSOR: 
                {
                    distanceSensor();
                    break;
                }
                case VIEWER_REQUEST_TYPE_DETECT_ENTITIES: 
                {
                    detectEntities();
                    break;
                }
            }
        }
        if (mServices.size() != 0) 
            checkRequestFromService();

        Sleep(8); // TODO: Magic number

    } else {
        Sleep(8); // TODO: Magic number
    }


    return true;
}
 
bool SgvMain::keyPressed( const OIS::KeyEvent &arg )
{
    return BaseApplication::keyPressed(arg);
}

bool SgvMain::keyReleased( const OIS::KeyEvent &arg )
{
    return BaseApplication::keyReleased(arg);
}


bool SgvMain::mouseMoved( const OIS::MouseEvent &arg )
{
    BaseApplication::mouseMoved(arg);

    /*
     * Since the position change of the camera is a common to all Window, it does at the time of receiving OIS event.
     */
    int xrel = arg.state.X.rel;
    int yrel = arg.state.Y.rel;

    // If we are dragging the left mouse button.
    if (mLMouseDown)
    {
//		printf("");
    }
    else if (mRMouseDown)
    {
        if (mShift)
        {
            if (oculusMode) {
                Ogre::Vector3 pos = oculus.GetPosition();
                Ogre::Vector3 right = oculus.m_cameras[0]->getRight();
                Ogre::Vector3 up    = oculus.m_cameras[0]->getUp();
                pos += right* -xrel * mMoveXYSpeed + up * yrel * mMoveXYSpeed;
                oculus.SetPosition(pos);
            }
            Ogre::Vector3 pos = mCamera->getPosition();
            Ogre::Vector3 right = mCamera->getRight();
            Ogre::Vector3 up = mCamera->getUp();
            pos += right* -xrel * mMoveXYSpeed + up * yrel * mMoveXYSpeed;
            mCamera->setPosition(pos);
        }
        else if (mCtrl)
        {
            if (oculusMode) {
                Ogre::Vector3 pos = oculus.GetPosition();
                Ogre::Vector3 dir = oculus.m_cameras[0]->getDirection();
                pos += dir * yrel * mMoveZSpeed;
                oculus.SetPosition(pos);
            }
            Ogre::Vector3 pos = mCamera->getPosition();
            Ogre::Vector3 dir = mCamera->getDirection();
            pos += dir * yrel * mMoveZSpeed;
            mCamera->setPosition(pos);
        }
        else
        { 
            mCamera->yaw(Ogre::Degree(-xrel * mRotateSpeed));
            mCamera->pitch(Ogre::Degree(yrel * mRotateSpeed));
        }
    }

    return true;
}
 
bool SgvMain::mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
    return BaseApplication::mousePressed(arg, id);
}

bool SgvMain::mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
    return BaseApplication::mouseReleased(arg, id);
}


void SgvMain::windowResized(Ogre::RenderWindow* rw)
{
    BaseApplication::windowResized(rw);
}

void SgvMain::windowClosed(Ogre::RenderWindow* rw)
{
    BaseApplication::windowClosed(rw);
}


bool SgvMain::quit(const CEGUI::EventArgs &e)
{
    int ret = MessageBox(NULL, _T("Quit SIGVerse simulation ?"), _T("Attention"), MB_OKCANCEL); 
    if (ret == IDNO || ret == IDCANCEL) return false;

    sendRequest(QUIT);

    std::map<std::string, sigverse::SgvSocket*>::iterator it;
    it = mServices.begin();
    char tmp[4];
    char *p = tmp;
    BINARY_SET_DATA_S_INCR(p,unsigned short, 0x0005); // TODO: Magic number
    BINARY_SET_DATA_S(p,unsigned short, 0x0004);
    while (it != mServices.end()) {
        (*it).second->sendData(tmp, 4);
        delete (*it).second;
        it++;
    }
    mServices.clear();

    disconnect(CEGUI::EventArgs());

    return true;
}


bool SgvMain::connect(const CEGUI::EventArgs &e)
{
    HCURSOR hCursor;
    HCURSOR hCursorOld;
    hCursor = ::LoadCursor( NULL, IDC_WAIT );
    if ( hCursor != NULL )
        hCursorOld = ::SetCursor( hCursor );

    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *sname = root_win->getChildRecursive("ServiceName");
    CEGUI::Window *ip = root_win->getChildRecursive("ServerIP");
    CEGUI::Window *port = root_win->getChildRecursive("PortNumber");

    CEGUI::ToggleButton *sshcheck = static_cast<CEGUI::ToggleButton*>(root_win->getChildRecursive("ssh_check"));

    CEGUI::String snameString = sname->getText();
    CEGUI::String ipString    = ip->getText();
    CEGUI::String portString  = port->getText();
    CEGUI::Window *main = root_win;

    int portnum = atoi(portString.c_str());

    if (portnum < 1000) {
        portnum = BASE_PORT_NUM + WORLD_PORT_WIDTH * portnum;
    }
    CEGUI::String uname  = "";
    CEGUI::String passwd = "";

    bool sshCheck = sshcheck->isSelected();
    if (sshCheck) {

        mSSHConnect = true;
        CEGUI::Window *username = root_win->getChildRecursive("UserName");
        CEGUI::Window *pass = root_win->getChildRecursive("Password");
        
        uname = username->getText();
        passwd = pass->getText();
        m_session =sshLogin(uname.c_str(),"../.ssh/id_rsa.pub", "../.ssh/id_rsa",passwd.c_str(),ipString.c_str());
        if (m_session == NULL) return false;
        boost::thread(boost::bind(&SgvMain::sshPortForwarding, this, portnum, portnum, "localhost"));

        //sshPortForwarding(portnum, portnum, "localhost");
    }
    std::string scheck = "";
    if (sshCheck) scheck = "true";
    else          scheck = "false";

    std::string tmpStr = 
        "SERVICE_NAME=" + std::string(snameString.c_str()) + "\n" +
        "SERVER_IP="    + std::string(ipString.c_str())    + "\n" +
        "SERVER_PORT="  + std::string(portString.c_str())  + "\n" +
        "SSH_CHECK="    + scheck                           + "\n" +
        "SSH_USERNAME=" + std::string(uname.c_str());

    const int connectInfoNum = 5;
    char val[PRIVATE_PROFILE_SECTION_LEN * connectInfoNum] = { 0 };

    sprintf(val, tmpStr.c_str());
    WritePrivateProfileSection("LOGIN", val, mSettingPath);

    if (sshCheck) ipString = "localhost";

    mSock = new sigverse::SgvSocket();
    mSock->initWinsock();

    if (mSock->connectTo(ipString.c_str(), portnum))
    {

        std::string tmpString = "SIGViewer," + std::string(snameString.c_str());
        if (!mSock->sendData(tmpString.c_str(), tmpString.size())) return false;

        Sleep(500);

        mService = new Sgv::ViewerService(snameString.c_str());

        if (sendRequest(GET_ALL_ENTITIES))
        {
            mLog->printf(Sgv::LogBase::INFO, "Connection succeeded %s", ipString.c_str());

            mConnectServer = true;
            if (!createAllEntities()) {
                return false;
            }
            mLog->info("Created all entities");
        }
        else
        {
            MessageBox( NULL, _T("Cannot get entity data from server."), _T("Error"), MB_OK);
            return false;
        }

        /*
         * Clear Camera list
         */
        CEGUI::Window *selectCamera = root_win->getChildRecursive("SelectCamera");
        //selectCamera->removeChild("SelectCameraList");
        

        /*
         * Add Camera list
         */
        CEGUI::Window *selectCameraList = wmgr.createWindow("TaharezLook/PopupMenu", "SelectCameraList");
        selectCameraList->setAlwaysOnTop(true);

        // Add Main view port 
        CEGUI::Window *mainViewPortCameraWindow = wmgr.createWindow("TaharezLook/MenuItem", "PlayerCam");
        mainViewPortCameraWindow->setAlwaysOnTop(true);
        mainViewPortCameraWindow->setText("PlayerCam");
        mainViewPortCameraWindow->subscribeEvent(CEGUI::Window::EventMouseClick, CEGUI::Event::Subscriber(&SgvMain::selectCameraList, this));
        mainViewPortCameraWindow->setUserString("CameraName", "PlayerCam");
        selectCameraList->addChild(mainViewPortCameraWindow);

        bool isCameraListNumberMax = false;

        // Add Avatar camera
        for(std::map<std::string ,Sgv::SgvEntity*>::iterator itAllentities = mAllEntities.begin(); itAllentities != mAllEntities.end(); itAllentities++) 
        {
            if (isCameraListNumberMax){ break; }

            std::map<int, std::string> cameras = (itAllentities->second)->getCameraName();

            for (std::map<int, std::string>::iterator itCameras = cameras.begin(); itCameras != cameras.end(); itCameras ++)
            {
                if (selectCameraList->getChildCount() == MAX_CAMERA_LIST)
                { 
                    mLog->printf(Sgv::LogBase::ERR, "Exceed the maximum number of camera. Maximum number is %d.", (MAX_CAMERA_LIST-1)); //Exclude PlayerCam.
                    isCameraListNumberMax = true; 
                }

                if (isCameraListNumberMax){ break; }

                CEGUI::Window *cameraWindow = wmgr.createWindow("TaharezLook/MenuItem", itCameras->second);
                cameraWindow->setAlwaysOnTop(true);
                cameraWindow->setText(itCameras->second.c_str());
                cameraWindow->subscribeEvent(CEGUI::Window::EventMouseClick, CEGUI::Event::Subscriber(&SgvMain::selectCameraList, this));
                cameraWindow->setUserString("CameraName", itCameras->second.c_str());
                selectCameraList->addChild(cameraWindow);

            }
        }

        selectCamera->addChild(selectCameraList);


        mHost = ipString.c_str();
        mPort = portString.c_str();

        int port = atoi(mPort.c_str());
        if (port < 1000) {
            port = BASE_PORT_NUM + port*WORLD_PORT_WIDTH;
            char tmp[6];
            sprintf_s(tmp, 6,"%d",port);
            mPort = std::string(tmp);
        }

        if (!mService->connect(ipString.c_str(), portnum)) {
            disconnect(CEGUI::EventArgs());
            return false;
        }

        mService->startService();

        boost::thread requestThread(boost::bind(&SgvMain::acceptFromService, this));

    }
    else
    {
        return false;
    }


    CEGUI::Window *connect = root_win->getChildRecursive("Connect");
    connect->setVisible(false);


    CEGUI::Window *selectCameraWindow = root_win->getChildRecursive("SelectCamera");
    selectCameraWindow->setEnabled(true);


    CEGUI::Window *start_plugin = root_win->getChildRecursive("start_plugin");
    start_plugin->setEnabled(true);

    CEGUI::Window* worktray_win = root_win->getChildRecursive("WorkTray");
    if (!wmgr.isAlive(worktray_win))
    {
        CEGUI::Window *worktray = wmgr.createWindow("OgreTray/FrameWindow", "WorkTray");
        worktray->setSize(CEGUI::USize(CEGUI::UDim(0.35f, 0.0f), CEGUI::UDim(0.2f, 0.0f)));
        worktray->setPosition(CEGUI::UVector2(CEGUI::UDim(0.64f, 0.0f), CEGUI::UDim(0.79f, 0.0f)));

        worktray->setMinSize(CEGUI::USize(CEGUI::UDim(0.1f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
        worktray->setText("Server:" + ipString + " Port:" + portString);


        CEGUI::Window *start = wmgr.createWindow("OgreTray/Button", "Start");
        start->setSize(CEGUI::USize(CEGUI::UDim(0.47f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        start->setPosition(CEGUI::UVector2(CEGUI::UDim(0.025f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
        if (!mSimRun)
            start->setText("START");
        else
            start->setText("STOP");

        CEGUI::Window *disconnect = wmgr.createWindow("OgreTray/Button", "Disconnect");
        disconnect->setSize(CEGUI::USize(CEGUI::UDim(0.47f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        disconnect->setPosition(CEGUI::UVector2(CEGUI::UDim(0.53f, 0.0f), CEGUI::UDim(0.35f, 0.0f)));
        disconnect->setText("Disconnect");

        CEGUI::Window *ent_data = wmgr.createWindow("OgreTray/Button", "EntityData");
        ent_data->setSize(CEGUI::USize(CEGUI::UDim(0.47f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        ent_data->setPosition(CEGUI::UVector2(CEGUI::UDim(0.025f, 0.0f), CEGUI::UDim(0.7f, 0.0f)));
        ent_data->setText("Entity Data");

        CEGUI::Window *msg = wmgr.createWindow("OgreTray/Button", "SendMessage");
        msg->setSize(CEGUI::USize(CEGUI::UDim(0.47f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        msg->setPosition(CEGUI::UVector2(CEGUI::UDim(0.025f, 0.0f), CEGUI::UDim(0.35f, 0.0f)));
        msg->setText("Message");
        if (!mSimRun)
            msg->disable();

        CEGUI::Window *time = wmgr.createWindow("OgreTray/Editbox", "Time");
        time->setSize(CEGUI::USize(CEGUI::UDim(0.3f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        time->setPosition(CEGUI::UVector2(CEGUI::UDim(0.53f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
        time->setText("0.00");

        CEGUI::Window *timeu = wmgr.createWindow("Vanilla/Button", "timeu");
        timeu->setSize(CEGUI::USize(CEGUI::UDim(0.15f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        timeu->setPosition(CEGUI::UVector2(CEGUI::UDim(0.85f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
        timeu->setText("sec");

        CEGUI::Window *quit = wmgr.createWindow("OgreTray/Button", "Quit");
        quit->setSize(CEGUI::USize(CEGUI::UDim(0.47f, 0.0f), CEGUI::UDim(0.3f, 0.0f)));
        quit->setPosition(CEGUI::UVector2(CEGUI::UDim(0.53f, 0.0f), CEGUI::UDim(0.70f, 0.0f)));
        quit->setText("Quit Simulation");

        worktray->addChild(start);
        worktray->addChild(disconnect);
        worktray->addChild(msg);
        worktray->addChild(quit);
        worktray->addChild(time);
        worktray->addChild(timeu);
        worktray->addChild(ent_data);

        main->addChild(worktray);

        start     ->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::startRequest     , this));
        disconnect->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::disconnect       , this));
        quit      ->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::quit             , this));
        msg       ->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::messageTray      , this));
        timeu     ->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::changeTimeUnit   , this));
        ent_data  ->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::displayEntityData, this));
    }
    else {
        CEGUI::Window *connect = root_win->getChildRecursive("WorkTray");
        connect->setVisible(true);
    }

    mGround = mSceneMgr->createEntity("plane", "MyPlane");
    mGround->setMaterialName("Plane");
    mGround->setCastShadows(false);
    mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(mGround);
    mGround->setVisible(false);

    CEGUI::Window *dview = root_win->getChildRecursive("dynamicsview");
    dview->setEnabled(true);

    if ( hCursor != NULL )
        hCursor = ::SetCursor( hCursorOld );


    TCHAR archeck[8];
    GetPrivateProfileString("PLUGIN_OPTION","AUTO_RUN",'\0', archeck, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
    if (strcmp(archeck, "true") == 0) {
        int srvsize = (int)mPSrvs.size();
        for (int i = 0; i < srvsize; i++) {
            if (!startService(mPSrvs[i])) {
                char tmp[MAX_STRING_NUM];
                sprintf_s(tmp, MAX_STRING_NUM, "Cannot start service [%s]",mPSrvs[i].c_str());
                MessageBox(NULL, tmp, _T("Error"), MB_OKCANCEL); 
            }
        }
        return true;
    }
    return true;
}

bool SgvMain::createAllEntities()
{
    if (m_pX3D != NULL) {
        delete m_pX3D;
        m_pX3D = NULL;
    }

    m_pX3D = new Sgv::X3D();
    if (!m_pX3D->init())
    {
        mLog->err("failed to initialize X3D object");
        return false;
    }
    char hbuf[6];

    memset(hbuf, 0, sizeof(hbuf));

    if (!mSock->recvData(hbuf, 6)) {
        return false;
    }

    int nbyte = atoi(hbuf) - 6;

    char *buf(NULL);
    buf = new char[nbyte];
    memset(buf, 0, sizeof(buf));

    int recieved = 0;

    if (!mSock->recvData(buf, nbyte)) { 
        //Sgv::LogBase base = mLog->getLevel();

        mLog->err("Could not receive entity data");
        delete [] buf;
        return false;
    }

    char *delim = ",";
    char *ctx;

    char *srun = strtok_s(buf, delim, &ctx);
    int run = atoi(srun);

    int serviceNum = atoi(strtok_s(NULL, delim, &ctx));

    // 
    for (int i = 0; i < serviceNum; i++) {
        std::string name = strtok_s(NULL, delim, &ctx);
        mService->setElseServiceName(name);
    }

    char *sesize = strtok_s(NULL, delim, &ctx);
    int esize = atoi(sesize);

    for (int i = 0; i < esize; i++) {
        char *id_str = strtok_s(NULL, delim, &ctx);

        char *scalex = strtok_s(NULL, delim, &ctx);
        char *scaley = strtok_s(NULL, delim, &ctx);
        char *scalez = strtok_s(NULL, delim, &ctx);
        float scx = atof(scalex);
        float scy = atof(scaley);
        float scz = atof(scalez);

        Ogre::Vector3 scale(scx, scy, scz);
        int id = atoi(id_str);

        char* j_str = strtok_s(NULL, delim, &ctx);
        int isJoint = atoi(j_str);

        Ogre::String name = strtok_s(NULL, delim, &ctx);

        int shapeNum = atoi(strtok_s(NULL, delim, &ctx));

        std::vector<Ogre::String> shapes;
        for (int i = 0; i < shapeNum; i++) {

            Ogre::String tmp_sfile = strtok_s(NULL, delim, &ctx);

            if (tmp_sfile == "nothing") {
                shapes.push_back("nothing");
                continue;
            }

            Ogre::String sfile = "./shape/" + tmp_sfile;

            if (!PathFileExists(sfile.c_str()) || mOverWrite)
            {
                if (!downloadFileRequest(tmp_sfile)) {
                    char tmp[MAX_STRING_NUM];
                    sprintf(tmp, "Failed to download shape file [%s]", tmp_sfile.c_str());
                    MessageBox( NULL, tmp, _T("Error"), MB_OK);
                }
            }
            shapes.push_back(sfile);
        }

        char *sx =strtok_s(NULL, delim, &ctx);
        char *sy =strtok_s(NULL, delim, &ctx);
        char *sz =strtok_s(NULL, delim, &ctx);
        float x = atof(sx);
        float y = atof(sy);
        float z = atof(sz);

        char *sqw =strtok_s(NULL, delim, &ctx);
        char *sqx =strtok_s(NULL, delim, &ctx);
        char *sqy =strtok_s(NULL, delim, &ctx);
        char *sqz =strtok_s(NULL, delim, &ctx);
        float qw = atof(sqw);
        float qx = atof(sqx);
        float qy = atof(sqy);
        float qz = atof(sqz);

        Ogre::Vector3 pos(x, y, z);
        Ogre::Quaternion rot(qw, qx, qy, qz);

        Sgv::SgvEntity* entity = new Sgv::SgvEntity(mHeadNode, name, pos, rot,scale, id);

        int fsize = shapes.size();
        for (int i = 0; i < fsize; i++) {
            if (shapes[i] == "nothing") continue;
            CX3DParser* parser =m_pX3D->loadX3DFromFile(shapes[i].c_str());
            if (!parser) {
                char tmp[MAX_STRING_NUM];
                sprintf(tmp, "Failed to load shape file. [%s]",shapes[i].c_str());
                MessageBox( NULL, tmp, _T("Error"), MB_OK);
                mHeadNode->removeAllChildren();
                delete [] buf;
                return false;
            }
            if (!isJoint) {
                bool currshape = false;
                if (i == 0) {
                    currshape = true;
                }

                entity->createObj(parser, mSceneMgr, currshape, mAlgEntityPos);
            }
            // 
            else{
                entity->createRobotObj(parser, mSceneMgr);
            }
        }

        int partsNum = atoi(strtok_s(NULL, delim, &ctx));

        double comx = 0.0;
        double comy = 0.0;
        double comz = 0.0;

        for (int i = 0; i < partsNum; i++) {
        //if (partsNum == 1) {
            std::string partsName = strtok_s(NULL, delim, &ctx);

            Ogre::SceneNode *parent;
            Ogre::SceneNode *parts = NULL;

            if (partsName == "body" ) {
                parent = entity->getHeadNode()->createChildSceneNode();
            }
            else{

                Ogre::SceneNode *tmp_node = mSceneMgr->getSceneNode(name + "/" + partsName);

                parts = (Ogre::SceneNode*)tmp_node->getChild(name + "/" + partsName + "/trans0");

                //if (tmp_node == NULL)
                //tmp_node = mSceneMgr->getSceneNode(name + "/" + partsName);

                parent = tmp_node->createChildSceneNode();
                parent->setInheritOrientation(true);
                //entity->getHeadNode()-
            }
            double px = atof(strtok_s(NULL, delim, &ctx));
            double py = atof(strtok_s(NULL, delim, &ctx));
            double pz = atof(strtok_s(NULL, delim, &ctx));


            //char p_tmp[128];
            //sprintf_s(p_tmp, 128, "partsname = %s", partsName.c_str());
            //MessageBox( NULL, p_tmp, "Error", MB_OK);

            parent->translate(px/scx, py/scy, pz/scz);
            //parent->setOrientation(rqw, rqx, rqy, rqz);
            //parent->setOrientation(0.707, 0.0, 0.707, 0.0);

            if (parts != NULL) {
                Ogre::Quaternion q = parts->getOrientation();
                parent->setOrientation(q);
            }

            int type = atoi(strtok_s(NULL, delim, &ctx));


            // sphere
            if (type == 0) {

                double r = atof(strtok_s(NULL, delim, &ctx));

                char tmp_name[MAX_STRING_NUM];
                sprintf_s(tmp_name, MAX_STRING_NUM, "%s/ODEShape%d",name.c_str(), i);
                //Ogre::Entity *oent = mSceneMgr->createEntity(tmp_name, "MySphere.mesh");
                Ogre::Entity *oent = mSceneMgr->createEntity(tmp_name, "pSphere1.mesh");
                Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("MA_Red_Clone");

                oent->setCastShadows(true);

                if (mtr.isNull()) {
                    Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("MA_Red");
                    mtr = tmp->clone("MA_Red_Clone");
                    mtr->setDepthCheckEnabled(true);
                    mtr->setDepthWriteEnabled(true);
                    mtr->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                    mtr->setAmbient(0.5, 0.0, 0.0);
                }
                oent->setMaterial(mtr);
                parent->scale(Ogre::Vector3(r/scx, r/scy, r/scz));
                parent->attachObject(oent);

                //parent->setOrientation(parts->getOrientation());

            }

            // box
            else if (type == 1) {
                double sx = atof(strtok_s(NULL, delim, &ctx));
                double sy = atof(strtok_s(NULL, delim, &ctx));
                double sz = atof(strtok_s(NULL, delim, &ctx));

                char tmp_name[MAX_STRING_NUM];
                sprintf_s(tmp_name, MAX_STRING_NUM, "%s/ODEShape%d",name.c_str(), i);
                Ogre::Entity *oent = mSceneMgr->createEntity(tmp_name, "MyBox.mesh");
                Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("MA_Green_Clone");

                oent->setCastShadows(true);

                if (mtr.isNull()) {
                    Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("MA_Green");
                    mtr = tmp->clone("MA_Green_Clone");

                    mtr->setDepthCheckEnabled(true);
                    mtr->setDepthWriteEnabled(true);
                    mtr->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                    mtr->setAmbient(0.0, 0.5, 0.0);
                }
                oent->setMaterial(mtr);
                parent->scale(Ogre::Vector3(sx/scx, sy/scy, sz/scz));
                parent->attachObject(oent);


            }
            // cylinder
            else if (type == 3) {

                double radius = atof(strtok_s(NULL, delim, &ctx));
                double length = atof(strtok_s(NULL, delim, &ctx));

                char tmp_name[MAX_STRING_NUM];
                sprintf_s(tmp_name, MAX_STRING_NUM, "%s/ODEShape%d",name.c_str(), i);
                //Ogre::Entity *oent = mSceneMgr->createEntity(tmp_name, "MyCylinder.mesh");
                Ogre::Entity *oent = mSceneMgr->createEntity(tmp_name, "pCylinder1.mesh");

                oent->setCastShadows(true);

                float mean = (scx + scz)/2; 
                parent->scale(Ogre::Vector3(radius ,length/2, radius));

                //parent->setOrientation(parts->getOrientation());

                /*
                char tmp[128];
                sprintf(tmp, "pname = %s", pname.c_str());
                MessageBox( NULL, tmp, "Error", MB_OK);
                */

                //parent->setOrientation(qua *Ogre::Quaternion(0.707f, 0.707f, 0.0f, 0.0f));
                parent->setInitialState();

                Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("MA_Blue_Clone");
                if (mtr.isNull()) {
                    Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("MA_Blue");
                    mtr = tmp->clone("MA_Blue_Clone");
                    mtr->setDepthCheckEnabled(true);
                    mtr->setDepthWriteEnabled(true);
                    mtr->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                    mtr->setAmbient(0.0, 0.0, 0.5);
                }
                oent->setMaterial(mtr);
                parent->attachObject(oent);
            }

            parent->setVisible(false);
            m_ODENodes.push_back(parent);
        } // for (int i = 0; i < partsNum; i++) {

        char *sisAttach = strtok_s(NULL, delim, &ctx);
        int isAttach = atoi(sisAttach);

        //char tmp[128];
        //sprintf(tmp, " %d", isAttach);
        //MessageBox( NULL, tmp, "Error", MB_OK);

        if (isAttach) {
            entity->setisAttach(true);

            char *scamNum = strtok_s(NULL, delim, &ctx);
            int camNum = atoi(scamNum);


            for ( int i = 0; i < camNum; i++) {

                // id
                Ogre::String sid = strtok_s(NULL, delim, &ctx);
                int cam_id = atoi(sid.c_str());

                Ogre::String link = strtok_s(NULL, delim, &ctx);

                char *sposx =strtok_s(NULL, delim, &ctx);
                char *sposy =strtok_s(NULL, delim, &ctx);
                char *sposz =strtok_s(NULL, delim, &ctx);
                float posx = atof(sposx);
                float posy = atof(sposy);
                float posz = atof(sposz);

                char *sdirx =strtok_s(NULL, delim, &ctx);
                char *sdiry =strtok_s(NULL, delim, &ctx);
                char *sdirz =strtok_s(NULL, delim, &ctx);
                float dirx = atof(sdirx);
                float diry = atof(sdiry);
                float dirz = atof(sdirz);
                Ogre::Vector3 cam_dir(dirx, diry, dirz);

                char *sqw =strtok_s(NULL, delim, &ctx);
                char *sqx =strtok_s(NULL, delim, &ctx);
                char *sqy =strtok_s(NULL, delim, &ctx);
                char *sqz =strtok_s(NULL, delim, &ctx);
                float qw = atof(sqw);
                float qx = atof(sqx);
                float qy = atof(sqy);
                float qz = atof(sqz);
                Ogre::Quaternion cam_qua(qw, qx, qy, qz);

                // Field of view
                char *sfov = strtok_s(NULL, delim, &ctx);
                float fov = atof(sfov);

                // aspcet ratio
                char *sar = strtok_s(NULL, delim, &ctx);
                float ar = atof(sar);

                char tmp[2];
                sprintf(tmp, "%d", i);
                Ogre::String num = std::string(tmp);

                Ogre::String cam_name = name + "/" + link + "/" + "camera" + sid;

                Ogre::Camera *cam = mSceneMgr->createCamera(cam_name);
                //cam->setPosition(posx, posy, posz);
                cam->setDirection(0.0, 0.0, 1.0);
                cam->setFOVy(Ogre::Radian(M_PI * fov / 180.0f));
                cam->setAspectRatio(ar);
                cam->setNearClipDistance(Ogre::Real(1.0f));
                cam->setFarClipDistance(Ogre::Real(3000.0f));

                Ogre::Vector3 vpos(posx, posy, posz);

                if (link == "body") {
                    Ogre::SceneNode *node = mSceneMgr->getSceneNode(name);
                    Ogre::SceneNode *camnode = node->createChildSceneNode(cam_name);

                    Ogre::Vector3 scale = entity->getScale();
                    vpos *= 1/scale;

                    camnode->translate(vpos);
                    camnode->setInheritOrientation(true);

                    Ogre::Vector3 zdir(0.0, 0.0, 1.0f);
                    Ogre::Quaternion rot = zdir.getRotationTo(cam_dir); 
                    camnode->setOrientation(rot);

                    if (cam_qua != Quaternion(1.0, 0.0, 0.0, 0.0)) {
                        camnode->setOrientation(cam_qua);
                    }
                    camnode->attachObject((Ogre::MovableObject*)cam);
                }
                else { 
                    Ogre::SceneNode *node = mSceneMgr->getSceneNode(name + "/" + link);
                    Ogre::SceneNode *camnode = node->createChildSceneNode(cam_name);

                    Ogre::Entity *cam_ent = mSceneMgr->createEntity(cam_name, "MySphere.mesh");
                    Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("Orange_Clone");

                    cam_ent->setCastShadows(true);

                    if (mtr.isNull()) {
                        Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
                        mtr = tmp->clone("Orange_Clone");
                        mtr->setAmbient(1.0f, 0.4f, 0.0f);
                        mtr->setSelfIllumination(1.0f, 0.4f, 0.0f);
                        mtr->setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
                        mtr->setLightingEnabled(true);
                    }

                    cam_ent->setMaterial(mtr);
                    camnode->scale(0.57f, 0.57f, 0.57f);

                    camnode->attachObject(cam_ent);

                    entity->addCameraPosEntity(cam_ent);

                    Ogre::ManualObject *cam_arrow = mSceneMgr->createManualObject(cam_name + "/arrow");
                    Ogre::MaterialPtr arrow = Ogre::MaterialManager::getSingleton().getByName("CameraArrow");
                    mCameraArrows.push_back(cam_arrow);

                    if (arrow.isNull()) {
                        Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
                        Ogre::MaterialPtr arrow = tmp->clone("CameraArrow");

                        arrow->setDiffuse(0.0f, 0.0f, 0.0f, 0.1f);
                        arrow->setAmbient(1.0f, 0.4f, 0.0f);
                        arrow->setSelfIllumination(1.0f, 0.4f, 0.0f);
                        arrow->setLightingEnabled(true);
                    }

                    cam_arrow->begin("CameraArrow", Ogre::RenderOperation::OT_POINT_LIST);
                    cam_arrow->position(0.0f, 0.0f, 0.0f);
                    for (int i = 0; i < 30; i++) {
                        //cam_arrow->position(dirx*i, diry*i, dirz*i);
                        cam_arrow->position(0.0, 0.0, i);
                    }
                    cam_arrow->end();
                    camnode->attachObject(cam_arrow);
                    entity->addCameraArrowEntity(cam_arrow);

                    Ogre::Vector3 scale = entity->getScale();
                    vpos *= 1/scale;

                    camnode->translate(vpos);
                    camnode->setInheritOrientation(true);

                    Ogre::Vector3 zdir(0.0, 0.0, 1.0f);
                    Ogre::Quaternion rot = zdir.getRotationTo(cam_dir); 
                    camnode->setOrientation(rot);

                    if (cam_qua != Quaternion(1.0, 0.0, 0.0, 0.0)) {
                        camnode->setOrientation(cam_qua);
                    }

                    camnode->attachObject((Ogre::MovableObject*)cam);
                    camnode->setVisible(false);
                }
                entity->addCameraName(cam_id, cam_name);
            } // if (isAttach) {
        }

        mAllEntities.insert(std::map<std::string, Sgv::SgvEntity*>::value_type(name, entity));

        //char tmp[128];
        //sprintf(tmp, "insert = (%s, %s) ", name.c_str(), entity->getName().c_str());
        //MessageBox( NULL, tmp, "Error", MB_OK);

    } 

    if (run)
    {
        mSimRun = true;
        //boost::thread requestThread(boost::bind(&SgvMain::requestLoop, this));
    }

    if (buf != NULL) {delete[] buf; buf = NULL;}
    return true;
}


bool SgvMain::startRequest(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    if (!mSimRun) {
        if (sendRequest(START)) {
            CEGUI::Window *start = root_win->getChildRecursive("Start");
            CEGUI::Window *dis = root_win->getChildRecursive("Disconnect");
            CEGUI::Window *msg = root_win->getChildRecursive("SendMessage");
            dis->enable();
            msg->enable();

            mSimRun = true;
            //boost::thread requestThread(boost::bind(&SgvMain::requestLoop, this));

            start->setText("STOP");

        }
    }
    else{
        //if (mSock->stopRequest()) {
        if (sendRequest(STOP)) {
            CEGUI::Window *start = root_win->getChildRecursive("Start");
            CEGUI::Window *msg = root_win->getChildRecursive("SendMessage");
            msg->disable();

            mSimRun = false;
            start->setText("START");
        }
    }
    return true;
}


bool SgvMain::agentView(const CEGUI::EventArgs &eventArgs)
{
    const CEGUI::WindowEventArgs windowEventArgs = static_cast<const CEGUI::WindowEventArgs&>(eventArgs);

    int subViewIndex = std::atoi(windowEventArgs.window->getUserString("SubViewIndex").c_str());

    if (oculusMode) {
        oculusCamera = mViews[subViewIndex]->getCamera();
        oculusCameraFlag = true;
    }
    else{
        Ogre::Camera *cam1 = mViews[subViewIndex]->getCamera();
        Ogre::Camera *cam2 = mViewPort->getCamera();
        mViewPort->setCamera(cam1);
        mViews[subViewIndex]->setCamera(cam2);
    }

    return true;
}



bool SgvMain::subView(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *swin = root_win->getChildRecursive("subview");

    if (mSubView) 
    {
        for (int i = 0; i < MAX_SUBVIEW; i++) 
        {
            mSubViews  [i]->setVisible(false);
        }
        swin->setText("   Sub View");

        mSubView = false;
    }
    else {
        for (int i = 0; i < MAX_SUBVIEW; i++) 
        {
            if (!mSubViews[i]->isVisible())
            {
                if (mViews[i]->getCamera() == NULL){ continue; }

                Ogre::Vector3 pos  = mViews[i]->getCamera()->getPosition();
                Ogre::Vector3 mpos = mCamera->getPosition(); 

                if (pos == mpos)
                {
                    mSubViews  [i]->setVisible(true);
                }
            }
            else{
                break;
            }
        }
        swin->setText("* Sub View");
        mSubView = true;
    }

    return true;
}


//! Event handler to select Camera list.
bool SgvMain::selectCameraList(const CEGUI::EventArgs &eventArgs)
{
    const CEGUI::WindowEventArgs windowEventArgs = static_cast<const CEGUI::WindowEventArgs&>(eventArgs);

    std::string cameraName = windowEventArgs.window->getUserString("CameraName").c_str();

    Ogre::Camera *cam = mSceneMgr->getCamera(cameraName);

    if (oculusMode)
    {
        oculusCamera = cam;
        oculusCameraFlag = true;
    }
    else
    {
        mViewPort->setCamera(cam);
    }

    return true;
}


bool SgvMain::overwriteShape(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *overwrite = root_win->getChildRecursive("overwrite");

    char over_set[PRIVATE_PROFILE_SECTION_LEN] = { 0 };

    if (!mOverWrite) {
        overwrite->setText("* Overwrite shape file");
        mOverWrite  = true;
        sprintf(over_set, "OVERWRITE=true");
    }
    else{
        overwrite->setText("   Overwrite shape file");
        mOverWrite  = false;
        sprintf(over_set, "OVERWRITE=false");
    }

    WritePrivateProfileSection("SHAPEFILE", over_set, mSettingPath);
    return true;
}

bool SgvMain::dynamicsView(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *dview = root_win->getChildRecursive("dynamicsview");
    if (m_dynamicsView == 0) {
        int odesize = m_ODENodes.size();
        for (int i = 0; i < odesize; i++) {
            m_ODENodes[i]->setVisible(true);
        }

        std::map<std::string, Sgv::SgvEntity*>::iterator it = mAllEntities.begin();
        while (it != mAllEntities.end()) {

            Sgv::SgvEntity *ent = (*it).second;
            //ent->setTransparency(0.0f);
            ent->setVisible(false);
            it++;
        }

        dview->setText("* Dynamics View");
        m_dynamicsView = 1;
    }
    else if (m_dynamicsView == 1) {

        std::map<std::string, Sgv::SgvEntity*>::iterator it = mAllEntities.begin();
        while (it != mAllEntities.end()) {
            Sgv::SgvEntity *ent = (*it).second;
            ent->setTransparency(0.5f);
            /*
            char jnum[32];
            sprintf(jnum, "ent = %s", ent->getName().c_str());
            MessageBox( NULL, jnum, "Error", MB_OK);
            */
            ent->setVisible(true);

            it++;
        }

        Ogre::MaterialPtr rm = Ogre::MaterialManager::getSingleton().getByName("MA_Red_Clone");
        Ogre::MaterialPtr gm = Ogre::MaterialManager::getSingleton().getByName("MA_Green_Clone");
        Ogre::MaterialPtr bm = Ogre::MaterialManager::getSingleton().getByName("MA_Blue_Clone");

        if (!rm.isNull()) rm->setDiffuse(0.8f, 0.0f, 0.0f, 0.5f);
        if (!gm.isNull()) gm->setDiffuse(0.0f, 0.8f, 0.0f, 0.5f);
        if (!bm.isNull()) bm->setDiffuse(0.0f, 0.0f, 0.8f, 0.5f);

        /*
        rm->setDiffuse(0.8f, 0.0f, 0.0f, 0.5f);
        gm->setDiffuse(0.0f, 0.8f, 0.0f, 0.5f);
        bm->setDiffuse(0.0f, 0.0f, 0.8f, 0.5f);
        */
        dview->setText("* Dynamics View (both)");
        m_dynamicsView = 2;
    }
    else {
        int odesize = m_ODENodes.size();
        for (int i = 0; i < odesize; i++) {
            m_ODENodes[i]->setVisible(false);
        }
        dview->setText("   Dynamics View");
        m_dynamicsView = 0;

        std::map<std::string, Sgv::SgvEntity*>::iterator it = mAllEntities.begin();
        while (it != mAllEntities.end()) {
            Sgv::SgvEntity *ent = (*it).second;
            ent->resetTransparency();
            it++;
        }

        Ogre::MaterialPtr rm = Ogre::MaterialManager::getSingleton().getByName("MA_Red_Clone");
        Ogre::MaterialPtr gm = Ogre::MaterialManager::getSingleton().getByName("MA_Green_Clone");
        Ogre::MaterialPtr bm = Ogre::MaterialManager::getSingleton().getByName("MA_Blue_Clone");

        if (!rm.isNull()) rm->setDiffuse(0.8f, 0.0f, 0.0f, 1.0f);
        if (!gm.isNull()) gm->setDiffuse(0.0f, 0.8f, 0.0f, 1.0f);
        if (!bm.isNull()) bm->setDiffuse(0.0f, 0.0f, 0.8f, 1.0f);
    }
    return true;
}

bool SgvMain::changeTimeUnit(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *timeu = root_win->getChildRecursive("timeu");
    CEGUI::String uni = timeu->getText();
    if (uni == "sec") {
        timeu->setText("min");
    }
    else if (uni == "min") {
        timeu->setText("h");
    }
    else if (uni == "h") {
        timeu->setText("sec");
    }
    return true;
}

bool SgvMain::messageTray(const CEGUI::EventArgs &e)
 {
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *main = root_win; //->getChildRecursive("MainSheet");

    CEGUI::Window* msg_tray_win = root_win->getChildRecursive("MessageTray");
    if (msg_tray_win == NULL || !wmgr.isAlive(msg_tray_win))
    {
        CEGUI::FrameWindow *msg = static_cast<CEGUI::FrameWindow*>(wmgr.createWindow("Vanilla/FrameWindow", "MessageTray"));
        msg->setText("Send Message");
        msg->setSize(CEGUI::USize(CEGUI::UDim(0.25f, 0.0f), CEGUI::UDim(0.45f, 0.0f)));
        msg->setPosition(CEGUI::UVector2(CEGUI::UDim(0.74f, 0.0f), CEGUI::UDim(0.01f, 0.0f)));
        msg->setCloseButtonEnabled(true);
        CEGUI::PushButton *cbutton = msg->getCloseButton();
        cbutton->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::closeSendMessageTray, this));
        msg->setAlpha(0.5f);

        // text
        CEGUI::Window *msgto = wmgr.createWindow("Vanilla/StaticText", "MsgTo");
        msgto->setText("Target entity:");
        msgto->setSize(CEGUI::USize(CEGUI::UDim(0.9f, 0.0f), CEGUI::UDim(0.075f, 0.0f)));
        msgto->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05f, 0.0f), CEGUI::UDim(0.00f, 0.0f)));
        msgto->setProperty("FrameEnabled", "false");
        msgto->setProperty("BackgroundEnabled", "false");
        msgto->setProperty("HorzFormatting","HorzCenterd");
        msgto->setInheritsAlpha(false);

        CEGUI::Listbox *elist;
        CEGUI::Window* ent_lst_win = root_win->getChildRecursive("EntityList");
        if (ent_lst_win == NULL || !wmgr.isAlive(ent_lst_win)) {
            elist = static_cast<CEGUI::Listbox *>(wmgr.createWindow("OgreTray/Listbox", "EntityList"));
            elist->setText("Send Message");
            elist->setSize(CEGUI::USize(CEGUI::UDim(0.9f, 0.0f), CEGUI::UDim(0.375f, 0.0f)));
            elist->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05f, 0.0f), CEGUI::UDim(0.075f, 0.0f)));
            elist->setMultiselectEnabled(true);
            elist->setInheritsAlpha(false);

            std::map<std::string, Sgv::SgvEntity*>::iterator it = mAllEntities.begin();
            while (it != mAllEntities.end()) {
                if ((*it).second->isAttach()) {
                    CEGUI::ListboxTextItem *item1 = new CEGUI::ListboxTextItem((*it).first.c_str());
                    item1->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.0f, 0.5f)));
                    item1->setSelectionBrushImage("TaharezLook/ListboxSelectionBrush");
                    elist->addItem(item1);
                    mMsgList.insert(std::map<CEGUI::String, CEGUI::ListboxTextItem*>::value_type((*it).first.c_str(), item1));
                }
                it++;
            }
        }
        else{
            elist = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("EntityList"));
        }
        CEGUI::Window *msg_txt = wmgr.createWindow("Vanilla/StaticText", "MsgTxt");
        msg_txt->setText("Message :");
        msg_txt->setSize(CEGUI::USize(CEGUI::UDim(0.9f, 0.0f), CEGUI::UDim(0.075f, 0.0f)));
        msg_txt->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05f, 0.0f), CEGUI::UDim(0.475f, 0.0f)));
        msg_txt->setInheritsAlpha(false);
        msg_txt->setProperty("FrameEnabled", "false");
        msg_txt->setProperty("BackgroundEnabled", "false");
        msg_txt->setProperty("HorzFormatting","HorzCenterd");

        CEGUI::Window *msgbox = wmgr.createWindow("OgreTray/MultiLineEditbox", "MessageBox");
        msgbox->setSize(CEGUI::USize(CEGUI::UDim(0.9f, 0.0f), CEGUI::UDim(0.325f, 0.0f)));
        msgbox->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05f, 0.0f), CEGUI::UDim(0.55f, 0.0f)));
        msgbox->setInheritsAlpha(false);

        CEGUI::Window *sendButton = wmgr.createWindow("OgreTray/Button", "button");
        sendButton->setText("Send");
        sendButton->setSize(CEGUI::USize(CEGUI::UDim(0.5f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
        sendButton->setPosition(CEGUI::UVector2(CEGUI::UDim(0.5f, 0.0f), CEGUI::UDim(0.9f, 0.0f)));
        sendButton->setInheritsAlpha(false);


        msg->addChild(msgto);
        msg->addChild(msg_txt);
        msg->addChild(elist);
        msg->addChild(msgbox);
        msg->addChild(sendButton);
        main->addChild(msg);

        sendButton->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::sendMessage, this));

    }
    else {
        CEGUI::Window *msg = root_win->getChildRecursive("MessageTray");
        if (msg->isVisible()) 
            msg->setVisible(false);
        else
            msg->setVisible(true);
    }

    return true;
}

bool SgvMain::sendMessage(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *mbox = root_win->getChildRecursive("MessageBox");
    CEGUI::String msg = mbox->getText();

    msg.erase(msg.size() - 1);

    std::map<CEGUI::String, CEGUI::ListboxTextItem*>::iterator it;
    it = mMsgList.begin();
    std::vector<std::string> toString;
    while (it != mMsgList.end()) {
        if ((*it).second->isSelected())
            toString.push_back((*it).first.c_str());
        it++;
    }
    if (toString.empty())
    {
        MessageBox( NULL, _T("Please select target entity."), "Error", MB_OK);
        return false;
    }


    else if (toString.size() == 1) {
        mService->sendMsg(toString[0], msg.c_str());
    }
    else {
        mService->sendMsg(toString, msg.c_str());
    }

    CEGUI::String Sender = mService->getName() + " ---> ";
    int StringSize = toString.size();
    for (int i = 0; i < StringSize; i++) {
        if (i == 0) {
            Sender += toString[0];
        }
        else {
            Sender += ", " + toString[i];
        }
    }

    CEGUI::ListboxTextItem *i_sender = new CEGUI::ListboxTextItem(Sender);
    CEGUI::ListboxTextItem *i_msg = new CEGUI::ListboxTextItem(msg + "\n");

    i_sender->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.5f, 0.0f)));

    i_sender->setAutoDeleted(true);
    i_msg->setAutoDeleted(true);

    CEGUI::Listbox *mlist = static_cast<CEGUI::Listbox *>(mTelop);
    mlist->addItem(i_sender);
    mlist->addItem(i_msg);
    mlist->ensureItemIsVisible(i_msg);

    return true;
}

bool SgvMain::sendRequest(RequestType type)
{
    char msg[5];
    unsigned short tmp = htons(type);
    memcpy(msg, (char*)&tmp, 2); 

    if (type == GET_ALL_ENTITIES) {
        //MessageBox(NULL, "before send request", "error", MB_OK);
        Sleep(500);
        mLog->debug("Before send connect request.");
    }

    if (!mSock->sendData(msg,4))
    {
        //char tmp[64];
        //memcpy(tmp, "Could not send request. type : %d", type);
        MessageBox(NULL, _T("Failed to send requst to server"), _T("error"), MB_OK);
        return false;
    }

    if (type == GET_ALL_ENTITIES) {
        //MessageBox(NULL, "after send request", "error", MB_OK);
        Sleep(500);
        mLog->debug("After send connect request.");
    }
    return true;
}

bool SgvMain::recvMoveEntities()
{
    SOCKET sock = mSock->getSocket();

    fd_set fds, readfds;

    int n;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 100;

    FD_ZERO(&readfds);
    FD_SET(sock,&readfds);

    memcpy(&fds, &readfds, sizeof(fd_set));

    n = select(0, &fds, NULL, NULL, &tv);

    if (n == 0) {
        return false;
    }

    char *bsize = new char[16];


    if (!mSock->recvData(bsize, sizeof(unsigned int))) {
        MessageBox( NULL, _T("Cannot get entities data from server."), _T("Error"), MB_OK);
        disconnect(CEGUI::EventArgs());
        return false;
    }

    unsigned int tmp_size = BINARY_GET_DATA_S_INCR(bsize, unsigned int);
    int nbyte = tmp_size - sizeof(unsigned int);

    if (nbyte <= 0) { return true;}
    
    //char *buf(NULL);
    char *buf;
    if (0 < nbyte && nbyte < 999999) {
        buf = new char[nbyte];
        memset(buf, 0, sizeof(buf));
    }
    else { return false; }

    int recieved = 0;

    if (!mSock->recvData(buf, nbyte)) {
            MessageBox( NULL, _T("Cannot get entities data from server."), _T("Error"), MB_OK);
            disconnect(CEGUI::EventArgs());
            delete [] buf;
            return false;
    }

    char *delim = ",";
    char *ctx;

    char *entSize = strtok_s(buf, delim, &ctx);
    int esize = atoi(entSize);

    int startSim = atoi(strtok_s(NULL, delim, &ctx));

    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    if (startSim && !mSimRun) {

        mSimRun = true;


        CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();

        CEGUI::Window *start = root_win->getChildRecursive("Start");
        CEGUI::Window *msg = root_win->getChildRecursive("SendMessage");
        msg->enable();
        start->setText("STOP");
    }

    if (!startSim && mSimRun) {

        mSimRun = false;

        CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();

        CEGUI::Window *start = root_win->getChildRecursive("Start");
        CEGUI::Window *msg = root_win->getChildRecursive("SendMessage");
        msg->disable();
        start->setText("START");
    }

    CEGUI::String stime = strtok_s(NULL, delim, &ctx);

    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window *time = root_win->getChildRecursive("Time");
    CEGUI::Window *timeu = root_win->getChildRecursive("timeu");
    CEGUI::String uni = timeu->getText();
    if (uni == "sec")
        time->setText(stime);
    else if (uni == "min") {
        float ftime = atof(stime.c_str());
        ftime /= 60.0f;
        char tmp[MAX_STRING_NUM];
        sprintf(tmp, "%.2f",ftime);
        time->setText(tmp);
    } 
    else if (uni == "h") {
        float ftime = atof(stime.c_str());
        ftime /= 3600.0f;
        char tmp[MAX_STRING_NUM];
        sprintf(tmp, "%.2f",ftime);
        time->setText(tmp);
    } 

    for (int i = 0; i < esize; i++) {

        Ogre::String name = strtok_s(NULL, delim, &ctx);

        Sgv::SgvEntity *entity = this->getSgvEntity(name);

        char *move = strtok_s(NULL, delim, &ctx);
        Ogre::SceneNode *child = (Ogre::SceneNode*)mHeadNode->getChild(name);
        int imove = atoi(move);

        if (imove) {
            char *sx =strtok_s(NULL, delim, &ctx);
            char *sy =strtok_s(NULL, delim, &ctx);
            char *sz =strtok_s(NULL, delim, &ctx);
            float x = atof(sx);
            float y = atof(sy);
            float z = atof(sz);
            child->setPosition(x, y, z);
        }

        char *rot = strtok_s(NULL, delim, &ctx);
        int irot = atoi(rot);
        if (irot) {
            char *qw =strtok_s(NULL, delim, &ctx);
            char *qx =strtok_s(NULL, delim, &ctx);
            char *qy =strtok_s(NULL, delim, &ctx);
            char *qz =strtok_s(NULL, delim, &ctx);
            float fqw = atof(qw);
            float fqx = atof(qx);
            float fqy = atof(qy);
            float fqz = atof(qz);
            child->setOrientation(Ogre::Quaternion(fqw, fqx, fqy, fqz));
        }

        char *jinirot = strtok_s(NULL, delim, &ctx);
        int inirotNum = atoi(jinirot);

        for (int i = 0; i < inirotNum; i++) {
            Ogre::String jname = strtok_s(NULL, delim, &ctx);

            char *qw =strtok_s(NULL, delim, &ctx);
            char *qx =strtok_s(NULL, delim, &ctx);
            char *qy =strtok_s(NULL, delim, &ctx);
            char *qz =strtok_s(NULL, delim, &ctx);
            float fqw = atof(qw);
            float fqx = atof(qx);
            float fqy = atof(qy);
            float fqz = atof(qz);

            Ogre::SceneNode *node = mSceneMgr->getSceneNode(name + "/" + jname);
            node->setOrientation(Ogre::Quaternion(fqw, fqx, fqy, fqz));
            node->setInitialState();
        }

        char *jrot = strtok_s(NULL, delim, &ctx);
        int rotNum = atoi(jrot);

        for (int i = 0; i < rotNum; i++) {
            Ogre::String jname = strtok_s(NULL, delim, &ctx);

            char *qw =strtok_s(NULL, delim, &ctx);
            char *qx =strtok_s(NULL, delim, &ctx);
            char *qy =strtok_s(NULL, delim, &ctx);
            char *qz =strtok_s(NULL, delim, &ctx);
            float fqw = atof(qw);
            float fqx = atof(qx);
            float fqy = atof(qy);
            float fqz = atof(qz);

            Ogre::SceneNode *node = mSceneMgr->getSceneNode(name + "/" + jname);
            Ogre::Quaternion iniq = node->getInitialOrientation();
            node->setOrientation(iniq * Ogre::Quaternion(fqw, fqx, fqy, fqz));
        }

        char *icam = strtok_s(NULL, delim, &ctx);
        int camNum = atoi(icam);

        for (int i = 0; i < camNum; i++) {

            Ogre::String id = strtok_s(NULL, delim, &ctx);

            std::string link =strtok_s(NULL, delim, &ctx);

            std::string cam_name = name + "/" + link + "/" + "camera" + id;

            Ogre::Camera *cam = mSceneMgr->getCamera(cam_name);

            Ogre::SceneNode *parent = mSceneMgr->getSceneNode(cam_name);

            char *tmp_pos =strtok_s(NULL, delim, &ctx);
            int chpos = atoi(tmp_pos);

            if (chpos) {
                char *x =strtok_s(NULL, delim, &ctx);
                char *y =strtok_s(NULL, delim, &ctx);
                char *z =strtok_s(NULL, delim, &ctx);
                float fx = atof(x);
                float fy = atof(y);
                float fz = atof(z);
                Ogre::Vector3 pos(fx, fy, fz);

                Ogre::Vector3 scale = entity->getScale();
                pos *= 1/scale;

                parent->setPosition(pos);
            }

            char *tmp_dir =strtok_s(NULL, delim, &ctx);
            int chdir = atoi(tmp_dir);

            if (chdir) {
                char *x =strtok_s(NULL, delim, &ctx);
                char *y =strtok_s(NULL, delim, &ctx);
                char *z =strtok_s(NULL, delim, &ctx);
                float fx = atof(x);
                float fy = atof(y);
                float fz = atof(z);

                Ogre::Vector3 dir(fx, fy, fz);

                Ogre::Vector3 zdir(0.0, 0.0, 1.0);
                Ogre::Quaternion rot = zdir.getRotationTo(dir); 
                parent->setOrientation(rot);

            }

            char *tmp_fov =strtok_s(NULL, delim, &ctx);
            int chfov = atoi(tmp_fov);

            if (chfov) {
                char *fov =strtok_s(NULL, delim, &ctx);
                float FOV = atof(fov);
                cam->setFOVy(Ogre::Radian(M_PI * FOV / 180.0f));
            }

            char *tmp_as =strtok_s(NULL, delim, &ctx);
            int chas = atoi(tmp_as);

            if (chas) {
                char *as =strtok_s(NULL, delim, &ctx);
                float AS = atof(as);
                cam->setAspectRatio(AS);

            }

            char *tmp_link =strtok_s(NULL, delim, &ctx);
            int chlink = atoi(tmp_link);
            if (chlink) {
                Ogre::String link =strtok_s(NULL, delim, &ctx);
            }

        }

        int chShape = atoi(strtok_s(NULL, delim, &ctx));
        if (chShape) {
            Ogre::String filename = strtok_s(NULL, delim, &ctx);
            mAllEntities.find(name)->second->changeShape(filename);

        }
    } 

    if (mService->getNewServiceNum() > 0) {

        CEGUI::Window* ent_lst_win = root_win->getChildRecursive("EntityList");
        if (ent_lst_win != NULL && wmgr.isAlive(ent_lst_win)) {

            while (mService->getNewServiceNum() != 0) {

                std::string name = mService->getElseServiceName();

                mElseUsers.push_back(name.c_str());

                CEGUI::Listbox *elist = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("EntityList"));
                CEGUI::ListboxTextItem *item = new CEGUI::ListboxTextItem(name.c_str());

                item->setSelectionBrushImage("TaharezLook/ListboxSelectionBrush");
                item->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.5f, 0.0f, 0.0f)));

                elist->addItem(item);

                mMsgList.insert(std::map<CEGUI::String, CEGUI::ListboxTextItem*>::value_type(name.c_str(), item));
            }
        }
    }

    CEGUI::String disname = mService->getElseDisconnect();
    if (disname.size() > 0) {
        CEGUI::Window* ent_lst_win = root_win->getChildRecursive("EntityList");
        if (ent_lst_win != NULL && wmgr.isAlive(ent_lst_win)) {

            std::list<CEGUI::String>::iterator it = mElseUsers.begin();
            while (it != mElseUsers.end()) {
                if (strcmp((*it).c_str(), disname.c_str()) == 0) {
                    mElseUsers.erase(it); 
                    break;
                }
                it++;
            }

            CEGUI::Listbox *elist = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("EntityList"));
            std::map<CEGUI::String, CEGUI::ListboxTextItem*>::iterator dit;
            dit = mMsgList.begin();
            while (dit != mMsgList.end()) {
                if (strcmp(dit->first.c_str(), disname.c_str()) == 0) {
                    CEGUI::ListboxTextItem *item = dit->second;
                    elist->removeItem(item);
                    mMsgList.erase(dit);
                    break;
                }
                dit++;
            }
        }
    }

    delete [] buf;
    return true; 
}

bool SgvMain::downloadFileRequest(std::string name)
{
    char msg[MAX_STRING_NUM];
    char *p = msg;

    unsigned short ssize = name.size();

    BINARY_SET_DATA_S_INCR(p, unsigned short, DOWNLOAD);
    BINARY_SET_DATA_S_INCR(p, unsigned short, ssize);

    memcpy(p, name.c_str(), name.size()); 

    if (!mSock->sendData(msg, name.size() + 4))
    {
        MessageBox(NULL, _T("Failed to download shape file."), _T("error"), MB_OK);
        return false;
    }

    char bsize[7];
    if (!mSock->recvData(bsize, 7)) {
        return false;
    }

    int nbyte = atoi(bsize) - 7;

    if (nbyte <= 0) { return false;}
    
    char *buf(NULL);
    if (0 < nbyte && nbyte < 9999999) {
        buf = new char[nbyte];
        memset(buf, 0, sizeof(buf));
    }
    else { return false; }

    if (!mSock->recvData(buf, nbyte)) {
            MessageBox( NULL, _T("Could not recieve entities data."), _T("Error"), MB_OK);
            delete [] buf;
            return false;
    }

    FILE *fp;	
    std::string s_file = "./shape/" + name;
    
    if ((fp = fopen(s_file.c_str(), "w+")) == NULL) {
        std::string tmp_msg = "Could not file open. " + s_file;
        MessageBox( NULL, tmp_msg.c_str(), _T("Error"), MB_OK);
        return false;
    }

    if (fputs(buf,fp) < 0) {
        std::string tmp_msg = "Could not create shape file. " + s_file;
        MessageBox( NULL, tmp_msg.c_str(), _T("Error"), MB_OK);
    }
    fclose(fp);	
    return true;
}

bool SgvMain::disconnect(const CEGUI::EventArgs &e)
{
    oculusCameraFlag = false;
    sendRequest(DISCONNECT);

    std::string tmp_name = mService->getName();
    //MessageBox( NULL, tmp_name.c_str(), "Error", MB_OK);

    mService->disconnect();

    mEntityData = NULL;
    mEntityDataList.clear();

    mConnectServer = false;

    mHost.clear();
    mPort.clear();

    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
//    CEGUI::ImagesetManager &image = CEGUI::ImagesetManager::getSingleton();

    CEGUI::Window *start_plugin = root_win->getChildRecursive("start_plugin");
    start_plugin->setEnabled(false);

    mSceneMgr->destroyCamera("RTTCam1");

    //mRenderer->destroyAllTextures();

    std::vector<Ogre::ManualObject*>::iterator cait;
    cait = mCameraArrows.begin();
    while (cait != mCameraArrows.end()) {
        mSceneMgr->destroyManualObject(*cait);
        cait++;
    }

    mCameraArrows.clear();

    Ogre::TextureManager &tmgr = mRoot->getTextureManager()->getSingleton();
    tmgr.removeAll();
    wmgr.destroyAllWindows();

    if (!mAllEntities.empty()) 
    {
        if (oculusMode) 
        {
            oculus.SetPosition(Ogre::Vector3(0, 0, 0));
            oculus.SetOrientation(Ogre::Quaternion::IDENTITY);
        }
        else{
            mViewPort->setCamera(mCamera);
        }
        std::map<std::string, Sgv::SgvEntity*>::iterator itr = mAllEntities.begin();
        while (itr != mAllEntities.end()) {

            std::map<int, std::string> names = (*itr).second->getCameraName();
            std::map<int, std::string>::iterator it;
            it = names.begin();
            while (it != names.end()) {
                mSceneMgr->destroyCamera((*it).second.c_str());
                it++;
            }
            itr++;
        }
        mAllEntities.clear();
    }

    mSubViews.clear();

    mSceneMgr->destroyAllEntities();
    //mSceneMgr->destroyAllManualObjects();

    mService->disconnectFromAllController();
    //mSceneMgr->destroyAllCameras();

    mHeadNode->removeAndDestroyAllChildren();

    m_ODENodes.clear();

    mSock->finalWinsock();

    delete mService;
    delete mSock;
    if (m_pX3D != NULL) {
        m_pX3D->destroyAllManualObjects(mSceneMgr, mHeadNode);
        m_pX3D->deleteAllExtEntityInfo(mHeadNode);
        delete m_pX3D;
        m_pX3D = NULL;
    }
    mSock = NULL;
    mService = NULL;

    this->createInitWindow();

    if (mSended) mSended = false;

    CEGUI::Window *dview = root_win->getChildRecursive("dynamicsview");
    if (dview != NULL)
    {
        dview->setEnabled(false);
        m_dynamicsView = 0;
    }

    std::map<std::string, sigverse::SgvSocket*>::iterator it;
    it = mServices.begin();
    char tmp[4];
    char *p = tmp;
    BINARY_SET_DATA_S_INCR(p,unsigned short, 0x0005); // TODO: Magic number
    BINARY_SET_DATA_S(p,unsigned short, 0x0004);
    while (it != mServices.end()) {
        (*it).second->sendData(tmp, 4);
        delete (*it).second;
        it++;
    }
    mServices.clear();

    return true;
}

bool SgvMain::closeRecvMessageTray(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *win = root_win->getChildRecursive("RecvMessage");
    win->setVisible(false);
    return true;
}

bool SgvMain::closeSendMessageTray(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *win = root_win->getChildRecursive("MessageTray");
    win->setVisible(false);
    return true;
}

void SgvMain::acceptFromService()
{
    //WSADATA wsaData;
    SOCKET sock0;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    SOCKET sock;
    int n;

    sock0 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock0 == INVALID_SOCKET) {
        printf("socket : %d\n", WSAGetLastError());
        return;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(11000);
    addr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (::bind(sock0, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("bind : %d\n", WSAGetLastError());
        return;
    }

    if (listen(sock0, 5) != 0) {
        printf("listen : %d\n", WSAGetLastError());
        return;
    }

    while (1) {
        len = sizeof(client);
        sock = accept(sock0, (struct sockaddr *)&client, &len);

        if (sock == INVALID_SOCKET) {
            mLog->printf(Sgv::LogBase::ERR,"accept : %d\n", WSAGetLastError());
            break;
        }

        char tmp[4];
        n = recv(sock, tmp, 4, 0);
        if (n < 1) {
            mLog->printf(Sgv::LogBase::ERR,"recv : %d\n", WSAGetLastError());
            break;
        }
        char *p = tmp;

        unsigned short header = BINARY_GET_DATA_S_INCR(p, unsigned short);
        if (header != 0x0000) {
            continue;
        }

        unsigned short size = BINARY_GET_DATA_S_INCR(p, unsigned short);
        size -= 4;

        char buf[124]; 

        n = recv(sock, buf, size, 0);
        if (n < 1) {
            mLog->printf(Sgv::LogBase::ERR,"recv : %d\n", WSAGetLastError());
            continue;
        }

        //recvBuff[size] = '\0';
        buf[size] = '\0';

        sigverse::SgvSocket *ssock = new sigverse::SgvSocket(sock);
        mServices.insert(std::map<std::string, sigverse::SgvSocket*>::value_type(buf, ssock));

        //boost::thread serviceRequestThread(boost::bind(&SgvMain::recvServiceLoop, this, ssock));
        //delete [] recvBuff;
    }

    closesocket(sock);
    return;
}

bool SgvMain::captureView()
{
    sigverse::RecvCptEvent evt = mService->getCptEvent();

    std::string ename = evt.getSender();
    int id = evt.getCameraID();

    //char tmp[32];
    //sprintf(tmp, "id = %d name = %s",id, ename.c_str());
    //MessageBox( NULL, tmp, "Error", MB_OK);

    Ogre::String cam_name = getSgvEntity(ename)->getCameraName(id);
    if (cam_name.size() == 0) {
        char tmp[sizeof(unsigned short)*2];
        char *p = tmp;

        BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0005); // TODO: Magic number
        BINARY_SET_DATA_S_INCR(p, unsigned short, sizeof(unsigned short)*2);
        mService->getControllerSocket(ename)->sendData(tmp, sizeof(unsigned short)*2);
        mService->setRequestType(NULL);
        return false;
    }

    int size = 320 * 240 * 3 + 4;
    unsigned char *bitImage = new unsigned char[size];

    Ogre::Camera *cam = mSceneMgr->getCamera(cam_name);

    getImage(cam, bitImage);

    unsigned char *p = bitImage;
    BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0003); // TODO: Magic number

    BINARY_SET_DATA_S_INCR(p, unsigned short, 1);

    sigverse::SgvSocket *sock = mService->getControllerSocket(ename);
    if (sock == NULL) {
        mService->setRequestType(NULL);
        MessageBox( NULL, _T("captureView: failed to send image"), _T("Error"), MB_OK);
        return false;
    }
    if (!sock->sendData((char*)bitImage, size)) {
        MessageBox( NULL, _T("captureView: failed to send image"), _T("Error"), MB_OK);
        mService->setRequestType(NULL);
        return false;
    }
    delete [] bitImage;

    mService->setRequestType(NULL);
    return true;
}

bool SgvMain::distanceSensor()
{
    sigverse::RecvDstEvent evt = mService->getDstEvent();

    std::string ename = evt.getSender();
    int id = evt.getCameraID();

    sigverse::SgvSocket *sock = mService->getControllerSocket(ename);
    if (sock == NULL) {
        mService->setRequestType(NULL);
        MessageBox( NULL, _T("distanceSensor: failed to send image"), _T("Error"), MB_OK);
        return false;
    }

    Ogre::String cam_name = getSgvEntity(ename)->getCameraName(id);

    if (cam_name.size() == 0) {
        char tmp[sizeof(unsigned short)*2];
        char *p = tmp;

        BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0006); // TODO: Magic number
        BINARY_SET_DATA_S_INCR(p, unsigned short, sizeof(unsigned short)*2);
        mService->getControllerSocket(ename)->sendData(tmp, sizeof(unsigned short)*2);
        mService->setRequestType(NULL);
        return false;
    }

    Ogre::Camera *cam = mSceneMgr->getCamera(cam_name);

    Ogre::Real offset = evt.getMin();
    Ogre::Real range = evt.getMax();

    int type = evt.getType();

    int sendSize;

    if (type == 0) {
        sendSize = 1 + 4;
    }

    if (type == 1) {
        sendSize = 320 + 4;
    }

    if (type == 2) {
        sendSize = 320 * 240 + 4;
    }

    // for send 
    unsigned char *sendImage = new unsigned char[sendSize];
    unsigned char *p = sendImage;

    BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0003); // TODO: Magic number

    BINARY_SET_DATA_S_INCR(p, unsigned short, type + 2);

    if (type == 0) {
        getDistance(cam, p, offset, range);
    }
    else{
        getDistanceImage(cam, p, offset, range, type);
    }

    mService->setRequestType(NULL);

    if (!sock->sendData((char*)sendImage, sendSize)) {
        MessageBox( NULL, _T("distanceSensor: failed to send image"), _T("Error"), MB_OK);
        setDepthView(false);
        //mService->setRequestType(NULL);
        return false;
    }

    delete [] sendImage;
    return true;
}

bool SgvMain::detectEntities()
{
    sigverse::RecvDtcEvent evt = mService->getDtcEvent();

    std::string ename = evt.getSender();
    int id = evt.getCameraID();

    sigverse::SgvSocket *sock = mService->getControllerSocket(ename);
    if (sock == NULL) {
        mService->setRequestType(NULL);
        MessageBox( NULL, _T("detectEntities: failed to send entity data"), _T("Error"), MB_OK);
        return false;
    }

    Ogre::String cam_name = getSgvEntity(ename)->getCameraName(id);

    if (cam_name.size() == 0) {
        char tmp[sizeof(unsigned short)*2];
        char *p = tmp;

        BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0006); // TODO: Magic number
        BINARY_SET_DATA_S_INCR(p, unsigned short, sizeof(unsigned short)*2);
        mService->getControllerSocket(ename)->sendData(tmp, sizeof(unsigned short)*2);
        mService->setRequestType(NULL);
        return false;
    }

    Ogre::Camera *cam = mSceneMgr->getCamera(cam_name);

    Ogre::Vector3 pos = cam->getPosition();
    Ogre::Quaternion ori = cam->getOrientation();

    Ogre::Real ar = cam->getAspectRatio();
    Ogre::Radian fov = cam->getFOVy();

    Ogre::Vector3 lpos = cam->getParentNode()->convertLocalToWorldPosition(pos);

    //Ogre::Quaternion orient = cam->getParentNode()->convertLocalToWorldOrientation(ori);

    float left    = 0.0f;
    float top     = 0.0f;
    float right   = 1.0f;
    float bottom  = 1.0f;

    Ogre::Ray topLeft     = cam->getCameraToViewportRay(left, top);
    Ogre::Ray topRight    = cam->getCameraToViewportRay(right, top);
    Ogre::Ray bottomLeft  = cam->getCameraToViewportRay(left, bottom);
    Ogre::Ray bottomRight = cam->getCameraToViewportRay(right, bottom);

    Ogre::PlaneBoundedVolume vol;

    // front plane
    vol.planes.push_back(
        Ogre::Plane(topLeft.getPoint(3),
        topRight.getPoint(3),
        bottomRight.getPoint(3)));

    // top plane
    vol.planes.push_back(
        Ogre::Plane(
        topLeft.getOrigin(),
        topLeft.getPoint(100),
        topRight.getPoint(100)));

    // left plane
    vol.planes.push_back(
        Ogre::Plane(
        topLeft.getOrigin(),
        bottomLeft.getPoint(100),
        topLeft.getPoint(100)));

    //      bottom plane
    vol.planes.push_back(
        Ogre::Plane(
        topLeft.getOrigin(),
        bottomRight.getPoint(100),
        bottomLeft.getPoint(100)));

    //      right plane
    vol.planes.push_back(
        Ogre::Plane(
        topLeft.getOrigin(),
        topRight.getPoint(100),
        bottomRight.getPoint(100)));

    Ogre::PlaneBoundedVolumeList volList;
    volList.clear();
    volList.push_back(vol);

    // set volumes to query
    m_VolQuery->setVolumes(volList);

    // set query mask to exclude unnecessary nodes
    //m_VolQuery->setQueryMask(1);

    // execute query
    Ogre::SceneQueryResult result = m_VolQuery->execute();

    std::vector<Ogre::String> names;

    // listup every object and select it
    SceneQueryResultMovableList::iterator itr;
    for (itr=result.movables.begin(); itr!=result.movables.end(); itr++)
    {
        Entity *ent = (Entity *)*itr;
        if (ent)
        {
            Ogre::String name = ent->getName();

            if (name == "plane") continue;

            int strPos2;
            Ogre::String namess; 

            strPos2 = name.find("/",0);
            namess.assign(name, 0, strPos2);

            if (namess == ename) continue;

            std::vector<Ogre::String>::iterator it = std::find(names.begin(), names.end(), namess);

            if (it != names.end()) continue;

            names.push_back(namess);

            Ogre::Vector3 epos = mAllEntities.find(namess)->second->getHeadNode()->getPosition();

            //char tmp[128];
            //sprintf(tmp, "ent = %s pos(%f, %f, %f) ",namess.c_str(),epos.x, epos.y, epos.z);
            //MessageBox( NULL, tmp, "Error", MB_OK);

            Ogre::Vector3 rayDir = epos - lpos;
            Ogre::Ray ray(lpos, rayDir);
            double dist = (double)rayDir.length();

            if (!mRaySceneQuery) mRaySceneQuery = mSceneMgr->createRayQuery(ray);
            else mRaySceneQuery->setRay(ray);

            mRaySceneQuery->setSortByDistance(true);

            RaySceneQueryResult &result = mRaySceneQuery->execute();
            RaySceneQueryResult::iterator itr;
            for (itr = result.begin(); itr != result.end(); itr++) {
                Entity *ent = (Entity *)itr->movable;
                if (ent) {
                    Ogre::String tmp_name = ent->getName();

                    int strPos2;
                    Ogre::String middle; 

                    strPos2 = tmp_name.find("/",0);
                    middle.assign(tmp_name, 0, strPos2);

                    if (middle == "plane") continue;

                    else if (middle == namess || middle == ename) continue;

                    Ogre::Vector3 pos = mAllEntities.find(middle)->second->getHeadNode()->getPosition();

                    Ogre::Vector3 dir = pos - lpos;
                    double d = (double)dir.length();

                    if (d < dist) {
                        //char tmp[128];
                        //sprintf(tmp, "2 entity name = %s", middle.c_str());
                        //MessageBox( NULL, tmp, "Error", MB_OK);
                        std::vector<Ogre::String>::iterator it = std::find(names.begin(), names.end(), namess);
                        if (it != names.end())  names.erase(it);
                        break;
                    }
                }
            }
        }
    }

    int entSize = names.size();

    char tmp[16];
    sprintf(tmp,"%d",entSize);

    std::string sendData = std::string(tmp);
    sendData += ",";

    for (int i = 0; i < entSize; i++) {
        sendData += names[i] + ",";
    } 

    int sendSize = sendData.size() + sizeof(unsigned short)*2;

    char *sendBuff = new char[sendSize];
    char *p = sendBuff;

    BINARY_SET_DATA_S_INCR(p, unsigned short, 0x0008); // TODO: Magic number
    BINARY_SET_DATA_S_INCR(p, unsigned short, sendSize);

    memcpy(p,sendData.c_str(),sendData.size());

    if (!sock->sendData(sendBuff, sendSize)) {
        MessageBox( NULL, _T("detectEntities: failed to send image"), _T("Error"), MB_OK);
        setDepthView(false);
        delete [] sendBuff;
        mService->setRequestType(NULL);
        return false;
    }

    delete [] sendBuff;

    mService->setRequestType(NULL);
    return true;
}

void SgvMain::setDepthView(bool depth)
{
    static bool first = true;
    std::map<std::string, Sgv::SgvEntity*>::iterator it;
    it = mAllEntities.begin();

    if (depth) {
        Ogre::MaterialPtr mptr = Ogre::MaterialManager::getSingleton().getByName("depth_material");

        while (it != mAllEntities.end()) {

            std::vector<Ogre::Entity*> entities = (*it).second->getAllEntities();
            int nsize = entities.size();

            for (int i = 0; i < nsize; i++) {
                entities[i]->setMaterial(mptr);
            }
            it++;
        }
        if (mPlane->isVisible()) mPlane->setVisible(false);
        if (mAxis->isVisible()) mAxis->setVisible(false);

        if (!mGround->isVisible()) mGround->setVisible(true);
        mGround->setMaterial(mptr);
    }
    else {
        while (it != mAllEntities.end()) {

            std::vector<Ogre::Entity*> entities = (*it).second->getAllEntities();
            int nsize = entities.size();

            for (int i = 0; i < nsize; i++) {
                entities[i]->setMaterialName(entities[i]->getName());
            }
            it++;
        }
        if (!mPlane->isVisible()) mPlane->setVisible(true);
        if (!mAxis->isVisible()) mAxis->setVisible(true);

        if (mGround->isVisible()) mGround->setVisible(false);
        mGround->setMaterialName("Plane");
    }
}

LIBSSH2_SESSION *SgvMain::sshLogin(const char *uname, const char *key1, const char *key2, const char *pass, const char *host)
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,0), &wsadata);

    unsigned long mode = 1;
    int rc, sock = -1, i, auth = AUTH_NONE;
    struct sockaddr_in sin;
    //socklen_t sinlen;
    const char *fingerprint;
    char *userauthlist;

    const char *keyfile1 = key1;
    const char *keyfile2 = key2;
    const char *username = uname;
    const char *password = pass;
    const char *deststr = host;

    rc = libssh2_init(0);
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    sin.sin_family = AF_INET;

    if (INADDR_NONE == (sin.sin_addr.s_addr = inet_addr(deststr))) {
        struct hostent *host;
        host = gethostbyname(deststr);
        if (host == NULL) {
            MessageBox(NULL, _T("Invalid host."), _T("error"), MB_OK | MB_ICONEXCLAMATION);
            return false;
        }
        sin.sin_addr.S_un.S_addr = *(unsigned int *)host->h_addr_list[0];
    }

    sin.sin_port = htons(22);
    if (::connect(sock, (struct sockaddr*)(&sin),
        sizeof(struct sockaddr_in)) != 0) {
            MessageBox(NULL, _T("Failed ssh connect"), _T("error"), MB_OK | MB_ICONEXCLAMATION);
            return false;
    }

    /* Create a session instance */
    LIBSSH2_SESSION *session = libssh2_session_init();

    if (!session) {
        MessageBox(NULL, _T("Could not initialize SSH session! \n"), _T("error"), MB_OK | MB_ICONEXCLAMATION);
        goto shutdown;
    }

    rc = libssh2_session_startup(session, sock);
    if (rc) {
        MessageBox(NULL, _T("Error when starting up SSH session\n"),_T("error"), MB_OK | MB_ICONEXCLAMATION);
        goto shutdown;
    }

    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

    for (i = 0; i < 20; i++)
    {
        mLog->printf(Sgv::LogBase::INFO,"%02X", (unsigned char)fingerprint[i]);
    }

    userauthlist = libssh2_userauth_list(session, username, strlen(username));

    mLog->printf(Sgv::LogBase::INFO,"Authentication methods: %s", userauthlist);

    if (strstr(userauthlist, "password"))
        auth |= AUTH_PASSWORD;
    if (strstr(userauthlist, "publickey"))
        auth |= AUTH_PUBLICKEY;

    if (auth & AUTH_PASSWORD) {
        if (libssh2_userauth_password(session, username, password)) {
            MessageBox(NULL, _T("Authentication by password failed.\n"),_T("error"), MB_OK | MB_ICONEXCLAMATION);
            goto shutdown;
        }
    } else if (auth & AUTH_PUBLICKEY) {
        int ret = libssh2_userauth_publickey_fromfile(session, username, keyfile1, keyfile2, password);
        if (ret) {
            MessageBox(NULL, _T("Authentication by public key failed\n"),_T("error"), MB_OK | MB_ICONEXCLAMATION);
            if ( ret = LIBSSH2_ERROR_ALLOC)
                mLog->err("LIBSSH2_ERROR_ALLOC");
            else if ( ret = LIBSSH2_ERROR_SOCKET_SEND)
                mLog->err("LIBSSH2_ERROR_SOCKET_SEND");
            else if ( ret = LIBSSH2_ERROR_SOCKET_TIMEOUT)
                mLog->err("LIBSSH2_ERROR_SOCKET_TIMEOUT");
            else if ( ret = LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED)
                mLog->err("LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED");
            else if ( ret = LIBSSH2_ERROR_AUTHENTICATION_FAILED)
                mLog->err("LIBSSH2_ERROR_AUTHENTICATION_FAILED");
            goto shutdown;
        }
        mLog->debug("\tAuthentication by public key succeeded.\n");
    } else {
        MessageBox(NULL, _T("No supported authentication methods found!\n"),_T("error"), MB_OK | MB_ICONEXCLAMATION);
        goto shutdown;
    }
    if (m_sshSock < 0)
        m_sshSock = sock;
    /*
    else
    {
        if (m_sshSock2 < 0)
            m_sshSock2 = sock;
    }
    */
    return session;

shutdown:

    libssh2_session_disconnect(session, "Client disconnecting normally");
    libssh2_session_free(session);

    session = false;

    closesocket(sock);
    mLog->printf(Sgv::LogBase::ERR, "failed ssh connect to %s", host);
    return NULL;
}

void SgvMain::sshPortForwarding(unsigned int localport, unsigned int remoteport, const char *listenhost)
{
    char sockopt;
    int listensock = -1, forwardsock = -1;
    //int listensock1 = -1, forwardsock1 = -1;
    struct sockaddr_in sin;
    //struct sockaddr_in sin1;
    socklen_t sinlen;
    LIBSSH2_CHANNEL *channel  = NULL;
    LIBSSH2_CHANNEL *channel1 = NULL;
    LIBSSH2_CHANNEL *channel2 = NULL;
    LIBSSH2_CHANNEL *channel3 = NULL;

    //m_remote_desthost = "localhost"; /* resolved by the server */
    //m_remote_destport = remoteport;

    listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    //listensock1 = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sin.sin_family = AF_INET;
    //sin1.sin_family = AF_INET;
    sin.sin_port = htons(localport);
    //sin1.sin_port = htons(localport+1);

    if (INADDR_NONE == (sin.sin_addr.s_addr = inet_addr(listenhost))) {
        struct hostent *host;
        host = gethostbyname(listenhost);
        if (host == NULL) {
            MessageBox(NULL, _T("Invalid listen host."), _T("error"), MB_OK | MB_ICONEXCLAMATION);
            goto shutdown;
        }
        sin.sin_addr.S_un.S_addr = *(unsigned int *)host->h_addr_list[0];
        //sin1.sin_addr.S_un.S_addr = *(unsigned int *)host->h_addr_list[0];
    }
    sockopt = 1;

    setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    sinlen=sizeof(sin);
    if (-1 == ::bind(listensock, (struct sockaddr *)&sin, sinlen)) {
        mLog->err("error bind");
        goto shutdown;
    }

    if (-1 == listen(listensock, 2)) {
        mLog->err("error listen");
        goto shutdown;
    }

    const char* shost = inet_ntoa(sin.sin_addr);
    unsigned short sport = ntohs(sin.sin_port);

    int count = 0;
    for (;;)
    {
        forwardsock = accept(listensock, (struct sockaddr *)&sin, &sinlen);
        if (-1 == forwardsock) {
            mLog->err("error accept");
            goto shutdown;
        }

        if (m_session == NULL) return;

        if (libssh2_session_get_blocking(m_session) != 1) {
            libssh2_session_set_blocking(m_session, 1);
        }

        if (count > 1)
            channel = libssh2_channel_direct_tcpip_ex(m_session, "localhost", remoteport+count-1, shost, sport+count-1);
        else
            channel = libssh2_channel_direct_tcpip_ex(m_session, "localhost", remoteport, shost, sport);
        libssh2_session_set_blocking(m_session, 0);

        m_transSocks.push_back(forwardsock);
        m_transChannels.push_back(channel);

        if (count == 0)
            boost::thread(boost::bind(&SgvMain::transportData, this, 0));


        count++;
    }
shutdown:
    return;
}

void SgvMain::transportData(int id)
{
    int rc, i;
    fd_set fds;
    struct timeval tv;


    ssize_t len, wr;
    bool vtrans = false;
    int wtime = 10;
    char rbuf[16384];
    char wbuf[16384];

    // Must use non-blocking IO hereafter due to the current libssh2 API

    rc = libssh2_init(0);

    while (1) {
        FD_ZERO(&fds);

        int tsize = m_transSocks.size();
        for (int j = 0; j < tsize; j++) {
            FD_SET(m_transSocks[j], &fds);
        }
        //FD_SET(forwardsock, &fds);
        if (id == 0)
            tv.tv_sec = 0;
        else
            tv.tv_sec = 2;

        tv.tv_usec = 1000;
        //if (mConnectServer == false)
        //	goto shutdown;

        rc = select(tsize + 1, &fds, NULL, NULL, &tv);
        if (-1 == rc) {
            mLog->err("error select");
            goto shutdown;
        }

        for (int j = 0; j < tsize; j++) {

            if (rc && FD_ISSET(m_transSocks[j], &fds)) {

                len = recv(m_transSocks[j], wbuf, sizeof(wbuf), 0);
                if (len < 0) {
                    mLog->err("error recv");
                    closeChAndSock(m_transChannels[j], m_transSocks[j]);
                    goto shutdown;
                }else if (0 == len) {
                    //mLog->printf(Sgv::LogBase::ERR, "The client at %s:%d disconnected!\n", m_shost, m_sport);
                    closeChAndSock(m_transChannels[j], m_transSocks[j]);
                    goto shutdown;
                }
                wr = 0;
                do {

                    i = libssh2_channel_write(m_transChannels[j], wbuf, len);

                    if (LIBSSH2_ERROR_EAGAIN == i)
                    {
                        continue;
                    }

                    if (i < 0) {
                        mLog->printf(Sgv::LogBase::ERR,"libssh2_channel_write: %d",i);
                        closeChAndSock(m_transChannels[j], m_transSocks[j]);
                        goto shutdown;
                    }
                    wr += i;

                }while (i > 0 && wr < len);
            }
        }
        for (int j = 0; j < tsize; j++) {
            if (libssh2_session_get_blocking(m_session) != 0)  break;
            len = libssh2_channel_read(m_transChannels[j], rbuf, sizeof(rbuf));

            if (LIBSSH2_ERROR_EAGAIN == len)
            {
                continue;
            }
            else if (len < 0) {

                mLog->printf(Sgv::LogBase::ERR,"error libssh2_channel_read: %d", (int)len);

                goto shutdown;
            }
            wr = 0;
            while (wr < len) {
                i = send(m_transSocks[j], rbuf + wr, len - wr, 0);
                if (i <= 0) {
                    mLog->err("error write");
                    closeChAndSock(m_transChannels[j], m_transSocks[j]);
                    goto shutdown;
                }
                wr += i;
            }

            if (libssh2_channel_eof(m_transChannels[j])) {
                //("The server at %s:%d disconnected!\n",
                //m_remote_desthost, m_remote_destport);
                closeChAndSock(m_transChannels[j], m_transSocks[j]);
                goto shutdown;
            }
        }
    }
    return;
shutdown:
    return;
}

void SgvMain::closeChAndSock(LIBSSH2_CHANNEL *channel, SOCKET sock)
{
    libssh2_channel_free(channel);
    closesocket(sock);
}

bool SgvMain::sshCheckONOFF(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *uname = root_win->getChildRecursive("UserName");
    CEGUI::Window *passwd = root_win->getChildRecursive("Password");
    CEGUI::Window *tuname = root_win->getChildRecursive("tusername");
    CEGUI::Window *tpasswd = root_win->getChildRecursive("tpasswd");

    CEGUI::ToggleButton *sshcheck = static_cast<CEGUI::ToggleButton*>(root_win->getChildRecursive("ssh_check"));

    // check ON
    if (sshcheck->isSelected()) 
    {
        uname->setAlpha(1.0f);
        passwd->setAlpha(1.0f);
        tuname->setAlpha(1.0f);
        tpasswd->setAlpha(1.0f);

        uname->enable();
        passwd->enable();
        tuname->enable();
        tpasswd->enable();
    }
    // check OFF
    else
    {
        uname->setAlpha(0.4f);
        passwd->setAlpha(0.4f);
        tuname->setAlpha(0.4f);
        tpasswd->setAlpha(0.4f);

        uname->disable();
        passwd->disable();
        tuname->disable();
        tpasswd->disable();
    }
    return true;
}

bool SgvMain::addService(const CEGUI::EventArgs &e)
{
    if (mPSrvs.size() >= MAX_SERVICE_SIZE)
    {
        char msg[MAX_STRING_NUM];
        sprintf_s(msg, MAX_STRING_NUM, "Exceed the maximum number of Service. Maximum number is %d.", MAX_SERVICE_SIZE);
        MessageBox( NULL, msg, _T("Error"), MB_OK);
        return false;
    }

    char curr[MAX_STRING_NUM];
    GetCurrentDirectory(MAX_STRING_NUM, curr);

    static OPENFILENAME ofn;
    static TCHAR filename_full[MAX_PATH];
    static TCHAR filename[MAX_PATH];


    ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = _T("SIGVerse file(*.sig)\0*.sig"); 
    ofn.lpstrFile = filename_full;
    ofn.lpstrFileTitle = filename;
    ofn.nMaxFile = sizeof(filename_full);
    ofn.nMaxFileTitle = sizeof(filename);
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = _T("Open file");
    ofn.lpstrDefExt = _T("txt");

    if (!GetOpenFileName( &ofn ))
    {
        return false;
    }


    std::vector<std::string>::iterator it;
    it = std::find(mPSrvs.begin(), mPSrvs.end(), filename_full);

    if (it != mPSrvs.end()) {
        char msg[MAX_STRING_NUM];
        sprintf_s(msg, MAX_STRING_NUM, "Service \"%s\" already exist", filename);
        MessageBox( NULL, msg, _T("Error"), MB_OK);
        return false;
    }

    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *startplugin_menu = root_win->getChildRecursive("startplugin_menu");
    CEGUI::Window* filename_win = root_win->getChildRecursive(filename);
    if (filename_win != NULL && wmgr.isAlive(filename_win))
        wmgr.destroyWindow(filename_win);
    CEGUI::Window *serviceItemWindow = wmgr.createWindow("TaharezLook/MenuItem", filename);
    serviceItemWindow->setText(filename);
    serviceItemWindow->setUserString("FullPath", filename_full);
    startplugin_menu->addChild(serviceItemWindow);

    CEGUI::Listbox *slist = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("Service_ServiceList"));
    CEGUI::ListboxTextItem *item1 = new CEGUI::ListboxTextItem(filename);
    item1->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.0f, 0.5f)));
    item1->setSelectionBrushImage("TaharezLook/ListboxSelectionBrush");
    item1->setAutoDeleted(true);
    slist->addItem(item1);

    mPSrvs.push_back(filename_full);
    int ssize = mPSrvs.size();

    std::string tmpStr = "";

    for (int i = 0; i < ssize; i++) {
        char tmp[2];
        sprintf_s(tmp, 2, "%d", i);
        std::string pathnum = std::string(tmp);
        tmpStr += "PATH" + pathnum + "=" + mPSrvs[i];
        if (i != ssize - 1){ tmpStr += "\n";  }
    }
    
    char val[PRIVATE_PROFILE_SECTION_LEN * MAX_SERVICE_SIZE] = { 0 };
    sprintf(val, tmpStr.c_str());
    WritePrivateProfileSection("PLUGIN", val, mSettingPath);

    serviceItemWindow->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::startService, this));

    SetCurrentDirectory(curr);
    return true;
}

bool SgvMain::startService(const CEGUI::EventArgs &eventArgs)
{
    const CEGUI::WindowEventArgs windowEventArgs = static_cast<const CEGUI::WindowEventArgs&>(eventArgs);

    std::string fullPath = windowEventArgs.window->getUserString("FullPath").c_str();


    if (!startService(fullPath))
        MessageBox(NULL, _T("Cannot start service."), _T("Error"), MB_OKCANCEL); 
    return true;
}

bool SgvMain::startService(std::string fullpath)
{
    char curr[MAX_STRING_NUM];
    GetCurrentDirectory(MAX_STRING_NUM, curr);

    std::string tmp_path = fullpath;
    char *fpath = (char*)tmp_path.c_str();
    PathRemoveFileSpec(fpath);

    int ret = SetCurrentDirectory(fpath);
    if (ret == 0) {
        mLog->printf(Sgv::LogBase::ERR, "startService: cannot change directory. [%s]", fpath);
        return false;
    }

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (mConnectServer) {
        fullpath += " " + mHost + " " + mPort;
    }

    if (CreateProcess(NULL, (LPSTR)fullpath.c_str(), NULL, NULL, FALSE, 0,
        NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
    }

    SetCurrentDirectory(curr);
    return true;
}

bool SgvMain::editService(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *sheet = root_win; //->getChildRecursive("MainSheet");
    CEGUI::Window* svc_lst_win = root_win->getChildRecursive("Service_List");
    if (!wmgr.isAlive(svc_lst_win)) {
        CEGUI::Window *list = wmgr.createWindow("OgreTray/FrameWindow", "Service_List");
        list->setText("Service list");
        list->setSize(CEGUI::USize(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.5f, 0.0f)));
        list->setPosition(CEGUI::UVector2(CEGUI::UDim(0.3f, 0.0f), CEGUI::UDim(0.25f, 0.0f)));
        list->setAlpha(1.0f);

        CEGUI::Listbox *slist = static_cast<CEGUI::Listbox *>(wmgr.createWindow("OgreTray/Listbox", "Service_ServiceList"));
        slist->setText("Service List");
        slist->setSize(CEGUI::USize(CEGUI::UDim(0.9f, 0.0f), CEGUI::UDim(0.6f, 0.0f)));
        slist->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05f, 0.0f), CEGUI::UDim(0.05f, 0.0f)));

        CEGUI::Window *autorun = wmgr.createWindow("Vanilla/StaticText", "autorun");
        autorun->setSize(CEGUI::USize(CEGUI::UDim(0.6f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
        autorun->setPosition(CEGUI::UVector2(CEGUI::UDim(0.1f, 0.0f), CEGUI::UDim(0.68f, 0.0f)));
        autorun->setText("Start automatically");
        autorun->setProperty("FrameEnabled", "false");
        autorun->setProperty("BackgroundEnabled", "false");
        autorun->setProperty("HorzFormatting","HorzCenterd");

        CEGUI::ToggleButton *autorun_checkbox = static_cast<CEGUI::ToggleButton*>(wmgr.createWindow("OgreTray/Checkbox","autorun_check"));
        autorun_checkbox->setSize(CEGUI::USize(CEGUI::UDim(0.2f, 0.0f), CEGUI::UDim(0.11f, 0.0f)));
        autorun_checkbox->setPosition(CEGUI::UVector2(CEGUI::UDim(0.7f, 0.0f), CEGUI::UDim(0.68f, 0.0f)));

        int count = 0;
        while (1) {
            TCHAR pathText[MAX_STRING_NUM];

            char tmp[2];
            sprintf_s(tmp, 2, "%d", count);
            std::string pathnum = std::string(tmp);
            std::string key = "PATH" + pathnum;

            GetPrivateProfileString("PLUGIN",key.c_str(),'\0', pathText, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);

            if (pathText[0] == '\0') break;

            if (count >= MAX_SERVICE_SIZE)
            {
                break;
            }

            Ogre::String ServiceName = getFileName(pathText);

            CEGUI::ListboxTextItem *item1 = new CEGUI::ListboxTextItem(ServiceName);
            item1->setTextColours(CEGUI::ColourRect(CEGUI::Colour(0.0f, 0.0f, 0.5f)));
            item1->setSelectionBrushImage("TaharezLook/ListboxSelectionBrush");
            slist->addItem(item1);

            count++;
        }

        TCHAR archeck[8];

        GetPrivateProfileString("PLUGIN_OPTION","AUTO_RUN",'\0', archeck, PRIVATE_PROFILE_STRING_SIZE, mSettingPath);
        if (strcmp(archeck, "true") == 0) {
            autorun_checkbox->setSelected(true);
        }

        CEGUI::Window *addButton = wmgr.createWindow("OgreTray/Button", "Service_AddButton");
        addButton->setText("  Add");
        addButton->setSize(CEGUI::USize(CEGUI::UDim(0.2f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
        addButton->setPosition(CEGUI::UVector2(CEGUI::UDim(0.1f, 0.0f), CEGUI::UDim(0.85f, 0.0f)));
        addButton->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::addService, this));

        CEGUI::Window *removeButton = wmgr.createWindow("OgreTray/Button", "Service_RemoveButton");
        removeButton->setText("Remove");
        removeButton->setSize(CEGUI::USize(CEGUI::UDim(0.2f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
        removeButton->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0.0f), CEGUI::UDim(0.85f, 0.0f)));
        removeButton->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::removeService, this));

        CEGUI::Window *okButton = wmgr.createWindow("OgreTray/Button", "Service_OKButton");
        okButton->setText("OK");
        okButton->setSize(CEGUI::USize(CEGUI::UDim(0.2f, 0.0f), CEGUI::UDim(0.1f, 0.0f)));
        okButton->setPosition(CEGUI::UVector2(CEGUI::UDim(0.7f, 0.0f), CEGUI::UDim(0.85f, 0.0f)));

        okButton->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&SgvMain::okService, this));

        list->addChild(slist);
        list->addChild(autorun_checkbox);
        list->addChild(autorun);
        list->addChild(addButton);
        list->addChild(removeButton);
        list->addChild(okButton);

        sheet->addChild(list);
    }
    else{
        CEGUI::Window *slist = root_win->getChildRecursive("Service_List");
        slist->setVisible(true);
    }
    return true;
}

bool SgvMain::removeService(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Listbox *slist = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("Service_ServiceList"));
    int size = slist->getItemCount();
    for (int i = 0; i < size; i++) {
        CEGUI::ListboxItem *item = static_cast<CEGUI::ListboxItem*>(slist->getListboxItemFromIndex(i));
        if (item->isSelected()) {

            std::vector<std::string>::iterator it;
            it = mPSrvs.begin();

            while (it != mPSrvs.end()) {
                std::string tmpstr = item->getText().c_str();
                if (strstr((*it).c_str(), tmpstr.c_str()) != NULL) {
                    mPSrvs.erase(it);
                    break;
                }
                it++;
            }
            CEGUI::Window *swin = root_win->getChildRecursive(item->getText());
            CEGUI::Window *parent = swin->getParent();
            parent->removeChild(swin);

            slist->removeItem(item);

            break;
        }
    }

    int ssize = mPSrvs.size();

    std::string tmpStr = "";
    for (int i = 0; i < ssize; i++) {
        char tmp[2];
        sprintf_s(tmp, 2, "%d", i);
        std::string pathnum = std::string(tmp);
        tmpStr += "PATH" + pathnum + "=" + mPSrvs[i];
        if (i != ssize - 1){ tmpStr += "\n";  }
    }

    char val[PRIVATE_PROFILE_SECTION_LEN * MAX_SERVICE_SIZE] = { 0 };
    sprintf(val, tmpStr.c_str());
    WritePrivateProfileSection("PLUGIN", val, mSettingPath);

    return true;
}

bool SgvMain::okService(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
    CEGUI::Window *slist = root_win->getChildRecursive("Service_List");

    std::string tmpStr;
    CEGUI::ToggleButton *autorun_check = static_cast<CEGUI::ToggleButton*>(root_win->getChildRecursive("autorun_check"));
    if (autorun_check->isSelected()){
        tmpStr = "AUTO_RUN=true";
    }
    else{
        tmpStr = "AUTO_RUN=false";
    }

    char val[PRIVATE_PROFILE_SECTION_LEN] = { 0 };
    sprintf(val, tmpStr.c_str());

    WritePrivateProfileSection("PLUGIN_OPTION", val, mSettingPath);

    slist->setVisible(false);
    return true;
}

bool SgvMain::displayEntityData(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *main = root_win; //->getChildRecursive("MainSheet");
    
    if (!wmgr.isAlive(root_win->getChildRecursive("EntityDataWindow"))) {

        CEGUI::Window *ent = wmgr.createWindow("OgreTray/FrameWindow", "EntityDataWindow");
        ent->setText("Entity Data");
        ent->setSize(CEGUI::USize(CEGUI::UDim(0.35f, 0.0f), CEGUI::UDim(0.33f, 0.0f)));
        ent->setPosition(CEGUI::UVector2(CEGUI::UDim(0.64f, 0.0f), CEGUI::UDim(0.46f, 0.0f)));

        CEGUI::Window *entdata = wmgr.createWindow("OgreTray/Listbox", "EntityDataList");
        entdata->setSize(CEGUI::USize(CEGUI::UDim(1.0f, 0.0f), CEGUI::UDim(1.0f, 0.0f)));
        entdata->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.0f, 0.0f)));
        entdata->setInheritsAlpha(false);

        ent->addChild(entdata);
        main->addChild(ent);
    }
    else{
        CEGUI::Window *ent = root_win->getChildRecursive("EntityDataWindow");
        bool visible = ent->isVisible();

        if (visible) {
            ent->setVisible(false);

            if (mEntityData != NULL) {
                mEntityData->resetTransparency();
                mEntityData->setPositionMarkVisible(false);


                if (mEntityData->isRobot()) {
                    mEntityData->setJointPositionVisible(false);
                    mEntityData->setSegmentPositionVisible(false);
                    mEntityData->setCameraArrowVisible(false);
                    mEntityData->setCameraPositionVisible(false);
                    mSceneMgr->getEntity(mCurrentEntityName)->getParentSceneNode()->showBoundingBox(false);
                }
                else{
                    mEntityData->setBoundingBoxVisible(false);
                }
                mEntityData = NULL;
                mCurrentEntityName.clear();

            }
        }
        else{

            CEGUI::Listbox *eDataList = static_cast<CEGUI::Listbox *>(root_win->getChildRecursive("EntityDataList"));
            int dataSize = mEntityDataList.size();
            for (int i = 0; i < dataSize; i++) {
                eDataList->removeItem(mEntityDataList[i]);
            }
            mEntityDataList.clear();
            ent->setVisible(true);
        }
    }
    return true;
}

bool SgvMain::setMOV(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *mov = root_win->getChildRecursive("MeanOfVertex");
    CEGUI::Window *cov = root_win->getChildRecursive("CenterOfVertex");
    mov->setText("* Average vertices (old)");
    cov->setText("   Center of vertices (default)");
    mAlgEntityPos = 1;
    WritePrivateProfileSection("ALGORITHM", "EntityPosition=1", mSettingPath);
    return true;
}

bool SgvMain::setCOV(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* root_win = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    CEGUI::Window *mov = root_win->getChildRecursive("MeanOfVertex");
    CEGUI::Window *cov = root_win->getChildRecursive("CenterOfVertex");
    mov->setText("   Average vertices (old)");
    cov->setText("* Center of vertices (default)");
    mAlgEntityPos = 2;
    WritePrivateProfileSection("ALGORITHM", "EntityPosition=2", mSettingPath);
    return true;
}

bool SgvMain::checkRequestFromService()
{
    fd_set readfds;

    int n;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 100;

    FD_ZERO(&readfds);

    //mServices
    SOCKET sock;
    std::map<std::string, sigverse::SgvSocket*>::iterator it = mServices.begin();
    while (it != mServices.end()) {
        sock = (*it).second->getSocket();
        FD_SET(sock,&readfds);
        it++;
    }

    //n = select(sock+1, &readfds, NULL, NULL, &tv);
    n = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

    if (n == 0) {
        //printf("timeout\n");
        return true;
    }
    if (n == -1) {
        perror("select");
        return false;
    }

    //char buf[2048];

    it = mServices.begin();
    while (it != mServices.end()) {
        SOCKET sock = (*it).second->getSocket();
        if (FD_ISSET(sock, &readfds)) {
            char hbuf[4];
            memset(hbuf, 0, sizeof(hbuf));
            recv(sock, hbuf, sizeof(hbuf), 0);

            char *p = hbuf;
            unsigned short header = BINARY_GET_DATA_S_INCR(p, unsigned short);
            unsigned short dataSize = BINARY_GET_DATA_S_INCR(p, unsigned short);
            dataSize -= sizeof(unsigned short) * 2;

            /*
            char tmp[128];
            sprintf(tmp, "sock = %d, header = %d, dataSize = %d ",sock, header, dataSize);
            MessageBox( NULL, tmp, "Error", MB_OK);
            */

            char *rbuf;
            if (dataSize > 0) {
                rbuf = new char[dataSize];
                if (header <= 0 || header >= SV_REQUEST_SIZE || !(*it).second->recvData(rbuf, dataSize)) {
                    //MessageBox( NULL, "failed to get request from service", "Error", MB_OK);
                    delete (*it).second;
                    mServices.erase(it);
                    return false;
                }
            }
            p = rbuf;

            char *delim = ",";
            char *ctx;

            switch(header)
            {
                case SV_GET_IMAGE: 
                {
                    char *ename = strtok_s(p, delim, &ctx);

                    int id = atoi(strtok_s(NULL, delim, &ctx));

                    int type = atoi(strtok_s(NULL, delim, &ctx));

                    int result;
                    Ogre::Camera *cam;

                    Sgv::SgvEntity *ent = getSgvEntity(ename);
                    if (ent == NULL)   result = 2;

                    else{
                        Ogre::String cam_name = ent->getCameraName(id);

                        if (cam_name.size() <= 0) { result = 3;}

                        else{
                            result = 1;
                            cam = mSceneMgr->getCamera(cam_name);
                        }
                    }
                    int headerSize = sizeof(unsigned short) + sizeof(double)*2;

                    int dataSize;
                    if (result == 1) {
                        dataSize = 320 * 240 * 3 + headerSize;
                    }
                    else{
                        dataSize = headerSize;
                    }
                    unsigned char *bitImage = new unsigned char[dataSize];

                    if (result == 1) {
                        getImage(cam, bitImage, headerSize);
                    }
                    unsigned char *tmpPtr = bitImage;


                    double fov = 0.0;
                    double ar  = 0.0;

                    if (result == 1) {
                        fov = cam->getFOVy().valueRadians();

                        ar  = cam->getAspectRatio();
                    }

                    /*
                    char tmp[64];
                    sprintf(tmp, "sizeof(double) = %d",sizeof(double));
                    MessageBox( NULL, tmp, "Error", MB_OK);
                    */

                    BINARY_SET_DATA_S_INCR(tmpPtr, unsigned short, result);
                    BINARY_SET_DOUBLE_INCR(tmpPtr, fov);
                    BINARY_SET_DOUBLE_INCR(tmpPtr, ar);

                    if (!(*it).second->sendData((char*)bitImage, dataSize)) {
                        //MessageBox( NULL, _T("captureView: failed to send image"), _T("Error"), MB_OK);
                        delete [] bitImage;
                        delete [] rbuf;
                        return false;
                    }

                    delete [] bitImage;
                    break;
                }
                case SV_GET_DISTANCE: 
                {

                    double offset = BINARY_GET_DOUBLE_INCR(p);
                    double range = BINARY_GET_DOUBLE_INCR(p);

                    char *ename = strtok_s(p, delim, &ctx);

                    int id = atoi(strtok_s(NULL, delim, &ctx));

                    int result;
                    Ogre::Camera *cam;

                    Sgv::SgvEntity *ent = getSgvEntity(ename);
                    if (ent == NULL)   result = 2;

                    else{
                        Ogre::String cam_name = ent->getCameraName(id);

                        if (cam_name.size() <= 0) { result = 3;}

                        else{
                            result = 1;
                            cam = mSceneMgr->getCamera(cam_name);
                        }
                    }

                    int dataSize = sizeof(unsigned short) + 1;
                    unsigned char *bitImage = new unsigned char[dataSize];
                    unsigned char *p = bitImage;

                    BINARY_SET_DATA_S_INCR(p, unsigned short, result);

                    if (result == 1) {
                        getDistance(cam, p, offset, range);
                    }

                    if (!(*it).second->sendData((char*)bitImage, dataSize)) {
                        //MessageBox( NULL, _T("captureView: failed to send image"), _T("Error"), MB_OK);
                        delete [] bitImage;
                        delete [] rbuf;
                        return false;
                    }
                    delete [] bitImage;

                    /*
                    char tmp[64];
                    sprintf(tmp, "p = %d", p[0]);
                    MessageBox( NULL, tmp, "Error", MB_OK);
                    */
                    break;
                }
                case SV_DISCONNECT: 
                {
                    delete (*it).second;
                    mServices.erase(it);
                    return false;
                }
                case SV_GET_DISTANCEIMAGE: 
                {
                    double offset = BINARY_GET_DOUBLE_INCR(p);
                    double range = BINARY_GET_DOUBLE_INCR(p);

                    char *ename = strtok_s(p, delim, &ctx);

                    int dimension = atoi(strtok_s(NULL, delim, &ctx));

                    int id = atoi(strtok_s(NULL, delim, &ctx));

                    int size = atoi(strtok_s(NULL, delim, &ctx));

                    int result;
                    Ogre::Camera *cam;

                    Sgv::SgvEntity *ent = getSgvEntity(ename);
                    if (ent == NULL)   result = 2;

                    else{
                        Ogre::String cam_name = ent->getCameraName(id);

                        if (cam_name.size() <= 0) { result = 3;}

                        else{
                            result = 1;
                            cam = mSceneMgr->getCamera(cam_name);
                        }
                    }

                    int sendSize;

                    if (result != 1) {
                        sendSize = sizeof(unsigned short) + sizeof(double)*2;
                    }
                    else{
                        int headerSize = sizeof(unsigned short) + sizeof(double)*2;

                        if (dimension == 1) {sendSize = 320 + headerSize;}
                        else if (dimension == 2) {sendSize = 320 * 240 + headerSize;}
                    }

                    unsigned char *sendImage = new unsigned char[sendSize];
                    unsigned char *p = sendImage;


                    BINARY_SET_DATA_S_INCR(p, unsigned short, result);

                    double fov = 0.0;
                    double ar  = 0.0;

                    if (result == 1) {

                        fov = cam->getFOVy().valueRadians();

                        ar  = cam->getAspectRatio();
                    }
                    BINARY_SET_DOUBLE_INCR(p, fov);
                    BINARY_SET_DOUBLE_INCR(p, ar);

                    if (result == 1)
                        getDistanceImage(cam, p, offset, range, dimension);


                    if (!(*it).second->sendData((char*)sendImage, sendSize)) {
                        delete [] sendImage;
                        delete [] rbuf;
                        return false;
                    }
                    delete [] sendImage;
                    break;

                    //char tmp[128];
                    //sprintf(tmp, "%s, %d, %d, %d", ename, dim, id, size);
                    //MessageBox( NULL, tmp, "Error", MB_OK);
                }
                case SV_GET_DEPTHIMAGE: 
                {
                    double offset = BINARY_GET_DOUBLE_INCR(p);
                    double range = BINARY_GET_DOUBLE_INCR(p);

                    char *ename = strtok_s(p, delim, &ctx);

                    int id = atoi(strtok_s(NULL, delim, &ctx));

                    int size = atoi(strtok_s(NULL, delim, &ctx));

                    int result;
                    Ogre::Camera *cam;

                    Sgv::SgvEntity *ent = getSgvEntity(ename);
                    if (ent == NULL)   result = 2;

                    else{
                        Ogre::String cam_name = ent->getCameraName(id);

                        if (cam_name.size() <= 0) { result = 3;}

                        else{
                            result = 1;
                            cam = mSceneMgr->getCamera(cam_name);
                        }
                    }
                    int sendSize;

                    if (result != 1) {
                        sendSize = sizeof(unsigned short) + sizeof(double)*2;
                    }
                    else{
                        int headerSize = sizeof(unsigned short) + sizeof(double)*2;
                        sendSize = 320 * 240 + headerSize;
                    }

                    unsigned char *sendImage = new unsigned char[sendSize];
                    unsigned char *p = sendImage;


                    BINARY_SET_DATA_S_INCR(p, unsigned short, result);

                    double fov = 0.0;
                    double ar  = 0.0;

                    if (result == 1) {

                        fov = cam->getFOVy().valueRadians();

                        ar  = cam->getAspectRatio();
                    }
                    BINARY_SET_DOUBLE_INCR(p, fov);
                    BINARY_SET_DOUBLE_INCR(p, ar);

                    if (result == 1)
                        getDepthImage(cam, p, offset, range);

                    if (!(*it).second->sendData((char*)sendImage, sendSize)) {
                        delete [] sendImage;
                        delete [] rbuf;
                        return false;
                    }
                    delete [] sendImage;
                    break;

                }
            } // switch (header)
            delete [] rbuf;
        }
        it++;
    }
    //MessageBox(NULL, _T("check!"), _T("Attention"), MB_OKCANCEL);

    return true;
}

bool SgvMain::getImage(Ogre::Camera *cam, unsigned char *image, int headSize,  ColorBitType ctype, ImageType itype)
{
    //Ogre::Camera *cam = mSceneMgr->getCamera(cameraName);

    Ogre::Vector3 pos = cam->getPosition();
    Ogre::Quaternion ori = cam->getOrientation();

    Ogre::Real ar = cam->getAspectRatio();
    Ogre::Radian fov = cam->getFOVy();

    Ogre::Vector3 lpos = cam->getParentNode()->convertLocalToWorldPosition(pos);

    Ogre::Quaternion orient = cam->getParentNode()->convertLocalToWorldOrientation(ori);

    mCaptureCamera->setPosition(lpos);
    mCaptureCamera->setOrientation(orient);
    mCaptureCamera->setAspectRatio(ar);
    mCaptureCamera->setFOVy(fov);

    mTex->getBuffer()->getRenderTarget()->update();
    Ogre::HardwarePixelBufferSharedPtr buf = mTex->getBuffer();

    buf->blitToMemory(
        Box(0, 0, 0, 320, 240, 1),
        PixelBox(320, 240, 1, Ogre::PF_R8G8B8, image + headSize));

    return true;
}

bool SgvMain::getDistance(Ogre::Camera *cam, unsigned char *distance, double offset, double range, ColorBitType ctype)
{
    setCameraForDistanceSensor(cam);

    Ogre::MaterialPtr mptr = Ogre::MaterialManager::getSingleton().getByName("depth_material");
    Ogre::GpuProgramParametersSharedPtr gpuparams = mptr->getTechnique(0)->getPass(0)->getVertexProgramParameters();

    gpuparams->setNamedConstant("depthOffset", Ogre::Real(offset));
    gpuparams->setNamedConstant("coefficient", Ogre::Real(255.0f/range));

    setDepthView(true);

    mDstTex->getBuffer()->getRenderTarget()->update();
    Ogre::HardwarePixelBufferSharedPtr buf = mDstTex->getBuffer();

    // for blit 
    char tmpImage[3];

    // blit
    buf->blitToMemory(
        Box(160, 120, 0, 161, 121, 1),
        PixelBox(1, 1, 1, Ogre::PF_R8G8B8, tmpImage));

    *distance = tmpImage[0];
    setDepthView(false);
    return true;
}

bool SgvMain::getDistanceImage(Ogre::Camera *cam, unsigned char *distance, double offset, double range, int dimension, ColorBitType ctype)
{
    setCameraForDistanceSensor(cam);

    Ogre::MaterialPtr mptr = Ogre::MaterialManager::getSingleton().getByName("depth_material");
    Ogre::GpuProgramParametersSharedPtr gpuparams = mptr->getTechnique(0)->getPass(0)->getVertexProgramParameters();

    gpuparams->setNamedConstant("depthOffset", Ogre::Real(offset));
    gpuparams->setNamedConstant("coefficient", Ogre::Real(255.0f/range));

    setDepthView(true);

    mDstTex->getBuffer()->getRenderTarget()->update();
    Ogre::HardwarePixelBufferSharedPtr buf = mDstTex->getBuffer();

    Ogre::Radian fovy = cam->getFOVy();

    Ogre::Real ar = cam->getAspectRatio();

    Ogre::Radian fovx = Ogre::Radian(2*atan(tan(fovy.valueRadians()*0.5)*ar));

    if (dimension == 1) {
        // for blit 
        unsigned char *tmpImage = new unsigned char[320*3];

        mDstTex->getBuffer()->getRenderTarget()->update();
        Ogre::HardwarePixelBufferSharedPtr buf = mDstTex->getBuffer();

        // blit
        buf->blitToMemory(
            Box(0, 120, 0, 320, 121, 1),
            PixelBox(320, 1, 1, Ogre::PF_R8G8B8, tmpImage));

        Ogre::Radian deltax = fovx / 319;

        for (int i = 0; i < 320; i++) {

            double theta = i * deltax.valueRadians() - (fovx.valueRadians()/2.0);

            int pos = (int)((tan(M_PI/2.0 - fovx.valueRadians()/2.0)/tan(M_PI/2.0 - theta) + 1.0)*160.0);

            if (pos == 320) pos--;

            unsigned char data = tmpImage[pos*3];

            Ogre::Real offset_diff = 256/range*(offset/cos(theta) - offset);

            int tmp2 = int(data/cos(theta) + offset_diff + 0.5);

            if (tmp2 > 255)
                distance[i] = 255;
            else
                distance[i] = tmp2;
        } // for (int i = 0; i < 320; i++) {
        delete [] tmpImage;
    } // if (dimension == 1)
    else if (dimension == 2) {
        double fovy_start = atan(tan(fovy.valueRadians()/2)*cos(fovx.valueRadians()/2))*2;

        double x = fovy.valueRadians() / fovy_start;

        Ogre::Radian fovy_ = fovy*x;

        mDistanceCamera->setFOVy(fovy_);

        mDistanceCamera->setAspectRatio(tan(fovx.valueRadians()*0.5)/tan(fovy_.valueRadians()*0.5));

        // for blit 
        unsigned char *tmpImage = new unsigned char[320*240*3];

        mDstTex->getBuffer()->getRenderTarget()->update();
        Ogre::HardwarePixelBufferSharedPtr buf = mDstTex->getBuffer();

        // blit
        buf->blitToMemory(
            Box(0, 0, 0, 320, 240, 1),
            PixelBox(320, 240, 1, Ogre::PF_R8G8B8, tmpImage));

        Ogre::Radian deltax = fovx / 319;
        Ogre::Radian deltay = fovy / 239;

        double height = tan(fovy_.valueRadians()/2)*2;

        for (int i = 0; i < 320; i++) {

            double phi = i * deltax.valueRadians() - (fovx.valueRadians()/2.0);

            double posx_tmp = tan(M_PI/2.0 - fovx.valueRadians()/2.0)/tan(M_PI/2.0 - phi)*160.0;

            int posx = (int)(posx_tmp + 0.5 + 160.0);

            if (posx == 320) posx--;

            double disx = tan(phi);

            double fovy_tmp = atan(tan(fovy_.valueRadians()/2)*cos(phi))*2;

            /*
            char tmp[128];
            sprintf(tmp, "fovy_start  (%f) ", fovy_tmp * 180/M_PI);
            MessageBox( NULL, tmp, "Error", MB_OK);
            */

            for (int j = 0; j < 240; j++) {
                double theta = j * deltay.valueRadians() - (fovy.valueRadians()/2.0);

                double posy_tmp = tan(M_PI/2.0 - fovy_tmp/2.0)/tan(M_PI/2.0 - theta);

                /*
                char tmp[128];
                sprintf(tmp, "fovy_tmp  (%f) ", posy_tmp);
                MessageBox( NULL, tmp, "Error", MB_OK);
                */

                int tmp_pos = 320*j+i;

                if (abs(posy_tmp) > 1.0) {

                    distance[tmp_pos] = 255;
                    continue;
                }

                double disy = posy_tmp*height/2;

                int posy = (int)(posy_tmp*120.0 + 0.5 + 120.0);

                if (posy == 240) posy--;

                double dist = sqrt(disx*disx + disy*disy);

                double angle = atan(dist);

                unsigned char data = tmpImage[320*3*posy+3*posx];

                Ogre::Real offset_diff = 256/range*(offset/cos(angle) - offset);

                int tmp2 = int(data/cos(angle) + offset_diff + 0.5);

                if (tmp2 > 255)
                    distance[tmp_pos] = 255;
                else
                    distance[tmp_pos] = tmp2;
            }
        }
        delete [] tmpImage;
    } // if (dimension == 2)
    setDepthView(false);
    return true;
}

bool SgvMain::getDepthImage(Ogre::Camera *cam, unsigned char *distance, double offset, double range, ColorBitType ctype, ImageType itype)
{
    setCameraForDistanceSensor(cam);

    Ogre::MaterialPtr mptr = Ogre::MaterialManager::getSingleton().getByName("depth_material");
    Ogre::GpuProgramParametersSharedPtr gpuparams = mptr->getTechnique(0)->getPass(0)->getVertexProgramParameters();

    gpuparams->setNamedConstant("depthOffset", Ogre::Real(offset));
    gpuparams->setNamedConstant("coefficient", Ogre::Real(255.0f/range));

    setDepthView(true);

    Ogre::Radian fovy = cam->getFOVy();
    Ogre::Real ar = cam->getAspectRatio();

    Ogre::Radian fovx = Ogre::Radian(2*atan(tan(fovy.valueRadians()*0.5)*ar));

    // for blit 
    unsigned char *tmpImage = new unsigned char[320*240*3];

    mDstTex->getBuffer()->getRenderTarget()->update();
    Ogre::HardwarePixelBufferSharedPtr buf = mDstTex->getBuffer();

    // blit
    buf->blitToMemory(
        Box(0, 0, 0, 320, 240, 1),
        PixelBox(320, 240, 1, Ogre::PF_R8G8B8, tmpImage));

    for (int i = 0; i < 240; i++) {
        for (int j = 0; j < 320; j++) {

            distance[i*320+j] = tmpImage[i*320*3+j*3];
        }
    }
    delete [] tmpImage;
    setDepthView(false);
    return true;
}

bool SgvMain::setCameraForDistanceSensor(Ogre::Camera *cam)
{
    Ogre::Real ar = cam->getAspectRatio();

    Ogre::Radian fovy = cam->getFOVy();

    Ogre::Radian fovy_;

    Ogre::Vector3 lpos = cam->getRealPosition();

    Ogre::Quaternion orient = cam->getRealOrientation();

    mDistanceCamera->setPosition(lpos);
    mDistanceCamera->setOrientation(orient);

    mDistanceCamera->setAspectRatio(ar);

    mDistanceCamera->setFOVy(fovy);

    return true;
}
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
 
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT )
#else
    int main(int argc, char *argv[])
#endif
    {
        // Create application object
        SgvMain app;
 
        try 
        {
            app.go();
        }
        catch( Ogre::Exception& e ) {
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
            MessageBox( NULL, e.getFullDescription().c_str(), "An exception has occured!", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
            std::cerr << "An exception has occured: " << e.getFullDescription().c_str() << std::endl;
#endif
        }
        
        return 0;
    }
 
#ifdef __cplusplus
}
#endif
