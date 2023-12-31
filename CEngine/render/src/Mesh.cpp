#include "../includes/Mesh.h"
#include "imgui.h"
#include "../core/includes/CXM.h"
#include <unordered_map>
#include <memory>
#include <libloaderapi.h>


std::string operator-(std::string source, const std::string& target)
{
	for(size_t pos, size{target.size() }; (pos = source.find(target)) != std::string::npos;)
		source.erase(source.cbegin() + pos,source.cbegin() + (pos + size));
	return source;
}



Mesh::Mesh(Graphics& gfx, std::vector<std::unique_ptr<Bindable>> bindPtrs)
{
	if (!IsStaticInitialized())
	{
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}

	for (auto& pb : bindPtrs)
	{
		if (auto pi = dynamic_cast<IndexBuffer*>(pb.get()))
		{
			AddIndexBuffer(std::unique_ptr<IndexBuffer>{ pi });
			pb.release();
		}
		else
		{
			AddBind(std::move(pb));
		}
	}

	AddBind(std::make_unique<TransformCbuf>(gfx, *this));
}


void Mesh::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const
{
	DirectX::XMStoreFloat4x4(&transform, accumulatedTransform);
	Drawable::Draw(gfx);
}


DirectX::XMMATRIX Mesh::GetTransformXM() const noexcept
{
	return DirectX::XMLoadFloat4x4(&transform);
}



Node::Node(int id, const std::string& name, std::vector<Mesh*> meshPtrs, const DirectX::XMMATRIX& transform)
	:
id(id), meshPtrs(std::move(meshPtrs)) , name(name)
{
	DirectX::XMStoreFloat4x4(&baseTransform, transform);
	DirectX::XMStoreFloat4x4(&appliedTransform, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&appliedScale, DirectX::XMMatrixIdentity());
}


void Node::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const 
{
	const auto built =
		DirectX::XMLoadFloat4x4(&appliedTransform) *
		DirectX::XMLoadFloat4x4(&baseTransform) *
		accumulatedTransform;
	for (const auto pm : meshPtrs)
	{
		pm->Draw(gfx, built);
	}
	for (const auto& pc : childPtrs)
	{
		pc->Draw(gfx, built);
	}
}


void Node::AddChild(std::unique_ptr<Node> pChild) 
{
	assert(pChild);
	childPtrs.push_back(std::move(pChild));
}


void Node::RenderTree(Node*& pSelectedNode) const noexcept
{
	const int selectedId = (pSelectedNode == nullptr) ? -1 : pSelectedNode->GetId();
	const auto node_flags = ImGuiTreeNodeFlags_NavLeftJumpsBackHere | ImGuiTreeNodeFlags_OpenOnArrow
		| ((GetId() == selectedId) ? ImGuiTreeNodeFlags_Selected : 0)
		| ((childPtrs.size() == 0) ? ImGuiTreeNodeFlags_Leaf : 0);
	const auto expanded = ImGui::TreeNodeEx((void*)(intptr_t)GetId(), node_flags, name.c_str());
	
		if (ImGui::IsItemClicked() || ImGui::IsItemActivated() || ImGui::IsItemFocused())
		{
			pSelectedNode = const_cast<Node*>(this);
		}
		if (expanded)
		{
			for (const auto& pChild : childPtrs)
			{
				pChild->RenderTree(pSelectedNode);
			}
			ImGui::TreePop();
		}
}

const std::vector<std::unique_ptr<Node>>& Node::getchildPtrs() const noexcept
{
	return childPtrs;
}



void Node::SetAppliedTransform(DirectX::FXMMATRIX transform) noexcept
{
	DirectX::XMStoreFloat4x4(&appliedTransform, transform);
}

void Node::SetAppliedScale(DirectX::FXMMATRIX scale) noexcept
{
	DirectX::XMStoreFloat4x4(&appliedScale, scale);
}



int Node::GetId() const noexcept
{
	return id;
}



std::string Node::GetName() const noexcept
{
	return name;
}




