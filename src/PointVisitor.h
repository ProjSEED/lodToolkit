#include "Common.h"

namespace seed
{
	namespace io {
		class PointsReader;
		class PointVisitor {
		public:
			PointVisitor();

			virtual ~PointVisitor() {};

			bool ReadFile(const std::string& i_filePath);

			int NextPoint(OSGBPoint& i_oPoint);	// >= 1 normal, 0 end, -1 error

			Point3F GetOffset();
			
			std::string GetSRSName();
			size_t GetNumOfPoints();

		private:
			std::shared_ptr<PointsReader> m_pointsReader;
		};
	}
}
