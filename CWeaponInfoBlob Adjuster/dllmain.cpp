#include <Windows.h>
#include <Psapi.h>
#include <Hooking.Patterns.h>

void Adjust()
{
	int newSize = GetPrivateProfileInt("Adjuster", "CWeaponInfoBlobs", 0xFFFFFFFF, ".\\CWeaponInfoBlob Adjuster.ini");

	void* address = hook::get_pattern("BA ? ? ? ? 4C 89 83 ? ? ? ? 44 89 83 ? ? ? ? E8", 1);
	if (address)
	{
		DWORD oldProtect;
		VirtualProtect((void*)address, sizeof(int), PAGE_EXECUTE_READWRITE, &oldProtect);

		int* size = (int*)address;

		if(newSize == 0xFFFFFFFF)
		{
			newSize = *size + 25;
		}
		
		*size = newSize;

		VirtualProtect((void*)address, sizeof(int), oldProtect, &oldProtect);
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		Adjust();
	}

	return TRUE;
}

