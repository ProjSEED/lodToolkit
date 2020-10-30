#include "pointVisitor.h"
#include <fstream>
#include "Ply.h"
#include "laszip_api.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

namespace seed
{
	namespace io {

		unsigned char Color8Bits(uint16_t color16Bit)
		{
			return (color16Bit < 256) ? color16Bit : static_cast<unsigned char>((color16Bit / 65535.0)*255.0);
		};

		//////////////////////////// Points Reader ///////////////////////
		class PointsReader
		{
		public:
			PointsReader(const std::string& filename);
			~PointsReader() {}
			virtual bool Init() = 0;
			virtual bool ReadNextPoint(PointCI& point) = 0;
			size_t GetPointsCount() { return _pointCount; }
			size_t GetCurrentPointId() { return _currentPointId; }
			osg::Vec3d GetOffset() { return _offset; } // set first point as offset
			std::string GetSRS() { return _srsName; }

		protected:
			std::string _filename;
			size_t _pointCount;
			size_t _currentPointId;
			osg::Vec3d _offset;
			std::string _srsName;
		};

		PointsReader::PointsReader(const std::string& filename):
			_filename(filename),
			_currentPointId(0),
			_pointCount(0),
			_offset(0, 0, 0),
			_srsName("")
		{

		}

		////////////////////////// Laz/Laz Reader(laszip lib can read both las or laz) /////////////////////////////////
		class LazReader:public PointsReader
		{
		public:
			LazReader(const std::string& filename);
			bool Init() override;
			~LazReader();
			bool ReadNextPoint(PointCI& point) override;

		private:
			osg::Vec3d _offsetFirstPoint;
			laszip_point* _pointRead;//current reading point
			laszip_POINTER _laszipReader;
			laszip_header* _laszipHeader;
		};

		LazReader::LazReader(const std::string& filename):
			PointsReader(filename)
		{
			
		}

