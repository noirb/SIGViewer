#ifndef __SGVENTITY__
#define __SGVENTITY__

#include <Ogre.h>
#include "CX3DParser.h"

namespace Sgv
{

	class SgvEntity
	{
	public:
		// Constructor
		//SgvEntity(Ogre::SceneNode *headnode, Ogre::String name, Ogre::String shapefile,
		//Ogre::Vector3 pos, Ogre::Quaternion rot, Ogre::Vector3 scale,unsigned int id = 0);

		SgvEntity(Ogre::SceneNode *headnode, Ogre::String name, Ogre::Vector3 pos, 
			Ogre::Quaternion rot, Ogre::Vector3 scale,unsigned int id = 0);

		~SgvEntity();

		//bool createObj(CX3DParser *parser, Ogre::SceneNode *headnode);
		//bool createShape(Ogre::ManualObject *obj, CX3DShapeNode *shape); 

		//! Create entity which does not have joints
		// alg type of algorithm calculation of entity position
		bool createObj(CX3DParser *parser, Ogre::SceneManager *mgr, bool currshape, int alg);
		//! Create from Transform node
		bool createTransform(Ogre::SceneNode *parent, CX3DTransformNode *trans, Ogre::SceneManager *mgr, int count); 
		//! Create from Shape node
		bool createShape(Ogre::SceneNode *parent, CX3DShapeNode *shape, Ogre::SceneManager *mgr); 
		//! Create simple shape from Geometry node
		Ogre::Entity *createSimpleShape(int type, Ogre::SceneNode *parent, Ogre::SceneManager *mgr, CX3DNode *geo);
		//! Create Entity for Robot which has joints
		bool createRobotObj(CX3DParser *parser, Ogre::SceneManager *mgr);
		//! Create of joint
		bool createRobotJoint(Ogre::SceneNode *parent, CX3DOpenHRPJointNode *jNode, Ogre::SceneManager *mgr);
		//! Create robot's parts
		bool createRobotSegment(Ogre::SceneNode *parent, CX3DOpenHRPSegmentNode *sNode, Ogre::SceneManager *mgr);
		//! Calculation of center of gravity
		bool computeCenterOfMass(CX3DParser *parser, int alg);
		//! Add camera name/ID owned by entity
		void addCameraName(int id ,std::string cam){ 
			m_cameraNames.insert(std::map<int, std::string>::value_type(id, cam));
		}
		//! Add entity for camera display
		void addCameraPosEntity(Ogre::Entity* ent){m_allCameraPosEntities.push_back(ent);}
		//! Add entity for camera display
		void addCameraArrowEntity(Ogre::ManualObject* ent){m_allCameraArrowEntities.push_back(ent);}
		//! Set the entity for camera display visible
		void setCameraPositionVisible(bool vis){
			std::vector<Ogre::Entity*>::iterator it = m_allCameraPosEntities.begin();
			while(it != m_allCameraPosEntities.end()){
				(*it)->setVisible(vis);
				it++;
			}
		}
		//! Display of camera vector
		void setCameraArrowVisible(bool vis){
			std::vector<Ogre::ManualObject*>::iterator it = m_allCameraArrowEntities.begin();
			while(it != m_allCameraArrowEntities.end()){
				(*it)->setVisible(vis);
				it++;
			}
		}
		//! Refer the name of camera
		std::map<int, std::string> getCameraName(){ return m_cameraNames; }
		//! Refer the name of camera
		std::string getCameraName(int id){ 
			std::map<int, std::string>::iterator it = m_cameraNames.find(id);
			if (it != m_cameraNames.end()) {
				return (*it).second;
			}
			else{
				return "";
			}
			return "";
		}
		//! Set the transparency of entity
		void setTransparency(float a);
		//! Reset the transparency of entity
		void resetTransparency();
		//! Refer the number of cameras
		int getCameraNum(){ 
			//if(m_cameraNames.empty()) return 0;
			//else
			return m_cameraNames.size();
		}
		//! Refer the number of joints
		int getJointNum(){ return m_allJoints.size();}
		//! Add shape file which will transit to another status
		void changeShape(Ogre::String shape);
		//! Set whether a controller is attached or not
		void setisAttach(bool isAttach){m_isAttach = isAttach;}
		//! Refer whether a controller is attached or not
		bool isAttach(){return m_isAttach;}
		//! Refer the name
		Ogre::String getName() { return m_name;}
		//! Refer all of the Ogre::Entity (except ODE geometry) of entity
		std::vector<Ogre::Entity*> getAllEntities() { return m_allEntities;}
		//! Refer the head node owned by entity
		Ogre::SceneNode* getHeadNode() { return m_entHeadNode;}
		//! Refer the Bounding box owned by entity
		Ogre::SceneNode* getBoundingNode() { return m_NodeForBounding;}
		//! Visualization of BoundingBox
		void setBoundingBoxVisible(bool vis) { m_NodeForBounding->showBoundingBox(vis);}
		//! Create mark display for entity
		void createPositionMark(Ogre::SceneManager *mgr);
		//! Make the mark display visible
		void setPositionMarkVisible(bool vis){mPosition->setVisible(vis);}
		//! Make marks for joints visible
		void setJointPositionVisible(bool vis){
			std::vector<Ogre::Entity*>::iterator it = m_allJPosEntities.begin();
			while(it != m_allJPosEntities.end()){
				(*it)->setVisible(vis);
				it++;
			}
		}
		//! Make the segment mark visible
		void setSegmentPositionVisible(bool vis){
			std::vector<Ogre::Entity*>::iterator it = m_allSegPosEntities.begin();
			while(it != m_allSegPosEntities.end()){
				(*it)->setVisible(vis);
				it++;
			}
		}

