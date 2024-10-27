#include "StdAfx.h"
#include "../eterBase/Error.h"
#include "../eterlib/Camera.h"
#include "../eterlib/AttributeInstance.h"
#include "../gamelib/AreaTerrain.h"
#include "../EterGrnLib/Material.h"
#include "../CWebBrowser/CWebBrowser.h"

#include "resource.h"
#include "PythonApplication.h"
#include "PythonCharacterManager.h"
#include "ProcessScanner.h"
#include "StdAfx.h"
#ifdef ENABLE_SWITCHBOT
#include "PythonSwitchbot.h"
#endif

#ifdef ENABLE_WHISPER_WINDOWS_NOTIYF
#include <Windows.h>
#include <iostream>
#include <string>
#include <shellapi.h>
#endif

#ifdef KAISER_HDR_MOD
#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <fstream>
namespace fs = std::filesystem;
#endif

#ifdef DEAD_SCREEN_ANIMATE
#include <objbase.h>
#endif
#include "effects/die_scene.h"

extern void GrannyCreateSharedDeformBuffer();
extern void GrannyDestroySharedDeformBuffer();

ShaderRenderTargetEffect grayscaleEffect;

float MIN_FOG = 2400.0f;
//double g_specularSpd=0.007f;
double g_specularSpd = 0.00007f;

CPythonApplication * CPythonApplication::ms_pInstance;

float c_fDefaultCameraRotateSpeed = 1.5f;
float c_fDefaultCameraPitchSpeed = 1.5f;
float c_fDefaultCameraZoomSpeed = 0.05f;

CPythonApplication::CPythonApplication() :
m_bCursorVisible(TRUE),
m_bLiarCursorOn(false),
m_iCursorMode(CURSOR_MODE_HARDWARE),
m_isWindowed(false),
m_isFrameSkipDisable(false),
m_poMouseHandler(NULL),
m_dwUpdateFPS(0),
m_dwRenderFPS(0),
m_fAveRenderTime(0.0f),
m_dwFaceCount(0),
m_fGlobalTime(0.0f),
m_fGlobalElapsedTime(0.0f),
m_dwLButtonDownTime(0),
m_dwLastIdleTime(0),
m_future_should_continue_processing(false),
m_future_acknowledged_stop_request(true)

{
#ifndef _DEBUG
	SetEterExceptionHandler();
#endif

	CTimer::Instance().UseCustomTime();
	m_dwWidth = 800;
	m_dwHeight = 600;

	ms_pInstance = this;
	m_isWindowFullScreenEnable = FALSE;

	m_v3CenterPosition = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	m_dwStartLocalTime = ELTimer_GetMSec();
	m_tServerTime = 0;
	m_tLocalStartTime = 0;

	m_iPort = 0;
	m_iFPS = 60;

	m_isActivateWnd = false;
	m_isMinimizedWnd = true;

	m_fRotationSpeed = 0.0f;
	m_fPitchSpeed = 0.0f;
	m_fZoomSpeed = 0.0f;

	m_fFaceSpd=0.0f;

	m_dwFaceAccCount=0;
	m_dwFaceAccTime=0;

	m_dwFaceSpdSum=0;
	m_dwFaceSpdCount=0;

	m_FlyingManager.SetMapManagerPtr(&m_pyBackground);

	m_iCursorNum = CURSOR_SHAPE_NORMAL;
	m_iContinuousCursorNum = CURSOR_SHAPE_NORMAL;

	m_isSpecialCameraMode = FALSE;
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed;

	m_iCameraMode = CAMERA_MODE_NORMAL;
	m_fBlendCameraStartTime = 0.0f;
	m_fBlendCameraBlendTime = 0.0f;
	m_iForceSightRange = -1;
	CCameraManager::Instance().AddCamera(EVENT_CAMERA_NUMBER);
}

CPythonApplication::~CPythonApplication()
{
}

void CPythonApplication::GetMousePosition(POINT* ppt)
{
	CMSApplication::GetMousePosition(ppt);
}

void CPythonApplication::SetMinFog(float fMinFog)
{
	MIN_FOG = fMinFog;
}

#ifdef DEAD_SCREEN_ANIMATE
bool CPythonApplication::CheckEffects()
{
	if (isDeadInstance 
		|| Frustum::Instance().isRealisticScene
		|| Frustum::Instance().sharpnessEff
		)
		return true;

	return false;
}
void CPythonApplication::GetDieCamera(float arg)
{
	CCameraManager& rCmrMgr=CCameraManager::Instance();
	CCamera* pCamera = rCmrMgr.GetCurrentCamera();
	if (!pCamera)
		return;

	float fDistance = pCamera->GetDistance();
	if (IsLockCurrentCamera())
		return;
	

	float zoomFactor = 0.1f;
	float fRatio = 1.0f + zoomFactor * m_fCameraZoomSpeed * float(arg);
	m_fZoomSpeed = fRatio;
}
#endif
void CPythonApplication::SetFrameSkip(bool isEnable)
{
	if (isEnable)
		m_isFrameSkipDisable=false;
	else
		m_isFrameSkipDisable=true;
}

void CPythonApplication::NotifyHack(const char* c_szFormat, ...)
{
	char szBuf[1024];

	va_list args;
	va_start(args, c_szFormat);	
	_vsnprintf(szBuf, sizeof(szBuf), c_szFormat, args);
	va_end(args);
	m_pyNetworkStream.NotifyHack(szBuf);
}

void CPythonApplication::GetInfo(UINT eInfo, std::string* pstInfo)
{
	switch (eInfo)
	{
	case INFO_ACTOR:
		m_kChrMgr.GetInfo(pstInfo);
		break;
	case INFO_EFFECT:
		m_kEftMgr.GetInfo(pstInfo);			
		break;
	case INFO_ITEM:
		m_pyItem.GetInfo(pstInfo);
		break;
	case INFO_TEXTTAIL:
		m_pyTextTail.GetInfo(pstInfo);
		break;
	}
}

void CPythonApplication::Abort()
{
	TraceError("============================================================================================================");
	TraceError("Abort!!!!\n\n");

	PostQuitMessage(0);
}

void CPythonApplication::Exit()
{
	PostQuitMessage(0);
}

