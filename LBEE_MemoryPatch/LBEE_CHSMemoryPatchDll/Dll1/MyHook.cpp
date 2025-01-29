#include "MyHook.h"

char buff[10000];
BYTE writeBuffer[10000];
const char* MemoryMapper = "mapper.txt";			// �洢�ı����ļ���

void errorMessageBox(const char* funcName) {
	const char* pattern = "������ִ��%sʱ��������LastError: %d";
	wsprintfA(buff, pattern, funcName, GetLastError());
	MessageBoxA(NULL, buff, "����", MB_ICONERROR);
}

bool writeCHSMemory(DWORD imageBase, HANDLE hProcess, DWORD rva, BYTE word[], UINT len) {
	// ����Ϸ��������̵�ĳ��ַ��д������
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
	// �ļ�ƫ��תRVA������.rdata������Ч���ļ�ƫ��0x361000����0x3DF000��
	return 0x362000 + (offset - 0x361000);
}

DWORD getImageBase(HANDLE hProcess) {
	return (DWORD)GetModuleHandleA(NULL);
}

void startMemoryPatch(HANDLE hProcess) {
	/*
	* ӳ���ļ��������ʽ���������־�Ϊ16���Ƶ���ʽ��
	* ������һ�����֣����������Ŀ
	* ÿ������ռ���У���һ����һ�����֣�Ϊ��������**�ļ�ƫ��**
	* �ڶ���ΪӢ��ԭ�ģ�������������ֽڳ���
	* ������Ϊ�������ģ�Ҫȷ�����ĵ�utf-8�ֽ���С�ڵ���Ӣ�ĵ�utf-8�ֽ��������򱾴����������д���
	*/

	DWORD imageBase = getImageBase(hProcess);
	if (imageBase == 0) {
		MessageBoxA(NULL, "��ȡ���̻���ַʧ�ܣ�", "��ʾ", MB_ICONWARNING);
		return;
	}
	cout << "ImageBase:" << hex << imageBase << endl;

	ifstream cin(MemoryMapper);
	if (!cin.is_open()) {
		MessageBoxA(NULL, "�ڴ�ӳ���ļ������ڣ�", "��ʾ", MB_ICONWARNING);
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