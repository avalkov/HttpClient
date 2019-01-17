
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "angel.hpp"
#include "users_listener.hpp"

#include <shlwapi.h>
#include <shlobj.h>

#pragma comment(lib, "shlwapi.lib")

const WCHAR MUTEX[] = L"AuroxHttpClient";

void KeepSingleInstance();
WCHAR *GetCommandLineArgValue(WCHAR *valueName);

#ifdef _DEBUG
int main()
#else
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#endif
{
	WCHAR *processType, *parentPID;

	SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);

	if (((processType = GetCommandLineArgValue(L"PROCESS_TYPE:")) != NULL) && lstrcmpiW(processType, L"angel") == 0)
	{
		parentPID = GetCommandLineArgValue(L"PARENT_PID:");

		DWORD pid = std::stoi(parentPID);

		free(processType);
		free(parentPID);

		auto monitor_angel = angel(pid);
		monitor_angel.start_monitoring();

		return 0;
	}
	else
	{
		KeepSingleInstance();

		angel::start_angel();
	}

    auto const address = boost::asio::ip::address::from_string("127.0.0.1");
    auto const port = static_cast<unsigned short>(std::atoi("8020"));
	size_t threads = std::thread::hardware_concurrency();
    
    // The io_service is required for all I/O

    boost::asio::io_service ios{threads};

	boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);

	ctx.set_default_verify_paths();

	std::make_shared<users_listener>(ios, ctx, tcp::endpoint{ address, port })->run();

    // Run the I/O service on the requested number of threads
    
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    
    for(auto i = threads - 1; i > 0; --i)
    {
        v.emplace_back(
        [&ios]
        {
            ios.run();
        });
    }
    
    ios.run();
    
    return 0;
}

void KeepSingleInstance()
{
	SECURITY_ATTRIBUTES securityAttributes;

	ZeroMemory(&securityAttributes, sizeof(securityAttributes));
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = TRUE;

	SetLastError(ERROR_SUCCESS);

	CreateMutexW(&securityAttributes, TRUE, MUTEX);

	DWORD lastError = GetLastError();

	if (lastError == ERROR_ACCESS_DENIED || lastError == ERROR_ALREADY_EXISTS)
	{
		ExitProcess(0);
	}
}

WCHAR *GetCommandLineArgValue(WCHAR *valueName)
{
	WCHAR *commandLine = GetCommandLineW();

	if (commandLine == NULL)
	{
		return NULL;
	}

	WCHAR *pValue = NULL, *value = NULL;

	if ((pValue = StrStrW(commandLine, valueName)) != NULL)
	{
		pValue += lstrlenW(valueName);

		int len = 0;

		while (pValue[len] != ' ' && pValue[len] != '\0')
		{
			len++;
		}

		if (len >= 20)
		{
			// Preventing heap buffer overflow

			return NULL;
		}

		value = (WCHAR *)malloc((len * sizeof(WCHAR)) + sizeof(WCHAR));

		for (int i = 0; i < len; i++)
		{
			value[i] = pValue[i];
		}
	}

	return value;
}