Model::Model(Graphics& gfx, const std::string& fileName, int id, std::string modelName) :
	modelName(modelName), id(id), rootPath(fileName)
{
	Assimp::Importer imp;
	const auto pScene = imp.ReadFile(fileName.c_str(),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_ConvertToLeftHanded |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace);
	if (pScene == NULL)
	{
		MessageBoxA(nullptr, imp.GetErrorString(), "Standart Exception", MB_OK | MB_ICONEXCLAMATION);
	}
	for (size_t i = 0; i < pScene->mNumMeshes; i++)
	{
		meshPtrs.push_back(ParseMesh(gfx, *pScene->mMeshes[i], pScene->mMaterials, fileName));
	}
	int nextId = 0;
	pRoot = ParseNode(nextId, *pScene->mRootNode);
	CUBE_TRACE(std::string("Successfully loaded model") + fileName);
}

Node& Model::getpRoot()
{
	return *pRoot;
}


void Model::Draw(Graphics& gfx) const
{
	if (auto node = GetSelectedNode())
	{
		node->SetAppliedTransform(GetTransform());
		node->SetAppliedScale(GetScale());
	}
	pRoot->Draw(gfx, DirectX::XMMatrixIdentity());
}


void Model::ShowWindow(Graphics& gfx, Model* pSelectedModel) noexcept
{
	int nodeIndexTracker = 0;

	if (pSelectedModel == this)
	{
		pRoot->RenderTree(pSelectedNode);
		if (pSelectedNode != nullptr)
		{
			ImGui::SetNextWindowSize({ 400, 340 }, ImGuiCond_FirstUseEver);
			ImGui::Begin("Proprieties");


			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), modelName.c_str());
			if (ImGui::InputText("Model Name", buffer, sizeof(buffer)))
			{
				modelName = std::string(buffer);
			}

			const auto id = pSelectedNode->GetId();
			auto i = poses.find(id);
			auto j = scales.find(id);
			if (i == poses.end())
			{
				const auto& applied = pSelectedNode->GetAppliedTransform();
				const auto& appliedScale = pSelectedNode->GetAppliedScale();
				const auto angles = ExtractEulerAngles(applied);
				const auto translation = ExtractTranslation(applied);
				TransformParameters tp;
				tp.roll = angles.x;
				tp.pitch = angles.y;
				tp.yaw = angles.z;
				tp.x = translation.x;
				tp.y = translation.y;
				tp.z = translation.z;
				std::tie(i, std::ignore) = poses.insert({ id,tp });
			}
			auto& pos = i->second;

			if (j == scales.end())
			{
				const auto& appliedScale = pSelectedNode->GetAppliedScale();
				const auto nodescales = ExtractScaling(appliedScale);
				ScaleParameters sc; 
				sc.xscale = nodescales.x; 
				sc.yscale = nodescales.y; 
				sc.zscale = nodescales.z; 
				std::tie(j, std::ignore) = scales.insert({ id, sc }); 
			}
			auto& scale = j->second;

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::Text("Position");
			ImGui::PushID("Position");
			ImGui::NextColumn();
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.2f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });

			if (ImGui::Button("X", buttonSize))
				pos.x = 0.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::DragFloat("##X", &pos.x, 0.1f);
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });

			if (ImGui::Button("Y", buttonSize))
				pos.y = 0.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::DragFloat("##Y", &pos.y, 0.1f);
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });

			if (ImGui::Button("Z", buttonSize))
				pos.z = 0.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::DragFloat("##Z", &pos.z, 0.1f);
			ImGui::PopItemWidth();
			ImGui::Columns(1);
			ImGui::PopStyleVar();
			ImGui::PopID();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::Text("Scale");
			ImGui::PushID("Scale");
			ImGui::NextColumn();
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.2f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });

			if (ImGui::Button("X", buttonSize))
				scale.xscale = 1.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::DragFloat("##X", &scale.xscale, 0.1f, 0.1f);
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });

			if (ImGui::Button("Y", buttonSize))
				scale.yscale = 1.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::DragFloat("##Y", &scale.yscale, 0.1f, 0.1f);
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });

			if (ImGui::Button("Z", buttonSize))
				scale.zscale = 1.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::DragFloat("##Z", &scale.zscale, 0.1f, 0.1f);
			ImGui::PopItemWidth();
			ImGui::Columns(1);
			ImGui::PopStyleVar();
			ImGui::PopID();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::Text("Orientation");
			ImGui::PushID("Orientation");
			ImGui::NextColumn();
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.2f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });

			if (ImGui::Button("X", buttonSize))
				pos.roll = 0.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::SliderAngle("##X", &pos.roll, -180.0f, 180.0f);
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });

			if (ImGui::Button("Y", buttonSize))
				pos.pitch = 0.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::SliderAngle("##Y", &pos.pitch, -180.0f, 180.0f);
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });

			if (ImGui::Button("Z", buttonSize))
				pos.yaw = 0.0f;
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::SliderAngle("##Z", &pos.yaw, -180.0f, 180.0f);
			ImGui::PopItemWidth();
			ImGui::Columns(1);
			ImGui::PopStyleVar();
			ImGui::PopID();

			if (ImGui::Button("Reset"))
			{
				pos.roll = 0.0f;
				pos.pitch = 0.0f;
				pos.yaw = 0.0f;

				pos.x = 0.0f;
				pos.y = 0.0f;
				pos.z = 0.0f;

				scale.xscale = 1.0f;
				scale.yscale = 1.0f;
				scale.zscale = 1.0f;
			}
			ImGui::End();
		}
	}
}

