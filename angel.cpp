
#include "angel.hpp"

void angel::start_monitoring()
{
	while (GetProcessVersion(monitor_pid_) > 0)
	{
		Sleep(1000);
	}

	start_self(NULL);

	Sleep(3000);

	ExitProcess(0);
}

void angel::start_angel()
{
	wchar_t command_line[256];

	ZeroMemory(command_line, sizeof(command_line));
	wsprintfW(command_line, L"PROCESS_TYPE:angel PARENT_PID:%d", GetCurrentProcessId());

	start_self(command_line);
}

void angel::start_self(wchar_t *command_line)
{
	STARTUPINFOW startupInfo;
	PROCESS_INFORMATION processInfo;
	WCHAR currentPath[MAX_PATH];

	ZeroMemory(currentPath, sizeof(currentPath));
	GetModuleFileNameW(NULL, currentPath, sizeof(currentPath) - sizeof(WCHAR));

	ZeroMemory(&startupInfo, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	ZeroMemory(&processInfo, sizeof(processInfo));

	CreateProcessW(currentPath, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}