		LazReader::~LazReader()
		{
			if (!_laszipReader) return;
			// close the reader
			if (laszip_close_reader(_laszipReader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in closing laszip reader!");
			}

			// destroy the reader
			if (laszip_destroy(_laszipReader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in destroying laszip reader!");
			}
		}

		bool LazReader::Init()
		{
			double start_time = 0.0;
			const char* file_name_in = _filename.c_str();
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
			if (laszip_create(&_laszipReader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in creating laszip reader!");
				return false;
			}

			// open the reader
			laszip_BOOL is_compressed = 0;
			if (laszip_open_reader(_laszipReader, file_name_in, &is_compressed))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in opening laszip reader for '%s'", file_name_in);
				return false;
			}

			seed::log::DumpLog(seed::log::Debug, "file '%s' is %scompressed\n", file_name_in, (is_compressed ? "" : "un"));
			// get a pointer to the header of the reader that was just populated
			if (laszip_get_header_pointer(_laszipReader, &_laszipHeader))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in getting header pointer from laszip reader!");
				return false;
			}

			// how many points does the file have
			_pointCount = (_laszipHeader->number_of_point_records ? 
				_laszipHeader->number_of_point_records : _laszipHeader->extended_number_of_point_records);

			int lengthRecords = _laszipHeader->number_of_variable_length_records;
			auto& vlrs = _laszipHeader->vlrs; // Variable Length Records
			for (int i = 0; i < lengthRecords; ++i)
			{
				if (vlrs[i].data == NULL) continue;
				if (vlrs[i].record_id == 2111 || vlrs[i].record_id == 2112)
					_srsName = (char*)vlrs[i].data;// wkt
			}
			
			// report how many points the file has
			seed::log::DumpLog(seed::log::Debug, "file '%s' contains %I64d points", file_name_in, _pointCount);

			// get a pointer to the points that will be read
			if (laszip_get_point_pointer(_laszipReader, &_pointRead))
			{
				seed::log::DumpLog(seed::log::Critical, "An error occured in getting point pointer from laszip reader!");
				return false;
			}

			return true;
		}

		bool LazReader::ReadNextPoint(PointCI& pt)
		{
			if (_currentPointId < _pointCount)
			{
				// read a point
				if (laszip_read_point(_laszipReader))
				{
					seed::log::DumpLog(seed::log::Critical, "An error occured in reading point %I64d", _currentPointId);
					return false;
				}

				// init offset
				if (_currentPointId == 0)
				{
					_offsetFirstPoint[0] = _pointRead->X * _laszipHeader->x_scale_factor;
					_offsetFirstPoint[1] = _pointRead->Y * _laszipHeader->y_scale_factor;
					_offsetFirstPoint[2] = _pointRead->Z * _laszipHeader->z_scale_factor;

					_offset[0] = _laszipHeader->x_offset;
					_offset[1] = _laszipHeader->y_offset;
					_offset[2] = _laszipHeader->z_offset;

					_offset += _offsetFirstPoint;
				}

				// add scale to coords
				pt.P[0] = _pointRead->X * _laszipHeader->x_scale_factor - _offsetFirstPoint[0];
				pt.P[1] = _pointRead->Y * _laszipHeader->y_scale_factor - _offsetFirstPoint[1];
				pt.P[2] = _pointRead->Z * _laszipHeader->z_scale_factor - _offsetFirstPoint[2];

				auto& rgb = _pointRead->rgb;
				pt.C[0] = Color8Bits(rgb[0]);
				pt.C[1] = Color8Bits(rgb[1]);
				pt.C[2] = Color8Bits(rgb[2]);
				pt.I = Color8Bits(_pointRead->intensity);

				_currentPointId++;
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
			bool ReadNextPoint(PointCI& point) override;

		private:
			PlyFile *_plyFile;
			int _nrElems;
			char **_elist;
			bool _foundColors;
			bool _foundVertices;
		};

		PlyReader::PlyReader(const std::string& filename) :
			PointsReader(filename),
			_foundVertices(true),
			_foundColors(true)
		{}

		PlyReader::~PlyReader()
		{
			if (!_plyFile) return;
			for (int i = 0; i < _nrElems; i++) {
				free(_plyFile->elems[i]->name);
				free(_plyFile->elems[i]->store_prop);
				for (int j = 0; j < _plyFile->elems[i]->nprops; j++) {
					free(_plyFile->elems[i]->props[j]->name);
					free(_plyFile->elems[i]->props[j]);
				}
				free(_plyFile->elems[i]->props);
			}
			for (int i = 0; i < _nrElems; i++) { free(_plyFile->elems[i]); }
			free(_plyFile->elems);
			for (int i = 0; i < _plyFile->num_comments; i++) { free(_plyFile->comments[i]); }
			free(_plyFile->comments);
			for (int i = 0; i < _plyFile->num_obj_info; i++) { free(_plyFile->obj_info[i]); }
			free(_plyFile->obj_info);
			ply_free_other_elements(_plyFile->other_elems);

			for (int i = 0; i < _nrElems; i++) { free(_elist[i]); }
			free(_elist);
			ply_close(_plyFile);
		}

		bool PlyReader::Init()
		{
			int fileType;
			float version;
			PlyProperty** plist;
			_plyFile = ply_open_for_reading((char*)_filename.data(), &_nrElems, &_elist, &fileType, &version);
			if (!_plyFile)
			{
				seed::log::DumpLog(seed::log::Critical, "Open file %s failed!", _filename.c_str());
				return false;
			}

			for (int i = 0; i < _nrElems; i++)
			{
				int num_elems;
				int nr_props;
				char* elem_name = _elist[i];
				plist = ply_get_element_description(_plyFile, elem_name, &num_elems, &nr_props);
				if (!plist)
				{
					for (int i = 0; i < _nrElems; i++) {
						free(_plyFile->elems[i]->name);
						free(_plyFile->elems[i]->store_prop);
						for (int j = 0; j < _plyFile->elems[i]->nprops; j++) {
							free(_plyFile->elems[i]->props[j]->name);
							free(_plyFile->elems[i]->props[j]);
						}
						free(_plyFile->elems[i]->props);
					}
					for (int i = 0; i < _nrElems; i++) { free(_plyFile->elems[i]); }
					free(_plyFile->elems);
					for (int i = 0; i < _plyFile->num_comments; i++) { free(_plyFile->comments[i]); }
					free(_plyFile->comments);
					for (int i = 0; i < _plyFile->num_obj_info; i++) { free(_plyFile->obj_info[i]); }
					free(_plyFile->obj_info);
					ply_free_other_elements(_plyFile->other_elems);

					for (int i = 0; i < _nrElems; i++) { free(_elist[i]); }
					free(_elist);
					ply_close(_plyFile);
					seed::log::DumpLog(seed::log::Critical, "Failed to get element description: %s", elem_name);
					return false;
				}

				if (equal_strings("vertex", elem_name))
				{
					_pointCount = num_elems;
					for (int j = 0; j < 3; j++)
						if (!ply_get_property(_plyFile, elem_name, &(PlyColorVertex< float >::ReadProperties[j]))) //  xyz
						{
							seed::log::DumpLog(seed::log::Critical, "Read Vertex failed!");
							return false;
						}

					for (int j = 3; j < 6; j++)
						if (!ply_get_property(_plyFile, elem_name, &(PlyColorVertex<float>::ReadProperties[j])))
						{
							//seed::log::DumpLog(seed::log::Critical, "Read file %s failed(no color)!", input);
							_foundColors = false;
						}
					if (!_foundColors)
						for (int j = 6; j < 9; j++)
							if (!ply_get_property(_plyFile, elem_name, &(PlyColorVertex<float>::ReadProperties[j])))
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
				if (_foundVertices) break;
			}
			if (!_foundVertices)
			{
				seed::log::DumpLog(seed::log::Critical, "No vertice in file %s!", _filename.c_str());
				return false;
			}

			return true;
		}

		bool PlyReader::ReadNextPoint(PointCI& point)
		{
			if (_currentPointId >= _pointCount) return false;
			PlyValueOrientedColorVertex<float> vertex;
			ply_get_element(_plyFile, (void *)&vertex);

			// init offset
			if (_currentPointId == 0)
			{
				_offset[0] = vertex.point[0];
				_offset[1] = vertex.point[1];
				_offset[2] = vertex.point[2];
			}

			for (int k = 0; k < 3; ++k)
			{
				point.P[k] = vertex.point[k] - _offset[k];
				if (_foundColors)
					point.C[k] = Color8Bits(vertex.color[k]);
			}

			_currentPointId++;
			return true;
		}

		////////////////////////// XYZRGB Reader /////////////////////////////////
		class XYZRGBReader :public PointsReader
		{
		public:
			XYZRGBReader(const std::string& filename);
			bool Init() override;
			~XYZRGBReader();
			bool ReadNextPoint(PointCI& point) override;

		private:
			std::ifstream _xyzFile;
		};

		XYZRGBReader::XYZRGBReader(const std::string& filename) :
			PointsReader(filename)
		{

		}

		XYZRGBReader::~XYZRGBReader()
		{
			_xyzFile.close();
		}

		bool XYZRGBReader::Init()
		{
			_xyzFile.open(_filename);
			if (_xyzFile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Open file %s failed!", _filename.c_str());
				return false;
			}
			std::string line;
			while (std::getline(_xyzFile, line).good())
			{
				++_pointCount;
			}
			_xyzFile.clear();
			_xyzFile.seekg(0, _xyzFile.beg);
			if (!_pointCount)
			{
				seed::log::DumpLog(seed::log::Critical, "No vertice in file %s!", _filename.c_str());
				return false;
			}
			return true;
		}

		bool XYZRGBReader::ReadNextPoint(PointCI& point)
		{
			if (_currentPointId >= _pointCount) return false;
			std::string line;
			if (!std::getline(_xyzFile, line).good())
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
			point.C[0] = Color8Bits(color);
			ss >> color;
			point.C[1] = Color8Bits(color);
			ss >> color;
			point.C[2] = Color8Bits(color);

			// init offset
			if (_currentPointId == 0)
			{
				_offset[0] = point.P[0];
				_offset[1] = point.P[1];
				_offset[2] = point.P[2];
			}

			for (int k = 0; k < 3; ++k)
			{
				point.P[k] = point.P[k] - _offset[k];
			}

			_currentPointId++;
			return true;
		}

		////////////////////////// Point Visitor /////////////////////////////////
		PointVisitor::PointVisitor()
		{
		}

		bool PointVisitor::PerpareFile(const std::string& input, bool runStatistic)
		{
			if(!(ResetFile(input)))
				return false;

			if (runStatistic)
			{
				seed::log::DumpLog(seed::log::Info, "Run statistic");

				// bbox and offset
				PointCI point;
				_bbox.init();
				while (_pointsReader->ReadNextPoint(point))
				{
					_bbox.expandBy(point.P);
				}
				osg::Vec3d l_offset = _pointsReader->GetOffset();
				seed::log::DumpLog(seed::log::Info, "Offset: %f, %f, %f", l_offset.x(), l_offset.y(), l_offset.z());
				seed::log::DumpLog(seed::log::Info, "LocalMin: %f, %f, %f", _bbox.xMin(), _bbox.yMin(), _bbox.zMin());
				seed::log::DumpLog(seed::log::Info, "LocalMax: %f, %f, %f", _bbox.xMax(), _bbox.yMax(), _bbox.zMax());

				// histogram
				if (!(ResetFile(input)))
					return false;

				_bboxZHistogram = _bbox;
				int histogram[256] = { 0 };
				while (_pointsReader->ReadNextPoint(point))
				{
					int index = (point.P.z() - _bbox.zMin()) / (_bbox.zMax() - _bbox.zMin()) * 255.;
					index = std::max(0, std::min(255, index));
					histogram[index]++;
				}
				int countFront = 0;
				int countBack = 0;
				int threshold = (float)_pointsReader->GetPointsCount() * 0.025;
				for (int i = 0; i < 256; ++i)
				{
					countFront += histogram[i];
					if (countFront >= threshold)
					{
						_bboxZHistogram.zMin() = (_bbox.zMax() - _bbox.zMin()) / 255. * (float)i + _bbox.zMin();
						break;
					}
				}
				for (int i = 255; i >= 0; --i)
				{
					countBack += histogram[i];
					if (countBack >= threshold)
					{
						_bboxZHistogram.zMax() = (_bbox.zMax() - _bbox.zMin()) / 255. * (float)i + _bbox.zMin();
						break;
					}
				}

				// reset file to read
				return ResetFile(input);
			}
			else
			{
				return true;
			}
		}

		bool PointVisitor::ResetFile(const std::string& input)
		{
			std::string ext = osgDB::getFileExtensionIncludingDot(input);
			if (ext == ".ply")
				_pointsReader.reset(new PlyReader(input));
			else if (ext == ".laz" || ext == ".las")
				_pointsReader.reset(new LazReader(input));
			else if (ext == ".xyz")
				_pointsReader.reset(new XYZRGBReader(input));
			else
			{
				seed::log::DumpLog(seed::log::Critical, "%s is NOT supported now.", ext.c_str());
				return false;
			}

			if (!_pointsReader->Init())
				return false;

			return true;
		}

		size_t PointVisitor::GetNumOfPoints()
		{
			return _pointsReader->GetPointsCount();
		}

		std::string PointVisitor::GetSRSName()
		{
			return _pointsReader->GetSRS();
		}

		int PointVisitor::NextPoint(PointCI& point)
		{
			if (!_pointsReader->ReadNextPoint(point)) return -1;

			return 1;
		}

		osg::Vec3d PointVisitor::GetOffset()
		{
			return _pointsReader->GetOffset();
		}
	}
}
