#include "KinectFrameListener.h"
#include "AR/gsub.h"
#include "AR/param.h"
#include "AR/gsubUtil.h"

DWORD WINAPI KinectFrameListener::ProcessThreadFunc(LPVOID pParam)
{
	KinectFrameListener* pthis = (KinectFrameListener *)pParam;
	HANDLE	hEvents[4];
	int		nEventIdx, t, dt;

	hEvents[0] = pthis->m_hEvNuiProcessStop;
	hEvents[1] = pthis->m_hNextDepthFrameEvent;
	hEvents[2] = pthis->m_hNextVideoFrameEvent;
	hEvents[3] = pthis->m_hNextSkeletonEvent;

	while (1)
	{	
		// Wait for an event to be signalled
		nEventIdx = WaitForMultipleObjects(sizeof(hEvents)/sizeof(hEvents[0]), hEvents, FALSE, 100);

		// If the stop event, stop looping and exit
		if (nEventIdx == 0) 
			break;

		// Perform PFS processing
		t = timeGetTime();
		if (pthis->m_LastFPStime == -1) {
			pthis->m_LastFPStime = t;
			pthis->m_LastFramesTotal = pthis->m_FramesTotal;
		}

		dt = t - pthis->m_LastFPStime;
		if (dt > 1000) {
			pthis->m_LastFPStime = t;
			int FrameDelta = pthis->m_FramesTotal - pthis->m_LastFramesTotal;
			pthis->m_LastFramesTotal = pthis->m_FramesTotal;
		}

		if (pthis->m_LastSkeletonFoundTime == -1)
			pthis->m_LastSkeletonFoundTime = t;

		dt = t - pthis->m_LastSkeletonFoundTime;
		if (dt > 250) {
			if (!pthis->m_bScreenBlanked) {
				//pthis->BlankSkeletonScreen();
				pthis->m_bScreenBlanked = true;
			}
		}

		//Process signal events
		switch(nEventIdx) {
		case 1:
			pthis->GotDepthAlert();
			pthis->m_FramesTotal++;
			break;
		
		case 2:
			pthis->GotVideoAlert();
			break;

		case 3:
			pthis->GotSkeletonAlert();
			break;

		default:
			break;
		}
	}

	return 0;
}

