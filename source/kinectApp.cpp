#include "app.h"
#include "utils.h"
#include "AssetLoader.h"

KinectApp::KinectApp() : 
	ogreRoot_(new Ogre::Root()), 
	shouldQuit(false), 
	renderWin_(0), 
	sceneMgr_(0), 
	inputMgr_(0)
{	
}

KinectApp::~KinectApp()
{
	Ogre::WindowEventUtilities::removeWindowEventListener(renderWin_, this);
	delete kinectListener;

	delete trayMgr_;
	delete inputMgr_;
	delete ogreRoot_;
}

/*! 初期化を行う
*/
void KinectApp::setup()
{
//	shouldQuit = !ogreRoot_->showConfigDialog();
//	if(shouldQuit) return;

	Ogre::RenderSystem* renderingSystem = 0;
	Ogre::RenderSystemList rsl = ogreRoot_->getAvailableRenderers();
	Ogre::RenderSystemList::const_iterator iter;
	for (iter = rsl.begin(); iter != rsl.end(); ++iter) {
		std::cout << (*iter)->getName() << std::endl;		
		if ((*iter)->getName() == "Direct3D9 Rendering Subsystem") {
			renderingSystem = *iter;
		}
	}

	if (renderingSystem == 0) {
		exit(0);
	}

	Ogre::ConfigOptionMap cmap = renderingSystem->getConfigOptions();
	std::cout << "Rendering Device: " << cmap["Rendering Device"].currentValue << std::endl;

	renderingSystem->setConfigOption("Video Mode", "1024 x 768 @ 32-bit colour");
	renderingSystem->setConfigOption("Full Screen", "No");
	renderingSystem->setConfigOption("VSync", "No");
	renderingSystem->setConfigOption("Allow NVPerfHUD", "No");
	//renderingSystem->setConfigOption("Anti aliasing", "None");
	renderingSystem->setConfigOption("Floating-point mode", "Fastest");
	renderingSystem->setConfigOption("sRGB Gamma Conversion", "No");


	ogreRoot_->setRenderSystem(renderingSystem);
	renderWin_ = ogreRoot_->initialise(true, "Kinect App");
	renderWin_->setDeactivateOnFocusChange(false); // フォーカスが外れても更新を続けるように
	sceneMgr_ = ogreRoot_->createSceneManager("DefaultSceneManager");
	Ogre::WindowEventUtilities::addWindowEventListener(renderWin_, this);

	Ogre::ResourceGroupManager::getSingleton().addResourceLocation("../../media/packs/SdkTrays.zip", "Zip", "Essential");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation("../../media/materials/textures", "FileSystem", "General");
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation("../../media/materials/myscripts", "FileSystem", "General");
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
	
	loadInputSystem();
	createScene();

	kinectListener = new KinectFrameListener();
	ogreRoot_->addFrameListener(kinectListener);

	trayMgr_ = new OgreBites::SdkTrayManager("InterfaceName", renderWin_, inputMgr_->getMouse(), this);
	trayMgr_->showFrameStats(OgreBites::TL_BOTTOMLEFT);
	trayMgr_->hideCursor();

	ogreRoot_->addFrameListener(this);
}

void KinectApp::go()
{
	setup();

	ogreRoot_->startRendering();
}

