#undef UNICODE
#undef _UNICODE
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>

using namespace std;

LPCSTR patchName = "LBEE_CHSMemoryPatch.exe";
LPCSTR gameBackup = "LITBUS_WIN32.exe.bak";
LPCSTR gameName = "LITBUS_WIN32.exe";
LPCSTR steamDllName = "gameoverlayrenderer.dll";
LPCSTR patchCommand = "--chspatch";
LPCSTR patchCommandSetup = "SETUP --chspatch";		// SETUP参数是原游戏自带的，用来启动清空存档功能的
LPCSTR patchEvent = "patchEvent";
LPCSTR gameNameSetupCommand = "LITBUS_WIN32.exe SETUP";
char buff[10000];
BYTE writeBuffer[10000];
const char* MemoryMapper = "mapper.txt";			// 存储文本的文件名
LPCSTR checkCommand = "--check";					// 用来检验内存映射文件是否正确

void errorMessageBox(const char* funcName) {
	const char* pattern = "程序在执行%s时遇到错误，LastError: %d";
	wsprintfA(buff, pattern, funcName, GetLastError());
	MessageBoxA(NULL, buff, "错误", MB_ICONERROR);
}

bool writeCHSMemory(DWORD imageBase, HANDLE hProcess, DWORD rva, BYTE word[], UINT len) {
	// 向游戏进程里进程的某地址里写入数据
	DWORD change;
	if (!VirtualProtectEx(hProcess, (LPVOID)(imageBase + rva), len, PAGE_READWRITE, &change)) {
		cout << "VirtualProtect Fail! LastError:" << GetLastError() << endl;
		return false;
	}
	SIZE_T tmp;
	if (WriteProcessMemory(hProcess, (LPVOID)(imageBase + rva), word, len, &tmp)) {
		return true;
	}
	else {
		cout << "WriteProcess Fail! LastError:" << GetLastError() << endl;
		return false;
	}
}

DWORD getImageBase(HANDLE hProcess) {
	// 获取游戏进程主模块的基地址，基地址每次不固定

	DWORD pid = GetProcessId(hProcess);
	if (pid == 0) {
		errorMessageBox("GetProcessId");
		return 0;
	}
	cout << "pid: " << pid << endl;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		errorMessageBox("CreateToolhelp32Snapshot");
		return 0;
	}

	MODULEENTRY32 moduleEntry;
	moduleEntry.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(hSnapshot, &moduleEntry)) {
		do
		{
			cout << "Module: " << moduleEntry.szModule << endl;
			if (_stricmp(moduleEntry.szModule, gameName) == 0)
			{
				CloseHandle(hSnapshot);
				return (DWORD)moduleEntry.modBaseAddr;
			}
		} while (Module32Next(hSnapshot, &moduleEntry));
	}
	CloseHandle(hSnapshot);
	return 0;
}

DWORD offsetToRVA(DWORD offset) {
	// 文件偏移转RVA，仅对.rdata区段有效（文件偏移0x361000——0x3DF000）
	return 0x362000 + (offset - 0x361000);
}

void startMemoryPatch(HANDLE hProcess) {
	/*
	* 映射文件的输入格式（所有数字均为16进制的形式）
	* 首先是一个数字，代表词条数目
	* 每个词条占三行，第一行有一个数字，为词条所在**文件偏移**
	* 第二行为英文原文，用来计算最大字节长度
	* 第三行为翻译中文，要确保中文的utf-8字节数小于等于英文的utf-8字节数，否则本词条将不进行处理
	*/

	DWORD imageBase = getImageBase(hProcess);
	if (imageBase == 0) {
		MessageBoxA(NULL, "获取进程基地址失败！", "提示", MB_ICONWARNING);
		return;
	}
	cout << "ImageBase:" << hex << imageBase << endl;

	ifstream cin(MemoryMapper);
	if (!cin.is_open()) {
		MessageBoxA(NULL, "内存映射文件不存在！", "提示", MB_ICONWARNING);
		return;
	}
	int t;
	cin >> std::hex >> t;
	while (t--) {
		int offset;
		cin >> std::hex >> offset;
		string eng, chs;
		cin.ignore();
		getline(cin, eng);
		getline(cin, chs);
		if (chs.length() > eng.length()) {
			cout << "offset=" << hex << offset << ": Chinese bytes are more than English" << endl;
			continue;
		}
		DWORD rva = offsetToRVA(offset);
		int len = eng.length();
		for (int i = 0; i < chs.length(); i++) {
			writeBuffer[i] = static_cast<unsigned char>(chs[i]);
		}
		for (int i = chs.length(); i < len; i++) {
			writeBuffer[i] = 0;
		}
		std::cout << "RVA=" << rva << ": patch" << (writeCHSMemory(imageBase, hProcess, rva, writeBuffer, len) ? "success" : "fail") << std::endl;
	}
}

