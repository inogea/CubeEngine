#pragma once
#include "Window.h"
#include "Timer.h"
#include "ImguiManager.h"
#include "../render/includes/Camera.h"
#include "../render/includes/PointLight.h"
#include "../render/includes/Mesh.h"
#include "../render/includes/SkyBox.h"
#include <set>

namespace Cube
{

	class Application
	{
	public:
		Application(int width, int height);
		virtual ~Application();
		int run();
		void ShowSceneWindow();
		void ShowMenuBar();

		void AddObj();
		void AddCube();

		void ShowToolBar();
		void HadleInput(float dt);
	private:
		void doFrame();					//������� ��������� ������ �����
		ImguiManager imgui;
		Camera cam;
		Window m_Window;
		PointLight light;
		std::vector<std::unique_ptr<Model>> models;
		ID3D11ShaderResourceView* pCubeIco = nullptr;
		SkyBox skybox;
		Timer timer;
		wchar_t path[260];
	};
}
