#ifndef __CameraListener_h_
#define __CameraListener_h_

#include <cv.h>
#include <highgui.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreRenderWindow.h>
#include <OgreMaterialManager.h>
#include <OgreHardwarePixelBuffer.h>
		
// kinect
#include "NuiApi.h"
#include "NuiSkeleton.h"

class KinectFrameListener : public Ogre::FrameListener
{
public:
	KinectFrameListener()
	{
		setupKinect();
		createBackgroundTexture();
	}

	virtual ~KinectFrameListener()
	{
		if (m_hEvNuiProcessStop != NULL) {
			SetEvent(m_hEvNuiProcessStop);

			if (m_hThNuiProcess != NULL) {
				WaitForSingleObject(m_hThNuiProcess, INFINITE);
				CloseHandle(m_hThNuiProcess);
			}
			CloseHandle(m_hEvNuiProcessStop);
		}

		DeleteCriticalSection(&section_);

		NuiShutdown();

		if( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
		{
			CloseHandle( m_hNextSkeletonEvent );
			m_hNextSkeletonEvent = NULL;
		}

		if( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
		{
			CloseHandle( m_hNextDepthFrameEvent );
			m_hNextDepthFrameEvent = NULL;
		}

		if( m_hNextVideoFrameEvent && ( m_hNextVideoFrameEvent != INVALID_HANDLE_VALUE ) )
		{
			CloseHandle( m_hNextVideoFrameEvent );
			m_hNextVideoFrameEvent = NULL;
		}
	}
	
	void KinectFrameListener::setupKinect()
	{
		HRESULT hr;
		m_hNextSkeletonEvent   = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_hNextVideoFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		
		DWORD nuiFlag = NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_COLOR;
		hr = NuiInitialize(nuiFlag);
		if (E_NUI_SKELETAL_ENGINE_BUSY == hr) {
			nuiFlag = NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_COLOR;
			hr = NuiInitialize(nuiFlag);
		}

		hr = NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0);
		if (FAILED(hr)) return;

		hr = NuiImageStreamOpen(
			NUI_IMAGE_TYPE_COLOR,
			NUI_IMAGE_RESOLUTION_640x480,
			0,
			2,
			m_hNextVideoFrameEvent,
			&m_pVideoStreamHandle);
		if (FAILED(hr)) return;


		hr = NuiImageStreamOpen(
			NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
			NUI_IMAGE_RESOLUTION_320x240,
			0,
			2,
			m_hNextDepthFrameEvent,
			&m_pDepthStreamHandle);
		if (FAILED(hr)) return;

		InitializeCriticalSection(&section_);
		m_hEvNuiProcessStop = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hThNuiProcess = CreateThread(NULL, 0, ProcessThreadFunc, this, 0, NULL);
	}
	
	void BlankSkeletonScreen()
	{

	}

	RGBQUAD ShortToQuad_Depth( USHORT s )
	{
		USHORT RealDepth = (s & 0xfff8) >> 3;
		USHORT Player = s & 7;

		// transform 13-bit depth information into an 8-bit intensity appropriate
		// for display (we disregard information in most significant bit)
		BYTE l = 255 - (BYTE)(256*RealDepth/0x0fff);

		RGBQUAD q;
		q.rgbRed = q.rgbBlue = q.rgbGreen = 0;

		switch( Player )
		{
		case 0:
			q.rgbRed = l / 2;
			q.rgbBlue = l / 2;
			q.rgbGreen = l / 2;
			break;
		case 1:
			q.rgbRed = l;
			break;
		case 2:
			q.rgbGreen = l;
			break;
		case 3:
			q.rgbRed = l / 4;
			q.rgbGreen = l;
			q.rgbBlue = l;
			break;
		case 4:
			q.rgbRed = l;
			q.rgbGreen = l;
			q.rgbBlue = l / 4;
			break;
		case 5:
			q.rgbRed = l;
			q.rgbGreen = l / 4;
			q.rgbBlue = l;
			break;
		case 6:
			q.rgbRed = l / 2;
			q.rgbGreen = l / 2;
			q.rgbBlue = l;
			break;
		case 7:
			q.rgbRed = 255 - ( l / 2 );
			q.rgbGreen = 255 - ( l / 2 );
			q.rgbBlue = 255 - ( l / 2 );
		}

		return q;
	}

	void GotVideoAlert( )
	{
		const NUI_IMAGE_FRAME * pImageFrame = NULL;

		HRESULT hr = NuiImageStreamGetNextFrame(
			m_pVideoStreamHandle,
			0,
			&pImageFrame );
		if( FAILED( hr ) )
		{
			return;
		}

		INuiFrameTexture * pTexture = pImageFrame->pFrameTexture;
		NUI_LOCKED_RECT LockedRect;
		pTexture->LockRect( 0, &LockedRect, NULL, 0 );
		if( LockedRect.Pitch != 0 )
		{
			BYTE * pBuffer = (BYTE*) LockedRect.pBits;

			// Ogreのテクスチャバッファにコピーする

			EnterCriticalSection(&section_);
			int bufferLen = sizeof(tmpColorImg_);//*480*4;
			memcpy_s(tmpColorImg_, bufferLen, pBuffer, bufferLen);
#if 0
			Ogre::HardwarePixelBufferSharedPtr hpBuffer = colorTex_->getBuffer();
			int bufferLen = hpBuffer->getSizeInBytes();
			unsigned char* buffer = static_cast<unsigned char*>(hpBuffer->lock(0, bufferLen, Ogre::HardwareBuffer::HBL_DISCARD));
			memcpy_s(colorImg_, bufferLen, pBuffer, bufferLen);
			hpBuffer->unlock();
#endif
			LeaveCriticalSection(&section_);
		}
		else
		{
			OutputDebugString("Buffer length of received texture is bogus\r\n" );
		}

		NuiImageStreamReleaseFrame( m_pVideoStreamHandle, pImageFrame );
	}
	
	void GotDepthAlert()
	{
		const NUI_IMAGE_FRAME* pImageFrame = NULL;
		HRESULT hr = NuiImageStreamGetNextFrame(
			m_pDepthStreamHandle,
			0,
			&pImageFrame);

		if (FAILED(hr)) {
			return;
		}

		INuiFrameTexture* pTexture = pImageFrame->pFrameTexture;
		NUI_LOCKED_RECT LockedRect;
		pTexture->LockRect(0, &LockedRect, NULL, 0);
		
		if (LockedRect.Pitch != 0) {
			BYTE* pBuffer = (BYTE*)LockedRect.pBits;

			// Ogreのテクスチャバッファにコピーする			
			EnterCriticalSection(&section_);
			USHORT* pBufferRun = (USHORT*)pBuffer;
			int bufferLen = sizeof(tmpDepthImg_);
			memcpy_s(tmpDepthImg_, bufferLen, pBufferRun, bufferLen);

#if 0
			Ogre::HardwarePixelBufferSharedPtr hpBuffer = depthTex_->getBuffer();
			int bufferLen = hpBuffer->getSizeInBytes();
			unsigned char* buffer = static_cast<unsigned char*>(hpBuffer->lock(0, bufferLen, Ogre::HardwareBuffer::HBL_DISCARD));

			//draw the bits to the bitmap
			//RGBQUAD* rgbrun = m_rgbWk;
			USHORT* pBufferRun = (USHORT*)pBuffer;
			for (int y = 0; y < 240; y++) {
				for (int x = 0; x < 320; x++) {
					RGBQUAD quad = this->ShortToQuad_Depth(*pBufferRun);
					*(buffer++) = quad.rgbBlue;
					*(buffer++) = quad.rgbGreen;
					*(buffer++) = quad.rgbRed;
					*(buffer++) = 255;

					pBufferRun++;
					//*rgbrun = quad;
					//rgbrun++;		
				}
			}

			hpBuffer->unlock();
#endif
			LeaveCriticalSection(&section_);
		}
		else
		{
			OutputDebugString("Buffer length of received texture is bogus\r\n");
		}

		NuiImageStreamReleaseFrame(m_pDepthStreamHandle, pImageFrame);
	}

	void GotSkeletonAlert()
	{
		//NUI_SKELETON_FRAME SkeletonFrame;

		HRESULT hr = NuiSkeletonGetNextFrame( 0, &SkeletonFrame );

		bool bFoundSkeleton = true;
		for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
		{
			if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
			{
				bFoundSkeleton = false;
			}
		}

		// no skeletons!
		//
		if( bFoundSkeleton )
		{
			return;
		}

		// smooth out the skeleton data
		NuiTransformSmooth(&SkeletonFrame, NULL);

		// we found a skeleton, re-start the timer
		m_bScreenBlanked = false;
		m_LastSkeletonFoundTime = -1;

		// draw each skeleton color according to the slot within they are found.
		//
		bool bBlank = true;
		for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
		{
			if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
			{
				DrawSkeleton( bBlank, &SkeletonFrame.SkeletonData[i]);
				bBlank = false;
			}
		}
	}

	void DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... )
	{
		va_list vl;
		va_start(vl,numJoints);
		POINT segmentPositions[NUI_SKELETON_POSITION_COUNT];

		for (int iJoint = 0; iJoint < numJoints; iJoint++)
		{
			NUI_SKELETON_POSITION_INDEX jointIndex = va_arg(vl, NUI_SKELETON_POSITION_INDEX);
			segmentPositions[iJoint].x = m_Points[jointIndex].x;
			segmentPositions[iJoint].y = m_Points[jointIndex].y;
		}

		//Polyline(m_SkeletonDC, segmentPositions, numJoints);

		va_end(vl);
	}

