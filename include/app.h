#ifndef __KinectApp_H__
#define __KinectApp_H__

#include <cv.h>
#include <highgui.h>
#include <Ogre.h>
#include <OIS/OIS.h>
#include <SdkTrays.h>
#include "InputManager.h"
#include "KinectFrameListener.h"

class KinectApp : 
	public Ogre::FrameListener,
	public OIS::MouseListener, 
	public OIS::KeyListener, 
	public Ogre::WindowEventListener,
	public OgreBites::SdkTrayListener
{
private:
	Ogre::Root* ogreRoot_;
	Ogre::Camera* camera_;
	Ogre::RenderWindow* renderWin_;
	Ogre::SceneManager* sceneMgr_;
	Ogre::Viewport* viewport;
	Ogre::SceneNode* kaeruNode_;
	Ogre::SceneNode* cameraNode_;
	Ogre::SceneNode* axisNode_;
	Ogre::ManualObject* axisMObj_;
	OgreBites::SdkTrayManager* trayMgr_;
	
	InputManager* inputMgr_;
	KinectFrameListener* kinectListener;

	void parseResources();
	void loadInputSystem();
	void updateStats();

public:
	bool shouldQuit;
	KinectApp();
	~KinectApp();

	void go();
	void setup();
	void createScene();
	
	virtual bool mouseMoved(const OIS::MouseEvent &arg);
	virtual bool mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id);
	virtual bool mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id);
	virtual bool keyPressed( const OIS::KeyEvent &arg );
	virtual bool keyReleased( const OIS::KeyEvent &arg );
	virtual void windowMoved(Ogre::RenderWindow* rw);
	virtual void windowResized(Ogre::RenderWindow* rw);
	virtual void windowClosed(Ogre::RenderWindow* rw);
	virtual void windowFocusChange(Ogre::RenderWindow* rw);
	virtual bool frameStarted(const Ogre::FrameEvent& evt);
	virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);
	virtual bool frameEnded(const Ogre::FrameEvent& evt) { return true; }
};

#endif // __KinectApp_H__