void Model::SetRootTransfotm(DirectX::FXMMATRIX tf)
{
	pRoot->SetAppliedTransform(tf);
}

void Model::SetRootScaling(DirectX::FXMMATRIX sf)
{
	pRoot->SetAppliedScale(sf);
}

const DirectX::XMFLOAT4X4& Node::GetAppliedTransform() const noexcept
{
	return appliedTransform;
}

const DirectX::XMFLOAT4X4& Node::GetAppliedScale() const noexcept
{
	return appliedScale;
}


DirectX::XMMATRIX Model::GetTransform() const noexcept
{
	assert(pSelectedNode != nullptr);
	const auto& transform = poses.at(pSelectedNode->GetId());
	const auto& scale = scales.at(pSelectedNode->GetId());
	return
		DirectX::XMMatrixRotationRollPitchYaw(transform.roll, transform.pitch, transform.yaw) *
		DirectX::XMMatrixScaling(scale.xscale, scale.yscale, scale.zscale) *
		DirectX::XMMatrixTranslation(transform.x, transform.y, transform.z);
}

DirectX::XMMATRIX Model::GetScale() const noexcept
{
	assert(pSelectedNode != nullptr);
	const auto& scale = scales.at(pSelectedNode->GetId());
	return
		DirectX::XMMatrixScaling(scale.xscale, scale.yscale, scale.zscale);
}



Node* Model::GetSelectedNode() const noexcept
{
	return pSelectedNode;
}



