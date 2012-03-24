#include "app.h"
#include <windows.h>

int main()
{
	try
	{
		KinectApp app;
		app.go();
	}
	catch(Ogre::Exception& e)
	{
		ShowCursor(true);
		MessageBox(NULL, e.getFullDescription().c_str(), "An exception has occured!", MB_OK | MB_ICONERROR | MB_TASKMODAL);
	}

	return 0;
}