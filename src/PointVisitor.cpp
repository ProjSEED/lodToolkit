#include "PointVisitor.h"
#include <fstream>
#include "liblas/liblas.hpp"
#include "Ply.h"
#include <boost/filesystem.hpp>
#ifdef _WIN32
	#pragma comment(lib, "../thirdparty/liblas/lib/liblas.lib")
	#ifdef _DEBUG
	#pragma comment(lib, "../thirdparty/PlyIO/lib/PlyIOd.lib")
	#else
	#pragma comment(lib, "../thirdparty/PlyIO/lib/PlyIO.lib")
	#endif
#endif


namespace tg
{
	namespace io {
		PointVisitor::PointVisitor() :
			m_fileFormat(INVALID),
			m_nCurIndexOfPoint(0)
		{

		}

		bool PointVisitor::ReadFile(const std::string& i_filePath)
		{
			std::string ext = boost::filesystem::extension(i_filePath);
			std::cout << "file format: ";
			if (ext == ".ply")
			{
				std::cout << "ply" << std::endl;
				if (!LoadPly(i_filePath))
					return false;
			}
			else if (ext == ".las")
			{
				std::cout << "las" << std::endl;
				if (!LoadLas(i_filePath))
					return false;
			}
			else
			{
				std::cout << "invalid" << std::endl;
				return false;
			}
				
			return true;
		}

		bool PointVisitor::LoadLas(const std::string& i_filePath)
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

		bool PointVisitor::LoadPly(const std::string& i_filePath)
		{
			m_lstPoints.clear();
			size_t l_nNumOfPts;
			int fileType;
			float version;
			int _nr_elems;
			char **_elist;
			PlyProperty** plist;
			PlyFile *ply = ply_open_for_reading((char*)i_filePath.data(), &_nr_elems, &_elist, &fileType, &version);
			if (!ply)
			{
				tg::log::DumpLog(tg::log::Critical, "Open file %s failed!", i_filePath);
				return false;
			}
			
			bool foundVertices = true;
			bool foundNormals = true;
			bool foundColors = true;
			bool foundClasses = true;
			for (int i = 0; i < _nr_elems; i++)
			{
				int num_elems;
				int nr_props;
				char* elem_name = _elist[i];
				plist = ply_get_element_description(ply, elem_name, &num_elems, &nr_props);
				if (!plist)
				{
					for (int i = 0; i < _nr_elems; i++) {
						free(ply->elems[i]->name);
						free(ply->elems[i]->store_prop);
						for (int j = 0; j < ply->elems[i]->nprops; j++) {
							free(ply->elems[i]->props[j]->name);
							free(ply->elems[i]->props[j]);
						}
						free(ply->elems[i]->props);
					}
					for (int i = 0; i < _nr_elems; i++) { free(ply->elems[i]); }
					free(ply->elems);
					for (int i = 0; i < ply->num_comments; i++) { free(ply->comments[i]); }
					free(ply->comments);
					for (int i = 0; i < ply->num_obj_info; i++) { free(ply->obj_info[i]); }
					free(ply->obj_info);
					ply_free_other_elements(ply->other_elems);

					for (int i = 0; i < _nr_elems; i++) { free(_elist[i]); }
					free(_elist);
					ply_close(ply);
					tg::log::DumpLog(tg::log::Critical, "Failed to get element description: %s", elem_name);
					return false;
				}

				if (equal_strings("vertex", elem_name))
				{
					l_nNumOfPts = num_elems;
					for (int j = 0; j < 3; j++)//至少得有点
						if (!ply_get_property(ply, elem_name, &(PlyValueOrientedColorVertex< float >::ReadProperties[j]))) // 读取 xyz
						{
							tg::log::DumpLog(tg::log::Critical, "Read Vertex failed!");
							return false;
						}
					//
					if (!ply_get_property(ply, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[3]))) // 读取 Class
					{
						tg::log::DumpLog(tg::log::Critical, "Read class failed!");
						foundClasses = false;
					}

					//法线 可选
					for(int j = 4; j < 7; j++)
						if (!ply_get_property(ply, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[j]))) // 读取 Normal
						{
							tg::log::DumpLog(tg::log::Critical, "Read normal failed!");
							foundNormals = false;
							break;
						}

					//颜色 可选
					for (int j = 7; j < 10; j++)
						if (!ply_get_property(ply, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[j])))
						{
							//tg::log::DumpLog(tg::log::Critical, "Read file %s failed(no color)!", i_filePath);
							foundColors = false;
						}
					if (!foundColors)
						for (int j = 10; j < 13; j++)
							if (!ply_get_property(ply, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[j])))
							{
								tg::log::DumpLog(tg::log::Critical, "Read color failed!");
								break;
							}
				}
				for (int j = 0; j < nr_props; j++)
				{
					free(plist[j]->name);
					free(plist[j]);
				}
				free(plist);
				if (foundVertices) break;
			}
			if (!foundVertices)
			{
				tg::log::DumpLog(tg::log::Critical, "No vertice in file %s!", i_filePath);
				return false;
			}
			m_lstPoints.resize(l_nNumOfPts);
			for (int i = 0; i < m_lstPoints.size(); ++i)
			{
				auto& l_oPoint = m_lstPoints[i];
				PlyValueOrientedColorVertex<float> l_oVertex;
				ply_get_element(ply, (void *)&l_oVertex);
				for (int k = 0; k < 3; ++k)
				{
					l_oPoint.P[k] = l_oVertex.point[k];
					if(foundNormals)
						l_oPoint.Normal[k] = l_oVertex.normal[k];
					if(foundColors)
						l_oPoint.I[k] = l_oVertex.color[k];
				}
				if(foundClasses)
					l_oPoint.Class = l_oVertex.value;
			}
			for (int i = 0; i < _nr_elems; i++) {
				free(ply->elems[i]->name);
				free(ply->elems[i]->store_prop);
				for (int j = 0; j < ply->elems[i]->nprops; j++) {
					free(ply->elems[i]->props[j]->name);
					free(ply->elems[i]->props[j]);
				}
				free(ply->elems[i]->props);
			}
			for (int i = 0; i < _nr_elems; i++) { free(ply->elems[i]); }
			free(ply->elems);
			for (int i = 0; i < ply->num_comments; i++) { free(ply->comments[i]); }
			free(ply->comments);
			for (int i = 0; i < ply->num_obj_info; i++) { free(ply->obj_info[i]); }
			free(ply->obj_info);
			ply_free_other_elements(ply->other_elems);

			for (int i = 0; i < _nr_elems; i++) { free(_elist[i]); }
			free(_elist);
			ply_close(ply);

			m_nNumOfPoints = l_nNumOfPts;
			m_nCurIndexOfPoint = 0;

			return 1;
		}

		int PointVisitor::NextPoint(tg::PointXYZINormalClassT<IntentType, IntenDim>& i_oPoint)
		{
			if (m_nCurIndexOfPoint >= m_nNumOfPoints) return -1;

			i_oPoint = m_lstPoints[m_nCurIndexOfPoint++];

			return 1;
		}
	}
}
