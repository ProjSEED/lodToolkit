#include "PointVisitor.h"
#include <fstream>
#include "Ply.h"
#include "laszip_api.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace seed
{
	namespace io {

		//////////////////////////// Points Reader ///////////////////////
		class PointsReader
		{
		public:
			PointsReader(const std::string& filename);
			~PointsReader() {}
			virtual bool Init() = 0;
			virtual bool ReadNextPoint(OSGBPoint& point) = 0;
			size_t GetPointsCount() { return m_pointCount; }
			size_t GetCurrentPointId() { return m_currentPointId; }
			Point3F GetOffset() { return m_offset; } // set first point as offset
			std::string GetSRS() { return m_srsName; }

		protected:
			std::string m_filename;
			size_t m_pointCount;
			size_t m_currentPointId;
			Point3F m_offset;
			std::string m_srsName;
		};

		PointsReader::PointsReader(const std::string& filename):
			m_filename(filename),
			m_currentPointId(0),
			m_pointCount(0),
			m_offset(0, 0, 0),
			m_srsName("")
		{

		}

		////////////////////////// Laz/Laz Reader(laszip lib can read both las or laz) /////////////////////////////////
		class LazReader:public PointsReader
		{
		public:
			LazReader(const std::string& filename);
			bool Init() override;
			~LazReader();
			bool ReadNextPoint(OSGBPoint& point) override;

		private:
			laszip_point* m_pointRead;//current reading point
			laszip_POINTER m_laszipReader;
			laszip_header* m_laszipHeader;
		};

		LazReader::LazReader(const std::string& filename):
			PointsReader(filename)
		{
			
		}

		LazReader::~LazReader()
		{
			if (!m_laszipReader) return;
			// close the reader
			if (laszip_close_reader(m_laszipReader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in closing laszip reader!");
			}

			// destroy the reader
			if (laszip_destroy(m_laszipReader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in destroying laszip reader!");
			}
		}

		bool LazReader::Init()
		{
			double start_time = 0.0;
			const char* file_name_in = m_filename.c_str();
			// get version of LASzip DLL
			laszip_U8 version_major;
			laszip_U8 version_minor;
			laszip_U16 version_revision;
			laszip_U32 version_build;

			if (laszip_get_version(&version_major, &version_minor, &version_revision, &version_build))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in getting LASzip version number!");
				return false;
			}
			
			// create the reader
			if (laszip_create(&m_laszipReader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in creating laszip reader!");
				return false;
			}

			// open the reader
			laszip_BOOL is_compressed = 0;
			if (laszip_open_reader(m_laszipReader, file_name_in, &is_compressed))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in opening laszip reader for '%s'", file_name_in);
				return false;
			}

			seed::log::DumpLog(seed::log::Debug, "file '%s' is %scompressed\n", file_name_in, (is_compressed ? "" : "un"));
			// get a pointer to the header of the reader that was just populated
			if (laszip_get_header_pointer(m_laszipReader, &m_laszipHeader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in getting header pointer from laszip reader!");
				return false;
			}

			// how many points does the file have
			m_pointCount = (m_laszipHeader->number_of_point_records ? 
				m_laszipHeader->number_of_point_records : m_laszipHeader->extended_number_of_point_records);

			int lengthRecords = m_laszipHeader->number_of_variable_length_records;
			auto& vlrs = m_laszipHeader->vlrs; // Variable Length Records
			for (int i = 0; i < lengthRecords; ++i)
			{
				if (vlrs[i].data == NULL) continue;
				if (vlrs[i].record_id == 2111 || vlrs[i].record_id == 2112)
					m_srsName = (char*)vlrs[i].data;// wkt
			}
			
			// report how many points the file has
			seed::log::DumpLog(seed::log::Debug, "file '%s' contains %I64d points", file_name_in, m_pointCount);

			// get a pointer to the points that will be read
			if (laszip_get_point_pointer(m_laszipReader, &m_pointRead))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in getting point pointer from laszip reader!");
				return false;
			}

			return true;
		}

		bool LazReader::ReadNextPoint(OSGBPoint& pt)
		{
			if (m_currentPointId < m_pointCount)
			{
				// read a point
				if (laszip_read_point(m_laszipReader))
				{
					seed::log::DumpLog(seed::log::Critical, "An error occured in reading point %I64d", m_currentPointId);
					return false;
				}

				// init offset
				if (m_currentPointId == 0)
				{
					m_offset[0] = m_laszipHeader->x_offset;
					m_offset[1] = m_laszipHeader->y_offset;
					m_offset[2] = m_laszipHeader->z_offset;
				}

				// add offset and scale to coords
				pt.P[0] = m_pointRead->X * m_laszipHeader->x_scale_factor;
				pt.P[1] = m_pointRead->Y * m_laszipHeader->y_scale_factor;
				pt.P[2] = m_pointRead->Z * m_laszipHeader->z_scale_factor;

				auto& rgb = m_pointRead->rgb;
				pt.I[0] = utils::Color8Bits(rgb[0]);
				pt.I[1] = utils::Color8Bits(rgb[1]);
				pt.I[2] = utils::Color8Bits(rgb[2]);

				m_currentPointId++;
				return true;
			}

			return false;
		}

		////////////////////////// PLY Reader /////////////////////////////////
		class PlyReader :public PointsReader
		{
		public:
			PlyReader(const std::string& filename);
			bool Init() override;
			~PlyReader();
			bool ReadNextPoint(OSGBPoint& point) override;

		private:
			PlyFile *m_plyFile;
			int m_nrElems;
			char **m_elist;
			bool m_foundColors;
			bool m_foundVertices;
		};

		PlyReader::PlyReader(const std::string& filename) :
			PointsReader(filename),
			m_foundVertices(true),
			m_foundColors(true)
		{}

		PlyReader::~PlyReader()
		{
			if (!m_plyFile) return;
			for (int i = 0; i < m_nrElems; i++) {
				free(m_plyFile->elems[i]->name);
				free(m_plyFile->elems[i]->store_prop);
				for (int j = 0; j < m_plyFile->elems[i]->nprops; j++) {
					free(m_plyFile->elems[i]->props[j]->name);
					free(m_plyFile->elems[i]->props[j]);
				}
				free(m_plyFile->elems[i]->props);
			}
			for (int i = 0; i < m_nrElems; i++) { free(m_plyFile->elems[i]); }
			free(m_plyFile->elems);
			for (int i = 0; i < m_plyFile->num_comments; i++) { free(m_plyFile->comments[i]); }
			free(m_plyFile->comments);
			for (int i = 0; i < m_plyFile->num_obj_info; i++) { free(m_plyFile->obj_info[i]); }
			free(m_plyFile->obj_info);
			ply_free_other_elements(m_plyFile->other_elems);

			for (int i = 0; i < m_nrElems; i++) { free(m_elist[i]); }
			free(m_elist);
			ply_close(m_plyFile);
		}

		bool PlyReader::Init()
		{
			int fileType;
			float version;
			PlyProperty** plist;
			m_plyFile = ply_open_for_reading((char*)m_filename.data(), &m_nrElems, &m_elist, &fileType, &version);
			if (!m_plyFile)
			{
				seed::log::DumpLog(seed::log::Critical, "Open file %s failed!", m_filename.c_str());
				return false;
			}

			for (int i = 0; i < m_nrElems; i++)
			{
				int num_elems;
				int nr_props;
				char* elem_name = m_elist[i];
				plist = ply_get_element_description(m_plyFile, elem_name, &num_elems, &nr_props);
				if (!plist)
				{
					for (int i = 0; i < m_nrElems; i++) {
						free(m_plyFile->elems[i]->name);
						free(m_plyFile->elems[i]->store_prop);
						for (int j = 0; j < m_plyFile->elems[i]->nprops; j++) {
							free(m_plyFile->elems[i]->props[j]->name);
							free(m_plyFile->elems[i]->props[j]);
						}
						free(m_plyFile->elems[i]->props);
					}
					for (int i = 0; i < m_nrElems; i++) { free(m_plyFile->elems[i]); }
					free(m_plyFile->elems);
					for (int i = 0; i < m_plyFile->num_comments; i++) { free(m_plyFile->comments[i]); }
					free(m_plyFile->comments);
					for (int i = 0; i < m_plyFile->num_obj_info; i++) { free(m_plyFile->obj_info[i]); }
					free(m_plyFile->obj_info);
					ply_free_other_elements(m_plyFile->other_elems);

					for (int i = 0; i < m_nrElems; i++) { free(m_elist[i]); }
					free(m_elist);
					ply_close(m_plyFile);
					seed::log::DumpLog(seed::log::Critical, "Failed to get element description: %s", elem_name);
					return false;
				}

				if (equal_strings("vertex", elem_name)) // 仅读取点信息
				{
					m_pointCount = num_elems;
					for (int j = 0; j < 3; j++)//至少得有点
						if (!ply_get_property(m_plyFile, elem_name, &(PlyColorVertex< float >::ReadProperties[j]))) // 读取 xyz
						{
							seed::log::DumpLog(seed::log::Critical, "Read Vertex failed!");
							return false;
						}

					//颜色 可选
					for (int j = 3; j < 6; j++)
						if (!ply_get_property(m_plyFile, elem_name, &(PlyColorVertex<float>::ReadProperties[j])))
						{
							//seed::log::DumpLog(seed::log::Critical, "Read file %s failed(no color)!", i_filePath);
							m_foundColors = false;
						}
					if (!m_foundColors)
						for (int j = 6; j < 9; j++)
							if (!ply_get_property(m_plyFile, elem_name, &(PlyColorVertex<float>::ReadProperties[j])))
							{
								seed::log::DumpLog(seed::log::Critical, "Read color failed!");
								break;
							}
				}
				for (int j = 0; j < nr_props; j++)
				{
					free(plist[j]->name);
					free(plist[j]);
				}
				free(plist);
				if (m_foundVertices) break;
			}
			if (!m_foundVertices)
			{
				seed::log::DumpLog(seed::log::Critical, "No vertice in file %s!", m_filename.c_str());
				return false;
			}

			return true;
		}

		bool PlyReader::ReadNextPoint(OSGBPoint& point)
		{
			if (m_currentPointId >= m_pointCount) return false;
			PlyValueOrientedColorVertex<float> l_oVertex;
			ply_get_element(m_plyFile, (void *)&l_oVertex);

			// init offset
			if (m_currentPointId == 0)
			{
				m_offset[0] = l_oVertex.point[0];
				m_offset[1] = l_oVertex.point[1];
				m_offset[2] = l_oVertex.point[2];
			}

			for (int k = 0; k < 3; ++k)
			{
				point.P[k] = l_oVertex.point[k] - m_offset[k];
				if (m_foundColors)
					point.I[k] = utils::Color8Bits(l_oVertex.color[k]);
			}

			m_currentPointId++;
			return true;
		}

		////////////////////////// XYZRGB Reader /////////////////////////////////
		class XYZRGBReader :public PointsReader
		{
		public:
			XYZRGBReader(const std::string& filename);
			bool Init() override;
			~XYZRGBReader();
			bool ReadNextPoint(OSGBPoint& point) override;

		private:
			std::ifstream m_xyzFile;
		};

		XYZRGBReader::XYZRGBReader(const std::string& filename) :
			PointsReader(filename)
		{

		}

		XYZRGBReader::~XYZRGBReader()
		{
			m_xyzFile.close();
		}

		bool XYZRGBReader::Init()
		{
			m_xyzFile.open(m_filename);
			if (m_xyzFile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Open file %s failed!", m_filename.c_str());
				return false;
			}
			std::string line;
			while (std::getline(m_xyzFile, line).good())
			{
				++m_pointCount;
			}
			m_xyzFile.clear();
			m_xyzFile.seekg(0, m_xyzFile.beg);
			if (!m_pointCount)
			{
				seed::log::DumpLog(seed::log::Critical, "No vertice in file %s!", m_filename.c_str());
				return false;
			}
			return true;
		}

		bool XYZRGBReader::ReadNextPoint(OSGBPoint& point)
		{
			if (m_currentPointId >= m_pointCount) return false;
			std::string line;
			if (!std::getline(m_xyzFile, line).good())
			{
				return false;
			}
			int color;
			std::stringstream ss;
			ss << line;
			ss >> point.P[0];
			ss >> point.P[1];
			ss >> point.P[2];
			ss >> color;
			point.I[0] = utils::Color8Bits(color);
			ss >> color;
			point.I[1] = utils::Color8Bits(color);
			ss >> color;
			point.I[2] = utils::Color8Bits(color);

			// init offset
			if (m_currentPointId == 0)
			{
				m_offset[0] = point.P[0];
				m_offset[1] = point.P[1];
				m_offset[2] = point.P[2];
			}

			for (int k = 0; k < 3; ++k)
			{
				point.P[k] = point.P[k] - m_offset[k];
			}

			m_currentPointId++;
			return true;
		}

		////////////////////////// Point Visitor /////////////////////////////////
		PointVisitor::PointVisitor() :
			m_pointsReader(NULL)
		{
		}

		bool PointVisitor::ReadFile(const std::string& i_filePath)
		{
			std::string ext = utils::getExtension(i_filePath);
			if (ext == ".ply")
				m_pointsReader.reset(new PlyReader(i_filePath));
			else if (ext == ".laz" || ext == ".las")
				m_pointsReader.reset(new LazReader(i_filePath));
			else if(ext == ".xyz")
				m_pointsReader.reset(new XYZRGBReader(i_filePath));
			else
			{
				seed::log::DumpLog(seed::log::Critical, "%s is NOT supported now.", ext.c_str());
				return false;
			}
			seed::log::DumpLog(seed::log::Info, "Input file format: %s", ext.c_str());

			if (!m_pointsReader->Init())
				return false;

			return true;
		}

		Point3F PointVisitor::GetOffset()
		{
			return m_pointsReader->GetOffset();
		}

		size_t PointVisitor::GetNumOfPoints()
		{
			return m_pointsReader->GetPointsCount();
		}

		std::string PointVisitor::GetSRSName()
		{
			return m_pointsReader->GetSRS();
		}

		int PointVisitor::NextPoint(OSGBPoint& i_oPoint)
		{
			if (!m_pointsReader->ReadNextPoint(i_oPoint)) return -1;

			return 1;
		}
	}
}
