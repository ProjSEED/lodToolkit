#include "Common.h"
#include <osg/BoundingBox>

namespace seed
{
	namespace io {
		class PointsReader;
		class PointVisitor {
		public:
			PointVisitor();

			virtual ~PointVisitor() {};

			bool PerpareFile(const std::string& i_filePath, bool runStatistic);

			int NextPoint(OSGBPoint& i_oPoint);	// >= 1 normal, 0 end, -1 error

			Point3F GetOffset();
			osg::BoundingBox GetBBox() { return m_bbox; }
			osg::BoundingBox GetBBoxZHistogram() { return m_bboxZHistogram; }
			
			std::string GetSRSName();
			size_t GetNumOfPoints();

		private:
			bool ResetFile(const std::string& i_filePath);

			std::shared_ptr<PointsReader> m_pointsReader;
			osg::BoundingBox m_bbox;
			osg::BoundingBox m_bboxZHistogram;
		};
	}
}
