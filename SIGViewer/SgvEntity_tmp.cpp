#include "SgvEntity.h"
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
		m_entHeadNode = headnode->createChildSceneNode(name, pos);
		m_entHeadNode->setInitialState();  
		m_entHeadNode->setOrientation(rot);
		m_entHeadNode->setScale(scale);    

		CX3DParser::printLog("name : %s", m_name.c_str());
		m_currShape = "./shape/nothing";
	}

	SgvEntity::~SgvEntity()
	{
		m_entHeadNode->removeAndDestroyAllChildren();
	}

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
	//////////////////////////////////////////////////////////////////////////////
	char *getFileName(char *lpszPath)
	{
		char    *lpszPtr=lpszPath;

		while(*lpszPtr != '\0')
		{
			if(IsDBCSLeadByte(*lpszPtr) == 0)
			{
				if((*lpszPtr == '\\') || (*lpszPtr == '/') || (*lpszPtr == ':'))
				{
					lpszPath=lpszPtr+1;
				}
			}
			lpszPtr=CharNext(lpszPtr);
		}
		return lpszPath;
	}

	bool SgvEntity::createRobotObj(CX3DParser *parser, Ogre::SceneManager *mgr)
	{
		m_isRobot = true;
		MFNode *humanoid = parser->searchNodesFromAllChildrenOfRoot("Humanoid");
		if(humanoid)
		{
			CX3DOpenHRPHumanoidNode *hnode =(CX3DOpenHRPHumanoidNode*)humanoid->getNode(0);
			if(hnode)
			{
				// -----------------------------------------
				// -----------------------------------------
				MFNode *hbody = hnode->getHumanoidBody();

				int nbody = hbody->count();
				for(int i = 0; i < nbody; i++)
				{
					CX3DNode *pNode = hbody->getNode(i);
					if(pNode)
					{
						switch(pNode->getNodeType())
						{
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

		//char tmp[32];
		//sprintf(tmp, "%s",jname.c_str());
		//MessageBox( NULL, tmp, "Error", MB_OK);

		SFRotation *rot = jNode->getRotation();

		Ogre::Vector3 vec(trans->x(), trans->y(), trans->z());

		float a = cos(rot->rot()/2);
		float b = sin(rot->rot()/2);
		//Ogre::Quaternion qua(a, rot->x() * b, rot->y() * b, rot->z() * b);
		Ogre::Quaternion qua(1.0, 0.0, 0.0, 0.0);

		Ogre::SceneNode *childScene = parent->createChildSceneNode(m_name + "/" + jname, vec, qua);
		//Ogre::SceneNode *childScene = parent->createChildSceneNode(vec, qua);

		MFNode *children = jNode->getChildren();

		for(int i = 0; i < children->count(); i++)
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

		Ogre::String name = sNode->getName()->getValue();
		//char tmp[128];
		//sprintf(tmp, "%s ,%s",m_name.c_str(),name.c_str());
		//MessageBox( NULL, tmp, "Error", MB_OK);

		//float a = cos(rot->rot()/2);
		//float b = sin(rot->rot()/2);
		//Ogre::Quaternion qua(a, rot->x() * b, rot->y() * b, rot->z() * b);
		Ogre::Quaternion qua(1.0f, 0.0f, 0.0f, 0.0f);

		Ogre::SceneNode *childScene = parent->createChildSceneNode(m_name + "/" + name, vec, qua);

		MFNode *children = sNode->getChildren();

		for(int i = 0; i < children->count(); i++)
		{
			CX3DNode *childNode = children->getNode(i);
			switch(childNode->getNodeType())
			{
			case TRANSFORM_NODE:
				{
					CX3DTransformNode *child = (CX3DTransformNode *)childNode;
					createTransform(childScene, child, mgr);
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
	bool SgvEntity::createObj(CX3DParser *parser, Ogre::SceneManager *mgr, bool currshape)
	{
		std::string fname = parser->getFileName();

		computeCenterOfMass(parser);

		Ogre::SceneNode *node = m_entHeadNode->createChildSceneNode(m_name + "/" + fname);

		MFNode *groupNodes = parser->searchNodesFromDirectChildrenOfRoot("Group");
		int groupNum = groupNodes->count();

		if (groupNum > 0)
		{
			for(int i = 0; i < groupNum; i++)
			{
				CX3DGroupNode *pGroup = (CX3DGroupNode *)(groupNodes->getNode(i));

				if (!pGroup)
				{
					CX3DParser::printLog("failed to get group node\n");
					break;
				}

				else
				{
					MFNode *transformNodes = pGroup->searchNodesFromDirectChildren("Transform");
					int transNum = transformNodes->count();

					if (transNum < 0)
					{
						CX3DParser::printLog("could not found Transform node\n");
						break;
					}

					for(int i = 0;i < transNum; i++)
					{
						CX3DTransformNode *pTrans = (CX3DTransformNode *)transformNodes->getNode(i);

						if(!createTransform(node, pTrans, mgr))
						{
							break;
						}
					}
				} // else
			}  // for(int i = 0; i < groupNodes->count(); i++)
		} // if (groupNodes)

		else
		{
			MFNode *transformNodes = parser->searchNodesFromDirectChildrenOfRoot("Transform");
			int transNum = transformNodes->count();

			if (transNum > 0)
			{
				for(int i = 0;i < transNum; i++)
				{
					CX3DTransformNode *pTrans = (CX3DTransformNode *)transformNodes->getNode(i);
					if(!createTransform(node, pTrans, mgr))
					{
						break; 
					}
				}
			} 
			else
			{
				MFNode *shape = parser->searchNodesFromDirectChildrenOfRoot("Shape");
				int shapeNum = shape->count();
				if(shapeNum > 0)
				{
					for(int i = 0; i < shapeNum; i++)
					{
						CX3DShapeNode *pShape = (CX3DShapeNode*)shape->getNode(i);
						if(!createShape(node, pShape, mgr))
						{
							break; 
						}
					}
				}
				else
				{
					return false;
				}
			}
		}
		if(currshape) {
			m_currShape = fname;
		}
		else node->setVisible(false);

		return true;
	}

	bool SgvEntity::createTransform(Ogre::SceneNode *parent, CX3DTransformNode *trans, Ogre::SceneManager *mgr)
	{

		SFVec3f    *vec = trans->getTranslation();
		SFRotation *rot = trans->getRotation();

		Ogre::Vector3 ovec(vec->x(), vec->y(), vec->z());

		float a = cos(rot->rot()/2);
		float b = sin(rot->rot()/2);
		Ogre::Quaternion qua(a, rot->x() * b, rot->y() * b, rot->z() * b);


		Ogre::SceneNode *node = (Ogre::SceneNode*)parent->createChild(ovec, qua);

		MFNode *shape = trans->searchNodesFromDirectChildren("Shape");

		if(!shape)
		{
			CX3DParser::printLog("failed to get shape node\n");
			return false;
		}

		else
		{
			for(int i = 0; i < shape->count(); i++)
			{
				CX3DShapeNode *pShape = (CX3DShapeNode *)shape->getNode(i);
				if(!pShape) 
				{
					CX3DParser::printLog("name: %s could not get shape node (%d th)\n",pShape->getNodeName(),i);
					break; 
				}
				else
				{
					if(!createShape(node, pShape, mgr))
					{
						break; 
					}
				}
			}
		}
		return true;
	} 

	bool SgvEntity::createShape(Ogre::SceneNode *parent, CX3DShapeNode *shape, Ogre::SceneManager *mgr)
	{

		bool isTexture = false;
		///////////////////////
		///////////////////////
		CX3DNode *geoNode = shape->getGeometry()->getNode();
		if(!geoNode)
		{
			CX3DParser::printLog("could not get geometry field");
			return false;
		}

		Ogre::Entity *entity;

		if(!(geoNode->getNodeType() == INDEXED_FACE_SET_NODE))
		{
			Ogre::Entity *ent = createSimpleShape(geoNode->getNodeType() ,parent, mgr, geoNode);

			if(!ent)  return false;
			entity = ent;
		}
		else
		{
			CX3DIndexedFaceSetNode *index_node = (CX3DIndexedFaceSetNode *)geoNode;
			if(index_node)
			{
				CX3DCoordinateNode *Coord = (CX3DCoordinateNode *)(index_node->getCoord()->getNode());
				if(Coord)
				{
					MFVec3f *coord = Coord->getPoint();

					int numPt = coord->count();

					MFVec3f *normal   = NULL; 
					MFVec2f *texCoord = NULL; 


					bool ccw = true;

					SFBool *sccw = index_node->getCCW();
					if(sccw != NULL)
						ccw = sccw->getValue();


					CX3DNormalNode *Normal = (CX3DNormalNode *)(index_node->getNormal()->getNode());
					if(Normal) 
					{
						normal = Normal->getVector();
					}
					else
					{
						CX3DParser::printLog("There is no normal vector.");
					}

					CX3DTextureCoordinateNode *TexCoord = (CX3DTextureCoordinateNode *)(index_node->getTexCoord()->getNode());
					if(TexCoord)
					{
						texCoord = TexCoord->getPoint();
					}
					else
					{
						CX3DParser::printLog("There is no normal texture coordinate.");
					}
					MFInt32 *coordIndex    = NULL; 
					MFInt32 *normalIndex   = NULL; 
					MFInt32 *texCoordIndex = NULL; 

					coordIndex = index_node->getCoordIndex();

					int numIndex = coordIndex->count();

					if (normal != NULL)
					{
						normalIndex = index_node->getNormalIndex();

						if(normalIndex->count() != numIndex)
							CX3DParser::printLog("Normal index value does not match with Coordinate Index.");

					}
					if (texCoord != NULL)
					{
						isTexture = true;
						texCoordIndex = index_node->getTexCoordIndex();

						if(texCoordIndex->count() != numIndex)
							CX3DParser::printLog("Texture index value does not match with Coordinate Index.");
					}

					float gx = 0.0f;
					float gy = 0.0f;
					float gz = 0.0f;

#define MEAN_OF_VERTEX_ROBOT
					if(m_isRobot) {
#ifdef MEAN_OF_VERTEX_ROBOT

						for(int i = 0; i < numPt; i++)
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
						float minx, miny, minz, maxx, maxy, maxz;
						minx = miny = minz = maxx = maxy = maxz = 0;
						for(int i = 0; i < numPt; i++)
						{
							SFVec3f vec = coord->getValue(i);
							if(vec.x() > maxx) { maxx = vec.x(); }
							if(vec.x() < minx) { minx = vec.x(); }

							if(vec.y() > maxy) { maxy = vec.y(); }
							if(vec.y() < miny) { miny = vec.y(); }

							if(vec.z() > maxz) { maxz = vec.z(); }
							if(vec.z() < minz) { minz = vec.z(); }
						}
						gx = (maxx + minx) / 2;
						gy = (maxy + miny) / 2;
						gz = (maxz + minz) / 2;
#endif
					}

					int numPol = 0;
					for(int i = 0; i < numIndex; i++)
					{
						int ind = coordIndex->getValue(i);
						if(ind == -1) numPol++;
					}
					CX3DParser::printLog("Polygon Number = %d\n",numPol);
					int *nkaku;
					nkaku = (int *)malloc(sizeof(int) * numPol);
					int j = 0;
					int d = 0;

					for(int i = 0; i < numIndex; i++)
					{
						int ind = coordIndex->getValue(i);
						if(ind == -1) 
						{
							nkaku[d] = j;
							d++;
							j = 0;
						}
						else j++;
					}
					//Ogre::String ind_name = getObjName();
					Ogre::ManualObject *mObj = mgr->createManualObject(getObjName());


					mObj->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);

					int nIndex = 0;
					int posIndex = 0;
					for(int i = 0; i < numPol; i++)
					{
						int nk = nkaku[i];

						for(int j = 0; j < nk; j++)
						{

							SFVec3f vec = coord->getValue(coordIndex->getValue(nIndex + j));

							if(m_isRobot)
								mObj->position(vec.x() - gx, vec.y() - gy, vec.z() - gz);
							else
								mObj->position((vec.x() - m_gx)/SCALE_PARAM, (vec.y() - m_gy)/SCALE_PARAM, (vec.z() - m_gz)/SCALE_PARAM);
								//mObj->position(vec.x(), vec.y(), vec.z());

							//CX3DParser::printLog("nind = %d\n vecx = %f, vecy = %f, vecz = %f\n",nind, vec.x(), vec.y(), vec.z());

							if(normal != NULL && normalIndex != NULL)
							{
								SFVec3f nvec = normal->getValue(normalIndex->getValue(nIndex + j));
								mObj->normal(nvec.x(), nvec.y(), nvec.z());
							}
							else
							{
								if(j == 0)
								{
									CX3DParser::printLog("normal automatically.\n");
									SFVec3f vec0 = vec;
									SFVec3f vec1 = coord->getValue(coordIndex->getValue(nIndex + 1));
									SFVec3f vec2 = coord->getValue(coordIndex->getValue(nIndex + 2));

									float a1 =  vec1.x() - vec0.x();
									float a2 =  vec1.y() - vec0.y();
									float a3 =  vec1.z() - vec0.z();

									float b1 =  vec2.x() - vec1.x();
									float b2 =  vec2.y() - vec1.y();
									float b3 =  vec2.z() - vec1.z();

									float x_ = a2*b3 - a3*b2;
									float y_ = a3*b1 - a1*b3;
									float z_ = a1*b2 - a2*b1;

									mObj->normal(x_, y_, z_);
								}
							}

							if(texCoord != NULL && texCoordIndex != NULL)
							{
								SFVec2f tvec = texCoord->getValue(texCoordIndex->getValue(nIndex + j));
								mObj->textureCoord(tvec.x(),1.0f - tvec.y());
								//CX3DParser::printLog("texture x = %f y = %f\n",tvec.x(), tvec.y());
							}
						}

						if(nk == 3)
						{
							if(!ccw)
							{

								mObj->triangle(posIndex + 2, posIndex + 1, posIndex );
								CX3DParser::printLog("ccw is true\n");
							}
							else
							{
								mObj->triangle(posIndex, posIndex + 1, posIndex + 2);
							}
						}

						else if(nk == 4)
						{
							if(!ccw)
							{
								mObj->quad(posIndex + 3, posIndex + 2, posIndex + 1, posIndex );
								CX3DParser::printLog("ccw is true\n");
							}
							else
							{
								mObj->quad(posIndex, posIndex + 1, posIndex + 2, posIndex + 3);
							}
						}

						else if (4 < nk && nk < MAX_POINTS_PER_ONE_POLYGON)
						{
							for(int j = 0; j < nk - 2; j++)
							{
								//CX3DParser::printLog("more than 4 vertex.\n");
								if(!ccw)
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
				} // if(coord)
				else
				{
					CX3DParser::printLog("** ERROR ** IndexedFaceSet has no coord.\n");
					return false;
				}
			}

		}
		///////////////////////
		///////////////////////
		CX3DAppearanceNode *appNode = (CX3DAppearanceNode*)(shape->getAppearance()->getNode());
		if(!appNode)
		{
			CX3DParser::printLog("could not get Appearance Node");
			return false; 
		}

		// 
		else
		{
			Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("BaseWhiteNoLighting");
			//Ogre::MaterialPtr tmp = Ogre::MaterialManager::getSingleton().getByName("MA_White");

			Ogre::MaterialPtr mptr = tmp->clone(entity->getName());
			CX3DParser::printLog("Create material name : %s\n", entity->getName().c_str());
			mptr->setLightingEnabled(true);

			/////////////////
			/////////////////
			CX3DImageTextureNode *pImageTexNode = (CX3DImageTextureNode *)(appNode->getTexture()->getNode());
			if(pImageTexNode)
			{
				MFString *texFiles = pImageTexNode->getUrl();
				if(texFiles->count() > 0)
				{
					const char *texFile = texFiles->getValue(0);

					char* filename = getFileName((char*)texFile);
					CX3DParser::printLog("Texture file : %s\n", filename);

					mptr->getTechnique(0)->getPass(0)->createTextureUnitState(filename);
				}
			}
			/////////////////
			/////////////////
			CX3DMaterialNode *matNode = (CX3DMaterialNode*)(appNode->getMaterial()->getNode());
			if(matNode)
			{
				SFColor *dcol = matNode->getDiffuseColor();
				SFColor *scol = matNode->getSpecularColor();
				SFColor *ecol = matNode->getEmissiveColor();

				SFFloat *flo = matNode->getTransparency();
				float a = flo->getValue();

				float amb = matNode->getAmbientIntensity()->getValue();
				if(amb == 0.0) amb = 0.8f;

				if(a == 0.0) a = 1.0f;
				else{

					a = 1.0f - a;
					mptr->setDepthCheckEnabled(true);
					mptr->setDepthWriteEnabled(false);
					mptr->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
				}
				mptr->setDiffuse(dcol->r(), dcol->g(), dcol->b(), a);

				//CX3DParser::printLog("color (%f, %f, %f)", col->r(), col->g(), col->b());

				mptr->setSpecular(scol->r(), scol->g(), scol->b(), a);

				mptr->setSelfIllumination(ecol->r(), ecol->g(), ecol->b());

				SFFloat *shi = matNode->getShininess();
				float shin = shi->getValue();
				mptr->setShininess(shin);

				mptr->setColourWriteEnabled(true);
				if(!isTexture)
					mptr->setAmbient(0.8f * dcol->r(), 0.8f * dcol->g(), 0.8f * dcol->b());
					//mptr->setAmbient(col->r(), col->g(), col->b());
					//mptr->setAmbient(amb * dcol->r(), amb * dcol->g(), amb * dcol->b());

			}
			entity->setMaterial(mptr);
		}

		parent->attachObject(entity);

		m_allEntities.push_back(entity);

		return true;
	} 

	Ogre::Entity *SgvEntity::createSimpleShape(int type, Ogre::SceneNode *parent, Ogre::SceneManager *mgr, CX3DNode *geo)
	{

		Ogre::Entity *ent;
		switch (type)
		{
		case SPHERE_NODE:
			{
				CX3DSphereNode *sphereNode = (CX3DSphereNode*)geo;
				float rad = sphereNode->getRadius()->getValue();

				parent->scale(Ogre::Vector3(rad));

				ent = mgr->createEntity(getObjName(),"MySphere.mesh");
				break;
			}
			//box
		case BOX_NODE:
			{
				CX3DBoxNode *boxNode = (CX3DBoxNode*)geo;
				float x, y, z;
				boxNode->getSize()->getValue(x, y, z);
				parent->scale(Ogre::Vector3(x,y,x));

				ent = mgr->createEntity(getObjName(),"MyBox.mesh");
				break;
			}
		case CYLINDER_NODE:
			{
				CX3DCylinderNode *cylinderNode = (CX3DCylinderNode*)geo;
				float rad = cylinderNode->getRadius()->getValue();
				float height = cylinderNode->getHeight()->getValue();
				parent->scale(Ogre::Vector3(rad, rad, height));

				ent = mgr->createEntity(getObjName(),"MyCylinder.mesh");
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

#define MEAN_OF_VERTEX



	bool SgvEntity::computeCenterOfMass(CX3DParser *parser)
	{
		MFNode *ind = parser->searchNodesFromAllChildrenOfRoot("IndexedFaceSet");
		int indsize = ind->count();
		float minx, miny, minz, maxx, maxy, maxz;
		minx = miny = minz = maxx = maxy = maxz = 0;

		for(int i = 0; i < indsize; i++) {
			CX3DIndexedFaceSetNode *index_node = (CX3DIndexedFaceSetNode *)ind->getNode(i);
			CX3DCoordinateNode *Coord = (CX3DCoordinateNode *)(index_node->getCoord()->getNode());

			MFVec3f *coord = Coord->getPoint();

			int numPt = coord->count();

#ifdef MEAN_OF_VERTEX
			for(int i = 0; i < numPt; i++)
			{
				SFVec3f vec = coord->getValue(i);
				m_gx += vec.x();
				m_gy += vec.y();
				m_gz += vec.z();
			}

			m_gx /= numPt;
			m_gy /= numPt;
			m_gz /= numPt;
		}


#else
			for(int i = 0; i < numPt; i++)
			{
				SFVec3f vec = coord->getValue(i);
				if     (vec.x() > maxx) { maxx = vec.x(); }
				else if(vec.x() < minx) { minx = vec.x(); }

				if     (vec.y() > maxy) { maxy = vec.y(); }
				else if(vec.y() < miny) { miny = vec.y(); }

				if     (vec.z() > maxz) { maxz = vec.z(); }
				else if(vec.z() < minz) { minz = vec.z(); }
			}

		}
		m_gx = (maxx + minx) / 2;
		m_gy = (maxy + miny) / 2;
		m_gz = (maxz + minz) / 2;

#endif
		CX3DParser::printLog("position :%f, %f, %f", m_gx, m_gy, m_gz);

		return true;
	}

	void SgvEntity::changeShape(Ogre::String shape)
	{
		Ogre::String pathShape = "./shape/" + shape;
		if(shape == m_currShape) return;

		if(m_currShape != "./shape/nothing"){
			Ogre::SceneNode *curr = (Ogre::SceneNode*)(m_entHeadNode->getChild(m_name + "/" + m_currShape));
			curr->setVisible(false);
		}

		if(shape != "./shape/nothing"){
			Ogre::SceneNode *chan = (Ogre::SceneNode*)(m_entHeadNode->getChild(m_name + "/" + pathShape));
			chan->setVisible(true);
		}

		m_currShape = pathShape;
	}
}

