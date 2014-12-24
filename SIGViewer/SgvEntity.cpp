#include "SgvEntity.h"
#include "SIGVerse.h"

//#include "Geometrik/Mesh_sphere.h"
//#include "Geometrik/Mesh_Cube.h"
//#include "CX3DAppearanceNode.h"
//#include "CX3DMaterialNode.h"

#include "CX3DParser.h"
#include "CX3DParserUtil.h"
#include "CX3DField.h"
#include "CX3DGroupNode.h"
#include "CX3DTransformNode.h"
#include "CX3DAppearanceNode.h"
#include "CX3DMaterialNode.h"
#include "CX3DIndexedFaceSetNode.h"
#include "CX3DImageTextureNode.h"
#include "CX3DCoordinateNode.h"
#include "CX3DNormalNode.h"
#include "CX3DTextureCoordinateNode.h"
#include "CSimplifiedShape.h"

#define MAX_POINTS_PER_ONE_POLYGON  256
#define SCALE_PARAM 100

//#include <boost/cast.hpp>

namespace Sgv
{

	SgvEntity::SgvEntity(Ogre::SceneNode *headnode, Ogre::String name, Ogre::Vector3 pos, Ogre::Quaternion rot,Ogre::Vector3 scale ,unsigned int id)
		: m_name(name), m_id(id), m_isRobot(false), m_isAttach(false)
	{
		m_gx = m_gy = m_gz = 0.0f;
		// Create headnode for entity
		m_entHeadNode = headnode->createChildSceneNode(name, pos);
		m_entHeadNode->setInitialState();
		m_entHeadNode->setOrientation(rot); // Orientation
		m_entHeadNode->setScale(scale);     // Scale

		CX3DParser::printLog("name : %s", m_name.c_str());
		m_currShape = "./shape/nothing";
		m_NodeForBounding =NULL;
		mPosition = NULL;
		mScale = scale;
	}

	SgvEntity::~SgvEntity()
	{
		m_entHeadNode->removeAndDestroyAllChildren();
	}

	// Attach index number to the created Entity; determine name in Ogre in order to create Entity for each parts
	char *getObjName()
	{
		static int num;
		static char buf[5];
		sprintf(buf,"%d",num);
		num++;
		return buf;
	}

	//////////////////////////////////////////////////////////////////////////////
	//getFileName
	//Get file name from full path string
	//////////////////////////////////////////////////////////////////////////////
	/*
	  input: char *lpszPath    Pointer for the string which includes both of path and file name
	  return char *            Pointer for file name
	                             - pointer to "" if file name is not found
	                             - return the input pointer as just a file name, if '\','/',':' are not found
	*/
	char *getFileName(char *lpszPath)
	{
		char    *lpszPtr=lpszPath;

		while (*lpszPtr != '\0')
		{
			// Skip 2-byte characters
			if (IsDBCSLeadByte(*lpszPtr) == 0)
			{
				// Keep current position + 1 as pointer if '\','/',':' are found
				if ((*lpszPtr == '\\') || (*lpszPtr == '/') || (*lpszPtr == ':'))
				{
					lpszPath=lpszPtr+1;
				}
			}
			// Shift to the next character
			lpszPtr=CharNext(lpszPtr);
		}
		return lpszPath;
	}

	// Create Robot Entity from parsing result
	bool SgvEntity::createRobotObj(CX3DParser *parser, Ogre::SceneManager *mgr)
	{
		// Mark is created to keep the position of Entity 
		createPositionMark(mgr);

		m_isRobot = true;
		MFNode *humanoid = parser->searchNodesFromAllChildrenOfRoot("Humanoid");
		if (humanoid)
		{
			// An assumption that only one node should exist, is used
			CX3DOpenHRPHumanoidNode *hnode =(CX3DOpenHRPHumanoidNode*)humanoid->getNode(0);
			if (hnode) {
				//  Extract HumanoidBody Field
				MFNode *hbody = hnode->getHumanoidBody();

				// Count the number of nodes which included by HumanoidBody
				int nbody = hbody->count();
				for (int i = 0; i < nbody; i++)
				{
					// Extract i-th node
					CX3DNode *pNode = hbody->getNode(i);
					if (pNode)
					{
						switch(pNode->getNodeType())
						{
						// In case of Joint node
						case OPENHRP_JOINT_NODE:
							{
								CX3DOpenHRPJointNode *jNode = (CX3DOpenHRPJointNode *)pNode;
								createRobotJoint(m_entHeadNode, jNode, mgr);
							}
						}
					}
				}
			}
		}
		return true;
	}