		//! Refer the size of Bounding Box
		Ogre::Vector3 getBBoxSize(){return mBBoxSize;}
		//! Check whether it is a robot which has joints
		bool isRobot(){return m_isRobot;}
		//! Calculate max and min value of vertex positions from MFNode
		bool MinMaxFromMFNode(MFNode *node,
			double *min_x, 
			double *min_y,
			double *min_z,
			double *max_x,
			double *max_y,
			double *max_z);
		//! Calculate max and min value of vertex positions from IndexedFaceNode
		bool SgvEntity::MinMaxFromIndexedNode(CX3DCoordinateNode* Coord,
			double *min_x, 
			double *min_y,
			double *min_z,
			double *max_x,
			double *max_y,
			double *max_z);

		bool COPFromMFNode(MFNode *node);

		void setVisible(bool vis);

		void createBoundingBox(Ogre::SceneManager *mgr);

		Ogre::Vector3 getScale(){return mScale;}

	private:
		//! Entity name
		Ogre::String m_name;
		//! Name of current shape file
		Ogre::String m_sfile;
		//! Head node of entity
		Ogre::SceneNode *m_entHeadNode;
		//! Current shape file
		Ogre::String m_currShape;
		//! ID number
		unsigned int m_id;
		//! Flag whether it is robot or not
		bool m_isRobot;
		//! flag whether controller is attached or not
		bool m_isAttach;
		//! Center of gravity
		float m_gx;
		float m_gy;
		float m_gz;
		//! Pair of Camera Name and ID
		std::map<int, std::string> m_cameraNames;
		//! All of the Ogre entity for each parts
		std::vector<Ogre::Entity*> m_allEntities;
		//! Material
		std::map<Ogre::MaterialPtr, float> m_allMaterials;
		//! Entity for display maker
		Ogre::Entity *mPosition;
		//! Size of Bounding box
		Ogre::Vector3 mBBoxSize;
		//! All the joint names
		std::vector<Ogre::String> m_allJoints;
		//! Mark for joint display
		std::vector<Ogre::Entity*> m_allJPosEntities;
		//! Mark for parts display
		std::vector<Ogre::Entity*> m_allSegPosEntities;
		//! Mark for camera display
		std::vector<Ogre::Entity*> m_allCameraPosEntities;
		//! Mark for camera direction vector display
		std::vector<Ogre::ManualObject*> m_allCameraArrowEntities;
		//! Node for Bounding Box
		Ogre::SceneNode *m_NodeForBounding;
		//! Scale
		Ogre::Vector3 mScale;
	};
}

#endif // __SGVENTITY__
