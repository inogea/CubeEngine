//������������ ����� ��� ���� ��������, ������� ����� �������� � ��������

#pragma once
#include "Graphics.h"

class Drawable;

	class Bindable
	{
	public:
		//������� Bind ���������������� � ������ �������� ������ Bindable 
		// � �������� �� ���������� ������� � �������� DirectX
		virtual void Bind(Graphics& gfx) noexcept = 0;
		virtual void InitializeParentReference(const Drawable&) noexcept
		{}
		virtual ~Bindable() = default;
	protected:
		static ID3D11DeviceContext* GetContext(Graphics& gfx);
		static ID3D11Device* GetDevice(Graphics& gfx);
	};