bool PERF_CHECKER_RENDER_GAME = false;
#include "../EterPack/EterPackManager.h"
int thunderstep = 10;
bool releaseEffect = false;
bool needRefresh = false;
bool startDynamicLight = false;
int dynamicCounter = 0;
void CPythonApplication::RenderGame()
{
#ifdef ENABLE_PACK_SECURITY
	bool t = CEterPackManager::Instance().CheckPackFile();
	if (!t)
	{
		CEterPackManager::Instance().fileInfoVec.clear();
		Abort();
	}
#endif
	
	if (!PERF_CHECKER_RENDER_GAME)
	{
		Frustum::Instance().funcCallCounter = 0;
#ifndef DISABLE_POST_PROCESSING
		if (Frustum::instance().dynamicLight == 1 || Frustum::instance().dynamicLight == 2)
		//if (Frustum::instance().isHDR == true)
		{
			GetPostProcessingChain().BeginScene();
		}
#endif
		
		
		
#ifdef RENDER_TARGET_SYSTEM
		m_kRenderTargetManager.RenderBackgrounds();
#endif
		float fAspect=m_kWndMgr.GetAspect();
		float fFarClip=m_pyBackground.GetFarClip();
		
#if defined(ENABLE_FOV_OPTION)
		m_pyGraphic.SetPerspective(CPythonSystem::instance().GetFOV(), fAspect, 100.0, fFarClip);
#else
		m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0, fFarClip);
#endif
		
		CCullingManager::Instance().Process();
		
		m_kChrMgr.Deform();
		//m_kEftMgr.Update();
#ifdef RENDER_TARGET_SYSTEM	
		m_kRenderTargetManager.DeformModels();
#endif
		m_pyBackground.RenderCharacterShadowToTexture();
		m_pyBackground.RenderCharacterShadowToTexture2();
		m_pyBackground.RenderCharacterShadowToTexture3();
		m_pyBackground.RenderCharacterShadowToTexture4();
#ifdef DEAD_SCREEN_ANIMATE
		//if (isDeadInstance)
		if (CheckEffects())
		{
			GetShaderRenderTarget().Begin(D3DCOLOR_ARGB(255, 0, 0, 0));
		}
#endif
		m_pyGraphic.SetGameRenderState();
		m_pyGraphic.PushState();
		
		{
			long lx, ly;
			m_kWndMgr.GetMousePosition(lx, ly);
			m_pyGraphic.SetCursorPosition(lx, ly);
		}

		m_pyBackground.RenderSky();
		
		//m_pyBackground.RenderBeforeLensFlare();

		//m_pyBackground.RenderCloud();

		m_pyBackground.BeginEnvironment();
		
		m_pyBackground.Render();
		
		

		m_pyBackground.SetCharacterDirLight();
		
		m_kChrMgr.Render();
#ifdef RENDER_TARGET_SYSTEM
		m_kRenderTargetManager.RenderModels();
#endif
		m_pyBackground.SetBackgroundDirLight();
		m_pyBackground.RenderWater();
		m_pyBackground.RenderSnow();
		

		if(Frustum::Instance().ambianceEffectsLevel == 0)
			m_pyBackground.RenderEffect();

		
		

		m_pyBackground.EndEnvironment();

		m_kEftMgr.Render();
		m_pyItem.Render();
#ifdef DISABLE_POST_PROCESSING
		m_FlyingManager.Render();
#endif
		
		//m_pyBackground.BeginEnvironment();
		//m_pyBackground.RenderPCBlocker();
		//m_pyBackground.EndEnvironment();

		//m_pyBackground.RenderAfterLensFlare();
#ifndef DISABLE_POST_PROCESSING

		//if (Frustum::instance().isHDR == true)
		if (Frustum::instance().dynamicLight == 1 || Frustum::instance().dynamicLight == 2)
		{
			GetPostProcessingChain().Render();
			GetPostProcessingChain().EndScene();	
		}
		m_FlyingManager.Render();
		//m_pyBackground.RenderAmbianceAudio();
#endif

#ifdef DEAD_SCREEN_ANIMATE
		if (CheckEffects())
		{
			GetShaderRenderTarget().End();
			GetShaderRenderTargetEffect().Apply();
		}
#endif
		if (startDynamicLight == false && dynamicCounter == 10)
			startDynamicLight = true;

		if (dynamicCounter < 10)
		{
			dynamicCounter += 1;
		}
		return;
	}
	
	//if (GetAsyncKeyState(VK_Z))
	//	STATEMANAGER.SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	DWORD t1=ELTimer_GetMSec();
	m_kChrMgr.Deform();
	DWORD t2=ELTimer_GetMSec();
	//m_kEftMgr.Update();
	DWORD t3=ELTimer_GetMSec();
	m_pyBackground.RenderCharacterShadowToTexture();
	m_pyBackground.RenderCharacterShadowToTexture2();
	m_pyBackground.RenderCharacterShadowToTexture3();
	m_pyBackground.RenderCharacterShadowToTexture4();
	DWORD t4=ELTimer_GetMSec();

	m_pyGraphic.SetGameRenderState();
	m_pyGraphic.PushState();

	float fAspect=m_kWndMgr.GetAspect();
	float fFarClip=m_pyBackground.GetFarClip();
	
#if defined(ENABLE_FOV_OPTION)
	m_pyGraphic.SetPerspective(CPythonSystem::instance().GetFOV(), fAspect, 100.0, fFarClip);
#else
	m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0, fFarClip);
#endif

	DWORD t5=ELTimer_GetMSec();

	CCullingManager::Instance().Process();

	DWORD t6=ELTimer_GetMSec();

	{
		long lx, ly;
		m_kWndMgr.GetMousePosition(lx, ly);
		m_pyGraphic.SetCursorPosition(lx, ly);
	}

	m_pyBackground.RenderSky();
	DWORD t7=ELTimer_GetMSec();
	m_pyBackground.RenderBeforeLensFlare();
	DWORD t8=ELTimer_GetMSec();
	m_pyBackground.RenderCloud();
	DWORD t9=ELTimer_GetMSec();
	m_pyBackground.BeginEnvironment();
	m_pyBackground.Render();

	m_pyBackground.SetCharacterDirLight();
	DWORD t10=ELTimer_GetMSec();
	m_kChrMgr.Render();
	DWORD t11=ELTimer_GetMSec();

	m_pyBackground.SetBackgroundDirLight();
	m_pyBackground.RenderWater();
	DWORD t12=ELTimer_GetMSec();
	m_pyBackground.RenderEffect();
	DWORD t13=ELTimer_GetMSec();
	m_pyBackground.EndEnvironment();
	m_kEftMgr.Render();
	DWORD t14=ELTimer_GetMSec();
	m_pyItem.Render();
	DWORD t15=ELTimer_GetMSec();
	m_FlyingManager.Render();
	DWORD t16=ELTimer_GetMSec();
	m_pyBackground.BeginEnvironment();
	m_pyBackground.RenderPCBlocker();
	m_pyBackground.EndEnvironment();
	DWORD t17=ELTimer_GetMSec();
	m_pyBackground.RenderAfterLensFlare();
	DWORD t18=ELTimer_GetMSec();
	DWORD tEnd=ELTimer_GetMSec();

	if (GetAsyncKeyState(VK_Z))
		STATEMANAGER.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

	if (tEnd-t1<3)
		return;

	static FILE* fp=fopen("perf_game_render.txt", "w");

	fprintf(fp, "GR.Total %d (Time %d)\n", tEnd-t1, ELTimer_GetMSec());
	fprintf(fp, "GR.DFM %d\n", t2-t1);
	fprintf(fp, "GR.EFT.UP %d\n", t3-t2);
	fprintf(fp, "GR.SHW %d\n", t4-t3);
	fprintf(fp, "GR.STT %d\n", t5-t4);
	fprintf(fp, "GR.CLL %d\n", t6-t5);
	fprintf(fp, "GR.BG.SKY %d\n", t7-t6);
	fprintf(fp, "GR.BG.LEN %d\n", t8-t7);
	fprintf(fp, "GR.BG.CLD %d\n", t9-t8);
	fprintf(fp, "GR.BG.MAIN %d\n", t10-t9);		
	fprintf(fp, "GR.CHR %d\n",	t11-t10);
	fprintf(fp, "GR.BG.WTR %d\n", t12-t11);
	fprintf(fp, "GR.BG.EFT %d\n", t13-t12);
	fprintf(fp, "GR.EFT %d\n", t14-t13);
	fprintf(fp, "GR.ITM %d\n", t15-t14);
	fprintf(fp, "GR.FLY %d\n", t16-t15);
	fprintf(fp, "GR.BG.BLK %d\n", t17-t16);
	fprintf(fp, "GR.BG.LEN %d\n", t18-t17);

	fflush(fp);
}

