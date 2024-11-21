// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "MemoryMgr.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <d3d9.h>


#define INI_NAME ".\\bb2patch.ini"


struct VideoMode
{
	int   width;
	int   height;

	// x + y, total pixels required? dunno
	int   combined;
};

std::vector<VideoMode> resolutions;
int gResolutionX, gResolutionY;
float gScaleRatio, gAspectRatioReversed, gFOVFactor;


bool mode_compare(VideoMode first, VideoMode in)
{
	return first.height == in.height && first.width == in.width;
}


void GetVideoModes()
{
	IDirect3D9* dev = Direct3DCreate9(D3D_SDK_VERSION);

	if (dev)
	{
		for (unsigned int i = 0; i < dev->GetAdapterModeCount(0, D3DFMT_X8R8G8B8); i++)
		{
			D3DDISPLAYMODE mode;

			if (SUCCEEDED(dev->EnumAdapterModes(0, D3DFMT_X8R8G8B8, i, &mode)))
			{
				VideoMode video_mode;
				video_mode.width = mode.Width;
				video_mode.height = mode.Height;
				video_mode.combined = mode.Width + mode.Height;
				resolutions.push_back(video_mode);
			}

		}
		auto pos = std::unique(resolutions.begin(), resolutions.end(), mode_compare);
		resolutions.erase(pos, resolutions.end());

		VideoMode dummy = { -1,-1,-1 };
		resolutions.push_back(dummy);

		dev->Release();
		dev = nullptr;
	}
	else
	{
		MessageBox(0, L"Failed to create Direct3D device!", L"Error", MB_ICONERROR);
	}
}


int bbOpenDisplay_Hook(int resX, int resY, int bpp, int z, int a5)
{
	// patch 2d
	int x = resX;
	int y = resY;
	float aspectRatio = (float)x / (float)y;
	float _4_3 = 4.0f / 3.0f;
	float scaleRatio = aspectRatio / _4_3;

	gAspectRatioReversed = (float)y / (float)x;

	float newScale = 1.0f / (float)x * ((float)y / 480.0f);
	Memory::VP::Patch<float>(0x6C2C58, newScale);

	float swapIconOffset = *(float*)0x6C85F8;
	swapIconOffset *= scaleRatio * 1.25f;
	Memory::VP::Patch(0x6C85F8, swapIconOffset);

	gResolutionX = x;
	gResolutionX = y;
	gScaleRatio = scaleRatio;

	return ((int(__cdecl*)(int, int, int, int ,int))0x61BE40)(resX, resY, bpp, z, a5);
}


class Camera {
public:
	char pad[80];
	float viewport[2];

	void Update()
	{
		float temp[2];
		temp[0] = viewport[0];
		temp[1] = viewport[1];

		viewport[0] *= gScaleRatio * gFOVFactor;
	    viewport[1] = viewport[0] * gAspectRatioReversed;

		((void(__thiscall*)(Camera*))0x42D480)(this);

		viewport[0] = temp[0];
		viewport[1] = temp[1];
	}
};

float GetINIFloat(const char* name)
{
	float result = 0.0f;
	static char buffer[64] = {};
	GetPrivateProfileStringA("Settings", name, "1.0", buffer, sizeof(buffer), INI_NAME);
	sscanf_s(buffer, "%f", &result);
	return result;
}

bool IsConfig()
{
	return GetModuleHandle(L"Launcher.exe");
}

void WINAPI GlobalMemoryStatus_Hook(LPMEMORYSTATUS lpBuffer)
{
	static MEMORYSTATUSEX mem;
	ZeroMemory(&mem, sizeof(MEMORYSTATUSEX));
	mem.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&mem);

	static DWORD _1gb = 1024 * 1024 * 1024 * 1;

	lpBuffer->dwAvailPageFile = min(static_cast<DWORD>(mem.ullAvailPageFile), _1gb);
	lpBuffer->dwAvailPhys = min(static_cast<DWORD>(mem.ullAvailPhys), _1gb);
	lpBuffer->dwAvailVirtual = min(static_cast<DWORD>(mem.ullAvailVirtual), _1gb);
	lpBuffer->dwTotalPageFile = min(static_cast<DWORD>(mem.ullTotalPageFile), _1gb);
	lpBuffer->dwTotalPhys = min(static_cast<DWORD>(mem.ullTotalPhys), _1gb);
	lpBuffer->dwTotalVirtual = min(static_cast<DWORD>(mem.ullTotalVirtual), _1gb);

	lpBuffer->dwMemoryLoad = static_cast<DWORD>(mem.dwMemoryLoad);
}

void InitConfigPatches()
{
	GetVideoModes();
	Memory::VP::Patch<int>(0x402E4B + 2, (int)&resolutions[0].combined);
	Memory::VP::Patch<int>(0x402E60 + 1, (int)&resolutions[0].height);
}

void Init()
{
	if (IsConfig())
	{
		InitConfigPatches();
	}
	else
	{	
		gFOVFactor = GetINIFloat("FOVFactor");
		// fix Babel Initialization Error due to GlobalMemoryStatus breaking over 4gbs, caps reported ram to 1GB
		Memory::VP::Patch(0x6C20B0, GlobalMemoryStatus_Hook);
		Memory::VP::InjectHook(0x42D3C5, bbOpenDisplay_Hook);
		Memory::VP::InjectHook(0x42F859, &Camera::Update);
	}

}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Init();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}