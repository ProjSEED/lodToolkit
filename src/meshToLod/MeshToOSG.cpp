#include "MeshToOSG.h"

#include <osg/BoundingBox>
#include <osg/ref_ptr>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PagedLOD>
#include <osg/ProxyNode>
#include <osg/Texture2D>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/MatrixTransform>
#include <osgDB/ReaderWriter>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgUtil/Simplifier>

namespace seed
{
	namespace io
	{
		MeshToOSG::MeshToOSG()
		{

		}

		MeshToOSG::~MeshToOSG()
		{

		}

		bool MeshToOSG::Convert(const std::string& input, const std::string& output, float lodRatio)
		{
			std::string ext = osgDB::getFileExtensionIncludingDot(input);
			if (ext == ".obj")
			{

			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "%s is NOT supported now.", ext.c_str());
				return false;
			}
			seed::log::DumpLog(seed::log::Info, "Input file format: %s", ext.c_str());
			seed::progress::UpdateProgress(1);

			osg::ref_ptr<osgDB::Options> pOptRead = new osgDB::Options("AMBIENT=0 noRotation");
			osg::ref_ptr<osg::Node> loadedModel = osgDB::readRefNodeFile(input, pOptRead);
			if (!loadedModel)
			{
				seed::log::DumpLog(seed::log::Critical, "Open file %s failed!", input.c_str());
				return false;
			}
			osg::ref_ptr<osg::Group> group = loadedModel->asGroup();
			if (!group)
			{
				seed::log::DumpLog(seed::log::Critical, "Open file %s failed!", input.c_str());
				return false;
			}
			seed::progress::UpdateProgress(30);

			std::string dirRoot(output);
			std::string dirDataRelative("Data/");
			std::string dirData(dirRoot + "/" + dirDataRelative);
			std::string mainName = dirRoot + "/Root.osgb";
			osgDB::makeDirectory(dirRoot);
			osgDB::makeDirectory(dirData);

			osg::ref_ptr<osgDB::Options> pOptWrite = new osgDB::Options("WriteImageHint=IncludeFile");

			osg::ref_ptr<osg::ProxyNode> pProxyNode = new osg::ProxyNode();
			osg::BoundingBox boundingBoxGlobal;
			for (unsigned int i = 0; i < group->getNumChildren(); ++i)
			{
				osg::Geode* geode = group->getChild(i)->asGeode();
				std::string highModelName = geode->getName() + "_detail.osgb";
				std::string highModelRelativePath = geode->getName() + "/" + highModelName;
				std::string highModelImgPath;
				std::string lowModelName = geode->getName() + ".osgb";
				std::string lowModelRelativePath = geode->getName() + "/" + lowModelName;
				std::string lowModelImgPath;
				osgDB::makeDirectoryForFile(dirData + highModelRelativePath);
				boundingBoxGlobal.expandBy(geode->getBoundingBox());
				osgDB::writeNodeFile(*geode, dirData + highModelRelativePath, pOptWrite);

				osg::Geode* geodeLow = new osg::Geode(*geode, osg::CopyOp::DEEP_COPY_ALL);
				
				osg::Geometry* geometry = geodeLow->getDrawable(0)->asGeometry();
				if (geometry)
				{
					int before = geometry->getVertexArray()->getNumElements();
					osgUtil::Simplifier simplifier;
					simplifier.setSampleRatio(0.001f);
					simplifier.simplify(*geometry);

					osg::StateSet* stateset = geometry->getStateSet();
					if (stateset)
					{
						//stateset->removeTextureAttribute(0, osg::StateAttribute::TEXTURE);
						osg::Texture2D* tex = static_cast<osg::Texture2D*>(stateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
						if (tex)
						{
							osg::Image* img = tex->getImage();
							if (img)
							{
								highModelImgPath = img->getFileName();
								lowModelImgPath = osgDB::getNameLessExtension(highModelImgPath) + "_low" + osgDB::getFileExtensionIncludingDot(highModelImgPath);
								img->scaleImage(128, 128, img->r());
								osgDB::writeImageFile(*img, lowModelImgPath);
								img->setFileName(lowModelImgPath);
							}
						}
					}
				}

				float pixelSize = geodeLow->getBoundingBox().radius() * 2.f * lodRatio;
				osg::PagedLOD* lod = new osg::PagedLOD;
				lod->setCenter(geodeLow->getBoundingBox().center());
				lod->setRadius(geodeLow->getBoundingBox().radius());
				lod->setRangeMode(osg::PagedLOD::PIXEL_SIZE_ON_SCREEN);
				lod->setRange(0, 0, pixelSize);
				lod->setRange(1, pixelSize, 1e30);
				lod->addChild(geodeLow);
				lod->setFileName(1, highModelName);
				osgDB::writeNodeFile(*lod, dirData + lowModelRelativePath, pOptWrite);
				pProxyNode->setFileName(i, dirDataRelative + lowModelRelativePath);

				remove(lowModelImgPath.c_str());
				seed::progress::UpdateProgress(30 + (i + 1) * 65 / group->getNumChildren());
			}
			pProxyNode->setCenter(boundingBoxGlobal.center());
			pProxyNode->setRadius(boundingBoxGlobal.radius());
			pProxyNode->setLoadingExternalReferenceMode(osg::ProxyNode::LOAD_IMMEDIATELY);
			osgDB::writeNodeFile(*pProxyNode, mainName);
			std::string xmlName = output + "/metadata.xml";
			this->ExportSRS(xmlName);
			seed::progress::UpdateProgress(100);
			return true;
		}

		int MeshToOSG::ExportSRS(const std::string& i_cFilePath)
		{
			std::ofstream outfile(i_cFilePath);
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s!", i_cFilePath.c_str());
				return 0;
			}
			outfile << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
			outfile << "<ModelMetadata version=\"1\">\n";
			outfile << "	<SRS></SRS>\n";
			outfile << "	<SRSOrigin>0, 0, 0</SRSOrigin>\n";
			outfile << "	<Texture>\n";
			outfile << "		<ColorSource>Visible</ColorSource>\n";
			outfile << "	</Texture>\n";
			outfile << "</ModelMetadata>\n";
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "An error has occurred while writing file %s!", i_cFilePath.c_str());
				return 0;
			}
			return 1;
		}
	}
}