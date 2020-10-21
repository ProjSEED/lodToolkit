#include "tileToLOD.h"
#include "c3mx.h"

namespace seed
{
	namespace io
	{
		void TileToLOD::CreateColorBar()
		{
			std::vector<osg::Vec4> steps;
			if (_colorMode == ColorMode::IntensityGrey)
			{
				osg::Vec4 black(0., 0., 0., 1.);
				osg::Vec4 white(1., 1., 1., 1.);
				steps.push_back(black);
				steps.push_back(white);
			}
			else if (_colorMode == ColorMode::IntensityBlueWhiteRed)
			{
				osg::Vec4 blue(0., 0., 1., 1.);
				osg::Vec4 white(1., 1., 1., 1.);
				osg::Vec4 red(1., 0., 0., 1.);
				steps.push_back(blue);
				steps.push_back(white);
				steps.push_back(red);
			}
			else if (_colorMode == ColorMode::IntensityHeightBlend)
			{
				osg::Vec4 blue(0., 0., 1., 1.);
				osg::Vec4 green(0., 1., 0., 1.);
				osg::Vec4 yellow(1., 1., 0., 1.);
				osg::Vec4 red(1., 0., 0., 1.);
				steps.push_back(blue);
				steps.push_back(green);
				steps.push_back(yellow);
				steps.push_back(red);
			}
			else
			{
				return;
			}
			assert(steps.size() >= 2);
			for (int i = 0; i < 256; ++i)
			{
				float intensity = (float)i / 255.f * (float)(steps.size() - 1);
				int floor = intensity;
				floor = std::max(0, floor);
				if (floor >= steps.size() - 1)
				{
					_colorBar[i] = steps.back();
				}
				else
				{
					float upper = intensity - floor;
					float lower = 1. - upper;
					_colorBar[i] = steps[floor] * lower + steps[floor + 1] * upper;
				}
			}
		}

		AxisInfo TileToLOD::FindMaxAxis(osg::BoundingBox boundingBox, osg::BoundingBox& boundingBoxLeft, osg::BoundingBox& boundingBoxRight)
		{
			AxisInfo maxAxisInfo;
			double xLen = boundingBox.xMax() - boundingBox.xMin();
			double yLen = boundingBox.yMax() - boundingBox.yMin();
			double zLen = boundingBox.zMax() - boundingBox.zMin();
			if (xLen >= yLen && xLen >= zLen)
			{
				maxAxisInfo.aixType = 0;
				maxAxisInfo.max = boundingBox.xMax();
				maxAxisInfo.min = boundingBox.xMin();
			}
			else if (yLen >= xLen && yLen >= zLen)
			{
				maxAxisInfo.aixType = 1;
				maxAxisInfo.max = boundingBox.yMax();
				maxAxisInfo.min = boundingBox.yMin();
			}
			else if (zLen >= xLen && zLen >= yLen)
			{
				maxAxisInfo.aixType = 2;
				maxAxisInfo.max = boundingBox.zMax();
				maxAxisInfo.min = boundingBox.zMin();
			}

			boundingBoxRight.init();
			boundingBoxLeft.init();

			boundingBoxLeft = boundingBox;
			boundingBoxRight = boundingBox;

			double mid = (maxAxisInfo.max + maxAxisInfo.min) / 2;
			if (maxAxisInfo.aixType == X)
			{
				boundingBoxLeft._max[0] = mid;
				boundingBoxRight._min[0] = mid;
			}
			else if (maxAxisInfo.aixType == Y)
			{
				boundingBoxLeft._max[1] = mid;
				boundingBoxRight._min[1] = mid;
			}
			else
			{
				boundingBoxLeft._max[2] = mid;
				boundingBoxRight._min[2] = mid;
			}
			return maxAxisInfo;
		}

		bool TileToLOD::Generate(const std::vector<PointCI> *pointSet,
			const std::string& saveFilePath, const std::string& strBlock, ExportMode exportMode, osg::BoundingBox& boundingBoxLevel0)
		{
			std::vector<unsigned int> pointIndex;
			osg::BoundingBox boundingBox;
			for (int i = 0; i < pointSet->size(); i++)
			{
				pointIndex.push_back(i);
				boundingBox.expandBy(pointSet->at(i).P);
			}
			boundingBoxLevel0 = boundingBox;
			try
			{
				BuildNode(pointSet, pointIndex, boundingBox, boundingBox, saveFilePath, strBlock, 0, 0, exportMode);
				pointIndex.swap(std::vector<unsigned int>());
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "KDTree generate error!");
				return false;
			}
			return true;
		}

