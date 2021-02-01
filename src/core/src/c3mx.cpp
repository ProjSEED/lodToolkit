#include "c3mx.h"

#include <osg/BoundingBox>
#include <osg/ref_ptr>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PagedLOD>
#include <osg/ProxyNode>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/MatrixTransform>
#include <osg/LineWidth>
#include <osg/Point>
#include <osgDB/ReaderWriter>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osgDB/XmlParser>
#include <osgDB/FileNameUtils>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "dxt_img.h"
#include "stb_image_write.h"
#include "openctm.h"
#include "CJsonObject.hpp"

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
			InfoVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

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

		struct Node3mx
		{
			std::string id;
			osg::BoundingBox bb;
			float maxScreenDiameter;
			std::vector<std::string> children;
			std::vector<std::string> resources;
		};

		struct Resource3mx
		{
			std::string type;
			std::string format;
			std::string id;

			std::string texture;
			float pointSize;
			osg::BoundingBox bb;

			std::vector<char> bufferData;
		};

		neb::CJsonObject NodeToJson(const Node3mx& node)
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

		neb::CJsonObject ResourceToJson(const Resource3mx& resource)
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
				else if (resource.format == "xyz")
				{
					oJson.Add("pointSize", resource.pointSize);
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

		bool Generate3mxb(const std::vector<Node3mx>& nodes, const std::vector<Resource3mx>& resourcesGeometry, const std::vector<Resource3mx>& resourcesTexture, const std::string& output)
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

		bool Generate3mxbRoot(const std::vector<std::string>& tileIds, const std::vector<std::string>& tileRelativePaths, const std::vector<osg::BoundingBox>& tileBBoxes, const std::string& output)
		{
			std::vector<Node3mx> nodes;
			std::vector<Resource3mx> resourcesGeometry;
			std::vector<Resource3mx> resourcesTexture;

			for (int i = 0; i < tileIds.size(); ++i)
			{
				Node3mx node;
				node.id = tileIds[i];
				node.maxScreenDiameter = 0;
				node.children.push_back(tileRelativePaths[i]);
				node.bb = tileBBoxes[i];
				nodes.push_back(node);
			}

			return Generate3mxb(nodes, resourcesGeometry, resourcesTexture, output);
		}

		bool Generate3mxMetadata(const std::string& output)
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

		bool Generate3mx(const std::string& srs, osg::Vec3d srsOrigin, osg::Vec3d offset, const std::string& outputDataRootRelative, const std::string& output)
		{
			neb::CJsonObject oJson;
			oJson.Add("3mxVersion", 1);
			oJson.Add("name", "Root");
			oJson.Add("description", "Generated by ProjSEED/lodToolkit, copyright <a href='https://github.com/ProjSEED/lodToolkit' target='_blank'>ProjSEED</a>.");
			oJson.Add("logo", "logo.png");

			neb::CJsonObject oJsonSceneOption;
			oJsonSceneOption.Add("navigationMode", "ORBIT");
			oJson.AddEmptySubArray("sceneOptions");
			oJson["sceneOptions"].Add(oJsonSceneOption);

			neb::CJsonObject oJsonLayer;
			oJsonLayer.Add("type", "meshPyramid");
			oJsonLayer.Add("id", "model0");
			oJsonLayer.Add("name", "Root");
			oJsonLayer.Add("description", "Converted by ProjSEED/lodToolkit, copyright <a href='https://github.com/ProjSEED/lodToolkit' target='_blank'>ProjSEED</a>.");
			oJsonLayer.Add("SRS", srs);
			oJsonLayer.AddEmptySubArray("SRSOrigin");
			oJsonLayer["SRSOrigin"].Add(srsOrigin.x());
			oJsonLayer["SRSOrigin"].Add(srsOrigin.y());
			oJsonLayer["SRSOrigin"].Add(srsOrigin.z());
			if (abs(offset.x()) > DBL_EPSILON || abs(offset.y()) > DBL_EPSILON || abs(offset.z()) > DBL_EPSILON)
			{
				oJsonLayer.AddEmptySubArray("offset");
				oJsonLayer["offset"].Add(offset.x());
				oJsonLayer["offset"].Add(offset.y());
				oJsonLayer["offset"].Add(offset.z());
			}
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

		int FindGeometryType(osg::Geometry* geometry)
		{
			if (geometry->getNumPrimitiveSets() == 0) {
				return -1;
			}
			int type = -1;
			for (unsigned int k = 0; k < geometry->getNumPrimitiveSets(); k++)
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

		osg::BoundingBox GeometryTriMeshToBuffer(const std::string& input, osg::Geometry* geometry, std::vector<char>& bufferData)
		{
			osg::BoundingBox bb;
			if (geometry->getNumPrimitiveSets() == 0) {
				return bb;
			}

			std::vector<CTMuint> aIndices;
			std::vector<CTMfloat> aVertices;
			std::vector<CTMfloat> aNormals;
			std::vector<CTMfloat> aUVCoords;

			// indc
			{
				int idx_size = 0;
				osg::PrimitiveSet::Type t_max = osg::PrimitiveSet::DrawElementsUBytePrimitiveType;
				for (unsigned int k = 0; k < geometry->getNumPrimitiveSets(); k++)
				{
					osg::PrimitiveSet* ps = geometry->getPrimitiveSet(k);
					osg::PrimitiveSet::Type t = ps->getType();
					if ((int)t > (int)t_max)
					{
						t_max = t;
					}
					idx_size += ps->getNumIndices();
				}

				for (unsigned int k = 0; k < geometry->getNumPrimitiveSets(); k++)
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
				bb.expandBy(point);
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
			if (aUVCoords.size())
			{
				ctm.AddUVMap(aUVCoords.data(), nullptr, nullptr);
			}
			ctm.SaveCustom(_ctm_write_buf, &bufferData);
			return bb;
		}

		unsigned char ColorFloatTo8Bits(float colorFloat)
		{
			int val = colorFloat * 255.f;
			if (val < 0) return 0;
			if (val > 255) return 255;
			return static_cast<unsigned char>(val);
		};

		osg::BoundingBox GeometryPointCloudToBuffer(const std::string& input, osg::Geometry* geometry, std::vector<char>& bufferData)
		{
			osg::BoundingBox bb;
			if (geometry->getNumPrimitiveSets() == 0) {
				return bb;
			}

			std::vector<float> aVertices;
			std::vector<unsigned char> aColors;

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
					bb.expandBy(point);
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
					aColors.push_back(ColorFloatTo8Bits(point.x()));
					aColors.push_back(ColorFloatTo8Bits(point.y()));
					aColors.push_back(ColorFloatTo8Bits(point.z()));
					aColors.push_back(ColorFloatTo8Bits(point.w()));
				}
			}

			if (vec_size == 0) {
				return bb;
			}
			if (vec_size != color_size) {
				return bb;
			}

			bufferData.insert(bufferData.end(), (char*)&vec_size, (char*)&vec_size + 4);
			bufferData.insert(bufferData.end(), (char*)aVertices.data(), (char*)aVertices.data() + sizeof(float) * aVertices.size());
			bufferData.insert(bufferData.end(), (char*)aColors.data(), (char*)aColors.data() + sizeof(unsigned char) * aColors.size());
			return bb;
		}

		void TextureToBuffer(const std::string& input, osg::Texture* texture, std::vector<char>& bufferData)
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

		void ParseGeode(const std::string& input, osg::Geode* geode, Node3mx& node, std::vector<Resource3mx>& resourcesGeometry, std::vector<Resource3mx>& resourcesTexture, osg::BoundingBox* pbb = nullptr)
		{
			osg::BoundingBox bb;

			node.maxScreenDiameter = 1e30;

			InfoVisitor infoVisitor;
			geode->accept(infoVisitor);
			std::map<osg::Texture*, std::string> texture_id_map;

			// handle texture
			for (auto tex : infoVisitor.texture_array)
			{
				Resource3mx resTexture;
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
					Resource3mx resGeometry;
					resGeometry.type = "geometryBuffer";
					resGeometry.format = "ctm";
					resGeometry.id = "geometry" + std::to_string(resourcesGeometry.size());
					if (infoVisitor.texture_map[g])
					{
						resGeometry.texture = texture_id_map[infoVisitor.texture_map[g]];
					}
					resGeometry.bb = GeometryTriMeshToBuffer(input, g, resGeometry.bufferData);
					bb.expandBy(resGeometry.bb);

					resourcesGeometry.emplace_back(resGeometry);
					node.resources.push_back(resGeometry.id);
				}
				else if (gl_type == 1) // point-cloud
				{
					Resource3mx resGeometry;
					resGeometry.type = "geometryBuffer";
					resGeometry.format = "xyz";
					resGeometry.id = "geometry" + std::to_string(resourcesGeometry.size());
					osg::Point* point = dynamic_cast<osg::Point*>(g->getOrCreateStateSet()->getAttribute(osg::StateAttribute::POINT));
					if (point)
					{
						resGeometry.pointSize = point->getSize();
					}
					else
					{
						resGeometry.pointSize = 10.0;
					}
					resGeometry.bb = GeometryPointCloudToBuffer(input, g, resGeometry.bufferData);
					bb.expandBy(resGeometry.bb);

					resourcesGeometry.emplace_back(resGeometry);
					node.resources.push_back(resGeometry.id);
				}
			}
			node.bb = bb;
			if (pbb)
			{
				*pbb = bb;
			}
		}

		void ParsePagedLOD(const std::string& input, osg::PagedLOD* lod, Node3mx& node, std::vector<Resource3mx>& resourcesGeometry, std::vector<Resource3mx>& resourcesTexture)
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

			osg::BoundingBox bb;
			if (lod->getNumChildren())
			{
				if (lod->getNumChildren() > 1)
				{
					seed::log::DumpLog(seed::log::Warning, "PagedLOD has more than 1 child in file %s, only the first child will be converted.", input.c_str());
				}
				osg::Geode* geode = lod->getChild(0)->asGeode();
				if (geode)
				{
					ParseGeode(input, geode, node, resourcesGeometry, resourcesTexture, &bb);
				}
			}

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

		void ParseGroup(const std::string& input, osg::Group* group, std::vector<Node3mx>& nodes, std::vector<Resource3mx>& resourcesGeometry, std::vector<Resource3mx>& resourcesTexture)
		{
			for (unsigned int i = 0; i < group->getNumChildren(); ++i)
			{
				Node3mx node;
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

		bool ConvertOsgbTo3mxb(osg::ref_ptr<osg::Node> osgNode, const std::string& output, osg::BoundingBox* pbb)
		{
			std::vector<Node3mx> nodes;
			std::vector<Resource3mx> resourcesGeometry;
			std::vector<Resource3mx> resourcesTexture;
			if (dynamic_cast<osg::PagedLOD*>(osgNode.get()))
			{
				osg::PagedLOD* lod = dynamic_cast<osg::PagedLOD*>(osgNode.get());

				Node3mx node;
				node.id = "node0";

				ParsePagedLOD(output, lod, node, resourcesGeometry, resourcesTexture);

				nodes.push_back(node);
			}
			else if (osgNode->asGeode())
			{
				osg::Geode* geode = osgNode->asGeode();

				Node3mx node;
				node.id = "node0";

				ParseGeode(output, geode, node, resourcesGeometry, resourcesTexture);

				nodes.push_back(node);
			}
			else if (osgNode->asGroup())
			{
				osg::Group* group = osgNode->asGroup();
				ParseGroup(output, group, nodes, resourcesGeometry, resourcesTexture);
			}
			else
			{
				Node3mx node;
				node.id = "node0";

				node.maxScreenDiameter = 1e30;

				nodes.push_back(node);
			}

			osg::BoundingBox bb;
			for (auto& node : nodes)
			{
				if (node.bb.valid())
				{
					bb.expandBy(node.bb);
				}
			}
			for (auto& node : nodes)
			{
				if (!node.bb.valid())
				{
					node.bb = bb;
				}
			}

			if (pbb && bb.valid())
			{
				*pbb = bb;
			}

			if (nodes.empty())
			{
				seed::log::DumpLog(seed::log::Warning, "Extract 0 node to %s", output.c_str());
				return false;
			}

			if (!Generate3mxb(nodes, resourcesGeometry, resourcesTexture, output))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output.c_str());
				return false;
			}

			return true;
		}
	}
}