void KinectApp::createScene()
{
	// 背景の板を作成する Depth
	{
		Ogre::ManualObject* mesh = Codelight::Utils::createBGMesh(1, "DepthPlane", "Plane/DepthMat");
		Ogre::SceneNode* node = sceneMgr_->getRootSceneNode()->createChildSceneNode("DepthPlane");
		node->attachObject(mesh);
		node->setScale(0.5, 0.5, 1.0);
		node->setPosition(0.5, 0.5, 0.0);
	}
	
	// 背景の板を作成する Color
	{
		Ogre::ManualObject* mesh = Codelight::Utils::createBGMesh(1, "ColorPlane", "Plane/ColorMat");
		Ogre::SceneNode* node = sceneMgr_->getRootSceneNode()->createChildSceneNode("ColorPlane");
		node->attachObject(mesh);
	}

	camera_ = sceneMgr_->createCamera("MainCam");
	viewport = renderWin_->addViewport(camera_);	
	camera_->setPosition(Ogre::Vector3(0, 0, 0));
	camera_->lookAt(Ogre::Vector3(0, 0, 1));
	camera_->setNearClipDistance(0.5);
	camera_->setFOVy(Ogre::Radian(Ogre::Degree(42.0)));
	camera_->setAspectRatio((float)viewport->getActualWidth() / (float)viewport->getActualHeight());

	cameraNode_ = sceneMgr_->getRootSceneNode()->createChildSceneNode("cameraNode");
	cameraNode_->setPosition(0, 0, 0);
	cameraNode_->lookAt(Ogre::Vector3(0, 0, -1), Ogre::Node::TS_WORLD);
	cameraNode_->attachObject(camera_);
	cameraNode_->setFixedYawAxis(true, Ogre::Vector3::UNIT_Y);

	Ogre::Light* light = sceneMgr_->createLight("MainLight");
	light->setType(Ogre::Light::LT_DIRECTIONAL);
	light->setDirection(Ogre::Vector3(-1, 1, -1));
	light->setDiffuseColour(Ogre::ColourValue::White);

	sceneMgr_->setAmbientLight(Ogre::ColourValue(0.2f, 0.2f, 0.2f));
	
	axisMObj_ = sceneMgr_->createManualObject("AxisObj");
	axisMObj_->setUseIdentityView(true);
	axisMObj_->setUseIdentityProjection(true);
	axisMObj_->setDynamic(true);
	axisMObj_->begin("Matte/Red", Ogre::RenderOperation::OT_LINE_LIST);

	axisMObj_->position( 0,  0,  0);
	axisMObj_->colour(Ogre::ColourValue::Red);
	axisMObj_->position(0.25,  0,  0);
	axisMObj_->colour(Ogre::ColourValue::Red);

	axisMObj_->position( 0,  0,  0);
	axisMObj_->colour(Ogre::ColourValue::Green);
	axisMObj_->position( 0, 0.25,  0);
	axisMObj_->colour(Ogre::ColourValue::Green);

	axisMObj_->end();
	axisMObj_->setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
	axisNode_ = sceneMgr_->getRootSceneNode()->createChildSceneNode("AxisNode");
	axisNode_->attachObject(axisMObj_);
}

bool KinectApp::frameStarted(const Ogre::FrameEvent& evt)
{
	const Ogre::Vector3* v = kinectListener->getSkeletonPoints();
//	std::cout << v[NUI_SKELETON_POSITION_HEAD] << std::endl;
	axisNode_->setPosition(v[NUI_SKELETON_POSITION_HAND_RIGHT]);

	return true;
}

bool KinectApp::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
	if (shouldQuit) 
		return false;

	inputMgr_->capture();
	trayMgr_->frameRenderingQueued(evt);

	const Ogre::RenderTarget::FrameStats& stats = renderWin_->getStatistics();


	return true;
}

void KinectApp::loadInputSystem()
{
	inputMgr_ = InputManager::getSingletonPtr();
    inputMgr_->initialise(renderWin_);
    inputMgr_->addMouseListener(this, "MouseListener");
	inputMgr_->addKeyListener(this, "KeyListener");
}

bool KinectApp::mouseMoved(const OIS::MouseEvent &arg)
{
	return true;
}

bool KinectApp::mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	return true;
}

bool KinectApp::mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	return true;
}

bool KinectApp::keyPressed( const OIS::KeyEvent &arg )
{
	return true;
}

bool KinectApp::keyReleased( const OIS::KeyEvent &arg )
{

	switch(arg.key)
	{
	case OIS::KC_ESCAPE:
		shouldQuit = true;
		break;
	case OIS::KC_F1:
	{
		const Ogre::RenderTarget::FrameStats& stats = renderWin_->getStatistics();
		Ogre::LogManager::getSingleton().logMessage("Current FPS: " + Ogre::StringConverter::toString(stats.lastFPS));
		Ogre::LogManager::getSingleton().logMessage("Triangle Count: " + Ogre::StringConverter::toString(stats.triangleCount));
		Ogre::LogManager::getSingleton().logMessage("Batch Count: " + Ogre::StringConverter::toString(stats.batchCount));
		break;
	}
	case OIS::KC_F2:
		sceneMgr_->getRootSceneNode()->flipVisibility(true);
		break;
	}

	return true;
}

void KinectApp::windowMoved(Ogre::RenderWindow* rw) {}

void KinectApp::windowResized(Ogre::RenderWindow* rw) 
{
	inputMgr_->setWindowExtents(rw->getWidth(), rw->getHeight());
}

void KinectApp::windowClosed(Ogre::RenderWindow* rw) 
{
	shouldQuit = true;
}

void KinectApp::windowFocusChange(Ogre::RenderWindow* rw) {}