		float Color8BitsToFloat(unsigned char color8Bit)
		{
			float val = color8Bit / 255.f;
			if (val < 0) val = 0.f;
			if (val > 1.f) val = 1.f;
			return val;
		};

		osg::Geode *TileToLOD::MakeNodeGeode(const std::vector<PointCI> *pointSet,
			std::vector<unsigned int> &pointIndex, ExportMode exportMode)
		{
			if (pointIndex.size() <= 0)
			{
				return 0;
			}
			osg::ref_ptr<osg::Geode> geode = new osg::Geode;
			osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
			osg::ref_ptr<osg::Vec3Array> pointArray = new osg::Vec3Array;
			osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array;
			osg::ref_ptr<osg::StateSet> set = new osg::StateSet;
			osg::ref_ptr<osg::Point> point = new osg::Point;
			
			for (std::vector<unsigned int>::iterator i = pointIndex.begin(); i != pointIndex.end(); i++)
			{
				PointCI tmpPoint = pointSet->at(*i);
				pointArray->push_back(tmpPoint.P);
				if (_colorMode == ColorMode::RGB)
				{
					colorArray->push_back(
						osg::Vec4(Color8BitsToFloat(tmpPoint.C[0]),
							Color8BitsToFloat(tmpPoint.C[1]),
							Color8BitsToFloat(tmpPoint.C[2]),
						1.f));
				}
				else if (_colorMode == ColorMode::IntensityGrey)
				{
					colorArray->push_back(_colorBar[tmpPoint.I]);
				}
				else if (_colorMode == ColorMode::IntensityBlueWhiteRed)
				{
					colorArray->push_back(_colorBar[tmpPoint.I]);
				}
				else if (_colorMode == ColorMode::IntensityHeightBlend)
				{
					float x = (tmpPoint.P.z() - this->_boundingBoxGlobal.zMin()) / (this->_boundingBoxGlobal.zMax() - this->_boundingBoxGlobal.zMin());
					//// sigmoid
					//x = (x - 0.5) * 4;
					//x = 1. / (1. + exp(-5 * x));
					int index = x * 255;
					index = std::max(0, std::min(255, index));
					osg::Vec4 color = _colorBar[index];
					color *= (tmpPoint.I / 255.);
					color.w() = 1.0;
					colorArray->push_back(color);
				}
			}

			if (_pointSize > 0)
			{
				point->setDistanceAttenuation(osg::Vec3(1.0f, 0.0f, 0.01f));
				point->setSize(_pointSize);
				set->setMode(GL_POINT_SMOOTH, osg::StateAttribute::ON);
				set->setAttribute(point);
				geometry->setStateSet(set);
			}
			geometry->setVertexArray(pointArray.get());
			geometry->setColorArray(colorArray.get());
			geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

			geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, pointIndex.size()));

			geometry->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
			geode->addDrawable(geometry.get());
			return geode.release();
		}

		bool TileToLOD::BuildNode(const std::vector<PointCI> *pointSet,
			std::vector<unsigned int> &pointIndex,
			osg::BoundingBox boundingBox,
			osg::BoundingBox boundingBoxLevel0,
			const std::string& saveFilePath,
			const std::string& strBlock,
			unsigned int level,
			unsigned int childNo, 
			ExportMode exportMode)
		{
			// format
			std::string format;
			if (exportMode == ExportMode::OSGB)
			{
				format = ".osgb";
			}
			else if (exportMode == ExportMode::_3MX)
			{
				format = ".3mxb";
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", exportMode);
				return false;
			}

			// filename of self, left, right
			std::string saveFileName, leftPageName, rightPageName;
			if (level == 0)
			{
				saveFileName = saveFilePath + "/" + strBlock + format;
			}
			else
			{
				char tmpSaveFileName[100];
				sprintf(tmpSaveFileName, "%s%s%d%s%d%s", strBlock.c_str(), "_L", level, "_", childNo, format.c_str());
				saveFileName.assign(tmpSaveFileName);
				saveFileName = saveFilePath + "/" + saveFileName;
			}
			char tmpLeftPageName[100], tmpRightPageName[100];
			sprintf(tmpLeftPageName, "%s%s%d%s%d%s", strBlock.c_str(), "_L", level + 1, "_", childNo * 2, format.c_str());
			leftPageName.assign(tmpLeftPageName);
			sprintf(tmpRightPageName, "%s%s%d%s%d%s", strBlock.c_str(), "_L", level + 1, "_", childNo * 2 + 1, format.c_str());
			rightPageName.assign(tmpRightPageName);

			// handle leaf case
			if (pointIndex.size() < _maxPointNumPerOneNode || level >= _maxTreeLevel)
			{
				osg::ref_ptr<osg::Geode> nodeGeode = MakeNodeGeode(pointSet, pointIndex, exportMode);
				if (exportMode == ExportMode::OSGB)
				{
					if (osgDB::writeNodeFile(*(nodeGeode.get()), saveFileName, new osgDB::ReaderWriter::Options("precision 20")) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
						return false;
					}
				}
				else if (exportMode == ExportMode::_3MX)
				{
					if(ConvertOsgbTo3mxb(nodeGeode, saveFileName) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
						return false;
					}
				}
				else
				{
					seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", exportMode);
					return false;
				}
				return true;
			}

			// prepare box
			AxisInfo maxAxisInfo;
			osg::BoundingBox leftBoundingBox;
			osg::BoundingBox rightBoundingBox;
			maxAxisInfo = FindMaxAxis(boundingBox, leftBoundingBox, rightBoundingBox);
			double mid = (maxAxisInfo.max + maxAxisInfo.min) / 2;

			// split self, left, right
			float interval = (float)pointIndex.size() / (float)_maxPointNumPerOneNode;
			int count = -1;
			std::vector<unsigned int> selfPointSetIndex;
			std::vector<unsigned int> leftPointSetIndex;
			std::vector<unsigned int> rightPointSetIndex;
			for (int i = 0; i < pointIndex.size(); i++)
			{
				int tmp = int((float)i / interval);
				if (tmp > count && selfPointSetIndex.size() < _maxPointNumPerOneNode)
				{
					count = tmp;
					selfPointSetIndex.push_back(pointIndex[i]);
				}
				else
				{
					PointCI tmpPoint = pointSet->at(pointIndex[i]);
					if (tmpPoint.P[int(maxAxisInfo.aixType)] > mid)
					{
						rightPointSetIndex.push_back(pointIndex[i]);
					}
					else
					{
						leftPointSetIndex.push_back(pointIndex[i]);
					}
				}
			}

			// export
			{
				osg::ref_ptr<osg::Group> mt(new osg::Group);
				osg::ref_ptr<osg::Geode> nodeGeode = MakeNodeGeode(pointSet, selfPointSetIndex, exportMode);
				mt->addChild(nodeGeode.get());
				selfPointSetIndex.swap(std::vector<unsigned int>());

				double rangeRatio = 4.;
				double rangeValue = boundingBoxLevel0.radius() * 2.f * _lodRatio * rangeRatio;

				if (leftPointSetIndex.size())
				{
					osg::ref_ptr<osg::PagedLOD>  leftPageNode = new osg::PagedLOD;
					leftPageNode->setRangeMode(osg::PagedLOD::PIXEL_SIZE_ON_SCREEN);
					leftPageNode->setFileName(0, leftPageName);
					leftPageNode->setRange(0, rangeValue, FLT_MAX);
					leftPageNode->setCenter(leftBoundingBox.center());
					leftPageNode->setRadius(leftBoundingBox.radius());
					mt->addChild(leftPageNode.get());
				}

				if (rightPointSetIndex.size())
				{
					osg::ref_ptr<osg::PagedLOD>  rightPageNode = new osg::PagedLOD;
					rightPageNode->setRangeMode(osg::PagedLOD::PIXEL_SIZE_ON_SCREEN);
					rightPageNode->setFileName(0, rightPageName);
					rightPageNode->setRange(0, rangeValue, FLT_MAX);
					rightPageNode->setCenter(rightBoundingBox.center());
					rightPageNode->setRadius(rightBoundingBox.radius());
					mt->addChild(rightPageNode.get());
				}
				if (exportMode == ExportMode::OSGB)
				{
					if (osgDB::writeNodeFile(*(mt.get()), saveFileName, new osgDB::ReaderWriter::Options("precision 20")) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
						return false;
					}
				}
				else if (exportMode == ExportMode::_3MX)
				{
					if (ConvertOsgbTo3mxb(mt, saveFileName) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
						return false;
					}
				}
				else
				{
					seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", exportMode);
					return false;
				}
			}

			// recursive left
			if (leftPointSetIndex.size())
			{
				BuildNode(pointSet, leftPointSetIndex, leftBoundingBox, boundingBoxLevel0, saveFilePath, strBlock, level + 1, childNo * 2, exportMode);
				leftPointSetIndex.swap(std::vector<unsigned int>());
			}
			// recursive right
			if (rightPointSetIndex.size())
			{
				BuildNode(pointSet, rightPointSetIndex, rightBoundingBox, boundingBoxLevel0, saveFilePath, strBlock, level + 1, childNo * 2 + 1, exportMode);
				rightPointSetIndex.swap(std::vector<unsigned int>());
			}
			return true;
		}
	}
}