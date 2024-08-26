/*

MinMem v1.0 By Aceinet
A begginer friendly library for writing/reading memory to process
----------------------
Example:

	Mem mem = Mem("process.exe"); // initalize process
	mem.find_address(some_pointer, some_unsignedint_offsets); // find pointer
	mem.write<int>(some_pointer, some_unsignedint_offsets, 123); // write pointer
	int a = mem.read<int>(some_pointer, some_unsignedint_offsets); // read pointer

*/

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <string_view>
#include <vector>
#include <tchar.h>
#include <comdef.h>

using namespace std;

wchar_t* GetWC(const char* c)
{
	const size_t cSize = strlen(c) + 1;
	wchar_t* wc = new wchar_t[cSize];
	mbstowcs(wc, c, cSize);

	return wc;
}

DWORD GetProcId(const char* processname) {
	std::wstring processName;
	processName.append(GetWC(processname));
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processesSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	Process32First(processesSnapshot, &processInfo);

	if (!processName.compare(processInfo.szExeFile))
	//if (!processName.compare(GetWC(processInfo.szExeFile)))
	{
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
		//if (!processName.compare(GetWC(processInfo.szExeFile)))
		{
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}

MODULEENTRY32 GetModule(DWORD procId, const char* modName)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				_bstr_t x(modEntry.szModule);
				if (!_stricmp(x, modName))
				{
					return modEntry;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	MODULEENTRY32 modEntryNULL;
	return modEntryNULL;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const char* modName)
{
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				_bstr_t x(modEntry.szModule);
				if (!_stricmp(x, modName))
				{
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}

void PatchEx(BYTE* dst, BYTE* src, unsigned int size, HANDLE hProcess) {
	DWORD oldprotect;
	VirtualProtectEx(hProcess, dst, size, PAGE_EXECUTE_READWRITE, &oldprotect);
	WriteProcessMemory(hProcess, dst, src, size, nullptr);
	VirtualProtectEx(hProcess, dst, size, oldprotect, &oldprotect);
}

struct PatternByte {
	bool isWildcard;
	char value;
};

class Mem {
public:
	uintptr_t moduleBase = NULL;
	DWORD pid = NULL;
	HANDLE process;

	Mem(const char* procname, const char* basename) {
		set_base(procname, basename);
	}

	void set_base(const char* procname, const char* basename) {
		/*
		Set base of current process
		===========================
		Args: const char* basename
		*/
		pid = GetProcId(procname);
		moduleBase = (uintptr_t)GetModuleBaseAddress(pid, basename); // Get module base of process name
		process = OpenProcess(PROCESS_ALL_ACCESS, NULL, pid);
	}

	uintptr_t find_address(uintptr_t ptr, std::vector<unsigned int> offsets) { // find pointer knowing address and offset
		/*
		Find address of pointer
		=======================
		Args: uintptr_t ptr_address
			  std::vector<unsigned int> offsets
		*/
		uintptr_t address = ptr;
		for (unsigned int i = 0; i < offsets.size(); ++i) {
			address = *(uintptr_t*)address;
			address += offsets[i];
		}
		return address;
	}

	template<typename T>
	T read(uintptr_t ptr, std::vector<unsigned int> offsets) {
		/*
		Read value from pointer
		=======================
		Args: uintptr_t ptr_address
			  std::vector<unsigned int> offsets

		Template:
			typename T (float, int, byte, double etc..)

		Example usage:
						// type specifier for pointer (template specifier, in cheat engine int is 4 bytes)
						//
						//   v
			int a = mem.read<int>(some_ptr, some_offsets);
		*/
		T value = *(T*)find_address(ptr, offsets); // find pointer and return it
		return value;
	}

	template<typename T>
	void write(uintptr_t ptr, std::vector<unsigned int> offsets, T value) {
		/*
		Read value from pointer
		=======================
		Args: uintptr_t ptr_address
			  std::vector<unsigned int> offsets
			  T value

		Template:
			typename T (float, int, byte, double etc..)

		Example usage:
					// type specifier for pointer (template specifier, in cheat engine int is 4 bytes)
					//
					// v
			 mem.write<int>(some_ptr, some_offsets, some_value);
		*/
		*(T*)find_address(ptr, offsets) = value; // find pointer and set value to it
	}

	void PatchBytes(uintptr_t address, BYTE* bytes, unsigned int size) {
		HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
		PatchEx((BYTE*)address, bytes, size, handle);
	}

	uintptr_t PatternScan(uintptr_t scanSize, const std::string signature, uintptr_t base = 0) {
		// Seriously, do not use this function, cuz it's really slow
		// you can do better I beleive
		std::vector<PatternByte> patternData;

		if (base == 0) {
			base = moduleBase;
		}

		for (size_t i = 0; i < signature.size(); ++i) {
			if (signature[i] == ' ') {
				continue;
			}

			if (signature[i] == '?') {
				patternData.push_back({ true, 0 });
			}
			else {
				std::string byteStr = signature.substr(i, 2);
				patternData.push_back({ false, static_cast<char>(std::stoul(byteStr, nullptr, 16)) });
				i++;
			}
		}

		// Search for the signature in the target region
		for (uintptr_t i = base; i < base + scanSize; ++i) {
			bool found = true;

			for (size_t j = 0; j < patternData.size(); ++j) {
				if (patternData[j].isWildcard) {
					continue;
				}
				uintptr_t byte_addr = i + j;
				DWORD oldProtect;
				char byte = 0;
				//VirtualProtectEx(process, (void*)byte_addr, sizeof(char), PROCESS_VM_READ, &oldProtect);
				ReadProcessMemory(process, (void*)byte_addr, &byte, sizeof(char), NULL);
				//VirtualProtectEx(process, (void*)byte_addr, sizeof(char), oldProtect, NULL);
				//byte = *reinterpret_cast<char*>(i + j);
				//cout << byte;
				if (patternData[j].value != byte) {
					found = false;
					break;
				}
			}


			if (found) {
				return i;
			}
		}

		return 0;
	}

};