	bool SgvEntity::createRobotJoint(Ogre::SceneNode *parent, CX3DOpenHRPJointNode *jNode, Ogre::SceneManager *mgr)
	{
		int id = jNode->getJointId()->getValue();

		char buf[5];
		sprintf(buf,"%d",id);

		SFVec3f *trans = jNode->getTranslation();

		Ogre::String jname = jNode->getName()->getValue();

		SFRotation *rot = jNode->getRotation();

		Ogre::Vector3 vec(trans->x(), trans->y(), trans->z());

		// Calculate quaternion from axis and rotational angle
		float a = cos(rot->rot()/2);
		float b = sin(rot->rot()/2);
		//Ogre::Quaternion qua(a, rot->x() * b, rot->y() * b, rot->z() * b);
		Ogre::Quaternion qua(1.0, 0.0, 0.0, 0.0);

		// Create child node whose name is [agent name]/[joint name]
		Ogre::SceneNode *childScene = parent->createChildSceneNode(m_name + "/" + jname, vec, qua);
		m_allJoints.push_back(jname);

		// Mark for display of joint
		// Create of entity for display
		Ogre::Entity *jPosEntity = mgr->createEntity(m_name + "/" + jname + "/" + "JointMark" ,"MySphere.mesh");
		Ogre::MaterialPtr clone = Ogre::MaterialManager::getSingleton().getByName("Blue_Clone");

		// Create clone if it does not exist
		if (clone.isNull()) {
			Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
			clone = mtr->clone("Blue_Clone");
			clone->setAmbient(0.0f, 0.0f, 1.0f);
			clone->setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
			clone->setLightingEnabled(true);
			clone->setSelfIllumination(0.0f, 0.0f, 1.0f);
		}

		jPosEntity->setMaterial(clone);
		Ogre::SceneNode *jPosNode = childScene->createChildSceneNode(m_name + "/" + jname + "/" + "JointMark");
		jPosNode->scale(0.5f, 0.5f, 0.5f);
		jPosNode->attachObject(jPosEntity);
		jPosEntity->setVisible(false);
		m_allJPosEntities.push_back(jPosEntity);
		/////


		//Ogre::SceneNode *childScene = parent->createChildSceneNode(vec, qua);

		MFNode *children = jNode->getChildren();

		for (int i = 0; i < children->count(); i++)
		{
			CX3DNode *childNode = children->getNode(i);
			switch(childNode->getNodeType())
			{
			case OPENHRP_JOINT_NODE:
				{
					CX3DOpenHRPJointNode *child = (CX3DOpenHRPJointNode *)childNode;
					createRobotJoint(childScene, child, mgr);
					break;
				}
			case OPENHRP_SEGMENT_NODE:
				{
					CX3DOpenHRPSegmentNode *child = ( CX3DOpenHRPSegmentNode *)childNode;
					createRobotSegment(childScene, child, mgr);
					break;
				}
			default:
				{
					CX3DParser::printLog("Could not create this node type.");
					break;
				}
			}
		}

		return true;
	}

	bool SgvEntity::createRobotSegment(Ogre::SceneNode *parent, CX3DOpenHRPSegmentNode *sNode, Ogre::SceneManager *mgr)
	{
		SFVec3f *com = sNode->getCenterOfMass();
		//CX3DParser::printLog("Center of mass (%f, %f, %f)\n", com->x(), com->y(), com->z());
		Ogre::Vector3 vec(com->x(), com->y(), com->z());

		// Name of link
		Ogre::String name = sNode->getName()->getValue();
		//char tmp[128];
		//sprintf(tmp, "%s ,%s",m_name.c_str(),name.c_str());
		//MessageBox( NULL, tmp, "Error", MB_OK);

		//Calculate quaternion from axis and rotational angle
		//float a = cos(rot->rot()/2);
		//float b = sin(rot->rot()/2);
		//Ogre::Quaternion qua(a, rot->x() * b, rot->y() * b, rot->z() * b);
		Ogre::Quaternion qua(1.0f, 0.0f, 0.0f, 0.0f);

		Ogre::SceneNode *childScene = parent->createChildSceneNode(m_name + "/" + name, vec, qua);

		// Mark for display of joint
		// Create of entity for display
		Ogre::Entity *jSegEntity = mgr->createEntity(m_name + "/" + name + "/" + "SegmentMark" ,"MySphere.mesh");
		Ogre::MaterialPtr clone = Ogre::MaterialManager::getSingleton().getByName("Green_Clone");

		// Create clone if it does not exist
		if (clone.isNull()) {
			Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
			clone = mtr->clone("Green_Clone");
			clone->setAmbient(0.0f, 1.0f, 0.0f);
			clone->setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
			clone->setLightingEnabled(true);
			clone->setSelfIllumination(0.0f, 1.0f, 0.0f);
		}

		jSegEntity->setMaterial(clone);
		Ogre::SceneNode *jSegNode = childScene->createChildSceneNode(m_name + "/" + name + "/" + "SegmentMark");
		jSegNode->scale(0.55f, 0.55f, 0.55f);
		jSegNode->attachObject(jSegEntity);
		jSegEntity->setVisible(false);
		m_allSegPosEntities.push_back(jSegEntity);
		/////


		MFNode *children = sNode->getChildren();

		COPFromMFNode(children);

		for (int i = 0; i < children->count(); i++)
		{
			CX3DNode *childNode = children->getNode(i);
			switch(childNode->getNodeType())
			{
			case TRANSFORM_NODE:
				{
					CX3DTransformNode *child = (CX3DTransformNode *)childNode;
					createTransform(childScene, child, mgr, i);
					break;
				}
			case SHAPE_NODE:
				{
					CX3DShapeNode *child = (CX3DShapeNode *)childNode;
					createShape(childScene, child, mgr);
					break;
				}
			default:
				{
					CX3DParser::printLog("Could not create this node type. %d", childNode->getNodeType());
					break;
				}
			}
		}
		return true;
	}

