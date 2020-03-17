#include "types.h"
#include <experimental/filesystem>

namespace tg
{
	namespace log
	{
		bool DumpLog(const int&		i_nType,	// log type
			const char*		i_cFormat,
			...)		
		{
			return true;
		}
	}

	namespace progress
	{
		void UpdateProgress(int value)
		{}

		void UpdateProgressLabel(const std::string& label)
		{}
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
				tg::log::DumpLog(tg::log::Critical, "Create folder %s failed!", i_strDir.c_str());
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