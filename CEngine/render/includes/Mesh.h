//�����, �������� ���������� � ���������� ������ � ����������� � ��� �����������������
//�������� �������� ������� DrawableBase

#pragma once
#include "Drawable.h"
#include "BindableBase.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <optional>
#include <filesystem>
#include <memory>
#include <assimp/postprocess.h>
#include <type_traits>
#include "imgui.h"

class Mesh : public Drawable
{
public:
	Mesh(Graphics& gfx, const Material& mat, const aiMesh& mesh) noexcept;
	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void Submit(FrameCommander& frame, DirectX::FXMMATRIX accumulatedTranform) const noexcept;
private:
	mutable DirectX::XMFLOAT4X4 transform;
};


//����� ����� ���� � ����� ����� ��� ����������� ������
class Node
{
	friend class Model;
	friend class ModelWindow;
public:
	Node(int id, const std::string& name,std::vector<Mesh*> meshPtrs, const DirectX::XMMATRIX& transform) ;
	Node(const Node&) = delete;
	Node& operator=(const Node&) = delete;
	void Submit(FrameCommander& frame, DirectX::FXMMATRIX accumulatedTransform) const noexcept;
	void SetAppliedTransform(DirectX::FXMMATRIX transform) noexcept;
	void SetAppliedScale(DirectX::FXMMATRIX scale) noexcept;
	const DirectX::XMFLOAT4X4& GetAppliedTransform() const noexcept;
	const DirectX::XMFLOAT4X4& GetAppliedScale() const noexcept;
	int GetId() const noexcept;
	std::string GetName() const noexcept;
	const std::vector<std::unique_ptr<Node>>& getchildPtrs() const noexcept;
	void RenderTree(Node*& pSelectedNode) const noexcept;
private:
	void AddChild(std::unique_ptr<Node> pChild) ;
	std::string name;
	int id;
	std::vector<std::unique_ptr<Node>> childPtrs;
	std::vector<Mesh*> meshPtrs;
	DirectX::XMFLOAT4X4 baseTransform;
	DirectX::XMFLOAT4X4 appliedTransform;
	DirectX::XMFLOAT4X4 appliedScale;

};


//����� ���� ������, �������������� �������������� � ���������� ������
class Model
{
public:
	//����������� ��������� ������ �� ����� � ����������� �� ����, ��������� ���� �� ������� ���� � ��������
	Model(Graphics& gfx, const std::string& fileName, int id=0, std::string modelName = "Unnamed Object");
	void Submit(FrameCommander& frame) const noexcept;
	std::unique_ptr<Mesh> ParseMesh(Graphics& gfx, const aiMesh& mesh, const aiMaterial* const* pMaterials, const std::filesystem::path& filePath);
	void ShowWindow(Graphics& gfx, Model* pSelectedModel) noexcept;
	void SetRootTransfotm(DirectX::FXMMATRIX tf);
	void SetRootScaling(DirectX::FXMMATRIX sf);
	std::vector<Technique> GetTechniques() const noexcept;
	~Model() noexcept;
	int GetId() const noexcept;
	Node& getpRoot();
	std::string modelName;
	std::string rootPath;
	int id;
private:
	CubeR::VertexLayout vtxLayout;
	std::vector<Technique> techniques;
	DirectX::XMMATRIX GetTransform() const noexcept;
	DirectX::XMMATRIX GetScale() const noexcept;
	Node* GetSelectedNode() const noexcept;

	//������� ParseNode ���� �������� ���� � ������������, �������� �����������
	std::unique_ptr<Node> ParseNode(int& nextId, const aiNode& node);

	std::unique_ptr<Node> pRoot;
	std::vector<std::unique_ptr<Mesh>> meshPtrs;
	Node* pSelectedNode;
	struct TransformParameters
	{
		float roll = 0.0f;
		float pitch = 0.0f;
		float yaw = 0.0f;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};
	std::unordered_map<int, TransformParameters> poses;
	struct ScaleParameters
	{
		float xscale = 1.0f;
		float yscale = 1.0f;
		float zscale = 1.0f;
	};
	std::unordered_map<int, ScaleParameters> scales;
};