	// Create scene node from parsing result
	bool SgvEntity::createObj(CX3DParser *parser, Ogre::SceneManager *mgr, bool currshape, int alg)
	{
		// Create mark for entity position
		createPositionMark(mgr);

		std::string fname = parser->getFileName();

		computeCenterOfMass(parser, alg);

		createBoundingBox(mgr);

		Ogre::SceneNode *node = m_entHeadNode->createChildSceneNode(m_name + "/" + fname);

		MFNode *groupNodes = parser->searchNodesFromDirectChildrenOfRoot("Group");
		int groupNum = groupNodes->count();

		// In case of existance of group node
		if (groupNum > 0)
		{
			for (int i = 0; i < groupNum; i++)
			{
				CX3DGroupNode *pGroup = (CX3DGroupNode *)(groupNodes->getNode(i));
				// Fail to find the Group node
				if (!pGroup)
				{
					CX3DParser::printLog("failed to get group node\n");
					break;
				}
				// Succeed to find the Group node
				else
				{
					// Search Transform node which owned by the Group node
					MFNode *transformNodes = pGroup->searchNodesFromDirectChildren("Transform");
					int transNum = transformNodes->count();

					if (transNum < 0)
					{
						CX3DParser::printLog("could not found Transform node\n");
						break;
					}

					for (int i = 0;i < transNum; i++)
					{
						CX3DTransformNode *pTrans = (CX3DTransformNode *)transformNodes->getNode(i);

						if (!createTransform(node, pTrans, mgr, i))
						{
							break;
						}
					}
				}
			}
		}

		// Search Transform node if Group node not found
		else
		{
			MFNode *transformNodes = parser->searchNodesFromDirectChildrenOfRoot("Transform");
			int transNum = transformNodes->count();

			if (transNum > 0)
			{
				for (int i = 0;i < transNum; i++)
				{
					// Create Transform node
					CX3DTransformNode *pTrans = (CX3DTransformNode *)transformNodes->getNode(i);
					if (!createTransform(node, pTrans, mgr, i))
					{
						break; 
					}
				}
			} 
			// Search Shape node if Transform node was not found
			else
			{
				MFNode *shape = parser->searchNodesFromDirectChildrenOfRoot("Shape");
				int shapeNum = shape->count();
				if (shapeNum > 0)
				{
					for (int i = 0; i < shapeNum; i++)
					{
						CX3DShapeNode *pShape = (CX3DShapeNode*)shape->getNode(i);
						if (!createShape(node, pShape, mgr))
						{
							break; 
						}
					}
				}
				else
				{
					// If even Shape node is not found, return false
					return false;
				}
			}
		}
		// Setting of current shape file
		if (currshape) {
			m_currShape = fname;
		}
		// Current shape should not be visible
		else node->setVisible(false);

		return true;
	}

	// Create Ogre shape from parsed Trasnform node
	bool SgvEntity::createTransform(Ogre::SceneNode *parent, CX3DTransformNode *trans, Ogre::SceneManager *mgr, int count)
	{
		SFVec3f    *vec = trans->getTranslation();
		SFRotation *rot = trans->getRotation();

		// Convert into Ogre vector
		Ogre::Vector3 ovec(vec->x(), vec->y(), vec->z());

		//Calculate quaternion from axis and rotational angle
		float a = cos(rot->rot()/2);
		float b = sin(rot->rot()/2);
		Ogre::Quaternion qua(a, rot->x() * b, rot->y() * b, rot->z() * b);

		// Create a child node for Transform
		//Ogre::SceneNode *node = (Ogre::SceneNode*)parent->createChild(ovec, qua);
		char tmp[MAX_STRING_NUM];
		sprintf_s(tmp, MAX_STRING_NUM, "%d", count);
		Ogre::String tname = "/trans" + Ogre::String(tmp);

		Ogre::String pname = parent->getName();
		Ogre::SceneNode *node = (Ogre::SceneNode*)parent->createChild(pname + tname, ovec, qua);
		
		// Search Shape node which is owned by Transform node
		MFNode *shape = trans->searchNodesFromDirectChildren("Shape");

		if (!shape)
		{
			CX3DParser::printLog("failed to get shape node\n");
			return false;
		}
		else
		{
			for (int i = 0; i < shape->count(); i++)
			{
				CX3DShapeNode *pShape = (CX3DShapeNode *)shape->getNode(i);
				// Fail to refer Shape node
				if (!pShape) 
				{
					CX3DParser::printLog("name: %s could not get shape node (%d th)\n",pShape->getNodeName(),i);
					break; 
				}
				else
				{
					// Create Shape node
					if (!createShape(node, pShape, mgr))
					{
						break; 
					}
				}
			}
		}
		return true;
	} 


