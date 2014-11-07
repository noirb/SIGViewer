#ifndef __SgvMain_h_
#define __SgvMain_h_
 
#include "BaseApplication.h"
#include "SgvSocket.h"
#include "SgvEntity.h"
#include "SgvX3D.h"
#include "ViewerService.h"

#include <stdio.h> 
#include <map>
#include <CEGUI.h>
#include <RendererModules/Ogre/CEGUIOgreRenderer.h>
#include <libssh2.h>

class SgvMain : public BaseApplication
{
public:
	SgvMain(void);
	virtual ~SgvMain(void);

	// Type of request to server
	enum RequestType{
		GET_ALL_ENTITIES  = 1, // get all entity data.
		START             = 2, // simulation start
		STOP              = 3, // simulation stop
		GET_MOVE_ENTITIES = 4, // update entities
		DOWNLOAD          = 5, // download shape file.
		DISCONNECT        = 6, // disconnect to server
		QUIT              = 7, // quit simulation
	};

	// Type of request from service provider
	enum ServiceRequestType{
		SV_NODATA            = 0, 
		SV_GET_IMAGE         = 1,  // captureView
		SV_GET_DISTANCE      = 2,  // distanceSensor
		SV_DISCONNECT        = 3,  // disconnect
		SV_GET_DISTANCEIMAGE = 4,  // distanceImage
		SV_GET_DEPTHIMAGE    = 5,  // depthImage
		SV_REQUEST_SIZE   ,
	};

	// Type of image
	enum ImageType{
		IM_320X240 = 0,
	};

	// Type of bit depth
	enum ColorBitType{
		COLORBIT_24 = 0,
		DEPTHBIT_8  = 1,
	};

protected:
	CEGUI::OgreRenderer* mRenderer;
 
	virtual void createScene(void);
	virtual void destroyScene(void);
 
	virtual void chooseSceneManager(void);
	virtual void createFrameListener(void);
 
	// Ogre::FrameListener
	virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);
 
	// OIS::KeyListener
	virtual bool keyPressed( const OIS::KeyEvent &arg );
	virtual bool keyReleased( const OIS::KeyEvent &arg );
	// OIS::MouseListener
	virtual bool mouseMoved( const OIS::MouseEvent &arg );
	virtual bool mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
	virtual bool mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id );

	virtual void windowResized(Ogre::RenderWindow* rw);

	bool quit(const CEGUI::EventArgs &e);

	bool connect(const CEGUI::EventArgs &e);

	bool disconnect(const CEGUI::EventArgs &e);

	bool closeRecvMessageTray(const CEGUI::EventArgs &e);

	bool closeSendMessageTray(const CEGUI::EventArgs &e);

	bool addService(const CEGUI::EventArgs &e);

	bool editService(const CEGUI::EventArgs &e);

	bool removeService(const CEGUI::EventArgs &e);

	bool okService(const CEGUI::EventArgs &e);

	//! Create entity after receiving all the entity data from server
	bool createAllEntities();   

	void closeChAndSock(LIBSSH2_CHANNEL *channel, SOCKET sock);

	//! Get SIGVerse entity class
	Sgv::SgvEntity *getSgvEntity(std::string name){
		std::map<std::string, Sgv::SgvEntity*>::iterator it = mAllEntities.find(name);
		if(it == mAllEntities.end()) return NULL;
		else return (*it).second;
	}

	//! Send a request of simulation start to the server
	bool startRequest(const CEGUI::EventArgs &e);

	//! Use agent's view as main view
	bool agentView1(const CEGUI::EventArgs &e);
	bool agentView2(const CEGUI::EventArgs &e);
	bool agentView3(const CEGUI::EventArgs &e);
	bool agentView4(const CEGUI::EventArgs &e);

	//! Create event handler for each service provider
	//TODO: Are there any smarter method?
	bool cameraView_Down  (const CEGUI::EventArgs &e);
	bool cameraView_Move  (const CEGUI::EventArgs &e);
	bool cameraView_Up    (const CEGUI::EventArgs &e);
	bool cameraView_Sized (const CEGUI::EventArgs &e);
	bool cameraView_Sizing(const CEGUI::EventArgs &e);

	int cameraView_Select(const CEGUI::EventArgs &e, int *result);

	bool startService1(const CEGUI::EventArgs &e);
	bool startService2(const CEGUI::EventArgs &e);
	bool startService3(const CEGUI::EventArgs &e);
	bool startService4(const CEGUI::EventArgs &e);
	bool startService5(const CEGUI::EventArgs &e);
	bool startService6(const CEGUI::EventArgs &e);

	bool startService(std::string fullpath);

	//! Toggle the flag of ssh connection
	bool sshCheckONOFF(const CEGUI::EventArgs &e);

	//! Event handler of switching sub view
	bool subView(const CEGUI::EventArgs &e);

	//! Event handler of over-writing function of shape file
	bool overwriteShape(const CEGUI::EventArgs &e);

	//! Event handler to show geometry shapes for dynamics calculation
	bool dynamicsView(const CEGUI::EventArgs &e);

	//! Change time unit
	bool changeTimeUnit(const CEGUI::EventArgs &e);

	//! Create Message Tray
	bool messageTray(const CEGUI::EventArgs &e);

	//! Sending text message
	bool sendMessage(const CEGUI::EventArgs &e);

	//! Display entity data
	bool displayEntityData(const CEGUI::EventArgs &e);

	//! Set the mean of vertex as entity position
	bool setMOV(const CEGUI::EventArgs &e);

	//! Set the center of vertex as entity position
	bool setCOV(const CEGUI::EventArgs &e);

	//! Send request to server
	bool sendRequest(RequestType type);

	//! Receive entity data which has been modified or added
	bool recvMoveEntities();

	//! Request to download shape file
	bool downloadFileRequest(std::string name);