void CPythonApplication::UpdateGame()
{
	DWORD t1=ELTimer_GetMSec();
	POINT ptMouse;
	GetMousePosition(&ptMouse);

	CGraphicTextInstance::Hyperlink_UpdateMousePos(ptMouse.x, ptMouse.y);

	DWORD t2=ELTimer_GetMSec();

	//if (m_isActivateWnd)
	{
		CScreen s;
		float fAspect = UI::CWindowManager::Instance().GetAspect();
		float fFarClip = CPythonBackground::Instance().GetFarClip();
#if defined(ENABLE_FOV_OPTION)
		s.SetPerspective(CPythonSystem::instance().GetFOV(), fAspect, 100.0f, fFarClip);
#else
		s.SetPerspective(30.0f,fAspect, 100.0f, fFarClip);
#endif
		s.BuildViewFrustum();
	}

	DWORD t3=ELTimer_GetMSec();
#ifdef RENDER_TARGET_SYSTEM
	m_kRenderTargetManager.UpdateModels();
#endif
	TPixelPosition kPPosMainActor;
	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);

	DWORD t4=ELTimer_GetMSec();
	m_pyBackground.Update(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);

	DWORD t5=ELTimer_GetMSec();
	m_GameEventManager.SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	m_GameEventManager.Update();

	DWORD t6=ELTimer_GetMSec();
	m_kChrMgr.Update();	
	DWORD t7=ELTimer_GetMSec();
	m_kEftMgr.Update(); //@fix25
	m_kEftMgr.UpdateSound();
	DWORD t8=ELTimer_GetMSec();
	m_FlyingManager.Update();
	DWORD t9=ELTimer_GetMSec();
	m_pyItem.Update(ptMouse);
	DWORD t10=ELTimer_GetMSec();
	m_pyPlayer.Update();
	DWORD t11=ELTimer_GetMSec();

	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);
	SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	DWORD t12=ELTimer_GetMSec();

	if (PERF_CHECKER_RENDER_GAME)
	{
		if (t12-t1>5)
		{
			static FILE* fp=fopen("perf_game_update.txt", "w");

			fprintf(fp, "GU.Total %d (Time %d)\n", t12-t1, ELTimer_GetMSec());
			fprintf(fp, "GU.GMP %d\n", t2-t1);
			fprintf(fp, "GU.SCR %d\n", t3-t2);
			fprintf(fp, "GU.MPS %d\n", t4-t3);
			fprintf(fp, "GU.BG %d\n", t5-t4);
			fprintf(fp, "GU.GEM %d\n", t6-t5);
			fprintf(fp, "GU.CHR %d\n", t7-t6);
			fprintf(fp, "GU.EFT %d\n", t8-t7);
			fprintf(fp, "GU.FLY %d\n", t9-t8);
			fprintf(fp, "GU.ITM %d\n", t10-t9);
			fprintf(fp, "GU.PLR %d\n", t11-t10);
			fprintf(fp, "GU.POS %d\n", t12-t11);
			fflush(fp);
		}
	}

}

void CPythonApplication::SkipRenderBuffering(DWORD dwSleepMSec)
{
	m_dwBufSleepSkipTime=ELTimer_GetMSec()+dwSleepMSec;
}