	// Create Ogre shape from parsed Shape node
	bool SgvEntity::createShape(Ogre::SceneNode *parent, CX3DShapeNode *shape, Ogre::SceneManager *mgr)
	{

		bool isTexture = false;

		// Geometory setting
		CX3DNode *geoNode = shape->getGeometry()->getNode();
		if (!geoNode)
		{
			CX3DParser::printLog("could not get geometry field");
			return false;
		}
		// Target entity to add Shape information
		Ogre::Entity *entity;

		if (!(geoNode->getNodeType() == INDEXED_FACE_SET_NODE))
		// Simple geometory
		{
			// Geometory creation for each type of simple geometory
			Ogre::Entity *ent = createSimpleShape(geoNode->getNodeType() ,parent, mgr, geoNode);

			if (!ent)  return false;
			entity = ent;
		}
		// Geometory creation by vertex positions
		else
		{
			// Refer IndexedFaceSetNode
			CX3DIndexedFaceSetNode *index_node = (CX3DIndexedFaceSetNode *)geoNode;
			if (index_node)
			{
				// Refer the CoordinateNode of Vertex (it is required to create Shape)
				CX3DCoordinateNode *Coord = (CX3DCoordinateNode *)(index_node->getCoord()->getNode());
				if (Coord)
				{
					// Refer the position of polygon vertex
					MFVec3f *coord = Coord->getPoint();
					// Refer the number of vertex
					int numPt = coord->count();

					MFVec3f *normal   = NULL; // array of normal vector
					MFVec2f *texCoord = NULL; // array of texture coordinate

					// order of polygon position array (default is ccw)
					bool ccw = true;

					SFBool *sccw = index_node->getCCW();
					if (sccw != NULL)
						ccw = sccw->getValue();

					// Refer a normal vector
					CX3DNormalNode *Normal = (CX3DNormalNode *)(index_node->getNormal()->getNode());
					if (Normal) 
					{
						//Refer array of normal vector
						normal = Normal->getVector();
					}
					else
					{
						CX3DParser::printLog("There is no normal vector.");
					}

					// Refer texture coordinate
					CX3DTextureCoordinateNode *TexCoord = (CX3DTextureCoordinateNode *)(index_node->getTexCoord()->getNode());
					if (TexCoord)
					{
						// Refer array of texture coordinate
						texCoord = TexCoord->getPoint();
					}
					else
					{
						CX3DParser::printLog("There is no normal texture coordinate.");
					}
					// Index information (vertex consists plane, normal vector, array of texture coordinate 
					MFInt32 *coordIndex    = NULL; // Vertex position index consist plane (array of index of vertex)
					MFInt32 *normalIndex   = NULL; // index for normal vector
					MFInt32 *texCoordIndex = NULL; // index for texture coordinate

					coordIndex = index_node->getCoordIndex();

					int numIndex = coordIndex->count();

					if (normal != NULL)
					{
						normalIndex = index_node->getNormalIndex();
						// Check correspondency between vertex index array and number of index
						// (If the numbers are different, process will keep on going without quit)
						if (normalIndex->count() != numIndex)
							CX3DParser::printLog("Normal index value does not match with Coordinate Index.");

					}
					if (texCoord != NULL)
					{
						isTexture = true;
						texCoordIndex = index_node->getTexCoordIndex();

						if (texCoordIndex->count() != numIndex)
							CX3DParser::printLog("Texture index value does not match with Coordinate Index.");
					}
					// Calculation of CoG
					float gx = 0.0f;
					float gy = 0.0f;
					float gz = 0.0f;

#define MEAN_OF_VERTEX_ROBOT
					// Calculate CoG for each parts if the target is robot
					if (m_isRobot) {
#ifdef MEAN_OF_VERTEX_ROBOT
						for (int i=0; i<numPt; i++)
						{
							SFVec3f vec = coord->getValue(i);
							gx += vec.x();
							gy += vec.y();
							gz += vec.z();
						}
						gx /= numPt;
						gy /= numPt;
						gz /= numPt;
#else
						// Calculate mean of max and min value for x,y,z axis
						float minx, miny, minz, maxx, maxy, maxz;
						minx = miny = minz = maxx = maxy = maxz = 0;
						for (int i=0; i<numPt; i++)
						{
							SFVec3f vec = coord->getValue(i);
							if (vec.x() > maxx) { maxx = vec.x(); }
							if (vec.x() < minx) { minx = vec.x(); }

							if (vec.y() > maxy) { maxy = vec.y(); }
							if (vec.y() < miny) { miny = vec.y(); }

							if (vec.z() > maxz) { maxz = vec.z(); }
							if (vec.z() < minz) { minz = vec.z(); }
						}
						gx = (maxx + minx) / 2;
						gy = (maxy + miny) / 2;
						gz = (maxz + minz) / 2;
#endif
					}

					// Number of polygon
					int numPol = 0;
					for (int i=0; i<numIndex; i++)
					{
						int ind = coordIndex->getValue(i);
						if (ind == -1) numPol++;
					}
					CX3DParser::printLog("Polygon Number = %d\n",numPol);
					// Array of type of polygon (cf. *nkaku=3, 4, 5,,,,)
					int *nkaku;
					nkaku = (int *)malloc(sizeof(int) * numPol);
					int j = 0;
					int d = 0;

					for (int i=0; i<numIndex; i++)
					{
						int ind = coordIndex->getValue(i);
						if (ind == -1) 
						{
							// type of polygon (number of vertex)
							nkaku[d] = j;
							d++;
							j = 0;
						}
						else j++;
					}
					// Refer the name (= serial number)
					//Ogre::String ind_name = getObjName();
					Ogre::ManualObject *mObj = mgr->createManualObject(getObjName());

					mObj->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);

					int nIndex = 0;
					int posIndex = 0;

					// Create index of vertex coordinate for each polygon
					for (int i=0; i<numPol; i++)
					{
						int nk = nkaku[i];  // Type of polygon (= number of vertex)

						for (int j=0; j<nk; j++)
						{

							SFVec3f vec = coord->getValue(coordIndex->getValue(nIndex + j));

#ifdef MEAN_OF_VERTEX_ROBOT
							// Registration of vertex position data
							if (m_isRobot)
								mObj->position(vec.x() - gx, vec.y() - gy, vec.z() - gz);
							else
								mObj->position(vec.x() - m_gx, vec.y() - m_gy, vec.z() - m_gz);
							//mObj->position(vec.x(), vec.y(), vec.z());
#else

							mObj->position(vec.x() - m_gx, vec.y() - m_gy, vec.z() - m_gz);
#endif
							//CX3DParser::printLog("nind = %d\n vecx = %f, vecy = %f, vecz = %f\n",nind, vec.x(), vec.y(), vec.z());
							// In a case which normal vector exist
							if (normal != NULL && normalIndex != NULL)
							{
								// Refer normal vector from idex
								SFVec3f nvec = normal->getValue(normalIndex->getValue(nIndex + j));
								// Setting of normal vector
								mObj->normal(nvec.x(), nvec.y(), nvec.z());
							}
							// Autonamic calculation
							else
							{
								if (j==0)
								{
									CX3DParser::printLog("normal automatically.\n");
									// Refer three vertex
									SFVec3f vec0 = vec;
									SFVec3f vec1 = coord->getValue(coordIndex->getValue(nIndex + 1));
									SFVec3f vec2 = coord->getValue(coordIndex->getValue(nIndex + 2));

									float a1 =  vec1.x() - vec0.x();
									float a2 =  vec1.y() - vec0.y();
									float a3 =  vec1.z() - vec0.z();

									float b1 =  vec2.x() - vec1.x();
									float b2 =  vec2.y() - vec1.y();
									float b3 =  vec2.z() - vec1.z();

									// Cross product
									float x_ = a2*b3 - a3*b2;
									float y_ = a3*b1 - a1*b3;
									float z_ = a1*b2 - a2*b1;

									// Setting of normal vector
									mObj->normal(x_, y_, z_);
								}
							} 

							if (texCoord != NULL && texCoordIndex != NULL)
							{
								SFVec2f tvec = texCoord->getValue(texCoordIndex->getValue(nIndex + j));
								mObj->textureCoord(tvec.x(),1.0f - tvec.y());
								//CX3DParser::printLog("texture x = %f y = %f\n",tvec.x(), tvec.y());
							}
						}
						// Triangle
						if (nk == 3)
						{
							// Head and tail are swapped
							if (!ccw)
							{
								mObj->triangle(posIndex + 2, posIndex + 1, posIndex );
								CX3DParser::printLog("ccw is true\n");
							}
							else
							{
								mObj->triangle(posIndex, posIndex + 1, posIndex + 2);
							}
						}
						// Tetragon
						else if (nk == 4)
						{
							// Head and tail are swapped
							if (!ccw)
							{
								mObj->quad(posIndex + 3, posIndex + 2, posIndex + 1, posIndex );
								CX3DParser::printLog("ccw is true\n");
							}
							else
							{
								mObj->quad(posIndex, posIndex + 1, posIndex + 2, posIndex + 3);
							}
						}

						// More than tetragon (>= pentagon)
						else if (4 < nk && nk < MAX_POINTS_PER_ONE_POLYGON)
						{
							for (int j=0; j<nk-2; j++)
							{
								//CX3DParser::printLog("more than 4 vertex.\n");
								if (!ccw)
								{
									mObj->triangle(posIndex, posIndex + (nk - 2) - j, posIndex + (nk - 3) - j);
								}
								else
								{
									mObj->triangle(posIndex, posIndex + 1 + j, posIndex + 2 + j);
								}
							}
						}

						else
						{
							CX3DParser::printLog("Could not create %d poins per polygon.",nk);
						}

						nIndex += nkaku[i] + 1;
						posIndex += nkaku[i];
					}

					mObj->end();

					//parent->attachObject(mObj);
					Ogre::MeshPtr meshPtr = mObj->convertToMesh(mObj->getName());
					entity = mgr->createEntity(m_name + "/" + mObj->getName(),mObj->getName());

					free(nkaku);
				} // if (coord)
				else
				{
					CX3DParser::printLog("** ERROR ** IndexedFaceSet has no coord.\n");
					return false;
				}
			}

		}
		///////////////////////
		// Set of appearance
		///////////////////////
		CX3DAppearanceNode *appNode = (CX3DAppearanceNode*)(shape->getAppearance()->getNode());
		if (!appNode)
		{
			CX3DParser::printLog("could not get Appearance Node");
			return false;  // It is required for appearance (perhaps)
		}
		else
		{
			// Prepare the material
			// Copy default material (TODO: consider to create from scratch)
			Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
			//Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("MA_White");

			// Entity name and Material name are the same
			Ogre::MaterialPtr mptr = tmp->clone(entity->getName());
			CX3DParser::printLog("Create material name : %s\n", entity->getName().c_str());
			mptr->setLightingEnabled(true);

			/////////////////
			// Set of Texture
			/////////////////
			CX3DImageTextureNode *pImageTexNode = (CX3DImageTextureNode *)(appNode->getTexture()->getNode());
			if (pImageTexNode)
			{
				MFString *texFiles = pImageTexNode->getUrl();
				if (texFiles->count() > 0)
				{
					const char *texFile = texFiles->getValue(0);

					char* filename = getFileName((char*)texFile);
					CX3DParser::printLog("Texture file : %s\n", filename);

					// Set texture for material
					mptr->getTechnique(0)->getPass(0)->createTextureUnitState(filename);
				}
			}
			// Set for availability of transparent
			mptr->setDepthCheckEnabled(true);
			mptr->setDepthWriteEnabled(true);
			mptr->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
			//////////////////
			// Set of Material
			//////////////////
			CX3DMaterialNode *matNode = (CX3DMaterialNode*)(appNode->getMaterial()->getNode());
			if (matNode)
			{
				// Set of color of diffuse reflex light
				SFColor *dcol = matNode->getDiffuseColor();
				SFColor *scol = matNode->getSpecularColor();
				SFColor *ecol = matNode->getEmissiveColor();

				// Degree of transparency
				SFFloat *flo = matNode->getTransparency();
				float a = flo->getValue();

				float amb = matNode->getAmbientIntensity()->getValue();
				// Default
				if (amb == 0.0) amb = 0.8f;

				//mptr->setDepthCheckEnabled(true);
				//mptr->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);

				// 1.0 means transparent in x3d; 0.0 means transparent in Ogre
				if (a==0.0) a = 1.0f;
				else {
					mptr->setDepthWriteEnabled(false);
					a = 1.0f - a;
				}

				mptr->setDiffuse(dcol->r(), dcol->g(), dcol->b(), a);
				//CX3DParser::printLog("color (%f, %f, %f)", col->r(), col->g(), col->b());

				// Set of color of specular reflex light
				mptr->setSpecular(scol->r(), scol->g(), scol->b(), a);
				// Set of color of self illumination
				mptr->setSelfIllumination(ecol->r(), ecol->g(), ecol->b());

				// Set of shineness
				SFFloat *shi = matNode->getShininess();
				float shin = shi->getValue();
				mptr->setShininess(shin);

				mptr->setColourWriteEnabled(true);
				// No color in object which does not have texture 
				if (!isTexture)
					mptr->setAmbient(0.8f * dcol->r(), 0.8f * dcol->g(), 0.8f * dcol->b());
					//mptr->setAmbient(col->r(), col->g(), col->b());
					//mptr->setAmbient(amb * dcol->r(), amb * dcol->g(), amb * dcol->b());
				m_allMaterials.insert(std::map<Ogre::MaterialPtr, float>::value_type(mptr, a));
			}
			else {
				m_allMaterials.insert(std::map<Ogre::MaterialPtr, float>::value_type(mptr, 1.0));
			}
			entity->setMaterial(mptr);
			//m_allMaterials.push_back(mptr);

		}
		// Attach the created eneity to the parent node
		parent->attachObject(entity);
		// Keep the Ogre entity created for each part
		m_allEntities.push_back(entity);

