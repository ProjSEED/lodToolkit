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

		bool DumpLog(const int&		logType,	// log type
			const char*		logContent,
			...);
	}

	namespace progress
	{
		void UpdateProgress(int value, bool resetTimer = false);
	}
}
