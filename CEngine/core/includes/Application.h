#pragma once
#include "Window.h"
#include "Timer.h"
#include "ImguiManager.h"
#include "../render/includes/Camera.h"
#include "../render/includes/PointLight.h"
#include "../render/includes/Mesh.h"
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
	private:
		void doFrame();					//������� ��������� ������ �����
		ImguiManager imgui;
		float speed_factor = 1.0f;
		Camera cam;
		Window m_Window;
		PointLight light;
		std::vector<std::unique_ptr<Model>> models;
		Timer timer;
	};
}
