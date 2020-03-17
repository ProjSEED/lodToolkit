#include "PointVisitor.h"
#include <fstream>
#include "liblas/liblas.hpp"

#pragma comment(lib, "../thirdparty/liblas/lib/liblas.lib")

namespace tg
{
	namespace io {
		PointVisitor::PointVisitor() :
			m_fileFormat(INVALID)
		{

		}

		bool PointVisitor::ReadLasFile(const std::string& i_filePath)
		{
			m_lstPoints.clear();
			std::ifstream if_lasfile;
			if (!liblas::Open(if_lasfile, i_filePath.c_str()))
			{
				//throw std::runtime_error(std::string("Can not open") + cloud_las_path_);
				std::cout << "Can not open " << i_filePath << "\n";
				return false;
			}

			liblas::Reader lasReader(if_lasfile);
			liblas::Header const& header = lasReader.GetHeader();

			m_nNumOfPoints = header.GetPointRecordsCount();
			std::cout << "Point Nums: " << m_nNumOfPoints << std::endl;
			// record global offset
			m_offset[0] = header.GetOffsetX();
			m_offset[1] = header.GetOffsetY();
			m_offset[2] = header.GetOffsetZ();
			std::cout << "Offset: " << m_offset[0] << " " << m_offset[1] << " " << m_offset[2] << std::endl;
			// record SRC
			auto SRC = header.GetSRS();
			m_srsName = SRC.GetWKT();
			std::cout << "SRS: " << m_srsName << std::endl;
			// record points count
			m_lstPoints.resize(m_nNumOfPoints);
			m_nCurIndexOfPoint = 0;

			int count = 0;
			while (lasReader.ReadNextPoint())
			{
				lasReader.ReadNextPoint();
				liblas::Point const&p = lasReader.GetPoint();
				liblas::Color color = p.GetColor();

				auto& pt = m_lstPoints[count++];
				pt.P[0] = p.GetX();
				pt.P[1] = p.GetY();
				pt.P[2] = p.GetZ();

				//RGB
				pt.I[0] = color.GetRed();
				pt.I[1] = color.GetGreen();
				pt.I[2] = color.GetBlue();

			}

			m_fileFormat = LAZ_FILE;

			return true;
		}

		int PointVisitor::NextPoint(tg::PointXYZINormalClassT<IntentType, IntenDim>& i_oPoint)
		{
			if (m_nCurIndexOfPoint >= m_nNumOfPoints) return -1;

			switch (m_fileFormat)
			{
			case LAZ_FILE:
			{
				i_oPoint = m_lstPoints[m_nCurIndexOfPoint++];
				break;
			}
			default:
				return -1;
			}

			return 1;
		}
	}
}