	void DrawSkeleton( bool bBlank, NUI_SKELETON_DATA * pSkel)
	{		
		float fx=0, fy=0;
		for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
		{
			NuiTransformSkeletonToDepthImage( pSkel->SkeletonPositions[i], &fx, &fy);
			//Vector4 p = NuiTransformDepthImageToSkeletonF(fx, f);
			//NuiImageGetColorPixelCoordinatesFromDepthPixel();
			//NuiCameraElevationGetAngle();
			//NuiCameraElevationSetAngle();
			m_Points[i].x = (int)(fx * 320 + 0.5f);
			m_Points[i].y = (int)(fy * 240 + 0.5f);
			tmpSkelPos_[i].x = fx;
			tmpSkelPos_[i].y = fy;
		}

/*		DrawSkeletonSegment(pSkel,4,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD);
		DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);
		DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);
		DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);
		DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);
		*/

		return;
	}

	virtual bool frameStarted(const Ogre::FrameEvent& evt) 
	{
		EnterCriticalSection(&section_);

		// copy skeleton points
		for (size_t i = 0; i < NUI_SKELETON_POSITION_COUNT; i++) {
			float x = tmpSkelPos_[i].x;
			float y = 1.0 - tmpSkelPos_[i].y;
			skeletonPoints_[i].x = (x - 0.5) * 2.0;
			skeletonPoints_[i].y = (y - 0.5) * 2.0;
			skeletonPoints_[i].z = 0.0;
		}

		// draw color image
		{
			Ogre::HardwarePixelBufferSharedPtr hpBuffer = colorTex_->getBuffer();
			int bufferLen = hpBuffer->getSizeInBytes();
			unsigned char* buffer = static_cast<unsigned char*>(hpBuffer->lock(0, bufferLen, Ogre::HardwareBuffer::HBL_DISCARD));
			memcpy_s(buffer, bufferLen, tmpColorImg_, bufferLen);
			hpBuffer->unlock();
		}

		// draw depth image
		{
			Ogre::HardwarePixelBufferSharedPtr hpBuffer = depthTex_->getBuffer();
			int bufferLen = hpBuffer->getSizeInBytes();
			unsigned char* buffer = static_cast<unsigned char*>(hpBuffer->lock(0, bufferLen, Ogre::HardwareBuffer::HBL_DISCARD));

			USHORT* pBufferRun = (USHORT*)tmpDepthImg_;
			for (int y = 0; y < 240; y++) {
				for (int x = 0; x < 320; x++) {
					RGBQUAD quad = this->ShortToQuad_Depth(*pBufferRun);
					*(buffer++) = quad.rgbBlue;
					*(buffer++) = quad.rgbGreen;
					*(buffer++) = quad.rgbRed;
					*(buffer++) = 255;
					pBufferRun++;
				}
			}	
			hpBuffer->unlock();
		}

		LeaveCriticalSection(&section_);

		return true;
	}

