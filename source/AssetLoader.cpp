#include "AssetLoader.h"

#include <list>
#include <boost/format.hpp>

template<> AssetLoader* Ogre::Singleton<AssetLoader>::ms_Singleton = 0;


AssetLoader* AssetLoader::getSingletonPtr(void)
{
	return ms_Singleton;
}

AssetLoader& AssetLoader::getSingleton(void)
{
	assert(ms_Singleton);
	return *ms_Singleton;
}

AssetLoader::AssetLoader()
{
	Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
}

AssetLoader::~AssetLoader() 
{
}

/*! アセットファイルの読み込みを行う
@param[in] sceneMgr
@param[in] assetFileName
@param[in] assetName
*/
Ogre::SceneNode* AssetLoader::createChildSceneNodeWithTransform(Ogre::SceneNode* scnNode, const aiNode* ainode)
{	
	Ogre::SceneNode *node = scnNode->createChildSceneNode(getFullPathName(ainode)); //std::string(ainode->mName.data)+"_Node");

	aiVector3D scl;
	aiQuaternion rot;
	aiVector3D pos;

	ainode->mTransformation.Decompose(scl, rot, pos);
	node->setOrientation(rot.w, rot.x, rot.y, rot.z);
	node->setPosition(pos.x, pos.y, pos.z);
	node->setScale(scl.x, scl.y, scl.z);

	return node;
}

Ogre::SceneNode* AssetLoader::readAsset(Ogre::SceneManager* sceneMgr, const char* assetFileName, const char* assetName)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(assetFileName, aiProcessPreset_TargetRealtime_Quality|aiProcess_FlipUVs);
	if (!scene) {
		return false;
	}

//	this->createCamera(sceneMgr, Ogre::String("camera1"));

	Ogre::SceneNode* rootNode = sceneMgr->getRootSceneNode();
	Ogre::SceneNode* scnNode = rootNode->createChildSceneNode(assetName);

	for (size_t i = 0; i < scene->mRootNode->mNumChildren; i++)
	{	
		aiNode* cnd = scene->mRootNode->mChildren[i];
		Ogre::SceneNode* node = createChildSceneNodeWithTransform(scnNode, cnd);

		readModel(sceneMgr, node, scene, cnd);
	}

	return scnNode;
}

Ogre::MaterialPtr AssetLoader::createMaterial(Ogre::String name, int index, aiMaterial* mat)
{
	Ogre::MaterialManager* omatMgr = Ogre::MaterialManager::getSingletonPtr();

	std::ostringstream matname;
	matname << name.data() << "_mat";
	matname.widen(4);
	matname.fill('0');
	matname << index;
	Ogre::String matName = matname.str();

	Ogre::ResourceManager::ResourceCreateOrRetrieveResult status = omatMgr->createOrRetrieve(matName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);
	Ogre::MaterialPtr omat = status.first;

	aiColor4D clr(1.0, 1.0, 1.0, 1.0);
	aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &clr);
	omat->getTechnique(0)->getPass(0)->setDiffuse(clr.r, clr.g, clr.b, clr.a);
	
	aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &clr);
	omat->getTechnique(0)->getPass(0)->setSpecular(clr.r, clr.g, clr.b, clr.a);
	
	aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &clr);
	omat->getTechnique(0)->getPass(0)->setAmbient(clr.r, clr.g, clr.b);
	
	aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &clr);
	omat->getTechnique(0)->getPass(0)->setSelfIllumination(clr.r, clr.g, clr.b);

	omat->getTechnique(0)->getPass(0)->setShadingMode(Ogre::SO_GOURAUD);
	omat->getTechnique(0)->getPass(0)->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
	omat->getTechnique(0)->getPass(0)->setLightingEnabled(true);
	omat->getTechnique(0)->getPass(0)->setDepthCheckEnabled(true);
	//omat->getTechnique(0)->getPass(0)->setVertexColourTracking(Ogre::TVC_DIFFUSE);
	
	aiString path;
	aiReturn res = mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
	if (res == AI_SUCCESS) {
		Ogre::TextureUnitState* stateBase = omat->getTechnique(0)->getPass(0)->createTextureUnitState();
		stateBase->setColourOperation(Ogre::LBO_MODULATE);
		stateBase->setTextureName(path.data);
	}
	
	return omat;
}


std::string AssetLoader::getFullPathName(const aiNode* node)
{
	std::list<std::string> path;
	path.push_front(std::string(node->mName.data));

	aiNode* parentNode = node->mParent;
	if (parentNode) {
		while (true) {
			if (parentNode) {
				path.push_front(boost::str(boost::format("%s_") % parentNode->mName.data));
				parentNode = parentNode->mParent;
			}
			else {
				break;
			}
		}
	}

	std::string name;
	for (std::list<std::string>::iterator itr = path.begin(); itr != path.end(); itr++)
	{
		name += static_cast<std::string>(*itr);
	}

	return name;
}

