#include "time_helpers.hpp"

long long get_milliseconds_unix_timestamp()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
