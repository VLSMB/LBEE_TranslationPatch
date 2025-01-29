#include "MyHook.h"

char buff[10000];
BYTE writeBuffer[10000];
const char* MemoryMapper = "mapper.txt";			// 存储文本的文件名

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

DWORD offsetToRVA(DWORD offset) {
	// 文件偏移转RVA，仅对.rdata区段有效（文件偏移0x361000——0x3DF000）
	return 0x362000 + (offset - 0x361000);
}

DWORD getImageBase(HANDLE hProcess) {
	return (DWORD)GetModuleHandleA(NULL);
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