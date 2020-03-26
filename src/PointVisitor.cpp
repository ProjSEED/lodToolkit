#include "PointVisitor.h"
#include <fstream>
#include "Ply.h"
//#include <boost/filesystem.hpp>
#include "laszip_api.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
	#ifdef _DEBUG
	#pragma comment(lib, "../thirdparty/PlyIO/lib/PlyIOd.lib")
	#pragma comment(lib, "../thirdparty/LASzip/lib/LASzipd.lib")
	#else
	#pragma comment(lib, "../thirdparty/PlyIO/lib/PlyIO.lib")
	#pragma comment(lib, "../thirdparty/LASzip/lib/LASzip.lib")
	#endif
#endif

///////////////////////////// Helper API /////////////////////////////////
unsigned char Color8Bits(uint16_t color16Bit)
{
	return (color16Bit < 256) ? color16Bit : (color16Bit / 65535.0)*255.0;
};

struct MatchPathSeparator
{
	bool operator()(char ch) const
	{
		return ch == '/' || ch == '\\';
	}
};

std::string getFileName(const std::string & full_path_in)
{
	return std::string(
		std::find_if(full_path_in.rbegin(), full_path_in.rend(),
			MatchPathSeparator()).base(),
		full_path_in.end());
}

// get extension .xxx
std::string getExtension(const std::string & full_path_in)
{
	std::string filename = getFileName(full_path_in);
	std::size_t found = filename.find_last_of('.');
	if (found < filename.size())
		return filename.substr(found);
	else
		return std::string();
}

//////////////////////////// Points Reader ///////////////////////
namespace tg
{
	namespace io {

		class PointsReader
		{
		public:
			PointsReader(const std::string& filename);
			~PointsReader() {}
			virtual bool Init() = 0;
			virtual bool ReadNextPoint(OSGBPoint& point) = 0;
			int GetPointsCount() { return m_pointCount; }
			int GetCurrentPointId() { return m_currentPointId; }
			Point3F GetOffset() { return m_offset; } // 将读取到的第一个点设为整体offset
			std::string GetSRS() { return m_srsName; }

		protected:
			std::string m_filename;
			int m_pointCount;
			int m_currentPointId;
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
				fprintf(stderr, "DLL ERROR: closing laszip reader\n");
			}

			// destroy the reader
			if (laszip_destroy(m_laszipReader))
			{
				fprintf(stderr, "DLL ERROR: destroying laszip reader\n");
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
				fprintf(stderr, "DLL ERROR: getting LASzip DLL version number\n");
				return false;
			}
			
			// create the reader
			if (laszip_create(&m_laszipReader))
			{
				fprintf(stderr, "DLL ERROR: creating laszip reader\n");
				return false;
			}

			// open the reader
			laszip_BOOL is_compressed = 0;
			if (laszip_open_reader(m_laszipReader, file_name_in, &is_compressed))
			{
				fprintf(stderr, "DLL ERROR: opening laszip reader for '%s'\n", file_name_in);
				return false;
			}

