#include "types.h"

namespace tg
{
	namespace io {
		class PointsReader;
		typedef tg::PointXYZINormalClassT<IntentType, IntenDim> OSGBPoint;
		class PointVisitor {
		public:
			PointVisitor();

			virtual ~PointVisitor() {};

			bool ReadFile(const std::string& i_filePath);

			int NextPoint(OSGBPoint& i_oPoint);	// >= 1 normal, 0 end, -1 error

			// TODO:ÍêÉÆ
			Point3F GetOffset() { return m_offset; }

			// TODO:ÍêÉÆ
			std::string GetSRSName() { return m_srsName; }

			size_t GetNumOfPoints()
			{
				return this->m_nNumOfPoints;
			}

		private:
			std::shared_ptr<PointsReader> m_pointsReader;
			size_t m_nNumOfPoints;
			Point3F m_offset;
			std::string m_srsName;
		};
	}
}
