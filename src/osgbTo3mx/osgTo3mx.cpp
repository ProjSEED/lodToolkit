#include "osgTo3mx.h"

#include <execution>
#include <mutex>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "dxt_img.h"
#include "stb_image_write.h"
#include "openctm.h"

namespace seed
{
	namespace io
	{
		void write_buf(void* context, void* data, int len) {
			std::vector<char>* buf = (std::vector<char>*)context;
			buf->insert(buf->end(), (char*)data, (char*)data + len);
		}

		static CTMuint CTMCALL _ctm_write_buf(const void * aBuf, CTMuint aCount, void * aUserData)
		{
			std::vector<char>* buf = (std::vector<char>*)aUserData;
			buf->insert(buf->end(), (char*)aBuf, (char*)aBuf + aCount);
			return aCount;
		}

		class InfoVisitor : public osg::NodeVisitor
		{
			std::string path;
		public:
			InfoVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)	{}

			~InfoVisitor() {}

			void apply(osg::Geometry& geometry) {
				geometry_array.push_back(&geometry);
				if (auto ss = geometry.getStateSet()) {
					osg::Texture* tex = dynamic_cast<osg::Texture*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
					if (tex) {
						texture_array.insert(tex);
						texture_map[&geometry] = tex;
					}
				}
			}

		public:
			std::vector<osg::Geometry*> geometry_array;
			std::set<osg::Texture*> texture_array;
			std::map<osg::Geometry*, osg::Texture*> texture_map;
		};

