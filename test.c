//Copyright (c) 2011-2020 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
// NO WARRANTY! NO GUARANTEE OF SUPPORT! USE AT YOUR OWN RISK

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "os_generic.h"
#include <GLES3/gl3.h>
#include <asset_manager.h>
#include <asset_manager_jni.h>
#include <android_native_app_glue.h>
#include <sys/prctl.h>
#include <android/sensor.h>

#define RAWDRAW_NEED_PBUFFER
#define CNFGOGL
#define CNFGOGL_NEED_EXTENSION
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#include <VrApi_Helpers.h>
#include <VrApi.h>

extern struct android_app * gapp;
extern EGLDisplay egl_display;
extern EGLSurface egl_surface;
extern EGLContext egl_context;

float mountainangle;
float mountainoffsetx;
float mountainoffsety;

unsigned frames = 0;
unsigned long iframeno = 0;

void AndroidDisplayKeyboard(int pShow);
short screenx, screeny;

int lastbuttonx = 0;
int lastbuttony = 0;
int lastmotionx = 0;
int lastmotiony = 0;
int lastbid = 0;
int lastmask = 0;
int lastkey, lastkeydown;

static int keyboard_up;

void HandleKey( int keycode, int bDown )
{
	lastkey = keycode;
	lastkeydown = bDown;
	if( keycode == 10 && !bDown ) { keyboard_up = 0; AndroidDisplayKeyboard( keyboard_up );  }

	if( keycode == 4 ) { AndroidSendToBack( 1 ); } //Handle Physical Back Button.
}

void HandleButton( int x, int y, int button, int bDown )
{
	lastbid = button;
	lastbuttonx = x;
	lastbuttony = y;

	if( bDown ) { keyboard_up = !keyboard_up; AndroidDisplayKeyboard( keyboard_up ); }
}

void HandleMotion( int x, int y, int mask )
{
	lastmask = mask;
	lastmotionx = x;
	lastmotiony = y;
}


void HandleDestroy()
{
	printf( "Destroying\n" );
	exit(10);
}

volatile int suspended;

void HandleSuspend()
{
	suspended = 1;
}

void HandleResume()
{
	suspended = 0;
}

uint32_t randomtexturedata[256*256];

//extern struct JNINativeInterface * env;

