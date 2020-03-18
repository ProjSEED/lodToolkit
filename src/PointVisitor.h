#include "types.h"

namespace liblas
{
	class Reader;
}

namespace tg
{
	namespace io {
		class PointVisitor {
		public:
			PointVisitor();
			PointVisitor(bool flag) {}

			virtual ~PointVisitor() {};

			virtual int Reset(bool i_bToInnerCoord = false) { return 1; }

			bool ReadFile(const std::string& i_filePath);

			int NextPoint(tg::PointXYZINormalClassT<IntentType, IntenDim>& i_oPoint);	// >= 1 normal, 0 end, -1 error

			Point3F GetOffset() { return m_offset; }

			std::string GetSRSName() { return m_srsName; }

			size_t GetNumOfPoints()
			{
				return this->m_nNumOfPoints;
			}

			int ApplyGeoTransform(std::vector<tg::PointXYZINormalClassT<IntentType, IntenDim>>& i_lstPoints) {}

		private:
			bool LoadLas(const std::string& i_filePath);
			bool LoadPly(const std::string& i_filePath);

		private:
			enum FileFormat {
				INVALID,
				LAS_FILE,
				LAZ_FILE
			};

			FileFormat m_fileFormat;
			//std::shared_ptr<liblas::Reader> m_lasReader;

		protected:
			size_t m_nNumOfPoints;
			size_t m_nCurIndexOfPoint;
			bool m_bToInnerCoord;
			std::vector<tg::PointXYZINormalClassT<IntentType, IntenDim>> m_lstPoints;
			Point3F m_offset;
			std::string m_srsName;
		};
	}
}
