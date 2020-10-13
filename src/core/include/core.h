#pragma once

#include <memory>
#include <vector>
#include <string>
#include <assert.h>

namespace seed
{
	namespace log
	{
		enum LogType
		{
			Debug = 0,
			Warning = 1,
			Critical = 2,
			Fatal = 3,
			Info = 4
		};

		bool DumpLog(const int&		i_nType,	// log type
			const char*		i_cFormat,
			...);
	}

	namespace progress
	{
		void UpdateProgress(int value);
	}

	namespace utils
	{
		bool CheckOrCreateFolder(const std::string& i_strDir);
		bool FileExists(const std::string& i_strPath);
	}
}
