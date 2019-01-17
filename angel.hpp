#ifndef ANGEL_HPP
#define ANGEL_HPP

#include <windows.h>

class angel
{

private:

	DWORD monitor_pid_;

	static void start_self(wchar_t *command_line);

public:

	angel(DWORD monitor_pid)
	{
		monitor_pid_ = monitor_pid;
	}

	void start_monitoring();

	static void start_angel();
};

#endif