private:
	//! Change to depth view
	void setDepthView(bool depth);

	//! Creation of window
	void createInitWindow();

	void acceptFromService();

	//! Get image data; send it to controller
	bool captureView();

	//! Get distance data; send it to controller
	bool distanceSensor();

	//! Find all the entities in a view; send them to controller
	bool detectEntities();

	//! Capture an image by camera
	bool getImage(Ogre::Camera *cam, unsigned char *image, int headSize = 4, ColorBitType ctype = COLORBIT_24, ImageType viewType = IM_320X240);

	//! Measure distance toward direction of camera
	bool getDistance(Ogre::Camera *cam, unsigned char *distance, double offset, double range, ColorBitType ctype = DEPTHBIT_8);

	//! Measure distance image toward direction of camera
	bool getDistanceImage(Ogre::Camera *cam, unsigned char *distance, double offset, double range, int dimension, ColorBitType ctype = DEPTHBIT_8);

	//! Get Z-buffer image
	bool getDepthImage(Ogre::Camera *cam, unsigned char *distance, double offset, double range, ColorBitType ctype = DEPTHBIT_8, ImageType viewType = IM_320X240);

	//! Set the normal image camera as a camera for distance sensor
	bool setCameraForDistanceSensor(Ogre::Camera *cam);

	//------------------------------------------------------
	// @brief SSH login
	// @param uname  user name
	// @param key1   Full path strig which shows the public key
	// @param key2   Full path strig which shows the private key
	// @param pass   Password
	// @param host   Hostname
	//------------------------------------------------------
	LIBSSH2_SESSION *sshLogin(const char *uname, const char *key1, const char *key2, const char *p\
ass, const char *host);

	//------------------------------------------------------
	// @brief  data transfer via ssh port forwarding
	// @param  localport  local port number
	// @param  remoteport remote port number
	// @param  listenhost host name of relay server
	//------------------------------------------------------
	void sshPortForwarding(unsigned int localport, unsigned int remoteport, const char *listenhost);


	//------------------------------------------------------
	// @brief data transfer through ssh port forwarding
	// @param forwardsock  socket for relay server to be listened
	// @param channel      channel for data transfer
	// @param id           ID of channel
	//------------------------------------------------------
	void transportData(int id);

	bool checkRequestFromService();