int startChecker() {
	// 注意文件以UTF-8的编码保存
	ifstream cin(MemoryMapper);
	if (!cin.is_open()) {
		MessageBoxA(NULL, "内存映射文件不存在！", "提示", MB_ICONWARNING);
		return 3;
	}
	int t;
	cin >> std::hex >> t;
	while (t--) {
		int offset;
		cin >> std::hex >> offset;
		string eng, chs;
		cin.ignore();
		getline(cin, eng);
		getline(cin, chs);
		if (chs.length() > eng.length()) {
			cout << "offet=" << hex << offset << ": Chinese bytes are more than English" << endl;
		}
		/*cout << "rva=" << hex << rva << ": ";
		for (int i = 0; i < chs.length(); i++) {
			cout << hex << static_cast<int>(static_cast<unsigned char>(chs[i])) << ' ';
		}
		cout << endl;*/
	}
	cout << "all checked!" << endl;
	getchar();
	return 0;
}

int startProcess1(int argc, char* argv[]) {
	// 第一启动阶段，用户在steam里启动游戏程序

	HMODULE hModule = GetModuleHandleA(steamDllName);
	if (hModule == NULL) {
		MessageBoxA(NULL, "请在Steam里启动！", "提示", MB_ICONWARNING);
		return 1;
	}

	// 将本程序复制为LBEE_CHSMemoryPatch.exe
	if (!CopyFileA(gameName, patchName, FALSE)) {
		errorMessageBox("CopyFileA");
		return 1;
	}

	// 启动LBEE_CHSMemoryPatch.exe --chspatch
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	if (!CreateProcessA(patchName, (LPSTR)((argc > 1 && !strcmp(argv[1], "SETUP")) ? patchCommandSetup : patchCommand), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		errorMessageBox("CreateProcessA");
		return 1;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}

int startProcess2(int argc, char* argv[]) {
	// 第二启动阶段，此时本程序名字为LBEE_CHSMemoryPatch.exe，将LITBUS_WIN32.exe.bak复制为LITBUS_WIN32.exe

	if (!CopyFileA(gameBackup, gameName, FALSE)) {
		errorMessageBox("CopyFileA-2");
		return 2;
	}

	// 启动LITBUS_WIN32.exe
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	auto rollback = [&]() {
			// 将LBEE_CHSMemoryPatch.exe再复制回LITBUS_WIN32.exe
			if (!CopyFileA(patchName, gameName, FALSE)) {
				errorMessageBox("CopyFileA-3");
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
		};

	if (!CreateProcessA(NULL, (argc > 0 && !strcmp(argv[0], "SETUP")) ? (LPSTR)gameNameSetupCommand : (LPSTR)gameName, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		errorMessageBox("CreateProcessA-2");
		//CloseHandle(hEvent);
		rollback();
		return 2;
	}

	// 等待进程初始化（否则寻找基地址时会被OS认为是64bit进程）
	DWORD result = WaitForInputIdle(pi.hProcess, INFINITE);
	if (result) {
		errorMessageBox("WaitForInputIdle");
	}

	// 启动内存补丁
	startMemoryPatch(pi.hProcess);

	// 等待游戏进程结束
	DWORD waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
	if (waitResult != WAIT_OBJECT_0) {
		errorMessageBox("WaitForSingleObject");
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		rollback();
		return 2;
	}

	rollback();
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	//CloseHandle(hEvent);
	return 0;
}

int main(int argc, char* argv[]) {

	cout << "argc:" << argc << endl;
	for (int i = 0; i < argc; i++) {
		cout << "argv[" << i << "]: " << argv[i] << endl;
	}

	if (argc>0 && !strcmp(argv[argc-1], checkCommand)) {
		return startChecker();
	}
	else if (argc == 0 || strcmp(argv[argc - 1], patchCommand)) {
		return startProcess1(argc, argv);
	}
	else {
		return startProcess2(argc, argv);
	}

	return 0;
}