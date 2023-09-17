#include "../includes/Application.h"
#include "../render/includes/Box.h"
#include "../includes/Window.h"
#include <memory>

namespace Cube
{

	Application::Application() : m_Window(1600, 900)
	{
		m_Window.WindowShow();
		std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<float> adist(0.0f, 3.1415f * 2.0f);
		std::uniform_real_distribution<float> ddist(0.0f, 3.1415f * 2.0f);
		std::uniform_real_distribution<float> odist(0.0f, 3.1415f * 0.3f);
		std::uniform_real_distribution<float> rdist(6.0f, 20.0f);
		for (auto i = 0; i < 100; i++)
		{
			boxes.push_back(std::make_unique<Box>(
				m_Window.Gfx(), rng, adist,
				ddist, odist, rdist
			));
		}
		m_Window.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 40.0f));
	}

	Application::~Application()
	{

	}
	int Application::run()
	{
		while ( true )
		{
			if (const auto ecode = Window::ProcessMessages())
			{
				return *ecode;
			}
			doFrame();
		}
	}

	void Application::doFrame()
	{
		auto dt = timer.Mark();
		m_Window.Gfx().ClearBuffer(0.07f, 0.0f, 0.12f);
		for (auto& b : boxes)
		{
			b->Update(dt);
			b->Draw(m_Window.Gfx());
		}
		m_Window.Gfx().EndFrame();
	}
}
