#include <windows.h>
#include "IniReader.h"

int nCount;
uintptr_t Addr[3];

void Scan()
{
	MEMORY_BASIC_INFORMATION mbi = { 0 };
	uintptr_t dwEndAddr;	

	while (VirtualQuery((VOID *)((uintptr_t)mbi.BaseAddress + mbi.RegionSize), &mbi, sizeof(MEMORY_BASIC_INFORMATION)))
	{
		if (mbi.Protect == PAGE_READWRITE)
		{
			dwEndAddr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize - 1 - 4;

			for (uintptr_t i = (uintptr_t)mbi.BaseAddress; i <= dwEndAddr; i++)
			{
				__try
				{
					if (*(DWORD*)i == 0x6F4C5246 && *(DWORD*)(i + 4) == 0x4C796262 && i != Addr[0] && i != Addr[1] && i != Addr[2])
					{
						Addr[nCount] = i;
						nCount++;
							if (nCount >= 3)
							{
								break;
							}
					}
				}
				__except (true)
				{
					i = dwEndAddr;
				}
			}
		}
	}
}


DWORD WINAPI Thread(LPVOID)
{
	CIniReader iniReader("");
	char* IniStr = iniReader.ReadString("MAIN", "LANGameModeDefault", "ZMLobbyLANGame");

	while (nCount < 3)
	{
		Scan();
	}
	
	strcpy((char*)Addr[0], IniStr);
	strcpy((char*)Addr[1], IniStr);
	strcpy((char*)Addr[2], IniStr);

	char* LANGameMode1 = iniReader.ReadString("SWITCH", "LANGameMode1", "FRLobbyLANGame");
	char* LANGameMode2 = iniReader.ReadString("SWITCH", "LANGameMode2", "ZMLobbyLANGame");
	char* LANGameMode3 = iniReader.ReadString("SWITCH", "LANGameMode3", "MPLobbyLANGame");
	char* LANGameMode4 = iniReader.ReadString("SWITCH", "LANGameMode4", "CP2LobbyLANGame");
	char* LANGameMode5 = iniReader.ReadString("SWITCH", "LANGameMode5", "CP2LobbyOnline");

	DWORD LANGameMode1Hotkey = iniReader.ReadInteger("SWITCH", "LANGameMode1Hotkey", VK_F1);
	DWORD LANGameMode2Hotkey = iniReader.ReadInteger("SWITCH", "LANGameMode2Hotkey", VK_F2);
	DWORD LANGameMode3Hotkey = iniReader.ReadInteger("SWITCH", "LANGameMode3Hotkey", VK_F3);
	DWORD LANGameMode4Hotkey = iniReader.ReadInteger("SWITCH", "LANGameMode4Hotkey", VK_F4);
	DWORD LANGameMode5Hotkey = iniReader.ReadInteger("SWITCH", "LANGameMode5Hotkey", VK_F5);
	
	while (true)
	{
		Sleep(0);

		if ((GetAsyncKeyState(LANGameMode1Hotkey) & 1))
		{ 
			strcpy((char*)Addr[0], LANGameMode1);
			strcpy((char*)Addr[1], LANGameMode1);
			strcpy((char*)Addr[2], LANGameMode1);
		}

		if ((GetAsyncKeyState(LANGameMode2Hotkey) & 1))
		{
			strcpy((char*)Addr[0], LANGameMode2);
			strcpy((char*)Addr[1], LANGameMode2);
			strcpy((char*)Addr[2], LANGameMode2);
		}

		if ((GetAsyncKeyState(LANGameMode3Hotkey) & 1))
		{
			strcpy((char*)Addr[0], LANGameMode3);
			strcpy((char*)Addr[1], LANGameMode3);
			strcpy((char*)Addr[2], LANGameMode3);
		}

		if ((GetAsyncKeyState(LANGameMode4Hotkey) & 1))
		{
			strcpy((char*)Addr[0], LANGameMode4);
			strcpy((char*)Addr[1], LANGameMode4);
			strcpy((char*)Addr[2], LANGameMode4);
		}

		if ((GetAsyncKeyState(LANGameMode5Hotkey) & 1))
		{
			strcpy((char*)Addr[0], LANGameMode5);
			strcpy((char*)Addr[1], LANGameMode5);
			strcpy((char*)Addr[2], LANGameMode5);
		}
	}
	return 0;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&Thread, NULL, 0, NULL);
	}
	return TRUE;
}