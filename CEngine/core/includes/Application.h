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
		Application(int width, int height, WindowType type);
		virtual ~Application();
		int run();
		void ShowSceneWindow();
		void ShowMenuBar();

		void AddObj(bool istextured);
		void AddCube();

		void ShowToolBar();
		void HadleInput(float dt);
	private:
		void doFrame();					//������� ��������� ������ �����
		ImguiManager imgui;
		Window m_Window;
		Timer timer;
		Camera cam;
		PointLight light;
		std::vector<std::unique_ptr<Model>> models;
		ID3D11ShaderResourceView* pCubeIco = nullptr;
		std::unique_ptr<SkyBox> skybox;
		Model* pSelectedModel;
		bool drawGrid = true;
		int id = 0;
	};
}
