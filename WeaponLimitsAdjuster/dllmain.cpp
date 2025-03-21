#include <Windows.h>
#include <Psapi.h>
#include <Hooking.Patterns.h>
#include <cstdint>

void* AllocateStubMemory(size_t size);

bool bEnhanced = false;

int CWeaponInfoBlob_Size;
int CWeaponComponentInfo_Size;

void CheckForEnhanced()
{
	auto pat = hook::pattern("47 54 41 56 20 45 6E 68 61 6E 63 65 64 00"); // "GTAV Enhanced" string
	if (pat.size() > 0)
	{
		OutputDebugStringA("nhanced");
		bEnhanced = true;
	}
}

void ReadINI()
{
	CWeaponInfoBlob_Size = GetPrivateProfileInt("WeaponLimitsAdjuster", "CWeaponInfoBlob", -1, ".\\WeaponLimitsAdjuster.ini");
	CWeaponInfoBlob_Size = CWeaponInfoBlob_Size <= 0 ? 512 : CWeaponInfoBlob_Size;

	CWeaponComponentInfo_Size = GetPrivateProfileInt("WeaponLimitsAdjuster", "CWeaponComponentInfo", -1, ".\\WeaponLimitsAdjuster.ini");
	CWeaponComponentInfo_Size = CWeaponComponentInfo_Size <= 0 ? 1024 : CWeaponComponentInfo_Size;
}

void AdjustWeaponInfoBlobsLimit()
{
	DWORD oldProtect;

	if (!bEnhanced)
	{
		auto size = hook::get_pattern<int32_t>("BA ? ? ? ? 4C 89 83 ? ? ? ? 44 89 83 ? ? ? ? E8", 1);

		VirtualProtect(size, sizeof(int32_t), PAGE_EXECUTE_READWRITE, &oldProtect);
		*size = CWeaponInfoBlob_Size;
		VirtualProtect(size, sizeof(int32_t), oldProtect, &oldProtect);
	}
	else
	{
		{
			auto location = hook::get_pattern<int32_t>("BA ? ? ? ? E8 ? ? ? ? 48 89 05 ? ? ? ? 66 C7 05 ? ? ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 05", 1);
			VirtualProtect(location, sizeof(int32_t), PAGE_EXECUTE_READWRITE, &oldProtect);
			*location = CWeaponInfoBlob_Size;
			VirtualProtect(location, sizeof(int32_t), oldProtect, &oldProtect);
		}

		{
			auto location = hook::get_pattern<int32_t>("C7 47 ? ? ? ? ? 0F 57 C0 0F 11 47 ? 8B 05", -22);
			VirtualProtect(location, sizeof(int32_t), PAGE_EXECUTE_READWRITE, &oldProtect);
			*location = CWeaponInfoBlob_Size;
			VirtualProtect(location, sizeof(int32_t), oldProtect, &oldProtect);
		}

		{
			auto location = hook::get_pattern<int32_t>("48 89 C6 48 8D 05 ? ? ? ? 48 89 06 C7 46 ? ? ? ? ? 0F 57 C0 0F 11 46 ? 8B 05", -9);
			VirtualProtect(location, sizeof(int32_t), PAGE_EXECUTE_READWRITE, &oldProtect);
			*location = CWeaponInfoBlob_Size;
			VirtualProtect(location, sizeof(int32_t), oldProtect, &oldProtect);
		}
	}
}

struct PatternPair
{
	std::string_view pattern;
	int count;
	int index;
	int offset;
};

void RelocateRelativeWeaponComponentsArray(void* newWeaponComponentsArray, std::initializer_list<PatternPair> list)
{
	void* oldAddress = nullptr;

	for (auto& entry : list)
	{
		auto location = hook::pattern(entry.pattern).count(entry.count).get(entry.index).get<int32_t>(entry.offset);

		if (!oldAddress)
		{
			oldAddress = (uint8_t*)location + *location + 4;
		}

		auto curTarget = (uint8_t*)location + *location + 4;
		assert(curTarget == oldAddress);

		DWORD oldProtect;
		VirtualProtect(location, sizeof(int32_t), PAGE_EXECUTE_READWRITE, &oldProtect);
		*location = (intptr_t)newWeaponComponentsArray - (intptr_t)location - 4;
		VirtualProtect(location, sizeof(int32_t), oldProtect, &oldProtect);
	}
}