int main()
{
	int x, y;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	int linesegs = 0;

	CNFGSetup( "Test Bench", 256, 256 );

	// Initialization code from VrSamples
	ovrJava java;
	java.Vm = gapp->activity->vm;
	(*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
	java.ActivityObject = gapp->activity->clazz;

	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

	printf( "Params Default Init\n" );

	ovrInitParms initParms = vrapi_DefaultInitParms(&java);
	initParms.GraphicsAPI = VRAPI_GRAPHICS_API_OPENGL_ES_3;
	initParms.Java = java;

	int32_t initResult = vrapi_Initialize(&initParms);
	if (initResult != VRAPI_INITIALIZE_SUCCESS)
	{
		// If intialization failed, vrapi_* function calls will not be available.
		exit(0);
	}

	ovrModeParms parms = vrapi_DefaultModeParms(&java);
	parms.Java = java;
	parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
	parms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
	parms.Display = (size_t)egl_display;
	parms.WindowSurface = (size_t)egl_surface;
	parms.ShareContext = (size_t)egl_context;
	ovrMobile * Ovr = vrapi_EnterVrMode(&parms);

	printf( "Ovr = %p\n", Ovr);


	int fbw, fbh;
	fbw = vrapi_GetSystemPropertyInt( &java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH );
	fbh = vrapi_GetSystemPropertyInt( &java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT );
	ovrTextureSwapChain * ColorTextureSwapChain[2];

	uint32_t * imageData = malloc( sizeof( uint32_t ) * fbw * fbh );
	for( int y = 0; y < fbh; y++ )
	{
		for( int x = 0; x < fbw; x++ )
		{
			imageData[x+y*fbw] = ((x+y)%2)?0xff00ff00:0xff000000;
		}
	}
	imageData[fbw/2+fbh/2*fbw] = 0xffffffff;

	int eye;
	for( eye = 0; eye < 2; eye++ )
	{
		ColorTextureSwapChain[eye] = vrapi_CreateTextureSwapChain3(
	        	VRAPI_TEXTURE_TYPE_2D,
		        GL_SRGB8_ALPHA8,
			fbw,
			fbh,
		        1,
	        	3);
		int TextureSwapChainLength = vrapi_GetTextureSwapChainLength(ColorTextureSwapChain[eye]);
		printf( "Swapchain from (%d,%d) = %p (%d)\n", fbw, fbh, ColorTextureSwapChain[eye], TextureSwapChainLength );

		for (int i = 0; i < TextureSwapChainLength; i++)
		{
        		// Create the color buffer texture.
			const GLuint colorTexture = vrapi_GetTextureSwapChainHandle(ColorTextureSwapChain[eye], i);
			glBindTexture( GL_TEXTURE_2D, colorTexture );
			printf( "Writing texture: %d\n", colorTexture );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, imageData );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}
	}

	while( CNFGHandleInput() )
	{
		iframeno++;
		if( suspended ) { usleep(50000); continue; }


		ovrEventDataBuffer eventDataBuffer = {};

		// Poll for VrApi events
		for (;;) {
			ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
			ovrResult res = vrapi_PollEvent(eventHeader);
			if (res != ovrSuccess) break;

			switch (eventHeader->EventType) {
			case VRAPI_EVENT_DATA_LOST:
				printf("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
				break;
			case VRAPI_EVENT_VISIBILITY_GAINED:
				printf("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
				break;
			case VRAPI_EVENT_VISIBILITY_LOST:
				printf("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
				break;
			case VRAPI_EVENT_FOCUS_GAINED:
		                printf("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
				break;
			case VRAPI_EVENT_FOCUS_LOST:
				printf("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
				break;
			default:
				printf("vrapi_PollEvent: Unknown event");
				break;
			}
		}

		// TODO: Render stuff.

		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(Ovr, iframeno);
		const ovrTracking2 tracking = vrapi_GetPredictedTracking2(Ovr, predictedDisplayTime);

		ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
		printf( "%d %f\n", tracking.Status, predictedDisplayTime );

		layer.Header.Type = VRAPI_LAYER_TYPE_PROJECTION2;
		layer.Header.Flags = 
			VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
			//VRAPI_FRAME_LAYER_FLAG_FIXED_TO_VIEW;
		layer.Header.ColorScale.x = 1.0f;
		layer.Header.ColorScale.y = 1.0f;
		layer.Header.ColorScale.z = 1.0f;
		layer.Header.ColorScale.w = 1.0f;
		layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
		layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;
		layer.Header.Reserved = NULL;
		//layer.HeadPose.Pose.Orientation.w = 1.0f; // For face-fixed 
		layer.HeadPose = tracking.HeadPose;

		for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
			layer.Textures[eye].SwapChainIndex = iframeno % 3;
			layer.Textures[eye].ColorSwapChain = ColorTextureSwapChain[eye];
			layer.Textures[eye].TextureRect.x = 0;
			layer.Textures[eye].TextureRect.y = 0;
			layer.Textures[eye].TextureRect.width = fbw;
			layer.Textures[eye].TextureRect.height = fbh;
			layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tracking.Eye[eye].ProjectionMatrix);
		}

		const ovrLayerHeader2* layerList[] = { &layer.Header };
		ovrSubmitFrameDescription2 frameDesc = {0};
		frameDesc.Flags = 0;
		frameDesc.SwapInterval = 1;
		frameDesc.FrameIndex = iframeno;
		frameDesc.DisplayTime = predictedDisplayTime;
		frameDesc.LayerCount = 1;
		frameDesc.Layers = layerList;

		// Hand over the eye images to the time warp.
		vrapi_SubmitFrame2(Ovr, &frameDesc);

		frames++;

		//On Android, CNFGSwapBuffers must be called, and CNFGUpdateScreenWithBitmap does not have an implied framebuffer swap.
		//CNFGSwapBuffers();

		ThisTime = OGGetAbsoluteTime();
		if( ThisTime > LastFPSTime + 1 )
		{
			printf( "FPS: %d\n", frames );
			frames = 0;
			linesegs = 0;
			LastFPSTime+=1;
		}
	}

	return(0);
}