		bool OsgTo3mx::Convert(const std::string& input, const std::string& output)
		{
			std::string inputMetadata = input + "/metadata.xml";
			std::string inputData = input + "/Data/";
			
			std::string outputMetadata = output + "/metadata.xml";
			std::string output3mx = output + "/Root.3mx";
			std::string outputData = output + "/Data/";
			std::string outputDataRootRelative = "Data/Root.3mxb";
			std::string outputDataRoot = output + "/" + outputDataRootRelative;

			seed::progress::UpdateProgress(0);
			if (!utils::CheckOrCreateFolder(output))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", output.c_str());
				return false;
			}
			if (!utils::CheckOrCreateFolder(outputData))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", outputData.c_str());
				return false;
			}

			if (!GenerateMetadata(outputMetadata))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputMetadata.c_str());
				return false;
			}

			if (!ConvertMetadataTo3mx(inputMetadata, outputDataRootRelative, output3mx))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output3mx.c_str());
				return false;
			}

			osgDB::DirectoryContents fileNames = osgDB::getDirectoryContents(inputData);

			std::vector<Node> nodes;
			std::vector<Resource> resourcesGeometry;
			std::vector<Resource> resourcesTexture;

			int processed = 0;
			int percent = -1;

			for each (std::string dir in fileNames)
			{
				if (dir.find(".") != std::string::npos)
					continue;

				std::string output3mxbName = dir + "/" + dir + ".3mxb";
				std::string output3mxb = outputData + output3mxbName;
				osg::BoundingBox bb;
				if (!ConvertTile(inputData, outputData, dir, bb))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output3mxb.c_str());
					return false;
				}
				if (bb.valid())
				{
					Node node;
					node.id = dir;
					node.bb = bb;
					node.maxScreenDiameter = 0;
					node.children.push_back(output3mxbName);
					nodes.push_back(node);
				}

				{
					processed++;
					int cur = processed * 100 / fileNames.size();
					if (cur > percent)
					{
						seed::progress::UpdateProgress(cur);
						percent = cur;
					}
				}
			}

			if (nodes.empty())
			{
				seed::log::DumpLog(seed::log::Warning, "Extract 0 node from %s", inputData.c_str());
				return false;
			}

			if (!Generate3mxb(nodes, resourcesGeometry, resourcesTexture, outputDataRoot))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputDataRoot.c_str());
				return false;
			}
			seed::progress::UpdateProgress(100);
			return true;
		}

		bool OsgTo3mx::GenerateMetadata(const std::string& output)
		{
			std::ofstream outfile(output);
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s!", output.c_str());
				return false;
			}
			outfile << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
			outfile << "<ModelMetadata version=\"1\">\n";
			outfile << "	<Texture>\n";
			outfile << "		<ColorSource>Visible</ColorSource>\n";
			outfile << "	</Texture>\n";
			outfile << "</ModelMetadata>\n";
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "An error has occurred while writing file %s!", output.c_str());
				return false;
			}
			return true;
		}

		bool OsgTo3mx::ConvertMetadataTo3mx(const std::string& input, const std::string& outputDataRootRelative, const std::string& output)
		{
			std::string srs;
			osg::Vec3d srsOrigin(0, 0, 0);
			auto xml = osgDB::readXmlFile(input);
			if (xml)
			{
				for (auto i : xml->children)
				{
					if (i->name == "ModelMetadata")
					{
						for (auto j : i->children)
						{
							if (j->name == "SRS")
							{
								srs = j->contents;
							}
							if (j->name == "SRSOrigin")
							{
								sscanf(j->contents.c_str(), "%lf,%lf,%lf", &srsOrigin._v[0], &srsOrigin._v[1], &srsOrigin._v[2]);
							}
						}
						break;
					}
				}
			}
			else
			{
				seed::log::DumpLog(seed::log::Warning, "Can NOT open file %s!", input.c_str());
			}

			neb::CJsonObject oJson;
			oJson.Add("3mxVersion", 1);
			oJson.Add("name", "Root");
			oJson.Add("description", "Converted by ProjSEED/To3mx, copyright <a href='https://github.com/ProjSEED/To3mx' target='_blank'>ProjSEED</a>.");
			oJson.Add("logo", "logo.png");

			neb::CJsonObject oJsonSceneOption;
			oJsonSceneOption.Add("navigationMode", "ORBIT");
			oJson.AddEmptySubArray("sceneOptions");
			oJson["sceneOptions"].Add(oJsonSceneOption);

			neb::CJsonObject oJsonLayer;
			oJsonLayer.Add("type", "meshPyramid");
			oJsonLayer.Add("id", "mesh0");
			oJsonLayer.Add("name", "Root");
			oJsonLayer.Add("description", "Converted by ProjSEED/To3mx, copyright <a href='https://github.com/ProjSEED/To3mx' target='_blank'>ProjSEED</a>.");
			oJsonLayer.Add("SRS", srs);
			oJsonLayer.AddEmptySubArray("SRSOrigin");
			oJsonLayer["SRSOrigin"].Add(srsOrigin.x());
			oJsonLayer["SRSOrigin"].Add(srsOrigin.y());
			oJsonLayer["SRSOrigin"].Add(srsOrigin.z());
			oJsonLayer.Add("root", outputDataRootRelative);
			oJson.AddEmptySubArray("layers");
			oJson["layers"].Add(oJsonLayer);

			std::ofstream outfile(output);
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s!", output.c_str());
				return false;
			}
			outfile << oJson.ToFormattedString();
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "An error has occurred while writing file %s!", output.c_str());
				return false;
			}
			return true;
		}

		bool OsgTo3mx::ConvertTile(const std::string& inputData, const std::string& outputData, const std::string& tileName, osg::BoundingBox& bb)
		{
			std::string inputTile = inputData + tileName + "/";
			std::string outputTile = outputData + tileName + "/";
			if (!utils::CheckOrCreateFolder(outputTile))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", outputTile.c_str());
				return false;
			}
			// top level
			{
				std::string inputOsgb = inputTile + tileName + ".osgb";
				std::string output3mxb = outputTile + tileName + ".3mxb";
				if (!ConvertOsgbTo3mxb(inputOsgb, output3mxb, &bb))
				{
					seed::log::DumpLog(seed::log::Critical, "Convert %s failed!", inputOsgb.c_str());
					return false;
				}
			}
			// all other
			osgDB::DirectoryContents fileNames = osgDB::getDirectoryContents(inputTile);
			osgDB::DirectoryContents osgbFileNames;
			std::vector<int> indices;
			std::vector<int> flags;
			int count = 0;
			for each (std::string file in fileNames)
			{
				std::string ext = osgDB::getLowerCaseFileExtension(file);
				if (ext != "osgb")
					continue;

				std::string baseName = osgDB::getNameLessExtension(file);
				if (baseName == tileName)
					continue;

				osgbFileNames.push_back(file);
				indices.push_back(count);
				flags.push_back(0);
				count++;
			}
			seed::log::DumpLog(seed::log::Debug, "Found %d files in %s...", osgbFileNames.size(), inputTile.c_str());

