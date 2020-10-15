#include "pointCI.h"
#include <osg/BoundingBox>

namespace seed
{
	namespace io {
		class PointsReader;
		class PointVisitor {
		public:
			PointVisitor();

			virtual ~PointVisitor() {};

			bool PerpareFile(const std::string& input, bool runStatistic);

			int NextPoint(PointCI& point);	// >= 1 normal, 0 end, -1 error

			osg::Vec3d GetOffset();
			osg::BoundingBox GetBBox() { return _bbox; }
			osg::BoundingBox GetBBoxZHistogram() { return _bboxZHistogram; }
			
			std::string GetSRSName();
			size_t GetNumOfPoints();

		private:
			bool ResetFile(const std::string& input);

			std::shared_ptr<PointsReader> _pointsReader;
			osg::BoundingBox _bbox;
			osg::BoundingBox _bboxZHistogram;
		};
	}
}
