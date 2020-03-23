#include "PointVisitor.h"
#include <fstream>
#include "liblas/liblas.hpp"
#include "Ply.h"
#include <boost/filesystem.hpp>
#include "laszip_api.h"

#ifdef _WIN32
	#pragma comment(lib, "../thirdparty/liblas/lib/liblas.lib")
	#ifdef _DEBUG
	#pragma comment(lib, "../thirdparty/PlyIO/lib/PlyIOd.lib")
	#pragma comment(lib, "../thirdparty/LASzip/lib/LASzipd.lib")
	#else
	#pragma comment(lib, "../thirdparty/PlyIO/lib/PlyIO.lib")
	#pragma comment(lib, "../thirdparty/LASzip/lib/LASzip.lib")
	#endif
#endif


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
			Point3F GetOffset() { return m_offset; }
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
		////////////////////////// Laz Reader /////////////////////////////////
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

				//laszip_dll_struct* pointer = (laszip_dll_struct*)laszip_reader;
				// add offset and scale to coords
				pt.P[0] = m_pointRead->X * m_laszipHeader->x_scale_factor + m_laszipHeader->x_offset;
				pt.P[1] = m_pointRead->Y * m_laszipHeader->y_scale_factor + m_laszipHeader->y_offset;
				pt.P[2] = m_pointRead->Z * m_laszipHeader->z_scale_factor + m_laszipHeader->z_offset;

				auto GetColor = [](laszip_U16& color16Bit) -> unsigned char
				{
					return (color16Bit < 256) ? color16Bit : (color16Bit / 65535.0)*255.0;
				};

				auto& rgb = m_pointRead->rgb;
				pt.I[0] = GetColor(rgb[0]);
				pt.I[1] = GetColor(rgb[1]);
				pt.I[2] = GetColor(rgb[2]);

				m_currentPointId++;
				return true;
			}

			return false;
		}

		////////////////////////// Las Reader /////////////////////////////////
		class LasReader :public PointsReader
		{
		public:
			LasReader(const std::string& filename);
			bool Init() override;
			~LasReader();
			bool ReadNextPoint(OSGBPoint& point) override;

		private:
			std::unique_ptr<liblas::Reader> m_lasReader;
			std::ifstream m_lasFileStream;
		};

		LasReader::LasReader(const std::string& filename):
			PointsReader(filename)
		{}

		LasReader::~LasReader()
		{}

		bool LasReader::Init()
		{
			if (!liblas::Open(m_lasFileStream, m_filename.c_str()))
			{
				//throw std::runtime_error(std::string("Can not open") + cloud_las_path_);
				std::cout << "Can not open " << m_filename << "\n";
				return false;
			}

			m_lasReader.reset(new liblas::Reader(m_lasFileStream));
			m_pointCount = m_lasReader->GetHeader().GetPointRecordsCount();
			return true;
		}

		bool LasReader::ReadNextPoint(OSGBPoint& pt)
		{
			if (m_currentPointId >= m_pointCount) return false;

			m_lasReader->ReadNextPoint();
			liblas::Point const&p = m_lasReader->GetPoint();
			liblas::Color color = p.GetColor();
			// XYZ
			pt.P[0] = p.GetX();
			pt.P[1] = p.GetY();
			pt.P[2] = p.GetZ();
			// RGB
			pt.I[0] = color.GetRed();
			pt.I[1] = color.GetGreen();
			pt.I[2] = color.GetBlue();

			m_currentPointId++;
			return true;
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
			for (int k = 0; k < 3; ++k)
			{
				point.P[k] = l_oVertex.point[k];
				if (m_foundNormals)
					point.Normal[k] = l_oVertex.normal[k];
				if (m_foundColors)
					point.I[k] = l_oVertex.color[k];
			}
			if (m_foundClasses)
				point.Class = l_oVertex.value;

			m_currentPointId++;
			return true;
		}


		////////////////////////// Point Visitor /////////////////////////////////
		PointVisitor::PointVisitor() :
			m_pointsReader(NULL),
			m_nNumOfPoints(0),
			m_offset(0, 0, 0),
			m_srsName("")
		{
		}

		bool PointVisitor::ReadFile(const std::string& i_filePath)
		{
			std::string ext = boost::filesystem::extension(i_filePath);
			std::cout << "file format: ";
			if (ext == ".ply")
			{
				std::cout << "ply" << std::endl;
				m_pointsReader.reset(new PlyReader(i_filePath));
			}
			else if (ext == ".las")
			{
				std::cout << "las" << std::endl;
				m_pointsReader.reset(new LasReader(i_filePath));
			}
			else if (ext == ".laz")
			{
				std::cout << "laz" << std::endl;
				m_pointsReader.reset(new LazReader(i_filePath));
			}
			else
			{
				std::cout << "invalid" << std::endl;
				return false;
			}
			
			if (!m_pointsReader->Init())
				return false;

			m_nNumOfPoints = m_pointsReader->GetPointsCount();
			m_offset = m_pointsReader->GetOffset();
			m_srsName = m_pointsReader->GetSRS();

			return true;
		}

		int PointVisitor::NextPoint(OSGBPoint& i_oPoint)
		{
			if(!m_pointsReader->ReadNextPoint(i_oPoint)) return -1;

			return 1;
		}
	}
}