std::unique_ptr<Mesh> Model::ParseMesh(Graphics& gfx, const aiMesh& mesh, const aiMaterial *const *pMaterials, const std::filesystem::path& filePath)
{
	namespace dx = DirectX;
	using CubeR::VertexLayout;
	std::string dir = filePath.parent_path().string() + "\\";

	std::vector<std::unique_ptr<Bindable>> bindablePtrs;
	using namespace std::string_literals;

		bool hasSpecMap = false;
		bool hasNormalMap = false;
		bool hasDiffuseMap = false;
		bool hasAlphaGloss = false;
		float shine = 2.0f;
		dx::XMFLOAT4 specularColor = { 0.18f, 0.18f, 0.18f, 1.0f };
		dx::XMFLOAT3 diffuseColor = { 0.45f, 0.45f, 0.85f };
		if (mesh.mMaterialIndex >= 0)
		{

			auto& material = *pMaterials[mesh.mMaterialIndex];
			aiString texFileName;
			if (material.GetTexture(aiTextureType_DIFFUSE, 0, &texFileName) == aiReturn_SUCCESS)
			{
				auto tex = std::make_unique<Texture>(gfx, dir + texFileName.C_Str());
				bindablePtrs.push_back(std::move(tex)); 
				hasDiffuseMap = true; 
			}
			else
			{
				material.Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor3D&>(diffuseColor));
			}
			if (material.GetTexture(aiTextureType_SPECULAR, 0, &texFileName) == aiReturn_SUCCESS)
			{
				auto tex = std::make_unique<Texture>(gfx, dir + texFileName.C_Str(), 1);
				hasAlphaGloss = tex->HasAlpha();
				bindablePtrs.push_back(std::move(tex));
				hasSpecMap = true;
			}
			else
			{
				material.Get(AI_MATKEY_COLOR_SPECULAR, reinterpret_cast<aiColor3D&>(specularColor));
			}
			if (!hasAlphaGloss)
			{
				material.Get(AI_MATKEY_SHININESS, shine);
			}
			if (material.GetTexture(aiTextureType_NORMALS, 0, &texFileName) == aiReturn_SUCCESS)
			{
				auto tex = std::make_unique<Texture>(gfx, dir + texFileName.C_Str(), 2);
				hasAlphaGloss = tex->HasAlpha(); 
				bindablePtrs.push_back(std::move(tex)); 
				hasNormalMap = true;
			}
			if (hasDiffuseMap || hasNormalMap || hasSpecMap)
			{
				bindablePtrs.push_back(std::make_unique<Sampler>(gfx));
			}
		}

		const float scale = 1.0f;

		if (hasDiffuseMap && hasNormalMap && hasSpecMap)
		{
			CubeR::VertexBuffer vbuf(std::move(
				VertexLayout{}
				.Append(VertexLayout::Position3D)
				.Append(VertexLayout::Normal)
				.Append(VertexLayout::Tangent)
				.Append(VertexLayout::Bitangent)
				.Append(VertexLayout::Texture2D)
			));
			for (unsigned int i = 0; i < mesh.mNumVertices; i++)
			{
				vbuf.EmplaceBack(
					dx::XMFLOAT3(mesh.mVertices[i].x * scale, mesh.mVertices[i].y * scale, mesh.mVertices[i].z * scale),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mNormals[i]),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mTangents[i]),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mBitangents[i]),
					*reinterpret_cast<dx::XMFLOAT2*>(&mesh.mTextureCoords[0][i])
				);
			}
			std::vector<unsigned short> indices;
			indices.reserve(mesh.mNumFaces * 3);
			for (unsigned int i = 0; i < mesh.mNumFaces; i++)
			{
				const auto& face = mesh.mFaces[i];
				assert(face.mNumIndices == 3);
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}
			bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vbuf));

			bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

			auto pvs = std::make_unique<VertexShader>(gfx, L"shaders\\PhongNormalMapVS.cso");

			auto pvsbc = pvs->GetBytecode();
			bindablePtrs.push_back(std::move(pvs));
			bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"shaders\\PhongSpecNormalMapPS.cso"));

			bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vbuf.GetLayout().GetD3DLayout(), pvsbc));
			Node::PSMaterialConstantFullmonte pmc = {};
			pmc.specularPower = shine;
			pmc.hasGlossMap = hasAlphaGloss ? TRUE : FALSE;
			bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<Node::PSMaterialConstantFullmonte>>(gfx, pmc, 1u));
		}

		else if (hasDiffuseMap && !hasNormalMap && hasSpecMap)
		{
			CubeR::VertexBuffer vbuf(std::move(
				VertexLayout{}
				.Append(VertexLayout::Position3D)
				.Append(VertexLayout::Normal)
				.Append(VertexLayout::Texture2D)
			));
			for (unsigned int i = 0; i < mesh.mNumVertices; i++)
			{
				vbuf.EmplaceBack(
					dx::XMFLOAT3(mesh.mVertices[i].x * scale, mesh.mVertices[i].y * scale, mesh.mVertices[i].z * scale), 
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mNormals[i]),
					*reinterpret_cast<dx::XMFLOAT2*>(&mesh.mTextureCoords[0][i]) 
				);
			}
			std::vector<unsigned short> indices;
			indices.reserve(mesh.mNumFaces * 3);
			for (unsigned int i = 0; i < mesh.mNumFaces; i++)
			{
				const auto& face = mesh.mFaces[i];
				assert(face.mNumIndices == 3);
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}
			bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vbuf));

			bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

			auto pvs = std::make_unique<VertexShader>(gfx, L"shaders\\PhongVS.cso");

			auto pvsbc = pvs->GetBytecode();
			bindablePtrs.push_back(std::move(pvs));
			bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"shaders\\PhongSpecPS.cso"));

			bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vbuf.GetLayout().GetD3DLayout(), pvsbc));
			struct PSMaterialConstantDissSpec {
				float specularMapWeight;
				BOOL hasGloss; 
				float specularConstPow;
				float padding;
			} pmc;
			pmc.specularConstPow = shine;
			pmc.hasGloss = hasAlphaGloss ? TRUE : FALSE;  
			pmc.specularMapWeight = 1.0f; 
			bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<PSMaterialConstantDissSpec>>(gfx, pmc, 1u));
		}

		else if (hasDiffuseMap && hasNormalMap)
		{
			CubeR::VertexBuffer vbuf(std::move(
				VertexLayout{}
				.Append(VertexLayout::Position3D)
				.Append(VertexLayout::Normal)
				.Append(VertexLayout::Tangent)
				.Append(VertexLayout::Bitangent)
				.Append(VertexLayout::Texture2D)
			));
			for (unsigned int i = 0; i < mesh.mNumVertices; i++)
			{
				vbuf.EmplaceBack(
					dx::XMFLOAT3(mesh.mVertices[i].x * scale, mesh.mVertices[i].y * scale, mesh.mVertices[i].z * scale),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mNormals[i]),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mTangents[i]),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mBitangents[i]),
					*reinterpret_cast<dx::XMFLOAT2*>(&mesh.mTextureCoords[0][i])
				);
			}
			std::vector<unsigned short> indices;
			indices.reserve(mesh.mNumFaces * 3);
			for (unsigned int i = 0; i < mesh.mNumFaces; i++)
			{
				const auto& face = mesh.mFaces[i];
				assert(face.mNumIndices == 3);
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}

			bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vbuf));

			bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));
			auto pvs = std::make_unique<VertexShader>(gfx, L"shaders\\PhongNormalMapVS.cso");

			auto pvsbc = pvs->GetBytecode();
			bindablePtrs.push_back(std::move(pvs));
			bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"shaders\\PhongNormalMapPS.cso"));

			bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vbuf.GetLayout().GetD3DLayout(), pvsbc));
			
			struct PSMaterialConstantDiffnorm
			{
				float specularIntensity = 0.18f;
				float specularPower = 2.0f;
				BOOL  normalMapEnabled = TRUE;
				float padding[1];
			} pmc;
			bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<PSMaterialConstantDiffnorm>>(gfx, pmc, 1u));
		}


		else if (hasDiffuseMap)
		{
			CubeR::VertexBuffer vbuf(std::move(
				VertexLayout{}
				.Append(VertexLayout::Position3D)
				.Append(VertexLayout::Normal)
				.Append(VertexLayout::Texture2D)
			));

			for (unsigned int i = 0; i < mesh.mNumVertices; i++)
			{
				vbuf.EmplaceBack(
					dx::XMFLOAT3(mesh.mVertices[i].x * scale, mesh.mVertices[i].y * scale, mesh.mVertices[i].z * scale),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mNormals[i]),
					*reinterpret_cast<dx::XMFLOAT2*>(&mesh.mTextureCoords[0][i])
				);
			}

			std::vector<unsigned short> indices;
			indices.reserve(mesh.mNumFaces * 3);
			for (unsigned int i = 0; i < mesh.mNumFaces; i++)
			{
				const auto& face = mesh.mFaces[i];
				assert(face.mNumIndices == 3);
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}
			bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vbuf)); 

			bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices)); 

			auto pvs = std::make_unique<VertexShader>(gfx, L"shaders\\PhongVS.cso");

			auto pvsbc = pvs->GetBytecode();
			bindablePtrs.push_back(std::move(pvs));
			bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"shaders\\PhongPS.cso"));

			bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vbuf.GetLayout().GetD3DLayout(), pvsbc));

			struct PSMaterialConstantDiffuse
			{
				float specularIntensity = 0.18f;
				float specularPower = 2.0f;
				float padding[2];
			} pmc;
			bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<PSMaterialConstantDiffuse>>(gfx, pmc, 1u));
		}


		else if (!hasDiffuseMap && !hasNormalMap && !hasSpecMap)
		{
			namespace dx = DirectX;
			using CubeR::VertexLayout;

			CubeR::VertexBuffer vbuf(std::move(
				VertexLayout{}
				.Append(VertexLayout::Position3D)
				.Append(VertexLayout::Normal)
			));

			for (unsigned int i = 0; i < mesh.mNumVertices; i++)
			{
				vbuf.EmplaceBack(
					dx::XMFLOAT3(mesh.mVertices[i].x* scale, mesh.mVertices[i].y* scale, mesh.mVertices[i].z* scale),
					*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mNormals[i])
				);
			}

			std::vector<unsigned short> indices;
			indices.reserve(mesh.mNumFaces * 3);
			for (unsigned int i = 0; i < mesh.mNumFaces; i++)
			{
				const auto& face = mesh.mFaces[i];
				assert(face.mNumIndices == 3);
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}

			bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vbuf));

			bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

			auto pvs = std::make_unique<VertexShader>(gfx, L"shaders\\NoTexPhongVS.cso");

			auto pvsbc = pvs->GetBytecode();
			bindablePtrs.push_back(std::move(pvs));

			bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"shaders\\NoTexPhongPS.cso"));

			bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vbuf.GetLayout().GetD3DLayout(), pvsbc));

			Node::PSMaterialConstantNotex pmc = {};
			pmc.color = diffuseColor;
			pmc.specularIntensity = (specularColor.x + specularColor.y + specularColor.z) / 3.0f;
			bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<Node::PSMaterialConstantNotex>>(gfx, pmc, 1u));
			}
			else
			{
				throw std::runtime_error("terrible combination of textures in material smh");
			}	
		bindablePtrs.push_back(std::make_unique<DepthStencil>(gfx, false));
		return std::make_unique<Mesh>(gfx, std::move(bindablePtrs));
}



Model::~Model() noexcept
{}

int Model::GetId() const noexcept
{
	return id;
}



std::unique_ptr<Node> Model::ParseNode(int& nextId, const aiNode& node)
{
	namespace dx = DirectX;
	const auto transform = dx::XMMatrixTranspose(dx::XMLoadFloat4x4(
		reinterpret_cast<const dx::XMFLOAT4X4*>(&node.mTransformation)
	));

	std::vector<Mesh*> curMeshPtrs;
	curMeshPtrs.reserve(node.mNumMeshes);
	for (size_t i = 0; i < node.mNumMeshes; i++)
	{
		const auto meshIdx = node.mMeshes[i];
		curMeshPtrs.push_back(meshPtrs.at(meshIdx).get());
	}

	auto pNode = std::make_unique<Node>(nextId++, node.mName.C_Str(), std::move(curMeshPtrs), transform);
	for (size_t i = 0; i < node.mNumChildren; i++)
	{
		pNode->AddChild(ParseNode(nextId, *node.mChildren[i]));
	}
	return pNode;
}