void AdjustWeaponComponentsLimit()
{
	// from https://github.com/citizenfx/fivem/blob/7f18e00424f684c7b147e7bb7136062f680bfc11/code/components/gta-streaming-five/src/PatchWeaponLimits.cpp#L74
	void** newArray = (void**)AllocateStubMemory(sizeof(void*) * CWeaponComponentInfo_Size);

	std::initializer_list<PatternPair> patternPairs = {
		{ "42 8D 0C 02 4C 8D 0D ? ? ? ? D1 F9", 1, 0, 7 },
		{ "45 8D 1C 1A 4C 8D 0D ? ? ? ? 41 D1 FB", 1, 0, 7 },
		{ "42 8D 14 01 4C 8D 0D ? ? ? ? D1 FA", 1, 0, 7 },
		{ "47 8D 0C 1A 4C 8D 05 ? ? ? ? 41 D1 F9", 1, 0, 7 },
		{ "43 8D 0C 08 48 8D 35 ? ? ? ? D1 F9", 1, 0, 7 },
		{ "43 8D 14 1A 48 8D 0D ? ? ? ? D1 FA", 1, 0, 7 },
		{ "42 8D 14 01 48 8D 1D ? ? ? ? D1 FA", 1, 0, 7 },
		{ "46 8D 04 0A 4C 8D 15 ? ? ? ? 41 D1 F8", 1, 0, 7 }, // 0x6CF598A2957C2BF8
		{ "46 8D 04 11 48 8D 15 ? ? ? ? 41 D1 F8", 1, 0, 7 }, // 0x5CEE3DF569CECAB0
		{ "47 8D 0C 10 48 8D 0D ? ? ? ? 41 D1 F9", 1, 0, 7 }, // 0xB3CAF387AE12E9F8
		{ "46 8D 04 0A 48 8D 1D ? ? ? ? 41 D1 F8 49 63 C0", 2, 0, 7 }, // 0x0DB57B41EC1DB083
		{ "46 8D 04 0A 48 8D 1D ? ? ? ? 41 D1 F8 49 63 C0", 2, 1, 7 }, // 0x6558AC7C17BFEF58
		{ "43 8D 14 08 48 8D 1D ? ? ? ? D1 FA", 1, 0, 7 }, // 0x4D1CB8DC40208A17
		{ "46 8D 04 0A 48 8D 0D ? ? ? ? 41 D1 F8", 1, 0, 7 }, // 0x3133B907D8B32053
		{ "33 DB 48 8D 3D ? ? ? ? 8B D3", 1, 0, 5 }, // _loadWeaponComponentInfos
		{ "48 8D 1D ? ? ? ? 41 39 BE", 1, 0, 3 },
		{ "48 8D 1D ? ? ? ? 45 3B C8 7E", 1, 0, 3 },
		{ "48 8D 1D ? ? ? ? 41 3B BE ? ? ? ? 72", 1, 0, 3 },
		{ "48 8D 1D ? ? ? ? 33 FF 41 39 BE", 1, 0, 3 },
		{ "48 8D 1D ? ? ? ? 41 3B BE ? ? ? ? 0F", 1, 0, 3 },
	};

	if (bEnhanced)
	{
		patternPairs = {
			{ "41 FF CA 4C 8D 05", 1, 0, 6 },
			{ "45 8D 53 ? 4C 8D 3D", 1, 0, 7 },
			{ "41 83 BE ? ? ? ? ? 0F 84 ? ? ? ? 4C 8D 2D", 1, 0, 17 },
			{ "88 44 24 ? 31 FF 48 8D 2D", 1, 0, 9 },
			{ "4C 8D B6 ? ? ? ? 45 31 FF 4C 8D 2D", 1, 0, 13 },
			{ "FF CE 31 FF 4C 8D 1D", 1, 0, 7 },
			{ "4D 8B 44 24 ? FF C9", 1, 0, 10 },
			{ "0F 84 ? ? ? ? 45 31 C9 4C 8D 15", 1, 0, 12 },
			{ "4C 8D 0D ? ? ? ? 41 B8 ? ? ? ? 48 FF 25 ? ? ? ? EF", 1, 0, -4 },
			{ "FF CB 31 FF 4C 8D 05", 1, 0, 7 },
			{ "7E ? FF CE 4C 8D 0D", 1, 0, 7 },
			{ "FF CF 31 F6 4C 8D 05", 1, 0, 7 },
			{ "FF CD 31 DB 48 8D 15", 1, 0, 7 },
			{ "FF CD 31 D2 48 8D 0D", 1, 0, 7 },
			{ "41 FF CB 31 C0 4C 8D 0D", 1, 0, 8 },
			{ "FF CE 31 DB 4C 8D 05", 1, 0, 7 },
			{ "FF CB 31 FF 4C 8D 0D", 1, 0, 7 },
		};
	}

	RelocateRelativeWeaponComponentsArray(newArray, patternPairs);
}