			fprintf(stderr, "file '%s' is %scompressed\n", file_name_in, (is_compressed ? "" : "un"));
			// get a pointer to the header of the reader that was just populated
			if (laszip_get_header_pointer(m_laszipReader, &m_laszipHeader))
			{
				fprintf(stderr, "DLL ERROR: getting header pointer from laszip reader\n");
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
			fprintf(stderr, "file '%s' contains %I64d points\n", file_name_in, m_pointCount);

			// get a pointer to the points that will be read
			if (laszip_get_point_pointer(m_laszipReader, &m_pointRead))
			{
				fprintf(stderr, "DLL ERROR: getting point pointer from laszip reader\n");
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
					fprintf(stderr, "DLL ERROR: reading point %I64d\n", m_currentPointId);
					return false;
				}

				// init offset
				if (m_currentPointId == 0)
				{
					m_offset[0] = m_laszipHeader->x_offset;
					m_offset[1] = m_laszipHeader->y_offset;
					m_offset[2] = m_laszipHeader->z_offset;
				}

				//laszip_dll_struct* pointer = (laszip_dll_struct*)laszip_reader;
				// add offset and scale to coords
				pt.P[0] = m_pointRead->X * m_laszipHeader->x_scale_factor;
				pt.P[1] = m_pointRead->Y * m_laszipHeader->y_scale_factor;
				pt.P[2] = m_pointRead->Z * m_laszipHeader->z_scale_factor;

				auto& rgb = m_pointRead->rgb;
				pt.I[0] = Color8Bits(rgb[0]);
				pt.I[1] = Color8Bits(rgb[1]);
				pt.I[2] = Color8Bits(rgb[2]);

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
			bool m_foundNormals;
			bool m_foundColors;
			bool m_foundClasses;
			bool m_foundVertices;
		};

		PlyReader::PlyReader(const std::string& filename) :
			PointsReader(filename),
			m_foundVertices(true),
			m_foundNormals(true),
			m_foundColors(true),
			m_foundClasses(true)
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
			size_t l_nNumOfPts;
			int fileType;
			float version;
			PlyProperty** plist;
			m_plyFile = ply_open_for_reading((char*)m_filename.data(), &m_nrElems, &m_elist, &fileType, &version);
			if (!m_plyFile)
			{
				tg::log::DumpLog(tg::log::Critical, "Open file %s failed!", m_filename);
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
					tg::log::DumpLog(tg::log::Critical, "Failed to get element description: %s", elem_name);
					return false;
				}

				if (equal_strings("vertex", elem_name)) // 仅读取点信息
				{
					m_pointCount = num_elems;
					for (int j = 0; j < 3; j++)//至少得有点
						if (!ply_get_property(m_plyFile, elem_name, &(PlyValueOrientedColorVertex< float >::ReadProperties[j]))) // 读取 xyz
						{
							tg::log::DumpLog(tg::log::Critical, "Read Vertex failed!");
							return false;
						}
					//
					if (!ply_get_property(m_plyFile, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[3]))) // 读取 Class
					{
						tg::log::DumpLog(tg::log::Critical, "Read class failed!");
						m_foundClasses = false;
					}

					//法线 可选
					for (int j = 4; j < 7; j++)
						if (!ply_get_property(m_plyFile, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[j]))) // 读取 Normal
						{
							tg::log::DumpLog(tg::log::Critical, "Read normal failed!");
							m_foundNormals = false;
							break;
						}

					//颜色 可选
					for (int j = 7; j < 10; j++)
						if (!ply_get_property(m_plyFile, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[j])))
						{
							//tg::log::DumpLog(tg::log::Critical, "Read file %s failed(no color)!", i_filePath);
							m_foundColors = false;
						}
					if (!m_foundColors)
						for (int j = 10; j < 13; j++)
							if (!ply_get_property(m_plyFile, elem_name, &(PlyValueOrientedColorVertex<float>::ReadProperties[j])))
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
				if (m_foundVertices) break;
			}
			if (!m_foundVertices)
			{
				tg::log::DumpLog(tg::log::Critical, "No vertice in file %s!", m_filename);
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
				if (m_foundNormals)
					point.Normal[k] = l_oVertex.normal[k];
				if (m_foundColors)
					point.I[k] = Color8Bits(l_oVertex.color[k]);
			}
			if (m_foundClasses)
				point.Class = l_oVertex.value;

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
			std::string ext = getExtension(i_filePath);
			std::cout << "file format: ";
			if (ext == ".ply")
				m_pointsReader.reset(new PlyReader(i_filePath));
			else if (ext == ".laz" || ext == ".las")
				m_pointsReader.reset(new LazReader(i_filePath));
			else
			{
				std::cout << "invalid" << std::endl;
				return false;
			}
			
			std::cout << ext << std::endl;

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
