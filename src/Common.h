#pragma once

#include <memory>
#include <vector>
#include <string>
#include <assert.h>

typedef unsigned char IntentType;
const int IntenDim = 3;

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
		unsigned char Color8Bits(uint16_t color16Bit);
		std::string getFileName(const std::string & full_path_in);
		std::string getExtension(const std::string & full_path_in);
	}

	struct Point3F
	{
		Point3F() {}
		Point3F(double _x, double _y, double _z)
		{
			_v[0] = _x;
			_v[1] = _y;
			_v[2] = _z;
		}
		
		double X() { return _v[0]; }
		double Y() { return _v[1]; }
		double Z() { return _v[2]; }

		double _v[3];
		double& operator [] (const int i) { return _v[i]; }
	};

	template <class T, int Dim = 3>
	class Vec
	{
	protected:
		T _v[Dim];

	public:
		void Fill(const T& i_oVal)
		{
			for (int i = 0; i < Dim; ++i)
			{
				_v[i] = i_oVal;
			}
		}

		constexpr int Size() const
		{
			return Dim;
		}

		inline T & operator [] (const int i)
		{
			assert(i >= 0 && i < Dim);
			return _v[i];
		}
		inline const T & operator [] (const int i) const
		{
			assert(i >= 0 && i < Dim);
			return _v[i];
		}
	};

	template <class IntentT, int Dim = 3>
	class PointXYZIT
	{
	public:
		PointXYZIT() :
			P(0,0,0)
		{
			I.Fill(0);
		}

		Point3F P;
		Vec<IntentT, Dim>	I;
	};
	typedef seed::PointXYZIT<IntentType, IntenDim> OSGBPoint;

}