void OverwriteWeaponComponentInfoPoolSize()
{
	// replace call to rage::fwConfigManager::GetSizeOfPool with 'mov eax, CWeaponComponentInfo_Size'
	auto addr = hook::get_pattern<uint8_t>("BA CF 3C E3 BA 41 B8 ? ? ? ? E8", 11); // unchanged on enhanced

	DWORD oldProtect;
	VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
	*addr = 0xB8;
	*(int*)(addr + 1) = CWeaponComponentInfo_Size;
	VirtualProtect(addr, 5, oldProtect, &oldProtect);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		ReadINI();
		CheckForEnhanced();
		AdjustWeaponInfoBlobsLimit();
		AdjustWeaponComponentsLimit();
		OverwriteWeaponComponentInfoPoolSize();
	}

	return TRUE;
}

LPVOID FindPrevFreeRegion(LPVOID pAddress, LPVOID pMinAddr, DWORD dwAllocationGranularity)
{
	ULONG_PTR tryAddr = (ULONG_PTR)pAddress;

	// Round down to the next allocation granularity.
	tryAddr -= tryAddr % dwAllocationGranularity;

	// Start from the previous allocation granularity multiply.
	tryAddr -= dwAllocationGranularity;

	while (tryAddr >= (ULONG_PTR)pMinAddr)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQuery((LPVOID)tryAddr, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) ==
			0)
			break;

		if (mbi.State == MEM_FREE)
			return (LPVOID)tryAddr;

		if ((ULONG_PTR)mbi.AllocationBase < dwAllocationGranularity)
			break;

		tryAddr = (ULONG_PTR)mbi.AllocationBase - dwAllocationGranularity;
	}

	return NULL;
}

void* AllocateStubMemory(size_t size)
{
	// Max range for seeking a memory block. (= 1024MB)
	const uint64_t MAX_MEMORY_RANGE = 0x40000000;

	void* origin = GetModuleHandle(NULL);

	ULONG_PTR minAddr;
	ULONG_PTR maxAddr;

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	minAddr = (ULONG_PTR)si.lpMinimumApplicationAddress;
	maxAddr = (ULONG_PTR)si.lpMaximumApplicationAddress;

	if ((ULONG_PTR)origin > MAX_MEMORY_RANGE &&
		minAddr < (ULONG_PTR)origin - MAX_MEMORY_RANGE)
		minAddr = (ULONG_PTR)origin - MAX_MEMORY_RANGE;

	if (maxAddr > (ULONG_PTR)origin + MAX_MEMORY_RANGE)
		maxAddr = (ULONG_PTR)origin + MAX_MEMORY_RANGE;

	LPVOID pAlloc = origin;

	void* stub = nullptr;
	while ((ULONG_PTR)pAlloc >= minAddr)
	{
		pAlloc = FindPrevFreeRegion(pAlloc, (LPVOID)minAddr, si.dwAllocationGranularity);
		if (pAlloc == NULL)
			break;

		stub = VirtualAlloc(pAlloc, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (stub != NULL)
			break;
	}

	return stub;
}