#ifdef ENABLE_WHISPER_WINDOWS_NOTIYF
bool CPythonApplication::CheckWindowsVersion()
{

	double osver = 0.0;

	NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
	OSVERSIONINFOEXW osInfo;

	*(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");

	if (NULL != RtlGetVersion)
	{
		osInfo.dwOSVersionInfoSize = sizeof(osInfo);
		RtlGetVersion(&osInfo);
		osver = osInfo.dwMajorVersion + osInfo.dwMinorVersion / 10.0;
	}
	if (osver >= 10)
		return true;
	else
	{

#ifdef _DEBUG
		TraceError("CPythonApplication::CheckWindowsVersion() HATA! - Hata kodu: ", GetLastError());
#endif
		return false;
	}
	return true;
}

void CPythonApplication::SendNotify(const char* c_Name)
{
	if (!CheckWindowsVersion() || !CPythonSystem::Instance().IsNotify())
		return;

	NOTIFYICONDATA nid = {};
	HWND hwnd = GetWindowHandle();

	const char* szInfo = "Bir Mesaj Gönderdi";
	nid.cbSize = sizeof(nid);
	nid.hWnd = hwnd;
	nid.uFlags = NIF_ICON | NIF_INFO | NIF_TIP;
	HMODULE hModule = GetModuleHandle(nullptr);
	HICON hIconSmall = (HICON)LoadImage(hModule, MAKEINTRESOURCE(IDI_METIN2), IMAGE_ICON, 16, 16, 0);
	nid.hIcon = hIconSmall;
	nid.hBalloonIcon = hIconSmall;

	lstrcpy(nid.szInfoTitle, c_Name);
	lstrcpy(nid.szInfo, szInfo);

	Shell_NotifyIcon(NIM_ADD, &nid);

	/*ShowWindow(hwnd, SW_RESTORE);
	SetForegroundWindow(hwnd);*/
	Shell_NotifyIcon(NIM_DELETE, &nid); 
}
#endif

#ifdef DISABLE_YMIR_WORK_FOLDER
bool CPythonApplication::CheckYmirWorkFolder()
{
	std::filesystem::path control = std::string("D:\\ymir work\\");
	bool check = std::filesystem::is_directory(control.parent_path());
	if (check)
	{
		MessageBox(NULL, "D Sürücüsünde ymir work klasörü tespit edildi.\nLütfen klasörü silin ve tekrar deneyin. Ýþlem iptal edildi.", NULL, MB_OK);
		Exit();
		return false;
	}
	return true;
}
#endif

bool CPythonApplication::Process()
{
	
#ifdef DISABLE_YMIR_WORK_FOLDER
	CheckYmirWorkFolder();
#endif
	ELTimer_SetFrameMSec();

	DWORD dwStart = ELTimer_GetMSec();

	///////////////////////////////////////////////////////////////////////////////////////////////////
	static DWORD	s_dwUpdateFrameCount = 0;
	static DWORD	s_dwRenderFrameCount = 0;
	static DWORD	s_dwFaceCount = 0;
	static UINT		s_uiLoad = 0;
	static DWORD	s_dwCheckTime = ELTimer_GetMSec();

	if (ELTimer_GetMSec() - s_dwCheckTime > 1000)
	{
		m_dwUpdateFPS		= s_dwUpdateFrameCount;
		m_dwRenderFPS		= s_dwRenderFrameCount;
		m_dwLoad			= s_uiLoad;

		m_dwFaceCount		= s_dwFaceCount / max(1, s_dwRenderFrameCount);

		s_dwCheckTime		= ELTimer_GetMSec();

		s_uiLoad = s_dwFaceCount = s_dwUpdateFrameCount = s_dwRenderFrameCount = 0;
	}

	// Update Time
	static BOOL s_bFrameSkip = false;
	static UINT s_uiNextFrameTime = ELTimer_GetMSec();

#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime1=ELTimer_GetMSec();
#endif
	CTimer& rkTimer=CTimer::Instance();
	rkTimer.Advance();

	m_fGlobalTime = rkTimer.GetCurrentSecond();
	m_fGlobalElapsedTime = rkTimer.GetElapsedSecond();

	UINT uiFrameTime = rkTimer.GetElapsedMilliecond();
	s_uiNextFrameTime += uiFrameTime;

	DWORD updatestart = ELTimer_GetMSec();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime2=ELTimer_GetMSec();
#endif
	// Network I/O	
	m_pyNetworkStream.Process();	
	//m_pyNetworkDatagram.Process();

	m_kGuildMarkUploader.Process();

	m_kGuildMarkDownloader.Process();
	m_kAccountConnector.Process();

#ifdef __PERFORMANCE_CHECK__		
	DWORD dwUpdateTime3=ELTimer_GetMSec();
#endif
	//////////////////////
	// Input Process
	// Keyboard
	UpdateKeyboard();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime4=ELTimer_GetMSec();
#endif
	// Mouse
	POINT Point;
	if (GetCursorPos(&Point))
	{
		ScreenToClient(m_hWnd, &Point);
		OnMouseMove(Point.x, Point.y);		
	}
	//////////////////////
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime5=ELTimer_GetMSec();
#endif
	//if (m_isActivateWnd)
	__UpdateCamera();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime6=ELTimer_GetMSec();
#endif
	// Update Game Playing
	CResourceManager::Instance().Update();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime7=ELTimer_GetMSec();
#endif
	OnCameraUpdate();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime8=ELTimer_GetMSec();
#endif
	OnMouseUpdate();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime9=ELTimer_GetMSec();
#endif
	OnUIUpdate();

#ifdef __PERFORMANCE_CHECK__		
	DWORD dwUpdateTime10=ELTimer_GetMSec();

	if (dwUpdateTime10-dwUpdateTime1>10)
	{			
		static FILE* fp=fopen("perf_app_update.txt", "w");

		fprintf(fp, "AU.Total %d (Time %d)\n", dwUpdateTime9-dwUpdateTime1, ELTimer_GetMSec());
		fprintf(fp, "AU.TU %d\n", dwUpdateTime2-dwUpdateTime1);
		fprintf(fp, "AU.NU %d\n", dwUpdateTime3-dwUpdateTime2);
		fprintf(fp, "AU.KU %d\n", dwUpdateTime4-dwUpdateTime3);
		fprintf(fp, "AU.MP %d\n", dwUpdateTime5-dwUpdateTime4);
		fprintf(fp, "AU.CP %d\n", dwUpdateTime6-dwUpdateTime5);
		fprintf(fp, "AU.RU %d\n", dwUpdateTime7-dwUpdateTime6);
		fprintf(fp, "AU.CU %d\n", dwUpdateTime8-dwUpdateTime7);
		fprintf(fp, "AU.MU %d\n", dwUpdateTime9-dwUpdateTime8);
		fprintf(fp, "AU.UU %d\n", dwUpdateTime10-dwUpdateTime9);			
		fprintf(fp, "----------------------------------\n");
		fflush(fp);
	}		
#endif

	m_dwCurUpdateTime = ELTimer_GetMSec() - updatestart;

	DWORD dwCurrentTime = ELTimer_GetMSec();
	BOOL  bCurrentLateUpdate = FALSE;

	s_bFrameSkip = false;

	if (dwCurrentTime > s_uiNextFrameTime)
	{
		int dt = dwCurrentTime - s_uiNextFrameTime;
		int nAdjustTime = ((float)dt / (float)uiFrameTime) * uiFrameTime; 

		if ( dt >= 500 )
		{
			s_uiNextFrameTime += nAdjustTime; 
			printf("FrameSkip Adjusting... %d\n",nAdjustTime);
			CTimer::Instance().Adjust(nAdjustTime);
		}

		s_bFrameSkip = true;
		bCurrentLateUpdate = TRUE;
	}

	if (m_isFrameSkipDisable)
		s_bFrameSkip = false;

	bool canRender = true;

	if (!s_bFrameSkip)
	{
		CGrannyMaterial::TranslateSpecularMatrix(g_specularSpd, g_specularSpd, 0.0f);
		DWORD dwRenderStartTime = ELTimer_GetMSec();		

		if (m_isMinimizedWnd)
		{
			canRender = false;
		}
		else
		{
#ifdef ENABLE_FIX_MOBS_LAG
			if (DEVICE_STATE_OK != CheckDeviceState())
			{
				//canRender = false;
				
				CPythonBackground& rkBG = CPythonBackground::Instance();
#ifdef RENDER_TARGET_SYSTEM
				CRenderTargetManager::Instance().ReleaseRenderTargetTextures();
#endif
#ifdef RENDER_TARGET_SYSTEM
				if (m_pyGraphic.RestoreDevice())
				{
					
					CRenderTargetManager::Instance().CreateRenderTargetTextures();
					rkBG.CreateCharacterShadowTexture();
				}
				else
					canRender = false;
#else
				if (m_pyGraphic.RestoreDevice())					
					rkBG.CreateCharacterShadowTexture();
				else
					canRender = false;
#endif
			}
#else


			// orjinal blok.
			if (m_pyGraphic.IsLostDevice())
			{
				CPythonBackground& rkBG = CPythonBackground::Instance();
				rkBG.ReleaseCharacterShadowTexture();
#ifdef RENDER_TARGET_SYSTEM
				CRenderTargetManager::Instance().ReleaseRenderTargetTextures();
#endif
#ifdef RENDER_TARGET_SYSTEM
				if (m_pyGraphic.RestoreDevice())					
				{
					CRenderTargetManager::Instance().CreateRenderTargetTextures();
					rkBG.CreateCharacterShadowTexture();
				}
				else
					canRender = false;
#else
				if (m_pyGraphic.RestoreDevice())					
					rkBG.CreateCharacterShadowTexture();
				else
					canRender = false;
#endif			
			}
#endif
		}

		if (!IsActive())
		{
			SkipRenderBuffering(3000);
		}

		if (!canRender)
		{
			SkipRenderBuffering(3000);
		}
		else
		{
			// RestoreLostDevice
			CCullingManager::Instance().Update();
			if (m_pyGraphic.Begin())
			{
				
				m_pyGraphic.ClearDepthBuffer();

#ifdef _DEBUG
				m_pyGraphic.SetClearColor(0.3f, 0.3f, 0.3f);
				m_pyGraphic.Clear();
#endif
				
				/////////////////////
				// Interface
				m_pyGraphic.SetInterfaceRenderState();

				OnUIRender();
				OnMouseRender();
				/////////////////////

				m_pyGraphic.End();

				//DWORD t1 = ELTimer_GetMSec();
				m_pyGraphic.Show();
				//DWORD t2 = ELTimer_GetMSec();
				
				DWORD dwRenderEndTime = ELTimer_GetMSec();

				static DWORD s_dwRenderCheckTime = dwRenderEndTime;
				static DWORD s_dwRenderRangeTime = 0;
				static DWORD s_dwRenderRangeFrame = 0;

				m_dwCurRenderTime = dwRenderEndTime - dwRenderStartTime;			
				s_dwRenderRangeTime += m_dwCurRenderTime;				
				++s_dwRenderRangeFrame;			

				if (dwRenderEndTime-s_dwRenderCheckTime>1000)
				{
					m_fAveRenderTime=float(double(s_dwRenderRangeTime)/double(s_dwRenderRangeFrame));

					s_dwRenderCheckTime=ELTimer_GetMSec();
					s_dwRenderRangeTime=0;
					s_dwRenderRangeFrame=0;
				}										

				DWORD dwCurFaceCount=m_pyGraphic.GetFaceCount();
				m_pyGraphic.ResetFaceCount();
				s_dwFaceCount += dwCurFaceCount;

				if (dwCurFaceCount > 5000)
				{
					if (dwRenderEndTime > m_dwBufSleepSkipTime)
					{	
						static float s_fBufRenderTime = 0.0f;

						float fCurRenderTime = m_dwCurRenderTime;

						if (fCurRenderTime > s_fBufRenderTime)
						{
							float fRatio = fMAX(0.5f, (fCurRenderTime - s_fBufRenderTime) / 30.0f);
							s_fBufRenderTime = (s_fBufRenderTime * (100.0f - fRatio) + (fCurRenderTime + 5) * fRatio) / 100.0f;
						}
						else
						{
							float fRatio = 0.5f;
							s_fBufRenderTime = (s_fBufRenderTime * (100.0f - fRatio) + fCurRenderTime * fRatio) / 100.0f;
						}

						if (s_fBufRenderTime > 100.0f)
							s_fBufRenderTime = 100.0f;

						DWORD dwBufRenderTime = s_fBufRenderTime;

						if (m_isWindowed)
						{						
							if (dwBufRenderTime>58)
								dwBufRenderTime=64;
							else if (dwBufRenderTime>42)
								dwBufRenderTime=48;
							else if (dwBufRenderTime>26)
								dwBufRenderTime=32;
							else if (dwBufRenderTime>10)
								dwBufRenderTime=16;
							else
								dwBufRenderTime=8;
						}
						//if (m_dwCurRenderTime<dwBufRenderTime)
						//	Sleep(dwBufRenderTime-m_dwCurRenderTime);			

						m_fAveRenderTime=s_fBufRenderTime;
					}

					m_dwFaceAccCount += dwCurFaceCount;
					m_dwFaceAccTime += m_dwCurRenderTime;

					m_fFaceSpd=(m_dwFaceAccCount/m_dwFaceAccTime);
					

					if (-1 == m_iForceSightRange)
					{
						static float s_fAveRenderTime = 16.0f;
						float fRatio=0.3f;
						s_fAveRenderTime=(s_fAveRenderTime*(100.0f-fRatio)+max(16.0f, m_dwCurRenderTime)*fRatio)/100.0f;


						float fFar=25600.0f;
						float fNear=MIN_FOG;
						double dbAvePow=double(1000.0f/s_fAveRenderTime);
						double dbMaxPow=60.0;
						float fDistance=max(fNear+(fFar-fNear)*(dbAvePow)/dbMaxPow, fNear);
						m_pyBackground.SetViewDistanceSet(0, fDistance);
						
						if (Frustum::Instance().isLobbyMap == true || 
							std::strcmp(Frustum::Instance().currentMapNameSave.c_str(), "metin2_map_dawnmist_dungeon_01") == 0 ||
							std::strcmp(Frustum::Instance().currentMapNameSave.c_str(), "metin2_map_defensewave") == 0)
						{
							m_pyBackground.SetViewDistanceSet(0, fDistance * 2);
						}

						if (Frustum::Instance().performanceMode == 2)
						{
							m_pyBackground.SetViewDistanceSet(0, fDistance / 1.15);
						}
					}
					else
					{
						
						m_pyBackground.SetViewDistanceSet(0, float(m_iForceSightRange));
					}
				}
				else
				{
					m_pyBackground.SetViewDistanceSet(0, 25600.0f);
					
					if (Frustum::Instance().isLobbyMap == true || 
						std::strcmp(Frustum::Instance().currentMapNameSave.c_str(), "metin2_map_dawnmist_dungeon_01") == 0 || 
						std::strcmp(Frustum::Instance().currentMapNameSave.c_str(), "metin2_map_defensewave") == 0)
					{
						m_pyBackground.SetViewDistanceSet(0, 25600.0f * 2);
					}

					if (Frustum::Instance().performanceMode == 2)
					{
						m_pyBackground.SetViewDistanceSet(0, 25600.0f / 1.15);
					}
				}

				++s_dwRenderFrameCount; 
			}
		}
	}

	int rest = s_uiNextFrameTime - ELTimer_GetMSec();

	if (rest > 0 && !bCurrentLateUpdate )
	{
		s_uiLoad -= rest;
		Sleep(rest);
	}	

	++s_dwUpdateFrameCount;

	s_uiLoad += ELTimer_GetMSec() - dwStart;
	return true;
}

void CPythonApplication::UpdateClientRect()
{

	RECT rcApp;
	GetClientRect(&rcApp);
	OnSizeChange(rcApp.right - rcApp.left, rcApp.bottom - rcApp.top);
}

void CPythonApplication::SetMouseHandler(PyObject* poMouseHandler)
{	
	m_poMouseHandler = poMouseHandler;
}

int CPythonApplication::CheckDeviceState()
{
	CGraphicDevice::EDeviceState e_deviceState = m_grpDevice.GetDeviceState();

	switch (e_deviceState)
	{
	case CGraphicDevice::DEVICESTATE_NULL:
		return DEVICE_STATE_FALSE;

	case CGraphicDevice::DEVICESTATE_BROKEN:
		return DEVICE_STATE_SKIP;
#ifdef ENABLE_FIX_MOBS_LAG
	case CGraphicDevice::DEVICESTATE_NEEDS_RESET:

		m_pyBackground.ReleaseCharacterShadowTexture();
		
#ifdef RENDER_TARGET_SYSTEM
		CRenderTargetManager::Instance().ReleaseRenderTargetTextures();
#endif
#ifndef DISABLE_POST_PROCESSING
		GetPostProcessingChain().ReleaseResources();
#endif
#ifdef DEAD_SCREEN_ANIMATE
		GetShaderRenderTargetEffect().ReleaseResources();
		GetShaderRenderTarget().ReleaseResources();
#endif
		Trace("DEVICESTATE_NEEDS_RESET - Deneniyor..\n");
		if (!m_grpDevice.Reset())
		{
			Trace("Reset islemi yapilamadi");
			return DEVICE_STATE_SKIP;
		}
#ifdef RENDER_TARGET_SYSTEM
		CRenderTargetManager::Instance().CreateRenderTargetTextures();
#endif
		m_pyBackground.CreateCharacterShadowTexture();
#ifndef DISABLE_POST_PROCESSING
		GetPostProcessingChain().CreateResources();
#endif
#ifdef DEAD_SCREEN_ANIMATE
		GetShaderRenderTargetEffect().Create();
		GetShaderRenderTarget().Create(m_dwWidth, m_dwHeight, D3DFMT_A8R8G8B8);
#endif
		break;

	case CGraphicDevice::DEVICESTATE_OK: break;
	default:;
#else
	case CGraphicDevice::DEVICESTATE_NEEDS_RESET:
		if (!m_grpDevice.Reset())
			return DEVICE_STATE_SKIP;

		break;
#endif
	}

	return DEVICE_STATE_OK;
}

bool CPythonApplication::CreateDevice(int width, int height, int Windowed, int bit /* = 32*/, int frequency /* = 0*/, bool antialias)
{
	int iRet;

	m_grpDevice.InitBackBufferCount(2);
	m_grpDevice.RegisterWarningString(CGraphicDevice::CREATE_BAD_DRIVER, ApplicationStringTable_GetStringz(IDS_WARN_BAD_DRIVER, "WARN_BAD_DRIVER"));
	m_grpDevice.RegisterWarningString(CGraphicDevice::CREATE_NO_TNL, ApplicationStringTable_GetStringz(IDS_WARN_NO_TNL, "WARN_NO_TNL"));

	iRet = m_grpDevice.Create(GetWindowHandle(), width, height, Windowed ? true : false, bit,frequency, antialias);
	TraceError("RET: %d", iRet);

	switch (iRet)
	{
	case CGraphicDevice::CREATE_OK:
		return true;

	case CGraphicDevice::CREATE_REFRESHRATE:
		return true;

	case CGraphicDevice::CREATE_ENUM:
	case CGraphicDevice::CREATE_DETECT:
		SET_EXCEPTION(CREATE_NO_APPROPRIATE_DEVICE);
		TraceError("CreateDevice: Enum & Detect failed");
		return false;

	case CGraphicDevice::CREATE_NO_DIRECTX:
		//PyErr_SetString(PyExc_RuntimeError, "DirectX 8.1 or greater required to run game");
		SET_EXCEPTION(CREATE_NO_DIRECTX);
		TraceError("CreateDevice: DirectX 8.1 or greater required to run game");
		return false;

	case CGraphicDevice::CREATE_DEVICE:
		//PyErr_SetString(PyExc_RuntimeError, "GraphicDevice create failed");
		SET_EXCEPTION(CREATE_DEVICE);
		TraceError("CreateDevice: GraphicDevice create failed");
		return false;

	case CGraphicDevice::CREATE_FORMAT:
		SET_EXCEPTION(CREATE_FORMAT);
		TraceError("CreateDevice: Change the screen format");
		return false;

		/*case CGraphicDevice::CREATE_GET_ADAPTER_DISPLAY_MODE:
		//PyErr_SetString(PyExc_RuntimeError, "GetAdapterDisplayMode failed");
		SET_EXCEPTION(CREATE_GET_ADAPTER_DISPLAY_MODE);
		TraceError("CreateDevice: GetAdapterDisplayMode failed");
		return false;*/

	case CGraphicDevice::CREATE_GET_DEVICE_CAPS:
		PyErr_SetString(PyExc_RuntimeError, "GetDevCaps failed");
		TraceError("CreateDevice: GetDevCaps failed");
		return false;

	case CGraphicDevice::CREATE_GET_DEVICE_CAPS2:
		PyErr_SetString(PyExc_RuntimeError, "GetDevCaps2 failed");
		TraceError("CreateDevice: GetDevCaps2 failed");
		return false;

	default:
		if (iRet & CGraphicDevice::CREATE_OK)
		{
			//if (iRet & CGraphicDevice::CREATE_BAD_DRIVER)
			//{
			//	LogBox(ApplicationStringTable_GetStringz(IDS_WARN_BAD_DRIVER), NULL, GetWindowHandle());
			//}
			if (iRet & CGraphicDevice::CREATE_NO_TNL)
			{
				CGrannyLODController::SetMinLODMode(true);
				//LogBox(ApplicationStringTable_GetStringz(IDS_WARN_NO_TNL), NULL, GetWindowHandle());
			}
			return true;
		}

		//PyErr_SetString(PyExc_RuntimeError, "Unknown Error!");
		SET_EXCEPTION(UNKNOWN_ERROR);
		TraceError("CreateDevice: Unknown Error! %d", iRet);
		return false;
	}
}

void CPythonApplication::BeginFutureLoop()
{

	m_future_acknowledged_stop_request = false;
	m_future_should_continue_processing = true;

	auto lmbd = [&]() {
		while (m_future_should_continue_processing)
		{
			if (!Process())
				break;

			m_dwLastIdleTime = ELTimer_GetMSec();
		}

		m_future_acknowledged_stop_request = true;
	};

	m_future = std::async(std::launch::async, lmbd);
}

void CPythonApplication::EndFutureLoop()
{
	m_future_should_continue_processing = false;
	m_future.wait();
}

void CPythonApplication::Loop()
{	
	while (1)
	{	
		if (IsMessage())
		{
			if (!MessageProcess())
				break;
		}
		else if (m_future_acknowledged_stop_request)
		{
			if (!Process())
				break;

			m_dwLastIdleTime=ELTimer_GetMSec();
		}
	}
}

// SUPPORT_NEW_KOREA_SERVER
bool LoadLocaleData(const char* localePath)
{
	CPythonNonPlayer&	rkNPCMgr	= CPythonNonPlayer::Instance();
	CItemManager&		rkItemMgr	= CItemManager::Instance();	
	CPythonSkill&		rkSkillMgr	= CPythonSkill::Instance();
	CPythonNetworkStream& rkNetStream = CPythonNetworkStream::Instance();

	char szItemList[256];
	char szItemProto[256];
	char szItemDesc[256];
#ifdef ENABLE_SHINING_SYSTEM
	char szShiningTable[256];
#endif
	char szMobProto[256];
	char szSkillDescFileName[256];
	char szSkillTableFileName[256];
	char szInsultList[256];
	snprintf(szItemList,	sizeof(szItemList) ,	"%s/item_list.txt",	localePath);		
	snprintf(szItemProto,	sizeof(szItemProto),	"%s/item_proto",	localePath);
	snprintf(szItemDesc,	sizeof(szItemDesc),	"%s/itemdesc.txt",	localePath);
#ifdef ENABLE_SHINING_SYSTEM
	snprintf(szShiningTable, sizeof(szShiningTable), "%s/shiningtable.txt", localePath);
#endif
	snprintf(szMobProto,	sizeof(szMobProto),	"%s/mob_proto",		localePath);	
	snprintf(szSkillDescFileName, sizeof(szSkillDescFileName),	"%s/SkillDesc.txt", localePath);
	snprintf(szSkillTableFileName, sizeof(szSkillTableFileName),	"%s/SkillTable.txt", localePath);	
	snprintf(szInsultList,	sizeof(szInsultList),	"%s/insult.txt", localePath);

	rkNPCMgr.Destroy();
	rkItemMgr.Destroy();	
	rkSkillMgr.Destroy();

	if (!rkItemMgr.LoadItemList(szItemList))
	{
		TraceError("LoadLocaleData - LoadItemList(%s) Error", szItemList);
	}	

	if (!rkItemMgr.LoadItemTable(szItemProto))
	{
		TraceError("LoadLocaleData - LoadItemProto(%s) Error", szItemProto);
		return false;
	}

	if (!rkItemMgr.LoadItemDesc(szItemDesc))
	{
		Tracenf("LoadLocaleData - LoadItemDesc(%s) Error", szItemDesc);	
	}
#ifdef ENABLE_SHINING_SYSTEM
	if (!rkItemMgr.LoadShiningTable(szShiningTable))
	{
		Tracenf("LoadLocaleData - LoadShiningTable(%s) Error", szShiningTable);
	}
#endif
	if (!rkNPCMgr.LoadNonPlayerData(szMobProto))
	{
		TraceError("LoadLocaleData - LoadMobProto(%s) Error", szMobProto);
		return false;
	}

	if (!rkSkillMgr.RegisterSkillDesc(szSkillDescFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillDesc(%s) Error", szSkillDescFileName);
		return false;
	}

	if (!rkSkillMgr.RegisterSkillTable(szSkillTableFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillTable(%s) Error", szSkillTableFileName);
		return false;
	}

	if (!rkNetStream.LoadInsultList(szInsultList))
	{
		Tracenf("CPythonApplication - CPythonNetworkStream::LoadInsultList(%s)", szInsultList);				
	}
	if (LocaleService_IsYMIR())
	{	
		char szEmpireTextConvFile[256];
		for (DWORD dwEmpireID=1; dwEmpireID<=3; ++dwEmpireID)
		{			
			sprintf(szEmpireTextConvFile, "%s/lang%d.cvt", localePath, dwEmpireID);
			if (!rkNetStream.LoadConvertTable(dwEmpireID, szEmpireTextConvFile))
			{
				TraceError("LoadLocaleData - CPythonNetworkStream::LoadConvertTable(%d, %s) FAILURE", dwEmpireID, szEmpireTextConvFile);			
			}
		}
	}
	return true;
}
// END_OF_SUPPORT_NEW_KOREA_SERVER

unsigned __GetWindowMode(bool windowed)
{
	if (windowed)
		return WS_OVERLAPPED | WS_CAPTION |   WS_SYSMENU | WS_MINIMIZEBOX;

	return WS_POPUP;
}

#ifdef KAISER_HDR_MOD
bool CPythonApplication::CheckDLLFile()
{

	char dllFile[MAX_PATH];
	::GetCurrentDirectory(sizeof(dllFile), dllFile);
	std::string dllDirection = std::string(dllFile) + "\\d3d9.dll";
	std::string dllDirection2 = std::string(dllFile) + "\\d3d9_42.dll";

	// check dll file
	// CreateResources() ile zaten olusturuluyor. Yine de kontrol etmekte fayda var..
	if (!std::filesystem::exists(dllDirection) && !std::filesystem::exists(dllDirection2))
	{
		MessageBox(NULL, "Query: DLL not found!", "Error", MB_ICONERROR | MB_OK);
		return false;
	}
	else
	{
		HANDLE hFile = CreateFile(dllDirection.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		const int currentSize = 62464;
		if (hFile != INVALID_HANDLE_VALUE)
		{
			size_t dwFileSize = GetFileSize(hFile, NULL);
			CloseHandle(hFile);
			if (dwFileSize != currentSize)
			{
				remove(dllDirection.c_str()); // remove wrong dll
				CreateResources(true, false); // create DLL in resources
			}
		}

		///////// HDR Controls //////////////
		if (m_pySystem.IsHDR() == 0 && !dllDirection.empty())
		{
			if (std::filesystem::exists(dllDirection))
			{
				rename(dllDirection.c_str(), "d3d9_42.dll");
			}
		}
		if (m_pySystem.IsHDR() == 1 && !dllDirection2.empty())
		{
			if (std::filesystem::exists(dllDirection2))
			{
				rename(dllDirection2.c_str(), "d3d9.dll");
			}
		}
		////////////////////////////////////////
	}
	return true;
}
bool CPythonApplication::CheckINIFile()
{
	char iniFile[MAX_PATH];
	::GetCurrentDirectory(sizeof(iniFile), iniFile);
	std::string iniFileDirection = std::string(iniFile) + "\\SweetFX_settings.ini";
	if (std::filesystem::exists(iniFileDirection))
	{
		TraceError("INI ERROR!");
		remove(iniFileDirection.c_str());
	}
	
	CreateResources(false, true);
	return true;
}
void DeleteIniFile(const std::string& filePath, std::chrono::seconds delay)
{
	std::this_thread::sleep_for(delay);
	std::filesystem::remove(filePath);
}
void CPythonApplication::CreateResources(bool isDll, bool isINI)
{
	
	if (isINI)
	{
		// First create INI file -- start
		HRSRC hRsrc = FindResource(ms_hInstance, MAKEINTRESOURCE(IDR_INI2), TEXT("INI"));
		if (hRsrc == NULL)
		{
			TraceError("hRsrc - Cannot find resources!");
			return;
		}
		HGLOBAL hGlobal = LoadResource(ms_hInstance, hRsrc);
		if (hGlobal == NULL)
		{
			TraceError("hGlobal - Cannot load resources!");
			return;
		}

		DWORD dwSize = SizeofResource(ms_hInstance, hRsrc);
		std::string outputPath = "SweetFX_settings.ini";

		HANDLE hFile = CreateFile(outputPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			TraceError("Cannot create HDR Mod!");
			FreeResource(hGlobal);
			return;
		}
		DWORD dwBytesWritten;
		BOOL bErrorFlag = WriteFile(hFile, LockResource(hGlobal), dwSize, &dwBytesWritten, NULL);
		if (!bErrorFlag)
		{
			TraceError("HDR MOD - FileWrite ERROR!");
			CloseHandle(hFile);
			FreeResource(hGlobal);
			return;
		}
		CloseHandle(hFile);
		FreeResource(hGlobal);
		// INI File -- end
		
	}
	// INI File -- end

	if (isDll)
	{
		HRSRC hRDLL = FindResource(ms_hInstance, MAKEINTRESOURCE(IDR_DLL2), TEXT("DLL"));
		if (hRDLL == NULL)
		{
			TraceError("Cannot find req. dll file!");
			return;
		}
			
		char exePath[MAX_PATH];
		GetModuleFileName(NULL, exePath, MAX_PATH);
		std::string exeDirectory = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));

		HGLOBAL hGlobalDll = LoadResource(ms_hInstance, hRDLL);
		if (hGlobalDll == NULL)
		{
			TraceError("Cannot load resources!");
			return;
		}
		DWORD dwSizeDLL = SizeofResource(ms_hInstance, hRDLL);
		std::string outputPathDLL = exeDirectory + "\\d3d9.dll";
		std::ifstream file(outputPathDLL);
		std::ofstream outputFile(outputPathDLL, std::ios::binary);

		if (!outputFile)
		{
			TraceError("Cannot create req file (Error: 0x001)");
			return;
		}

		outputFile.write((char*)LockResource(hGlobalDll), dwSizeDLL);
		outputFile.close();
	}
}
#endif

bool CPythonApplication::Create(PyObject * poSelf, const char * c_szName, int width, int height, int Windowed)
{
	Windowed = CPythonSystem::Instance().IsWindowed() ? 1 : 0;

	bool bAnotherWindow = false;
	//CreateResources(false, true);
#ifdef KAISER_HDR_MOD
	if (!CheckINIFile() || !CheckDLLFile())
	{
		TraceError("Resources check FAILED! Please re-install game files.");
		return false;
	}
#endif
	
	if (FindWindow(NULL, c_szName))
		bAnotherWindow = true;

	m_dwWidth = width;
	m_dwHeight = height;

	// Window
	UINT WindowMode = __GetWindowMode(Windowed ? true : false);

	if (!CMSWindow::Create(c_szName, 4, 0, WindowMode, ::LoadIcon( GetInstance(), MAKEINTRESOURCE( IDI_METIN2 ) ), IDC_CURSOR_NORMAL))
	{
		//PyErr_SetString(PyExc_RuntimeError, "CMSWindow::Create failed");
		TraceError("CMSWindow::Create failed");
		SET_EXCEPTION(CREATE_WINDOW);
		return false;
	}

	if (m_pySystem.IsUseDefaultIME())
	{
		CPythonIME::Instance().UseDefaultIME();
	}


	if (!m_pySystem.IsWindowed() && (m_pySystem.IsUseDefaultIME() || LocaleService_IsEUROPE()))
	{
		m_isWindowed = false;
		m_isWindowFullScreenEnable = TRUE;
		__SetFullScreenWindow(GetWindowHandle(), width, height, m_pySystem.GetBPP());

		Windowed = true;
	}
	else
	{
		AdjustSize(m_pySystem.GetWidth(), m_pySystem.GetHeight());

		if (Windowed)
		{
			m_isWindowed = true;

			if (bAnotherWindow)
			{
				RECT rc;

				GetClientRect(&rc);

				int windowWidth = rc.right - rc.left;
				int windowHeight = (rc.bottom - rc.top);

				CMSApplication::SetPosition(GetScreenWidth() - windowWidth, GetScreenHeight() - 60 - windowHeight);
			}
		}
		else
		{
			m_isWindowed = false;
			SetPosition(0, 0);
		}
	}


		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Cursor
		if (!CreateCursors())
		{
			//PyErr_SetString(PyExc_RuntimeError, "CMSWindow::Cursors Create Error");
			TraceError("CMSWindow::Cursors Create Error");
			SET_EXCEPTION("CREATE_CURSOR");
			return false;
		}

		if (!m_pySystem.IsNoSoundCard())
		{
			// Sound
			if (!m_SoundManager.Create())
			{
				//		LogBox(ApplicationStringTable_GetStringz(IDS_WARN_NO_SOUND_DEVICE));
			}
		}

		extern bool GRAPHICS_CAPS_SOFTWARE_TILING;

		if (!m_pySystem.IsAutoTiling())
			GRAPHICS_CAPS_SOFTWARE_TILING = m_pySystem.IsSoftwareTiling();

		
		// Device
		if (!CreateDevice(m_pySystem.GetWidth(), m_pySystem.GetHeight(), Windowed, m_pySystem.GetBPP(), m_pySystem.GetFrequency(), m_pySystem.IsAntiAliased()))
			return false;
		
#ifdef DEAD_SCREEN_ANIMATE
		if (!GetShaderRenderTargetEffect().Create()) return false;
		if (!GetShaderRenderTarget().Create(width, height, D3DFMT_A8R8G8B8)) return false;
#endif
		GrannyCreateSharedDeformBuffer();

		if (m_pySystem.IsAutoTiling())
		{
			if (m_grpDevice.IsFastTNL())
			{
				m_pyBackground.ReserveSoftwareTilingEnable(false);
			}
			else
			{
				m_pyBackground.ReserveSoftwareTilingEnable(true);
			}
		}
		else
		{
			m_pyBackground.ReserveSoftwareTilingEnable(m_pySystem.IsSoftwareTiling());
		}

		SetVisibleMode(true);

		if (m_isWindowFullScreenEnable) //m_pySystem.IsUseDefaultIME() && !m_pySystem.IsWindowed())
		{
			SetWindowPos(GetWindowHandle(), HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW);
		}

		if (!InitializeKeyboard(GetWindowHandle()))
			return false;

		m_pySystem.GetDisplaySettings();

		// Mouse
		if (m_pySystem.IsSoftwareCursor())
			SetCursorMode(CURSOR_MODE_SOFTWARE);
		else
			SetCursorMode(CURSOR_MODE_HARDWARE);

		// Network
		if (!m_netDevice.Create())
		{
			//PyErr_SetString(PyExc_RuntimeError, "NetDevice::Create failed");
			TraceError("NetDevice::Create failed");
			SET_EXCEPTION("CREATE_NETWORK");
			return false;
		}

		if (!m_grpDevice.IsFastTNL())
			CGrannyLODController::SetMinLODMode(true);

		m_pyItem.Create();

		// Other Modules
		DefaultFont_Startup();

		CPythonIME::Instance().Create(GetWindowHandle());
		CPythonIME::Instance().SetText("", 0);
		CPythonTextTail::Instance().Initialize();

		// Light Manager
		m_LightManager.Initialize();

		CGraphicImageInstance::CreateSystem(32);

		// ¹é¾÷
		STICKYKEYS sStickKeys;
		memset(&sStickKeys, 0, sizeof(sStickKeys));
		sStickKeys.cbSize = sizeof(sStickKeys);
		SystemParametersInfo( SPI_GETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0 );
		m_dwStickyKeysFlag = sStickKeys.dwFlags;

		// ¼³Á¤
		sStickKeys.dwFlags &= ~(SKF_AVAILABLE|SKF_HOTKEYACTIVE);
		SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0 );

		// SphereMap
		CGrannyMaterial::CreateSphereMap(0, "d:/ymir work/special/spheremap.jpg");
		CGrannyMaterial::CreateSphereMap(1, "d:/ymir work/special/spheremap01.jpg");
#ifndef KAISER_HDR_MOD // after if (!CreateDevice(..
		char iniFile[MAX_PATH];
		::GetCurrentDirectory(sizeof(iniFile), iniFile);
		std::string INIDirection = std::string(iniFile) + "\\SweetFX_settings.ini";
		TraceError("%s", INIDirection.c_str());

		if (std::filesystem::exists(INIDirection))
		{
			DeleteFile(INIDirection.c_str());
			INIDirection = "";
		}
#endif
		CPlayerSettingsModule::Load();
		return true;
}


void CPythonApplication::SetGlobalCenterPosition(LONG x, LONG y)
{
	CPythonBackground& rkBG=CPythonBackground::Instance();
	rkBG.GlobalPositionToLocalPosition(x, y);

	float z = CPythonBackground::Instance().GetHeight(x, y);

	CPythonApplication::Instance().SetCenterPosition(x, y, z);
}

void CPythonApplication::SetCenterPosition(float fx, float fy, float fz)
{
	m_v3CenterPosition.x = +fx;
	m_v3CenterPosition.y = -fy;
	m_v3CenterPosition.z = +fz;
}

void CPythonApplication::GetCenterPosition(TPixelPosition * pPixelPosition)
{
	pPixelPosition->x = +m_v3CenterPosition.x;
	pPixelPosition->y = -m_v3CenterPosition.y;
	pPixelPosition->z = +m_v3CenterPosition.z;
}


void CPythonApplication::SetServerTime(time_t tTime)
{
	m_dwStartLocalTime	= ELTimer_GetMSec();
	m_tServerTime		= tTime;
	m_tLocalStartTime	= time(0);
}

time_t CPythonApplication::GetServerTime()
{
	return (ELTimer_GetMSec() - m_dwStartLocalTime) + m_tServerTime;
}

time_t CPythonApplication::GetServerTimeStamp()
{
	return (time(0) - m_tLocalStartTime) + m_tServerTime;
}

float CPythonApplication::GetGlobalTime()
{
	return m_fGlobalTime;
}

float CPythonApplication::GetGlobalElapsedTime()
{
	return m_fGlobalElapsedTime;
}

void CPythonApplication::SetFPS(int iFPS)
{
	m_iFPS = iFPS;
}

int CPythonApplication::GetWidth()
{
	return m_dwWidth;
}

int CPythonApplication::GetHeight()
{
	return m_dwHeight;
}

void CPythonApplication::SetConnectData(const char * c_szIP, int iPort)
{
	m_strIP = c_szIP;
	m_iPort = iPort;
}

void CPythonApplication::GetConnectData(std::string & rstIP, int & riPort)
{
	rstIP	= m_strIP;
	riPort	= m_iPort;
}

void CPythonApplication::EnableSpecialCameraMode()
{
	m_isSpecialCameraMode = TRUE;
}

void CPythonApplication::SetCameraSpeed(int iPercentage)
{
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed * float(iPercentage) / 100.0f;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed * float(iPercentage) / 100.0f;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed * float(iPercentage) / 100.0f;
}

void CPythonApplication::SetForceSightRange(int iRange)
{
	m_iForceSightRange = iRange;
}

void CPythonApplication::Clear()
{
	m_pySystem.Clear();
}

void CPythonApplication::Destroy()
{
	WebBrowser_Destroy();

	// SphereMap
	CGrannyMaterial::DestroySphereMap();

	m_kWndMgr.Destroy();

	CPythonSystem::Instance().SaveConfig();

#ifdef RENDER_TARGET_SYSTEM
	m_kRenderTargetManager.Destroy();
#endif

	DestroyCollisionInstanceSystem();

	m_pySystem.SaveInterfaceStatus();

	m_pyEventManager.Destroy();	
	m_FlyingManager.Destroy();

	m_pyMiniMap.Destroy();

	m_pyTextTail.Destroy();
	m_pyChat.Destroy();	
	m_kChrMgr.Destroy();
	m_RaceManager.Destroy();

	m_pyItem.Destroy();
	m_kItemMgr.Destroy();

	m_pyBackground.Destroy();

	m_kEftMgr.Destroy();
	m_LightManager.Destroy();

	// DEFAULT_FONT
	DefaultFont_Cleanup();
	// END_OF_DEFAULT_FONT

	GrannyDestroySharedDeformBuffer();

	m_pyGraphic.Destroy();
	//m_pyNetworkDatagram.Destroy();	

	m_pyRes.Destroy();

	m_kGuildMarkDownloader.Disconnect();

	CGrannyModelInstance::DestroySystem();
	CGraphicImageInstance::DestroySystem();


	m_SoundManager.Destroy();
	m_grpDevice.Destroy();
	//CSpeedTreeForestDirectX8::Instance().Clear();

	CAttributeInstance::DestroySystem();
	CTextFileLoader::DestroySystem();
	DestroyCursors();

	CMSApplication::Destroy();

	STICKYKEYS sStickKeys;
	memset(&sStickKeys, 0, sizeof(sStickKeys));
	sStickKeys.cbSize = sizeof(sStickKeys);
	sStickKeys.dwFlags = m_dwStickyKeysFlag;
	SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0 );
}
