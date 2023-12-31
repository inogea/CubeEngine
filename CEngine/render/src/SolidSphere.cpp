#include "../includes/SolidSphere.h"
#include "../includes/BindableBase.h"
#include "../includes/Sphere.h"


SolidSphere::SolidSphere(Graphics& gfx, float radius)
{
	namespace dx = DirectX;
		struct Vertex
		{
			dx::XMFLOAT3 pos;
		};
		if (!IsStaticInitialized())
		{
			AddStaticBind(std::make_unique<Rasterizer>(gfx, false));
			AddStaticBind(std::make_unique<DepthStencil>(gfx, false));
			auto model = Sphere::Make<Vertex>();
			model.Transform(dx::XMMatrixScaling(radius, radius, radius));
			AddStaticBind(std::make_unique<VertexBuffer>(gfx, model.vertices));
			AddStaticIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

			auto pvs = std::make_unique<VertexShader>(gfx, L"shaders\\SolidVS.cso");
			auto pvsbc = pvs->GetBytecode();
			AddStaticBind(std::move(pvs));

			AddStaticBind(std::make_unique<PixelShader>(gfx, L"shaders\\SolidPS.cso"));

			struct PSColorConstant
			{
				dx::XMFLOAT3 color = { 1.0f,1.0f,1.0f };
				float padding;
			} colorConst;
			AddStaticBind(std::make_unique<PixelConstantBuffer<PSColorConstant>>(gfx, colorConst, 1u));

			const std::vector<D3D11_INPUT_ELEMENT_DESC> ied =
			{
				{ "Position",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			};
			AddStaticBind(std::make_unique<InputLayout>(gfx, ied, pvsbc));

			AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
		}
		else
		{
			SetIndexFromStatic();
		}
		AddBind(std::make_unique<TransformCbuf>(gfx, *this));
}

void SolidSphere::Update(float dt) noexcept {}

void SolidSphere::SetPos(DirectX::XMFLOAT3 pos) noexcept
{
	this->pos = pos;
}

DirectX::XMMATRIX SolidSphere::GetTransformXM() const noexcept
{
	return DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
}