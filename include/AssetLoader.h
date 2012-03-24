#pragma once

#include "Ogre.h"
#include "Ogre/OgreSingleton.h"
#include "assimp.hpp"        // C++ importer interface
#include "aiPostProcess.h" // Output data structure
#include "aiScene.h"       // Post processing files
#include "DefaultLogger.h"

class AssetLoader : public Ogre::Singleton<AssetLoader>
{
public:
	AssetLoader();
	~AssetLoader();
	Ogre::SceneNode* readAsset(Ogre::SceneManager* sceneMgr, const char* assetFileName, const char* assetName);

	static AssetLoader& getSingleton(void);
	static AssetLoader* getSingletonPtr(void);

private:
	Ogre::MaterialPtr createMaterial(Ogre::String name, int index, aiMaterial* mat);
	void readModel(Ogre::SceneManager* sceneMgr, Ogre::SceneNode* node, const aiScene* scene, aiNode* nd);
	void createCamera(Ogre::SceneManager* sceneMgr, const aiScene* scene, Ogre::String name);
	std::string getFullPathName(const aiNode* node);

	Ogre::SceneNode* createChildSceneNodeWithTransform(Ogre::SceneNode* scnNode, const aiNode* ainode);
};