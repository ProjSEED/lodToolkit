#include "core.h"
#include <chrono>
#include <mutex>
#include <stdarg.h>

namespace seed
{
	namespace log
	{
		bool DumpLog(const int&		logType,	// log type
			const char*		logContent,
			...)		
		{
			if (logType == Debug)
			{
#ifndef _DEBUG
				return true;
#endif
			}
			const int MAX_LOG_SIZE = 512;
			char buffer[MAX_LOG_SIZE];
			va_list args;
			va_start(args, logContent);
			_vsnprintf_s(buffer, _TRUNCATE, logContent, args);
			va_end(args);
			switch (logType)
			{
			case Debug:
				printf("Debug: %s\n", buffer);
				break;
			case Warning:
				printf("Warning: %s\n", buffer);
				break;
			case Critical:
				printf("Critical: %s\n", buffer);
				break;
			case Fatal:
				printf("Fatal: %s\n", buffer);
				break;
			case Info:
				printf("Info: %s\n", buffer);
				break;
			default:
				break;
			}
			fflush(stdout);
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
		};

		Timer timer;
		std::mutex mtx;
		int percent = -1;
		double elapsedLast = 0;

		std::string secondToString(double sec)
		{
			if (sec > 60) {
				int secInt = (int)sec;
				int minPart = secInt / 60;
				int secPart = secInt % 60;
				return std::to_string(minPart) + "m " + std::to_string(secPart) + "s";
			}
			else if (sec > 1)
			{
				int secInt = (int)sec;
				return std::to_string(secInt) + "s";
			}
			else {
				int msecInt = (int)(sec * 1000);
				return std::to_string(msecInt) + "ms";
			}
		}

		void UpdateProgress(int value, bool reset)
		{
			std::lock_guard<std::mutex> lck(mtx);
			if (reset)
			{
				timer.restart();
				percent = -1;
			}
			double elapsed = timer.elapsed();
			if (percent < value)
			{
				percent = value;
			}
			else if(elapsed - elapsedLast < 30)
			{
				return;
			}
			elapsedLast = elapsed;
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
}