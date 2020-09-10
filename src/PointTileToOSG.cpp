#include "PointTileToOSG.h"

namespace seed
{
	namespace io
	{
		void PointTileToOSG::CreateColorBar()
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

		AxisInfo PointTileToOSG::FindMaxAxis(osg::BoundingBox boundingBox)
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
			return maxAxisInfo;
		}

		bool PointTileToOSG::Generate(const std::vector<OSGBPoint> *pointSet,
			const std::string& saveFilePath, const std::string& strBlock)
		{
			std::vector<unsigned int> pointIndex;
			osg::BoundingBox boundingBox;
			for (int i = 0; i < pointSet->size(); i++)
			{
				pointIndex.push_back(i);
				OSGBPoint tmpPoint = pointSet->at(i);
				boundingBox.expandBy(osg::Vec3(tmpPoint.P.X(), tmpPoint.P.Y(), tmpPoint.P.Z()));
			}
			try
			{
				BuildNode(pointSet, pointIndex, boundingBox, boundingBox, saveFilePath, strBlock, 0, 0);
				pointIndex.swap(std::vector<unsigned int>());
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "KDTree generate error!");
				return false;
			}
			return true;
		}

		osg::Geode *PointTileToOSG::MakeNodeGeode(const std::vector<OSGBPoint> *pointSet,
			std::vector<unsigned int> &pointIndex)
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
				OSGBPoint tmpPoint = pointSet->at(*i);
				pointArray->push_back(osg::Vec3(tmpPoint.P.X(), tmpPoint.P.Y(), tmpPoint.P.Z()));
				if (_colorMode == ColorMode::RGB)
				{
					colorArray->push_back(
						osg::Vec4((float)tmpPoint.C[0] / 255.0f,
						(float)tmpPoint.C[1] / 255.0f,
						(float)tmpPoint.C[2] / 255.0f,
						1));
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
					float x = (tmpPoint.P.Z() - this->_boundingBoxGlobal.zMin()) / (this->_boundingBoxGlobal.zMax() - this->_boundingBoxGlobal.zMin());
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

		bool PointTileToOSG::BuildNode(const std::vector<OSGBPoint> *pointSet,
			std::vector<unsigned int> &pointIndex,
			osg::BoundingBox boundingBox,
			osg::BoundingBox boundingBoxLevel0,
			const std::string& saveFilePath,
			const std::string& strBlock,
			unsigned int level,
			unsigned int childNo)
		{
			osg::ref_ptr<osg::Group> mt(new osg::Group);

			osg::ref_ptr<osg::Geode> nodeGeode;
			AxisInfo maxAxisInfo;
			std::vector<unsigned int> selfPointSetIndex;
			std::vector<unsigned int> leftPointSetIndex;
			std::vector<unsigned int> rightPointSetIndex;
			osg::BoundingBox leftBoundingBox;
			osg::BoundingBox rightBoundingBox;
			std::string saveFileName, leftPageName, rightPageName;
			osg::ref_ptr<osg::PagedLOD>  leftPageNode = new osg::PagedLOD;
			osg::ref_ptr<osg::PagedLOD>  rightPageNode = new osg::PagedLOD;

			if (level == 0)
			{
				saveFileName = saveFilePath + "/" + strBlock + ".osgb";
			}
			else
			{
				char tmpSaveFileName[100];
				sprintf(tmpSaveFileName, "%s%s%s%d%s%d%s", "/", strBlock.c_str(), "_L", level, "_", childNo, ".osgb");
				saveFileName.assign(tmpSaveFileName);
				saveFileName = saveFilePath + saveFileName;
			}

			if (pointIndex.size() < _maxPointNumPerOneNode || level >= _maxTreeLevel)
			{
				nodeGeode = MakeNodeGeode(pointSet, pointIndex);
				try
				{
					if (osgDB::writeNodeFile(*(nodeGeode.get()), saveFileName, new osgDB::ReaderWriter::Options("precision 20")) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
					}
				}
				catch (...)
				{
					seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
				}
				return true;
			}

			maxAxisInfo = FindMaxAxis(boundingBox);
			double mid = (maxAxisInfo.max + maxAxisInfo.min) / 2;

			rightBoundingBox.init();
			leftBoundingBox.init();

			leftBoundingBox = boundingBox;
			rightBoundingBox = boundingBox;

			if (maxAxisInfo.aixType == X)
			{
				leftBoundingBox._max[0] = mid;
				rightBoundingBox._min[0] = mid;
			}
			else if (maxAxisInfo.aixType == Y)
			{
				leftBoundingBox._max[1] = mid;
				rightBoundingBox._min[1] = mid;
			}
			else
			{
				leftBoundingBox._max[2] = mid;
				rightBoundingBox._min[2] = mid;
			}

			float interval = (float)pointIndex.size() / (float)_maxPointNumPerOneNode;
			int count = -1;
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
					OSGBPoint tmpPoint = pointSet->at(pointIndex[i]);
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
			try
			{
				nodeGeode = MakeNodeGeode(pointSet, selfPointSetIndex);
				selfPointSetIndex.swap(std::vector<unsigned int>());
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "Make self node geode error!", saveFileName.c_str());
				return false;
			}

			char tmpLeftPageName[100], tmpRightPageName[100];
			sprintf(tmpLeftPageName, "%s%s%s%d%s%d%s", "/", strBlock.c_str(), "_L", level + 1, "_", childNo * 2, ".osgb");
			leftPageName.assign(tmpLeftPageName);
			sprintf(tmpRightPageName, "%s%s%s%d%s%d%s", "/", strBlock.c_str(), "_L", level + 1, "_", childNo * 2 + 1, ".osgb");
			rightPageName.assign(tmpRightPageName);
			
			double rangeRatio = 1;
			double rangeValue = boundingBoxLevel0.radius() * 2.f * _lodRatio * rangeRatio;
			leftPageNode->setRangeMode(osg::PagedLOD::PIXEL_SIZE_ON_SCREEN);
			leftPageNode->setFileName(0, leftPageName);
			leftPageNode->setRange(0, rangeValue, FLT_MAX);
			leftPageNode->setCenter(leftBoundingBox.center());
			leftPageNode->setRadius(leftBoundingBox.radius());
			rightPageNode->setRangeMode(osg::PagedLOD::PIXEL_SIZE_ON_SCREEN);
			rightPageNode->setFileName(0, rightPageName);
			rightPageNode->setRange(0, rangeValue, FLT_MAX);
			rightPageNode->setCenter(rightBoundingBox.center());
			rightPageNode->setRadius(rightBoundingBox.radius());

			mt->addChild(nodeGeode.get());

			// left
			if (leftPointSetIndex.size())
			{
				BuildNode(pointSet, leftPointSetIndex, leftBoundingBox, boundingBoxLevel0, saveFilePath, strBlock, level + 1, childNo * 2);
				leftPointSetIndex.swap(std::vector<unsigned int>());
				mt->addChild(leftPageNode.get());
			}
			// right
			if (rightPointSetIndex.size())
			{
				BuildNode(pointSet, rightPointSetIndex, rightBoundingBox, boundingBoxLevel0, saveFilePath, strBlock, level + 1, childNo * 2 + 1);
				rightPointSetIndex.swap(std::vector<unsigned int>());
				mt->addChild(rightPageNode.get());
			}

			try
			{
				if (osgDB::writeNodeFile(*(mt.get()), saveFileName, new osgDB::ReaderWriter::Options("precision 20")) == false)
				{
					seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
				}
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
			}
			selfPointSetIndex.swap(std::vector<unsigned int>());
			return true;
		}
	}
}