		return true;
	} 

	// Create sphere, box and cylinder
	Ogre::Entity *SgvEntity::createSimpleShape(int type, Ogre::SceneNode *parent, Ogre::SceneManager *mgr, CX3DNode *geo)
	{

		Ogre::Entity *ent;
		switch (type)
		{
		case SPHERE_NODE:
			{
				// Refer the size
				CX3DSphereNode *sphereNode = (CX3DSphereNode*)geo;
				float rad = sphereNode->getRadius()->getValue();

				// Determine the scale for scene node from referred size
				parent->scale(Ogre::Vector3(rad));

				// Use prepared mesh
				//ent = mgr->createEntity(getObjName(),"MySphere.mesh");
				//ent = mgr->createEntity(m_name + "/" + getObjName(),"MySphere.mesh");
				ent = mgr->createEntity(m_name + "/" + getObjName(),"pSphere1.mesh");

				break;
			}
		case BOX_NODE:
			{
				CX3DBoxNode *boxNode = (CX3DBoxNode*)geo;
				float x, y, z;
				boxNode->getSize()->getValue(x, y, z);
				parent->scale(Ogre::Vector3(x,y,z));
				//ent = mgr->createEntity(getObjName(),"MyBox.mesh");
				ent = mgr->createEntity(m_name + "/" + getObjName(),"MyBox.mesh");
				break;
			}
		case CYLINDER_NODE:
			{
				CX3DCylinderNode *cylinderNode = (CX3DCylinderNode*)geo;
				float rad = cylinderNode->getRadius()->getValue();
				float height = cylinderNode->getHeight()->getValue();

				Ogre::String name =	parent->getName();

				parent->scale(Ogre::Vector3(rad, height/2, rad));

				//parent->setInitialState();				
				//ent = mgr->createEntity(getObjName(),"MyCylinder.mesh");
				ent = mgr->createEntity(m_name + "/" + getObjName(),"pCylinder1.mesh");
				//parent->setOrientation(qua*Ogre::Quaternion(0.707f, 0.707f, 0.0f, 0.0f));

				break;
			}
		default:
			{
				CX3DParser::printLog("Cannot support this shape. NodeType :%d",type);
				return NULL;
			}
		}
		return ent;
	}