protected:
	Ogre::RaySceneQuery *mRaySceneQuery;// The ray scene query pointer
	bool mLMouseDown, mRMouseDown;      // True if it clicked
	bool mShift;                        // True if shift is pressed
	bool mCtrl;                         // True if Ctrl is pressed
	int  mCount;                        // The number of robots on the screen
	Ogre::SceneNode *mCurrentObject;    // pointer to our currently selected object
	Ogre::SceneNode *mHeadNode;
	float mRotateSpeed;
	float mMoveXYSpeed;
	float mMoveZSpeed;
	bool  mConnectServer;               // Whether this is connected to server or not
	bool  mSimRun;                      // Whether the simulation has been started
	bool  mSubView;                     // Whether the subwindow should be displayed or not
	std::string mHost;
	std::string mPort;
	CEGUI::Renderer *mGUIRenderer;      // our CEGUI renderer

	bool mMove;
	int  mTidx;
	int  mBm[2];

	// Socket to receive entity data 
	sigverse::SgvSocket *mSock;

	// Services held by SIGViewer
	Sgv::ViewerService *mService;

	// All the entities
	std::map<std::string ,Sgv::SgvEntity*> mAllEntities;

	Sgv::X3D *m_pX3D;

	std::vector<Ogre::Viewport*> mViews;

	//! Subwindow
	std::vector<CEGUI::Window*> mSubWindows;

	//! Window for superimpose
	std::vector<CEGUI::Window*> mSubViews;

	CEGUI::Window *mTelop;
	//CEGUI::Listbox *mTelop;

	//! listbox text for SendMesssage
	std::map<CEGUI::String, CEGUI::ListboxTextItem*> mMsgList; 

	//! Item list for entity data display
	std::vector<CEGUI::ListboxTextItem*> mEntityDataList; 

	//! Creation of log
	Sgv::LogPtr mLog;

	//std::vector<CEGUI::String> mElseUser;
	std::list<CEGUI::String> mElseUsers;

	//! Map between connected service name and socket
	std::map<std::string, sigverse::SgvSocket*> mServices;

	//! Socket for receive services
	sigverse::SgvSocket *mRecvService; 

	//! Texture pointer for camera image capture
	Ogre::TexturePtr mTex;

	//! Texture pointer for distance sensor
	Ogre::TexturePtr mDstTex;

	//! Camera for image capture
	Ogre::Camera  *mCaptureCamera;

	//! Camera for distance sensor
	Ogre::Camera  *mDistanceCamera;

	//! Camera for OcurusRift
	Ogre::Camera  *OculusCamera;

	//Ogre::Camera  *mDetectCamera;

	//! Ground (Grid)
	Ogre::ManualObject* mPlane;

	//! Origin of coordinate
	Ogre::ManualObject* mAxis;

	//! Ground
	Ogre::Entity *mGround;

	//! Flag whether entity data request is sending to server or not
	bool mSended;

	//! for detectEntities
	Ogre::PlaneBoundedVolumeListSceneQuery *m_VolQuery;

	//! Socket for SSH connection
	SOCKET m_sshSock;

	//! SSH connection session between server
	LIBSSH2_SESSION *m_session;

	//! Flag whether SSH connection is used or not
	bool mSSHConnect;

	//! Services as plug-in
	std::vector<std::string> mPSrvs;

	//! File path name of configuration
	TCHAR mSettingPath[MAX_STRING_NUM];

	//! Socket for port forwarding (Used at relay server)
	std::vector<SOCKET> m_transSocks;

	//! SSH channel for port forwarding (Used at relay server)
	std::vector<LIBSSH2_CHANNEL*> m_transChannels;

	//! Whether dynamics geometry display mode or not
	int m_dynamicsView;

	//! ODE shape
	std::vector<Ogre::SceneNode*> m_ODENodes;

	//! Whether overwrite of shape file is allowed or not
	bool mOverWrite;

	//! Entities which data is showing now
	Sgv::SgvEntity* mEntityData;

	//! Method to set the position of entity
	// 1 Average vertices (old)  
	// 2 Center of Vertices (default)
	int mAlgEntityPos;

	//! Entity name which is focused now
	Ogre::String mCurrentEntityName;

	//! Arrow for camera
	std::vector<Ogre::ManualObject*>  mCameraArrows;

};
#endif // #ifndef __SgvMain_h_

