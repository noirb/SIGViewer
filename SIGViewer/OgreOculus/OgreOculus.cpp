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
using namespace OVR;

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
	ovrHmd_Destroy(mHMD);
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


	// Configure Render Textures:
	Sizei recommendedTex0Size = ovrHmd_GetFovTextureSize( mHMD, ovrEye_Left,
			mHMD->DefaultEyeFov[0], 1.0f );
	Sizei recommendedTex1Size = ovrHmd_GetFovTextureSize( mHMD, ovrEye_Right,
			mHMD->DefaultEyeFov[1], 1.0f );

	// Generate a texture for each eye, as a rendertarget:
	mLeftEyeRenderTexture = Ogre::TextureManager::getSingleton().createManual(
			"RiftRenderTextureLeft", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
			Ogre::TEX_TYPE_2D, recommendedTex0Size.w, recommendedTex0Size.h, 0, Ogre::PF_X8B8G8R8,
			Ogre::TU_RENDERTARGET );
	mRightEyeRenderTexture = Ogre::TextureManager::getSingleton().createManual(
			"RiftRenderTextureRight", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
			Ogre::TEX_TYPE_2D, recommendedTex1Size.w, recommendedTex1Size.h, 0, Ogre::PF_X8B8G8R8,
			Ogre::TU_RENDERTARGET );

	// Assign the textures to the eyes used later:
	mMatLeft = Ogre::MaterialManager::getSingleton().getByName( "Oculus/LeftEye" );
	mMatLeft->getTechnique(0)->getPass(0)->getTextureUnitState(0)->
		setTexture( mLeftEyeRenderTexture );
		
	mMatRight = Ogre::MaterialManager::getSingleton().getByName( "Oculus/RightEye" );
	mMatRight->getTechnique(0)->getPass(0)->getTextureUnitState(0)->
		setTexture( mRightEyeRenderTexture );
		
	ovrEyeRenderDesc eyeRenderDesc[2];

	eyeRenderDesc[0] = ovrHmd_GetRenderDesc( mHMD, ovrEye_Left, mHMD->DefaultEyeFov[0] );
	eyeRenderDesc[1] = ovrHmd_GetRenderDesc( mHMD, ovrEye_Right, mHMD->DefaultEyeFov[1] );

	ovrVector2f UVScaleOffset[2];
	ovrRecti viewports[2];
	viewports[0].Pos.x = 0;
	viewports[0].Pos.y = 0;
	viewports[0].Size.w = recommendedTex0Size.w;
	viewports[0].Size.h = recommendedTex0Size.h;
	viewports[1].Pos.x = recommendedTex0Size.w;
	viewports[1].Pos.y = 0;
	viewports[1].Size.w = recommendedTex1Size.w;
	viewports[1].Size.h = recommendedTex1Size.h;
	Ogre::ManualObject* manual;
	// Create the Distortion Meshes:
	for ( int eyeNum = 0; eyeNum < 2; eyeNum ++ )
	{
		ovrDistortionMesh meshData;
		ovrHmd_CreateDistortionMesh( mHMD,
				eyeRenderDesc[eyeNum].Eye,
				eyeRenderDesc[eyeNum].Fov,
				0,
				&meshData );

		Ogre::GpuProgramParametersSharedPtr params;

		if( eyeNum == 0 )
		{
			ovrHmd_GetRenderScaleAndOffset( eyeRenderDesc[eyeNum].Fov,
					recommendedTex0Size, viewports[eyeNum],
					UVScaleOffset);
			params = mMatLeft->getTechnique(0)->getPass(0)->getVertexProgramParameters();
		} else {
			ovrHmd_GetRenderScaleAndOffset( eyeRenderDesc[eyeNum].Fov,
					recommendedTex1Size, viewports[eyeNum],
					UVScaleOffset);
			params = mMatRight->getTechnique(0)->getPass(0)->getVertexProgramParameters();
		}

		params->setNamedConstant( "eyeToSourceUVScale",
				Ogre::Vector4( UVScaleOffset[0].x, UVScaleOffset[0].y,0,0 ) );
		params->setNamedConstant( "eyeToSourceUVOffset",
				Ogre::Vector4( UVScaleOffset[1].x, UVScaleOffset[1].y,0,0 ) );

		std::cout << "UVScaleOffset[0]: " << UVScaleOffset[0].x << ", " << UVScaleOffset[0].y << std::endl;
		std::cout << "UVScaleOffset[1]: " << UVScaleOffset[1].x << ", " << UVScaleOffset[1].y << std::endl;

		// create ManualObject
		// TODO: Destroy the manual objects!!
			
		if( eyeNum == 0 )
		{
			manual = mSceneMgr->createManualObject("RiftRenderObjectLeft");
			manual->begin("Oculus/LeftEye", Ogre::RenderOperation::OT_TRIANGLE_LIST);
			//manual->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);
		}
		else
		{
			manual = mSceneMgr->createManualObject("RiftRenderObjectRight");
			manual->begin("Oculus/RightEye", Ogre::RenderOperation::OT_TRIANGLE_LIST);
			//manual->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);
		}

		for( unsigned int i = 0; i < meshData.VertexCount; i++ )
		{
			ovrDistortionVertex v = meshData.pVertexData[i];
			manual->position( v.ScreenPosNDC.x,
					v.ScreenPosNDC.y, 0 );
			manual->textureCoord( v.TanEyeAnglesR.x,//*UVScaleOffset[0].x + UVScaleOffset[1].x,
					v.TanEyeAnglesR.y);//*UVScaleOffset[0].y + UVScaleOffset[1].y);
			manual->textureCoord( v.TanEyeAnglesG.x,//*UVScaleOffset[0].x + UVScaleOffset[1].x,
					v.TanEyeAnglesG.y);//*UVScaleOffset[0].y + UVScaleOffset[1].y);
			manual->textureCoord( v.TanEyeAnglesB.x,//*UVScaleOffset[0].x + UVScaleOffset[1].x,
					v.TanEyeAnglesB.y);//*UVScaleOffset[0].y + UVScaleOffset[1].y);
			//float vig = std::max( v.VignetteFactor, (float)0.0 );
			//manual->colour( vig, vig, vig, vig );
			manual->colour( 1, 1, 1, 1 );

		}
		for( unsigned int i = 0; i < meshData.IndexCount; i++ )
		{
			manual->index( meshData.pIndexData[i] );
		}

		// tell Ogre, your definition has finished
		manual->end();

		ovrHmd_DestroyDistortionMesh( &meshData );

		meshNode->attachObject( manual );
	}
	
	// Set up IPD in meters:
	mIPD = ovrHmd_GetFloat(mHMD, OVR_KEY_IPD,  0.064f);
	
	// Set a default value for interpupillary distance:
	mIPD = 0.064f;
	
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
	



	ovrFovPort fovLeft = mHMD->DefaultEyeFov[ovrEye_Left];
	ovrFovPort fovRight = mHMD->DefaultEyeFov[ovrEye_Right];

	float combinedTanHalfFovHorizontal = std::max( fovLeft.LeftTan, fovLeft.RightTan );
	float combinedTanHalfFovVertical = std::max( fovLeft.UpTan, fovLeft.DownTan );

	float aspectRatio = combinedTanHalfFovHorizontal / combinedTanHalfFovVertical;

	m_cameras[0]->setAspectRatio( aspectRatio );
	m_cameras[1]->setAspectRatio( aspectRatio );

	ovrMatrix4f projL = ovrMatrix4f_Projection ( fovLeft, g_defaultNearClip, g_defaultFarClip, true );
	ovrMatrix4f projR = ovrMatrix4f_Projection ( fovRight, g_defaultNearClip, g_defaultFarClip, true );

	m_cameras[0]->setCustomProjectionMatrix( true,
			Ogre::Matrix4( 
				projL.M[0][0], projL.M[0][1], projL.M[0][2], projL.M[0][3],
				projL.M[1][0], projL.M[1][1], projL.M[1][2], projL.M[1][3],
				projL.M[2][0], projL.M[2][1], projL.M[2][2], projL.M[2][3],
				projL.M[3][0], projL.M[3][1], projL.M[3][2], projL.M[3][3] ) );
	m_cameras[1]->setCustomProjectionMatrix( true,
			Ogre::Matrix4( 
				projR.M[0][0], projR.M[0][1], projR.M[0][2], projR.M[0][3],
				projR.M[1][0], projR.M[1][1], projR.M[1][2], projR.M[1][3],
				projR.M[2][0], projR.M[2][1], projR.M[2][2], projR.M[2][3],
				projR.M[3][0], projR.M[3][1], projR.M[3][2], projR.M[3][3] ) );
}



bool Oculus::InitOculusVR()
{
	if (!ovr_Initialize())
	{
		Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Initalization for LibOVR is failed.");
		return false;
	}//if
		
	mHMD = ovrHmd_Create(0);
	if (nullptr == mHMD) 
	{
		mHMD = ovrHmd_CreateDebug(ovrHmd_DK2);
	}
	if (!mHMD)
	{
		Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "HMD is not found.");
		return false;
	}

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

	// Set a default value for interpupillary distance:
	mIPD = ovrHmd_GetFloat(mHMD, OVR_KEY_IPD,  0.064f);

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