void AssetLoader::readModel(Ogre::SceneManager* sceneMgr, Ogre::SceneNode* scnNode, const aiScene* scene, aiNode* nd)
{
	for (size_t n = 0; n < nd->mNumChildren; n++) 
	{	
		aiNode* cnd = nd->mChildren[n];
		Ogre::SceneNode* cnode = createChildSceneNodeWithTransform(scnNode, cnd);

		for (size_t i = 0; i < cnd->mNumMeshes; i++) {
			aiMesh* m = scene->mMeshes[cnd->mMeshes[i]];	
			aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];

			std::string nodeName = getFullPathName(cnd);
			Ogre::MaterialPtr omat = createMaterial(Ogre::String(nodeName), m->mMaterialIndex, mat);

			aiVector3D* vec  = m->mVertices;
			aiVector3D* norm = m->mNormals;
			aiVector3D* uv   = m->mTextureCoords[0];
			aiColor4D*  vcol = NULL;	
			if (m->HasVertexColors(0)) vcol = m->mColors[0];

			// 頂点情報の読み込み
			Ogre::AxisAlignedBox aab;
			Ogre::ManualObject* mobj = new Ogre::ManualObject(nodeName+"_MObj");
			mobj->begin(omat->getName());
			//mobj->begin("Ogre/Skin");
			for (size_t n = 0; n < m->mNumVertices; n ++) {
				Ogre::Vector3 position(vec->x, vec->y, vec->z);
				aab.merge(position);

				mobj->position(vec->x, vec->y, vec->z); 
				vec++;  

				mobj->normal(norm->x, norm->y, norm->z);
				norm++; 

				if (uv)   { 
					mobj->textureCoord(uv->x, uv->y);        
					uv++;   
				}
				if (vcol) { 
					mobj->colour(vcol->r, vcol->g, vcol->b, vcol->a); 
					vcol++;
				}
			}

			// ポリゴンの構築
			for (size_t n = 0; n < m->mNumFaces; n++) {
				aiFace* face = &m->mFaces[n];
				for (size_t k = 0; k < face->mNumIndices; k++) {
					mobj->index(face->mIndices[k]);
				}
			}
			mobj->end();
			mobj->setBoundingBox(aab);

			Ogre::String meshName(nodeName+"_Mesh");
			mobj->convertToMesh(meshName);
			delete mobj;

			Ogre::String entityName(nodeName+"_Entity");
			std::cout << "entity: " << entityName << std::endl;
			Ogre::Entity* ent = sceneMgr->createEntity(entityName, meshName);
			cnode->attachObject(ent);
		}

		readModel(sceneMgr, cnode, scene, cnd);
	}
}

void AssetLoader::createCamera(Ogre::SceneManager* sceneMgr, const aiScene* scene, Ogre::String camName)
{
	for (size_t n = 0; n < scene->mNumCameras; n++)
	{
		// カメラを作成
		Ogre::Camera* cam = sceneMgr->createCamera(scene->mCameras[n]->mName.data);
		std::cout << "Create Camra " << cam->getName() << " " << scene->mCameras[n]->mHorizontalFOV << std::endl;
		cam->setFOVy(Ogre::Radian(scene->mCameras[n]->mHorizontalFOV));
		
		// 視点アニメーション用ノード
		Ogre::SceneNode* camNode = sceneMgr->getRootSceneNode()->createChildSceneNode(cam->getName()+"CamNode");
		camNode->attachObject(cam);

		// アニメーションを走査
		for (size_t na = 0; na < scene->mNumAnimations; na++) {
			aiAnimation* aiani = scene->mAnimations[na];
			for (size_t nc = 0; nc < aiani->mNumChannels; nc++) {
				// カメラと同じ名前のチャネルを取得する
				if (Ogre::String(scene->mCameras[n]->mName.data) == cam->getName()) {
					
					//アニメーションを付けるトラックを作成しておく
					Ogre::Animation* ogani = sceneMgr->createAnimation(cam->getName()+"Animation", aiani->mDuration);
					std::cout << "Animation : " << ogani->getName() << std::endl; 
					Ogre::NodeAnimationTrack* track = ogani->createNodeTrack(0, camNode);
					ogani->setInterpolationMode(Ogre::Animation::IM_LINEAR);

					// アニメーションチャネルからキーフレームアニメーションを取得
					aiNodeAnim* chan = aiani->mChannels[n];
					for (size_t np = 0; np < chan->mNumPositionKeys; np++) {						
						aiVectorKey* vk = &(chan->mPositionKeys[np]);
						Ogre::TransformKeyFrame* key = track->createNodeKeyFrame(vk->mTime);
						key->setTranslate(Ogre::Vector3(vk->mValue[0], vk->mValue[1], vk->mValue[2]));

						aiQuatKey* qk = &(chan->mRotationKeys[np]);
						key->setRotation(Ogre::Quaternion(qk->mValue.w, qk->mValue.x, qk->mValue.y, qk->mValue.z));
					}

					// 管理するアニメーションの名前を付けておく
					Ogre::AnimationState* aniState = sceneMgr->createAnimationState(ogani->getName());
					aniState->setEnabled(true);
					aniState->setLoop(true);
					aniState->setTimePosition(0.0);

					//ループを抜ける
					na = scene->mNumAnimations;
					break;
				}
			}
		}
	}
}