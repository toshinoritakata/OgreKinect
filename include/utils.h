//
// Codelight Ogre Utils
//
#pragma once

#include "Ogre.h"

namespace Codelight {
	namespace Utils {
		Ogre::ManualObject* createBGMesh(int num, Ogre::String name, Ogre::String materialName) 
		{
			Ogre::ManualObject* grid = new Ogre::ManualObject(name);
			grid->setUseIdentityProjection(true);
			grid->setUseIdentityView(true);
			grid->begin(materialName);
			const int meshNum = 2;
			for (int yn = 0; yn < meshNum; yn++) {
				float yp = (1.0f/(meshNum-1))*yn;
				for (int xn = 0; xn < meshNum; xn++) {
					float xp = (1.0f/(meshNum-1))*xn;

					float xs = xp*2.0f-1.0f;
					float ys = yp*2.0f-1.0f;
					grid->position(xs, ys, 0.0);
					grid->textureCoord(xp, 1.-yp);
				}
			}

			for (int yn = 0; yn < meshNum-1; yn++) {
				for (int xn = 0; xn < meshNum-1; xn++) {
					int id0 = yn * meshNum + xn;
					int id1 = id0+1;
					int id2 = id1+meshNum;			
					grid->index(id0);
					grid->index(id1);
					grid->index(id2);

					id1 = id2;
					id2 = id1-1;
					grid->index(id0);
					grid->index(id1);
					grid->index(id2);
				}
			}
			grid->end();
			grid->setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
			grid->setRenderQueueGroup(Ogre::RENDER_QUEUE_BACKGROUND);

			return grid;
			//sceneMgr->getRootSceneNode()->createChildSceneNode(name)->attachObject(grid);
		}
	}
}
