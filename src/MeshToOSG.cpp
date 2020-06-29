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

#include "tinyxml.h" 

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

		bool MeshToOSG::Convert(const std::string& input, const std::string& output)
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
				osg::StateSet* stateset = geodeLow->getDrawable(0)->asGeometry()->getStateSet();
				if (stateset)
				{
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

				float pixelSize = geodeLow->getBoundingBox().radius() * 2.f * 10.f;
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
			}
			pProxyNode->setCenter(boundingBoxGlobal.center());
			pProxyNode->setRadius(boundingBoxGlobal.radius());
			pProxyNode->setLoadingExternalReferenceMode(osg::ProxyNode::DEFER_LOADING_TO_DATABASE_PAGER);
			osgDB::writeNodeFile(*pProxyNode, mainName);
			std::string xmlName = output + "/metadata.xml";
			this->ExportSRS(xmlName);
			return true;
		}

		int MeshToOSG::ExportSRS(const std::string& i_cFilePath)
		{
			TiXmlDocument xmlDoc(i_cFilePath.c_str());
			TiXmlDeclaration Declaration("1.0", "utf-8", "");
			xmlDoc.InsertEndChild(Declaration);

			TiXmlElement elmRoot("ModelMetadata");
			elmRoot.SetAttribute("Version", "1");

			utils::AddLeafNode(&elmRoot, "SRS", "");
			utils::AddLeafNode(&elmRoot, "SRSOrigin", "0, 0, 0");

			TiXmlElement * Texture = new TiXmlElement("Texture");
			elmRoot.LinkEndChild(Texture);
			utils::AddLeafNode(Texture, "ColorSource", "Visible");

			xmlDoc.InsertEndChild(elmRoot);
			xmlDoc.SaveFile();
			return 1;
		}
	}
}