	virtual bool frameEnded(const Ogre::FrameEvent& evt) 
	{ 	
		return true;
	}

private:
	void createBackgroundTexture()
	{		
		colorTex_ = Ogre::TextureManager::getSingleton().createManual("Plane/ColorTex", "General", Ogre::TEX_TYPE_2D, 640, 480, 1, 1, Ogre::PF_A8R8G8B8, Ogre::TU_STATIC_WRITE_ONLY);
		colorMat_ = Ogre::MaterialManager::getSingleton().create("Plane/ColorMat", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		colorMat_->getTechnique(0)->getPass(0)->createTextureUnitState("Plane/ColorTex");
		colorMat_->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);
		colorMat_->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
		colorMat_->getTechnique(0)->getPass(0)->setLightingEnabled(false);

		depthTex_ = Ogre::TextureManager::getSingleton().createManual("Plane/DepthTex", "General", Ogre::TEX_TYPE_2D, 320, 240, 1, 1, Ogre::PF_A8B8G8R8, Ogre::TU_STATIC_WRITE_ONLY);
		depthMat_ = Ogre::MaterialManager::getSingleton().create("Plane/DepthMat", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		depthMat_->getTechnique(0)->getPass(0)->createTextureUnitState("Plane/DepthTex");
		depthMat_->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);
		depthMat_->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
		depthMat_->getTechnique(0)->getPass(0)->setLightingEnabled(false);
	}

public:
	const Ogre::Vector3* getSkeletonPoints() const { return skeletonPoints_; }

private:
	Ogre::TexturePtr	colorTex_;
	Ogre::TexturePtr	depthTex_;
	Ogre::MaterialPtr	colorMat_;
	Ogre::MaterialPtr	depthMat_;
	Ogre::Vector3       skeletonPoints_[NUI_SKELETON_POSITION_COUNT];
	Ogre::Vector2       tmpSkelPos_[NUI_SKELETON_POSITION_COUNT];
	BYTE				tmpColorImg_[640*480*4];
	SHORT				tmpDepthImg_[320*240];

	NUI_SKELETON_FRAME SkeletonFrame;

	static DWORD WINAPI     ProcessThreadFunc(LPVOID pParam);
	CRITICAL_SECTION		section_;
	// thread handling
	HANDLE        m_hThNuiProcess;
	HANDLE        m_hEvNuiProcessStop;
	HANDLE        m_hNextDepthFrameEvent;
	HANDLE        m_hNextVideoFrameEvent;
	HANDLE        m_hNextSkeletonEvent;
	HANDLE        m_pDepthStreamHandle;
	HANDLE        m_pVideoStreamHandle;
	POINT         m_Points[NUI_SKELETON_POSITION_COUNT];
	//RGBQUAD       m_rgbWk[640*480];
	int           m_LastSkeletonFoundTime;
	bool          m_bScreenBlanked;
	int           m_FramesTotal;
	int           m_LastFPStime;
	int           m_LastFramesTotal;
};

#endif //__CameraListener_h_