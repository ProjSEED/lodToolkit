#include "types.h"

namespace tg
{
	namespace io {
		class PointsReader;
		typedef tg::PointXYZINormalClassT<IntentType, IntenDim> OSGBPoint;
		class PointVisitor {
		public:
			PointVisitor();
			PointVisitor(bool flag) {}

			virtual ~PointVisitor() {};

			virtual int Reset(bool i_bToInnerCoord = false) { return 1; }

			bool ReadFile(const std::string& i_filePath);

			int NextPoint(OSGBPoint& i_oPoint);	// >= 1 normal, 0 end, -1 error

			Point3F GetOffset() { return m_offset; }

			std::string GetSRSName() { return m_srsName; }

			size_t GetNumOfPoints()
			{
				return this->m_nNumOfPoints;
			}

			int ApplyGeoTransform(OSGBPoint& i_lstPoints) {}

		private:
			bool LoadLas(const std::string& i_filePath);
			bool LoadLaz(const std::string& i_filePath);
			bool LoadPly(const std::string& i_filePath);

		private:
			enum FileFormat {
				INVALID,
				LAS_FILE,
				LAZ_FILE,
				PLY_FILE
			};

			FileFormat m_fileFormat;
			std::shared_ptr<PointsReader> m_pointsReader;

		protected:
			size_t m_nNumOfPoints;
			size_t m_nCurIndexOfPoint;
			bool m_bToInnerCoord;
			std::vector<OSGBPoint> m_lstPoints;
			Point3F m_offset;
			std::string m_srsName;
		};
	}
}