#if _HAS_CXX17 && !_DEBUG
			std::for_each(std::execution::par_unseq, std::begin(indices), std::end(indices), [&](int i)
#else
			std::for_each(std::begin(indices), std::end(indices), [&](int i)
#endif
				{
					std::string file = osgbFileNames[i];
					std::string baseName = osgDB::getNameLessExtension(file);
					std::string inputOsgb = inputTile + baseName + ".osgb";
					std::string output3mxb = outputTile + baseName + ".3mxb";

					if (!ConvertOsgbTo3mxb(inputOsgb, output3mxb))
					{
						flags[i] = 1;
						seed::log::DumpLog(seed::log::Critical, "Convert %s failed!", inputOsgb.c_str());
					}
				}
			);
			for (int i = 0; i < flags.size(); ++i)
			{
				if (flags[i])
				{
					return false;
				}
			}
			return true;
		}

		void OsgTo3mx::ParsePagedLOD(const std::string& input, osg::PagedLOD* lod, Node& node, std::vector<Resource>& resourcesGeometry, std::vector<Resource>& resourcesTexture)
		{
			if (lod->getNumFileNames() >= 2)
			{
				std::string baseName = osgDB::getNameLessExtension(lod->getFileName(1));
				node.children.push_back(baseName + ".3mxb");
			}
			else if (lod->getRangeList().size() == 1)
			{
				std::string baseName = osgDB::getNameLessExtension(lod->getFileName(0));
				node.children.push_back(baseName + ".3mxb");
			}
			
			if (lod->getNumChildren())
			{
				if (lod->getNumChildren() > 1)
				{
					seed::log::DumpLog(seed::log::Warning, "PagedLOD has more than 1 child in file %s, only the first child will be converted.", input.c_str());
				}
				osg::Geode* geode = lod->getChild(0)->asGeode();
				if (geode)
				{
					ParseGeode(input, geode, node, resourcesGeometry, resourcesTexture);
				}
			}

			osg::BoundingBox bb;
			bb.expandBy(lod->getBound());
			node.bb = bb;
			if (lod->getRangeList().size() >= 2)
			{
				node.maxScreenDiameter = lod->getRangeList()[1].first;
			}
			else if (lod->getRangeList().size() == 1)
			{
				node.maxScreenDiameter = lod->getRangeList()[0].first;
			}
			else
			{
				node.maxScreenDiameter = 0;
			}
		}

		void OsgTo3mx::ParseGeode(const std::string& input, osg::Geode* geode, Node& node, std::vector<Resource>& resourcesGeometry, std::vector<Resource>& resourcesTexture)
		{
			osg::BoundingBox bb;
			bb.expandBy(geode->getBound());

			node.bb = bb;
			node.maxScreenDiameter = 1e30;
			
			InfoVisitor infoVisitor;
			geode->accept(infoVisitor);
			std::map<osg::Texture*, std::string> texture_id_map;

			// handle texture
			for (auto tex : infoVisitor.texture_array)
			{
				Resource resTexture;
				resTexture.type = "textureBuffer";
				resTexture.format = "jpg";
				resTexture.id = "texture" + std::to_string(resourcesTexture.size());
				texture_id_map[tex] = resTexture.id;
				TextureToBuffer(input, tex, resTexture.bufferData);

				resourcesTexture.emplace_back(resTexture);
			}

			// handle geometry
			for (auto g : infoVisitor.geometry_array)
			{
				int gl_type = FindGeometryType(g);
				if (gl_type == 0) // tri-mesh
				{
					Resource resGeometry;
					resGeometry.type = "geometryBuffer";
					resGeometry.format = "ctm";
					resGeometry.id = "geometry" + std::to_string(resourcesGeometry.size());
					if (infoVisitor.texture_map[g])
					{
						resGeometry.texture = texture_id_map[infoVisitor.texture_map[g]];
					}
					resGeometry.bb = bb;
					GeometryTriMeshToBuffer(input, g, resGeometry.bufferData);

					resourcesGeometry.emplace_back(resGeometry);
					node.resources.push_back(resGeometry.id);
				}
				else if (gl_type == 1) // point-cloud
				{
					Resource resGeometry;
					resGeometry.type = "geometryBuffer";
					resGeometry.format = "xyz";
					resGeometry.id = "geometry" + std::to_string(resourcesGeometry.size());
					resGeometry.bb = bb;
					GeometryPointCloudToBuffer(input, g, resGeometry.bufferData);

					resourcesGeometry.emplace_back(resGeometry);
					node.resources.push_back(resGeometry.id);
				}
			}
		}

		void OsgTo3mx::ParseGroup(const std::string& input, osg::Group* group, std::vector<Node>& nodes, std::vector<Resource>& resourcesGeometry, std::vector<Resource>& resourcesTexture)
		{
			for (uint32 i = 0; i < group->getNumChildren(); ++i)
			{
				Node node;
				node.id = "node" + std::to_string(nodes.size());
				if (dynamic_cast<osg::PagedLOD*>(group->getChild(i)))
				{
					osg::PagedLOD* lod = dynamic_cast<osg::PagedLOD*>(group->getChild(i));
					ParsePagedLOD(input, lod, node, resourcesGeometry, resourcesTexture);
					nodes.push_back(node);
				}
				else if (group->getChild(i)->asGeode())
				{
					osg::Geode* geode = group->getChild(i)->asGeode();
					ParseGeode(input, geode, node, resourcesGeometry, resourcesTexture);
					nodes.push_back(node);
				}
				else if (group->getChild(i)->asGroup())
				{
					osg::Group* subGroup = group->getChild(i)->asGroup();
					ParseGroup(input, subGroup, nodes, resourcesGeometry, resourcesTexture);
				}
				else
				{
					node.maxScreenDiameter = 1e30;
					nodes.push_back(node);
				}
			}
		}

		bool OsgTo3mx::ConvertOsgbTo3mxb(const std::string& input, const std::string& output, osg::BoundingBox* pbb)
		{
			seed::log::DumpLog(seed::log::Debug, "Convert %s ...", input.c_str());
			std::vector<Node> nodes;
			std::vector<Resource> resourcesGeometry;
			std::vector<Resource> resourcesTexture;
			osg::ref_ptr<osg::Node> osgNode = osgDB::readNodeFile(input);
			if (dynamic_cast<osg::PagedLOD*>(osgNode.get()))
			{
				osg::PagedLOD* lod = dynamic_cast<osg::PagedLOD*>(osgNode.get());

				Node node;
				node.id = "node0";

				ParsePagedLOD(input, lod, node, resourcesGeometry, resourcesTexture);

				nodes.push_back(node);
			}
			else if (osgNode->asGeode())
			{
				osg::Geode* geode = osgNode->asGeode();
				osg::BoundingBox bb;
				bb.expandBy(geode->getBound());

				Node node;
				node.id = "node0";

				ParseGeode(input, geode, node, resourcesGeometry, resourcesTexture);

				nodes.push_back(node);
			}
			else if (osgNode->asGroup())
			{
				osg::Group* group = osgNode->asGroup();
				ParseGroup(input, group, nodes, resourcesGeometry, resourcesTexture);
			}
			else
			{
				Node node;
				node.id = "node0";

				node.maxScreenDiameter = 1e30;

				nodes.push_back(node);
			}

			if (pbb && nodes.size())
			{
				*pbb = nodes[0].bb;
			}

			if (nodes.empty())
			{
				seed::log::DumpLog(seed::log::Warning, "Extract 0 node from %s", input.c_str());
				return false;
			}

			if(!Generate3mxb(nodes, resourcesGeometry, resourcesTexture, output))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output.c_str());
				return false;
			}

			return true;
		}

		bool OsgTo3mx::Generate3mxb(const std::vector<Node>& nodes, const std::vector<Resource>& resourcesGeometry, const std::vector<Resource>& resourcesTexture, const std::string& output)
		{
			neb::CJsonObject oJson;
			oJson.Add("version", 1);

			oJson.AddEmptySubArray("nodes");
			for (const auto& node : nodes)
			{
				oJson["nodes"].Add(NodeToJson(node));
			}

			oJson.AddEmptySubArray("resources");
			for (const auto& resource : resourcesTexture)
			{
				oJson["resources"].Add(ResourceToJson(resource));
			}
			for (const auto& resource : resourcesGeometry)
			{
				oJson["resources"].Add(ResourceToJson(resource));
			}

			std::string jsonStr = oJson.ToString();
			uint32_t length = jsonStr.size();

			std::ofstream outfile(output, std::ios::out | std::ios::binary);
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s!", output.c_str());
				return false;
			}

			outfile.write("3MXBO", 5);
			outfile.write((char*)&length, 4);
			outfile.write(jsonStr.c_str(), length);

			for (const auto& resource : resourcesTexture)
			{
				outfile.write(resource.bufferData.data(), resource.bufferData.size());
			}
			for (const auto& resource : resourcesGeometry)
			{
				outfile.write(resource.bufferData.data(), resource.bufferData.size());
			}

			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "An error has occurred while writing file %s!", output.c_str());
				return false;
			}
			return true;
		}

		neb::CJsonObject OsgTo3mx::NodeToJson(const Node& node)
		{
			neb::CJsonObject oJson;
			oJson.Add("id", node.id);

			oJson.AddEmptySubArray("bbMin");
			oJson["bbMin"].Add(node.bb.xMin());
			oJson["bbMin"].Add(node.bb.yMin());
			oJson["bbMin"].Add(node.bb.zMin());

			oJson.AddEmptySubArray("bbMax");
			oJson["bbMax"].Add(node.bb.xMax());
			oJson["bbMax"].Add(node.bb.yMax());
			oJson["bbMax"].Add(node.bb.zMax());

			oJson.Add("maxScreenDiameter", node.maxScreenDiameter);

			oJson.AddEmptySubArray("children");
			for (auto child : node.children)
			{
				oJson["children"].Add(child);
			}

			oJson.AddEmptySubArray("resources");
			for (auto resource : node.resources)
			{
				oJson["resources"].Add(resource);
			}
			return oJson;
		}

		neb::CJsonObject OsgTo3mx::ResourceToJson(const Resource& resource)
		{
			neb::CJsonObject oJson;
			oJson.Add("type", resource.type);
			oJson.Add("format", resource.format);
			oJson.Add("id", resource.id);
			if (resource.type == "geometryBuffer")
			{
				if (resource.format == "ctm")
				{
					oJson.Add("texture", resource.texture);
				}

				oJson.AddEmptySubArray("bbMin");
				oJson["bbMin"].Add(resource.bb.xMin());
				oJson["bbMin"].Add(resource.bb.yMin());
				oJson["bbMin"].Add(resource.bb.zMin());

				oJson.AddEmptySubArray("bbMax");
				oJson["bbMax"].Add(resource.bb.xMax());
				oJson["bbMax"].Add(resource.bb.yMax());
				oJson["bbMax"].Add(resource.bb.zMax());
			}
			oJson.Add("size", resource.bufferData.size());
			return oJson;
		}

		int OsgTo3mx::FindGeometryType(osg::Geometry* geometry)
		{
			if (geometry->getNumPrimitiveSets() == 0) {
				return -1;
			}
			int type = -1;
			for (uint32 k = 0; k < geometry->getNumPrimitiveSets(); k++)
			{
				osg::PrimitiveSet* ps = geometry->getPrimitiveSet(k);
				osg::PrimitiveSet::Type t = ps->getType();
				auto mode = ps->getMode();
				if (mode == GL_TRIANGLES)
				{
					if (k == 0)
					{
						type = 0;
					}
					else if (type != 0)
					{
						type = -1;
					}
				}
				else if (mode == GL_POINTS)
				{
					if (k == 0)
					{
						type = 1;
					}
					else if (type != 1)
					{
						type = -1;
					}
				}
				else
				{
					type = -1;
				}
			}
			return type;
		}

		void OsgTo3mx::GeometryTriMeshToBuffer(const std::string& input, osg::Geometry* geometry, std::vector<char>& bufferData)
		{
			if (geometry->getNumPrimitiveSets() == 0) {
				return;
			}

			std::vector<CTMuint> aIndices;
			std::vector<CTMfloat> aVertices;
			std::vector<CTMfloat> aNormals;
			std::vector<CTMfloat> aUVCoords;

			// indc
			{
				int idx_size = 0;
				osg::PrimitiveSet::Type t_max = osg::PrimitiveSet::DrawElementsUBytePrimitiveType;
				for (uint32 k = 0; k < geometry->getNumPrimitiveSets(); k++)
				{
					osg::PrimitiveSet* ps = geometry->getPrimitiveSet(k);
					osg::PrimitiveSet::Type t = ps->getType();
					if ((int)t > (int)t_max)
					{
						t_max = t;
					}
					idx_size += ps->getNumIndices();
				}

				for (uint32 k = 0; k < geometry->getNumPrimitiveSets(); k++)
				{
					osg::PrimitiveSet* ps = geometry->getPrimitiveSet(k);
					osg::PrimitiveSet::Type t = ps->getType();
					auto mode = ps->getMode();
					if (mode != GL_TRIANGLES) {
						seed::log::DumpLog(seed::log::Warning, "Found none-GL_TRIANGLES primitive set in file %s, none-GL_TRIANGLES primitive set will be ignored.", input.c_str());
						continue;
					}

					switch (t)
					{
					case(osg::PrimitiveSet::DrawElementsUBytePrimitiveType):
					{
						const osg::DrawElementsUByte* drawElements = static_cast<const osg::DrawElementsUByte*>(ps);
						int IndNum = drawElements->getNumIndices();
						for (size_t m = 0; m < IndNum; m++)
						{
							aIndices.push_back(drawElements->at(m));
						}
						break;
					}
					case(osg::PrimitiveSet::DrawElementsUShortPrimitiveType):
					{
						const osg::DrawElementsUShort* drawElements = static_cast<const osg::DrawElementsUShort*>(ps);
						int IndNum = drawElements->getNumIndices();
						for (size_t m = 0; m < IndNum; m++)
						{
							aIndices.push_back(drawElements->at(m));
						}
						break;
					}
					case(osg::PrimitiveSet::DrawElementsUIntPrimitiveType):
					{
						const osg::DrawElementsUInt* drawElements = static_cast<const osg::DrawElementsUInt*>(ps);
						unsigned int IndNum = drawElements->getNumIndices();
						for (size_t m = 0; m < IndNum; m++)
						{
							aIndices.push_back(drawElements->at(m));
						}
						break;
					}
					case osg::PrimitiveSet::DrawArraysPrimitiveType: {
						osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(ps);
						if (k == 0) {
							int first = da->getFirst();
							int count = da->getCount();
							int max_num = first + count;
							if (max_num >= 65535) {
								max_num = 65535; idx_size = 65535;
							}
							for (int i = first; i < max_num; i++) {
								aIndices.push_back(i);
							}
						}
						break;
					}
					default:
					{
						seed::log::DumpLog(seed::log::Critical, "Found un-handled osg::PrimitiveSet::Type [%d] in file %s", t, input.c_str());
						break;
					}
					}
				}
			}
			osg::Array* va = geometry->getVertexArray();
			osg::Vec3Array* v3f = (osg::Vec3Array*)va;
			int vec_size = v3f->size();
			for (int vidx = 0; vidx < vec_size; vidx++)
			{
				osg::Vec3f point = v3f->at(vidx);
				aVertices.push_back(point.x());
				aVertices.push_back(point.y());
				aVertices.push_back(point.z());
			}
			// normal
			int normal_size = 0;
			osg::Array* na = geometry->getNormalArray();
			if (na)
			{
				osg::Vec3Array* v3f = (osg::Vec3Array*)na;
				normal_size = v3f->size();
				for (int vidx = 0; vidx < normal_size; vidx++)
				{
					osg::Vec3f point = v3f->at(vidx);
					aNormals.push_back(point.x());
					aNormals.push_back(point.y());
					aNormals.push_back(point.z());
				}
			}
			// texture
			int texture_size = 0;
			osg::Array* ta = geometry->getTexCoordArray(0);
			if (ta) {
				osg::Vec2Array* v2f = (osg::Vec2Array*)ta;
				texture_size = v2f->size();
				for (int vidx = 0; vidx < texture_size; vidx++)
				{
					osg::Vec2f point = v2f->at(vidx);
					aUVCoords.push_back(point.x());
					aUVCoords.push_back(point.y());
				}
			}

			CTMexporter ctm;
			ctm.DefineMesh(aVertices.data(), aVertices.size() / 3, aIndices.data(), aIndices.size() / 3, aNormals.data());
			ctm.AddUVMap(aUVCoords.data(), nullptr, nullptr);
			ctm.SaveCustom(_ctm_write_buf, &bufferData);
		}

		void OsgTo3mx::GeometryPointCloudToBuffer(const std::string& input, osg::Geometry* geometry, std::vector<char>& bufferData)
		{
			if (geometry->getNumPrimitiveSets() == 0) {
				return;
			}

			std::vector<float> aVertices;
			std::vector<char> aColors;

			osg::Array* va = geometry->getVertexArray();
			int vec_size = 0;
			if (va)
			{
				osg::Vec3Array* v3f = (osg::Vec3Array*)va;
				vec_size = v3f->size();
				for (int vidx = 0; vidx < vec_size; vidx++)
				{
					osg::Vec3f point = v3f->at(vidx);
					aVertices.push_back(point.x());
					aVertices.push_back(point.y());
					aVertices.push_back(point.z());
				}
			}

			// color
			osg::Array* ca = geometry->getColorArray();
			int color_size;
			if (ca)
			{
				osg::Vec4Array* v4f = (osg::Vec4Array*)ca;
				color_size = v4f->size();
				for (int vidx = 0; vidx < color_size; vidx++)
				{
					osg::Vec4f point = v4f->at(vidx);
					aColors.push_back(char(point.x() * 255));
					aColors.push_back(char(point.y() * 255));
					aColors.push_back(char(point.z() * 255));
					aColors.push_back(char(point.w() * 255));
				}
			}

			if (vec_size == 0) {
				return;
			}
			if (vec_size != color_size) {
				return;
			}

			bufferData.insert(bufferData.end(), (char*)&vec_size, (char*)&vec_size + 4);
			bufferData.insert(bufferData.end(), (char*)aVertices.data(), (char*)aVertices.data() + sizeof(float) * aVertices.size());
			bufferData.insert(bufferData.end(), (char*)aColors.data(), (char*)aColors.data() + sizeof(char) * aColors.size());
		}

		void OsgTo3mx::TextureToBuffer(const std::string& input, osg::Texture* texture, std::vector<char>& bufferData)
		{
			std::vector<unsigned char> jpeg_buf;
			jpeg_buf.reserve(512 * 512 * 3);
			int width, height, comp;
			{
				if (texture) {
					if (texture->getNumImages() > 0) {
						osg::Image* img = texture->getImage(0);
						if (img) {
							width = img->s();
							height = img->t();
							comp = img->getPixelSizeInBits();
							if (comp == 8) comp = 1;
							if (comp == 24) comp = 3;
							if (comp == 4) {
								comp = 3;
								fill_4BitImage(jpeg_buf, img, width, height);
							}
							else
							{
								unsigned row_step = img->getRowStepInBytes();
								unsigned row_size = img->getRowSizeInBytes();
								for (int i = height - 1; i >= 0; i--)
								{
									jpeg_buf.insert(jpeg_buf.end(),
										img->data() + row_step * i,
										img->data() + row_step * i + row_size);
								}
								//for (size_t i = 0; i < height; i++)
								//{
								//	jpeg_buf.insert(jpeg_buf.end(),
								//		img->data() + row_step * i,
								//		img->data() + row_step * i + row_size);
								//}
							}
						}
					}
				}
			}
			if (!jpeg_buf.empty()) {
				bufferData.reserve(width * height * comp);
				stbi_write_jpg_to_func(write_buf, &bufferData, width, height, comp, jpeg_buf.data(), 80);
			}
			else {
				std::vector<char> v_data;
				width = height = 256;
				v_data.resize(width * height * 3);
				stbi_write_jpg_to_func(write_buf, &bufferData, width, height, 3, v_data.data(), 80);
			}
		}
	}
}