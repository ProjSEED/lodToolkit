#include "core.h"
#include <experimental/filesystem>
#include <algorithm>
#include <iostream>
#include <stdarg.h>

namespace seed
{
	namespace log
	{
		bool DumpLog(const int&		i_nType,	// log type
			const char*		i_cFormat,
			...)		
		{
			if (i_nType == Debug)
			{
#ifndef _DEBUG
				return true;
#endif
			}
			const int MAX_LOG_SIZE = 512;
			char l_cLog[MAX_LOG_SIZE];
			va_list args;
			va_start(args, i_cFormat);
			_vsnprintf_s(l_cLog, _TRUNCATE, i_cFormat, args);
			va_end(args);
			switch (i_nType)
			{
			case Debug:
				printf("Debug: %s\n", l_cLog);
				break;
			case Warning:
				printf("Warning: %s\n", l_cLog);
				break;
			case Critical:
				printf("Critical: %s\n", l_cLog);
				break;
			case Fatal:
				printf("Fatal: %s\n", l_cLog);
				break;
			case Info:
				printf("Info: %s\n", l_cLog);
				break;
			default:
				break;
			}
			return true;
		}
	}

	namespace progress
	{

		class Timer {
		public:

			Timer(){
				restart();
			}

			inline void restart() {
				_start_time = std::chrono::steady_clock::now();
			}

			// sec
			inline double elapsed(bool restart = false) {
				_end_time = std::chrono::steady_clock::now();
				std::chrono::duration<double> diff = _end_time - _start_time;
				if (restart)
					this->restart();
				return diff.count();
			}

		private:
			std::chrono::steady_clock::time_point _start_time;
			std::chrono::steady_clock::time_point _end_time;
		}; // timer

		Timer timer;

		std::string secondToString(double sec)
		{
			if (sec > 60) {
				return std::to_string(sec / 60) + " min";
			}
			else if (sec > 1)
			{
				return std::to_string(sec) + " s";
			}
			else {
				return std::to_string(sec * 1000) + " ms";
			}
		}

		void UpdateProgress(int value)
		{
			double elapsed = timer.elapsed();
			if (value != 0 && elapsed > 30)
			{
				double remaining = elapsed / value * (100 - value);
				printf("Progress: %d, Time elapsed: %s, Time remaining: %s\n", value, secondToString(elapsed).c_str(), secondToString(remaining).c_str());
			}
			else
			{
				printf("Progress: %d, Time elapsed: %s, Time remaining: calculating...\n", value, secondToString(elapsed).c_str());
			}
		}
	}

	namespace utils
	{
		bool CheckOrCreateFolder(const std::string& i_strDir)
		{
			if (std::experimental::filesystem::exists(i_strDir))
			{
				return true;
			}
			if (std::experimental::filesystem::create_directories(i_strDir) == false)
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", i_strDir.c_str());
				return false;
			}
			else
			{
				return true;
			}
		}

		bool FileExists(const std::string& i_strPath)
		{
			return std::experimental::filesystem::exists(i_strPath);
		}
	}
}