//#define OLD_VERSION


	// Calculate center of bounding box as center of gravity
	bool SgvEntity::computeCenterOfMass(CX3DParser *parser, int alg)
	{
		MFNode *ind = parser->searchNodesFromAllChildrenOfRoot("IndexedFaceSet");
		int indsize = ind->count();
		float minx, miny, minz, maxx, maxy, maxz;

		maxx = maxy = maxz = -100000.0;
		minx = miny = minz = 100000.0;

		int allPt = 0;

		if (indsize == 0) {
			return true;
		}

		for (int i = 0; i < indsize; i++) {
			CX3DIndexedFaceSetNode *index_node = (CX3DIndexedFaceSetNode *)ind->getNode(i);
			CX3DCoordinateNode *Coord = (CX3DCoordinateNode *)(index_node->getCoord()->getNode());

			MFVec3f *coord = Coord->getPoint();

			int numPt = coord->count();
			allPt += numPt;

			//#ifdef OLD_VERSION
			for (int i = 0; i < numPt; i++)
			{
				SFVec3f vec = coord->getValue(i);
				if (alg == 1) {
					m_gx += vec.x();
					m_gy += vec.y();
					m_gz += vec.z();
				}
				// Check the exceptional case in which all the values are zero
				if (alg == 2 && vec.x() == 0.0 && vec.y() == 0.0 && vec.z() == 0.0 )
					continue;

				if      (vec.x() > maxx) { maxx = vec.x(); }
				else if (vec.x() < minx) { minx = vec.x(); }

				if      (vec.y() > maxy) { maxy = vec.y(); }
				else if (vec.y() < miny) { miny = vec.y(); }

				if      (vec.z() > maxz) { maxz = vec.z(); }
				else if (vec.z() < minz) { minz = vec.z(); }
			}
			if (alg == 1) {
				m_gx /= numPt;
				m_gy /= numPt;
				m_gz /= numPt;
			}
		}
		if (alg == 2) {
			m_gx = (maxx + minx) / 2;
			m_gy = (maxy + miny) / 2;
			m_gz = (maxz + minz) / 2;

			mBBoxSize.x = maxx - minx;
			mBBoxSize.y = maxy - miny;
			mBBoxSize.z = maxz - minz;
		}

		CX3DParser::printLog("%s position :%f, %f, %f", m_name.c_str(), m_gx, m_gy, m_gz);
		return true;
	}


	void SgvEntity::changeShape(Ogre::String shape)
	{
		Ogre::String pathShape = "./shape/" + shape;
		// Return if target is the current shape
		if (shape == m_currShape) return;
		// Refer the current scene node
		if (m_currShape != "./shape/nothing") {
			Ogre::SceneNode *curr = (Ogre::SceneNode*)(m_entHeadNode->getChild(m_name + "/" + m_currShape));
			curr->setVisible(false);
		}

		if (shape != "./shape/nothing") {
			// Refer the scene node to be modified
			Ogre::SceneNode *chan = (Ogre::SceneNode*)(m_entHeadNode->getChild(m_name + "/" + pathShape));
			chan->setVisible(true);
		}
		// Set to the current shape file
		m_currShape = pathShape;
	}

	void SgvEntity::setTransparency(float a)
	{
		std::map<Ogre::MaterialPtr, float>::iterator it;
		it = m_allMaterials.begin();
		while (it != m_allMaterials.end()) {
			(*it).first->setDepthWriteEnabled(false);
			Ogre::Pass *p = (*it).first->getTechnique(0)->getPass(0);
			Ogre::ColourValue color = p->getDiffuse();
			p->setDiffuse(color.r, color.g, color.b, a);
			it++;
		}
	}

	void SgvEntity::resetTransparency()
	{
		std::map<Ogre::MaterialPtr, float>::iterator it;
		it = m_allMaterials.begin();
		while (it != m_allMaterials.end()) {
			if ((*it).second == 1.0)
				(*it).first->setDepthWriteEnabled(true);
			Ogre::Pass *p = (*it).first->getTechnique(0)->getPass(0);
			Ogre::ColourValue color = p->getDiffuse();
			p->setDiffuse(color.r, color.g, color.b, (*it).second);
			++it;
		}
	}


	bool SgvEntity::MinMaxFromMFNode(MFNode *node,
		double *min_x, 
		double *min_y,
		double *min_z,
		double *max_x,
		double *max_y,
		double *max_z)
	{
		int size = node->count();
		
		for (int i = 0; i < size; i++) {
			CX3DNode *childNode = node->getNode(i);
			switch(childNode->getNodeType())
			{
			case TRANSFORM_NODE:
				{

					CX3DTransformNode *child = (CX3DTransformNode *)childNode;
					// Refer the Translation and Rotation of Transform node
					SFVec3f    *t = child->getTranslation();
					MFNode *children = child->getChildren();
					// Refer the children field
					MinMaxFromMFNode(children, min_x, min_y, min_z, max_x, max_y, max_z);

					break;
				}
			case SHAPE_NODE:
				{

					CX3DShapeNode *child = (CX3DShapeNode *)childNode;

					CX3DNode *geoNode = child->getGeometry()->getNode();
					if (!geoNode)
					{
						return false;
					}
	  
					int nodeType = geoNode->getNodeType();
					// In a case of simple geometry
					if (nodeType != INDEXED_FACE_SET_NODE)
					{
						switch(nodeType)
						{
						case SPHERE_NODE:
							{
								CX3DSphereNode *sphereNode = (CX3DSphereNode*)geoNode;
								float r = sphereNode->getRadius()->getValue();
								*min_x = *min_y = *min_z = -r;
								*max_x = *max_y = *max_z =  r;
								break;
							}
						case BOX_NODE:
							{
								CX3DBoxNode *boxNode = (CX3DBoxNode*)geoNode;
								float x, y, z;
								boxNode->getSize()->getValue(x, y, z);
								*min_x = -x;
								*min_y = -y;
								*min_z = -z;
								*max_x = x;
								*max_y = y;
								*max_z = z;
								break;
							}
						case CYLINDER_NODE:
							{
								CX3DCylinderNode *cylinderNode = (CX3DCylinderNode*)geoNode;
								float rad    = cylinderNode->getRadius()->getValue();
								float height = cylinderNode->getHeight()->getValue();
								*min_x = *min_z = -rad;
								*max_x = *max_z =  rad;
								*min_y = -height/2;
								*max_y =  height/2;
							}
						}
					}
					else {
						CX3DIndexedFaceSetNode *index_node = (CX3DIndexedFaceSetNode *)geoNode;
						if (index_node) {
							// Refer the vertex coordinate node (It is required to creat Shape)
							CX3DCoordinateNode *Coord = (CX3DCoordinateNode *)(index_node->getCoord()->getNode());
							if (Coord) {
								MinMaxFromIndexedNode(Coord, min_x, min_y, min_z, max_x, max_y, max_z);
							}
						}
					}
					break;
				}
			}
		}
	}

	bool SgvEntity::MinMaxFromIndexedNode(CX3DCoordinateNode* Coord,
		double *min_x,
		double *min_y,
		double *min_z,
		double *max_x,
		double *max_y,
		double *max_z)
	{
		// Refer the vertex of polygon
		MFVec3f *coord = Coord->getPoint();
		// Refer the number of vertex
		int numPt = coord->count();

		for (int i = 0; i < numPt; i++)
		{
			SFVec3f vec = coord->getValue(i);
			double x = vec.x();
			double y = vec.y();
			double z = vec.z();
			//LOG_MSG(("vec(%f, %f, %f)", x, y, z));
			if      (*max_x < x) *max_x = x;
			else if (x < *min_x) *min_x = x;
			if      (*max_y < y) *max_y = y;
			else if (y < *min_y) *min_y = y;
			if      (*max_z < z) *max_z = z;
			else if (z < *min_z) *min_z = z;
		}
		return true;
	}

	bool SgvEntity::COPFromMFNode(MFNode *node)
	{

		int size = node->count();
		if (size == 0) return true;
		int allPt = 0;

		double max_x = -10000.0;
		double max_y = -10000.0;
		double max_z = -10000.0;

		double min_x = 10000.0;
		double min_y = 10000.0;
		double min_z = 10000.0;

		MinMaxFromMFNode(node, &min_x, &min_y, &min_z, &max_x, &max_y, &max_z);

		m_gx = (min_x + max_x)/2;
		m_gy = (min_y + max_y)/2;
		m_gz = (min_z + max_z)/2;

		//LOG_MSG(("center of point(%f, %f, %f)",m_gx, m_gy, m_gz));
		return true;
	}


	void SgvEntity::createPositionMark(Ogre::SceneManager *mgr)
	{
		if (mPosition != NULL) return;
		// Create entity for display the position
		mPosition = mgr->createEntity(m_name + "/" + "position" ,"MySphere.mesh");
		Ogre::MaterialPtr clone = Ogre::MaterialManager::getSingleton().getByName("Red_Clone");

		// Create clone if it is not existed
		if (clone.isNull()) {
			Ogre::MaterialPtr mtr = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
			clone = mtr->clone("Red_Clone");
			clone->setAmbient(1.0f, 0.0f, 0.0f);
			clone->setSelfIllumination(1.0f, 0.0f, 0.0f);
			clone->setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
			clone->setLightingEnabled(true);
		}

		mPosition->setMaterial(clone);

		Ogre::SceneNode *posNode = m_entHeadNode->createChildSceneNode();
		posNode->scale(0.56f, 0.56f, 0.56f);
		mPosition->setVisible(false);
		posNode->attachObject(mPosition);
	}

	void SgvEntity::setVisible(bool vis)
	{
		int size = m_allEntities.size();
		if (vis) {
			for (int i = 0; i < size; i++) {
				m_allEntities[i]->setVisible(true);
			}
		}
		else {
			for (int i = 0; i < size; i++) {
				m_allEntities[i]->setVisible(false);
			}
		}
	}

	void SgvEntity::createBoundingBox(Ogre::SceneManager *mgr)
	{
		if (m_NodeForBounding != NULL) return;
		m_NodeForBounding = m_entHeadNode->createChildSceneNode();
		Ogre::Entity *ent = mgr->createEntity(m_name + "/" + "BoundingBox","MyBox.mesh");
		m_NodeForBounding->attachObject(ent);
		m_NodeForBounding->scale(mBBoxSize.x, mBBoxSize.y, mBBoxSize.z);
		m_NodeForBounding->setVisible(false);
	}
}

