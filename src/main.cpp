#include <iostream>
#include <format>
#include <thread>

#include "memory.hpp"
#include "vector.h"
#include "viewMatrix.h"

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

namespace offset
{
	// client
	constexpr ::std::ptrdiff_t dwLocalPlayer = 0xDEA98C; //local player
	constexpr ::std::ptrdiff_t m_iTeamNum = 0xF4; //team number
	constexpr ::std::ptrdiff_t m_vecOrigin = 0x138; //origin
	constexpr ::std::ptrdiff_t m_vecViewOffset = 0x108;
	constexpr ::std::ptrdiff_t dwClientState = 0x59F19C;
	constexpr ::std::ptrdiff_t dwClientState_GetLocalPlayer = 0x180;
	constexpr ::std::ptrdiff_t dwClientState_ViewAngles = 0x4D90;
	constexpr ::std::ptrdiff_t m_aimPunchAngle = 0x303C;
	constexpr ::std::ptrdiff_t dwEntityList = 0x4DFFF7C; //entity list
	constexpr ::std::ptrdiff_t m_bDormant = 0xED; //dormant
	constexpr ::std::ptrdiff_t m_lifeState = 0x25F; //life state
	constexpr ::std::ptrdiff_t m_bSpottedByMask = 0x980; //bone matrix
	constexpr ::std::ptrdiff_t m_dwBoneMatrix = 0x26A8;
	constexpr ::std::ptrdiff_t m_bSpotted = 0x93D;
	constexpr ::std::ptrdiff_t m_iHealth = 0x100;
	constexpr ::std::ptrdiff_t dwViewMatrix = 0x4DF0DC4; //view matrix
}

static bool world_to_screen(const Vector3& world, Vector3& screen, const ViewMatrix& vm) noexcept {
	float w = vm[3][0] * world.x + vm[3][1] * world.y + vm[3][2] * world.z + vm[3][3];
	if (w < 0.001f) {
		return false;
	}
	
	const float x = world.x * vm[0][0] + world.y * vm[0][1] + world.z * vm[0][2] + vm[0][3];
	const float y = world.x * vm[1][0] + world.y * vm[1][1] + world.z * vm[1][2] + vm[1][3];

	w = 1.f / w;
	float nx = x * w;
	float ny = y * w;

	const ImVec2 size = ImGui::GetIO().DisplaySize;

	screen.x = (size.x * 0.5f * nx) + (nx + size.x * 0.5f);
	screen.y = -(size.y * 0.5f * ny) + (ny + size.y * 0.5f);

	return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param)) {
		return 1L;
	}

	switch (message) {
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0L;
		}
	}

	return DefWindowProc(window, message, w_param, l_param);
}

bool create_directx(HWND window) {

}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show) {
	// allocate this program a console
	if (!AllocConsole()) {
		return FALSE;
	}

	FILE* file{ nullptr };
	freopen_s(&file, "CONIN$", "r", stdin);
	freopen_s(&file, "CONOUT$", "w", stdout);
	freopen_s(&file, "CONOUT$", "w", stderr);

	DWORD memory = memory::Get_process_id(L"csgo.exe");
	if (!memory) {

		std::cout << "Waiting for CS:GO...\n";
		do {
			memory = memory::Get_process_id(L"csgo.exe");
			Sleep(200UL);
		} while (!memory);
	}

	DWORD client = memory::Get_module_address(memory, L"client.dll");

	if (!client) {
		std::cout << "Failed to get game module";
		FreeConsole();
		return FALSE;
	}

	std::cout << std::format("Client -> {:#x}\n", client);
	
	const HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, memory);

	if (!handle) {
		return FALSE;
	}

	const WNDCLASSEXW wc{
		.cbSize = sizeof(WNDCLASSEXW),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = window_procedure,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = instance,
		.hIcon = nullptr,
		.hCursor = nullptr,
		.hbrBackground = nullptr,
		.lpszMenuName = nullptr,
		.lpszClassName = L"External Overlay Class",
		.hIconSm = nullptr
	};

	if (!RegisterClassExW(&wc)) {
		return FALSE;
	}

	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		wc.lpszClassName,
		L"External Overlay",
		WS_POPUP,
		0,
		0,
		1920,
		1080,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);
	if (!window) {
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return FALSE;
	}

	if (!SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA)) {
		DestroyWindow(window);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return FALSE;
	}

	{
		RECT client_arena{};
		if (!GetClientRect(window, &client_arena)) {
			DestroyWindow(window);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return FALSE;
		}

		RECT window_arena{};
		if (!GetWindowRect(window, &window_arena)) {
			DestroyWindow(window);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return FALSE;
		}

		POINT diff{};
		if (!ClientToScreen(window, &diff)) {
			DestroyWindow(window);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return FALSE;
		}

		const MARGINS margins{
			window_arena.left + (diff.x - window_arena.left),
			window_arena.top + (diff.y - window_arena.top),
			client_arena.right,
			client_arena.bottom
		};

		if (FAILED(DwmExtendFrameIntoClientArea(window, &margins))) {
			DestroyWindow(window);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return FALSE;
		}
	}

	DXGI_SWAP_CHAIN_DESC sd{};

	ZeroMemory(&sd, sizeof(sd));
	sd.BufferDesc.Width = 0U;
	sd.BufferDesc.Height = 0U;

	sd.BufferDesc.RefreshRate.Numerator = 75U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	sd.SampleDesc.Count = 1U;
	sd.SampleDesc.Quality = 0U;

	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL feature_levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};
	D3D_FEATURE_LEVEL feature_level{};
	
	// directx variables
	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	
	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		feature_levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&feature_level,
		&device_context))) {
		DestroyWindow(window);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return FALSE;
	}

	ID3D11Texture2D* back_buffer{ nullptr };

	if (FAILED(swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer)))) {
		return FALSE;
	}

	if (FAILED(device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view))) {
		return FALSE;
	}

	if (back_buffer) {
		back_buffer->Release();
	}

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);

	bool running = true;

	while (running) {

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		
			if (msg.message == WM_QUIT) {
				running = false;
			}
		}

		if (!running)
			break;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();

		const auto localPlayer = memory::Read<DWORD>(handle, client + offset::dwLocalPlayer);

		if (localPlayer) {
			const auto localTeam = memory::Read<int>(handle, localPlayer + offset::m_iTeamNum);
			const auto viewMatrix = memory::Read<ViewMatrix>(handle, client + offset::dwViewMatrix);

			for (int i = 1; i < 15; i++) {
				const auto player = memory::Read<DWORD>(handle, client + offset::dwEntityList + i * 0x10);

				if (!player)
					continue;

				if (memory::Read<bool>(handle, player + offset::m_bDormant))
					continue;

				if (memory::Read<int>(handle, player + offset::m_iTeamNum) == localTeam)
					continue;

				if (memory::Read<int>(handle, player + offset::m_lifeState) != 0)
					continue;

				const auto bones = memory::Read<DWORD>(handle, player + offset::m_dwBoneMatrix);

				if (!bones)
					continue;

				//Vector3 headPosition{
				//	memory::Read<float>(handle, bones + 0x30 * 8 + 0x0C),
				//	memory::Read<float>(handle, bones + 0x30 * 8 + 0x1C),
				//	memory::Read<float>(handle, bones + 0x30 * 8 + 0x2C)
				//};

				//auto feet_pos = memory::Read<Vector3>(handle, player + offset::m_vecOrigin);

				Vector3 top;
				Vector3 bottom;

				if (world_to_screen(
					Vector3 {memory::Read<float>(handle, bones + 0x30 * 8 + 0x0C),
						memory::Read<float>(handle, bones + 0x30 * 8 + 0x1C),
						memory::Read<float>(handle, bones + 0x30 * 8 + 0x2C)}
						+ Vector3{ 0, 0, 11.f }, top, viewMatrix) && world_to_screen(memory::Read<Vector3>(handle, player + offset::m_vecOrigin) - Vector3{0, 0, 9.f}, bottom, viewMatrix)) {
					const float h = bottom.y - top.y;
					const float w = h * 0.35f;

					ImGui::GetBackgroundDrawList()->AddRect({ top.x - w, top.y }, { top.x + w, bottom.y }, ImColor(1.f, 0.f, 0.f));
				}
			}
		}
		
		ImGui::Render();

		constexpr float color[4]{ 0.f, 0.f, 0.f, 0.f };
		device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
		device_context->ClearRenderTargetView(render_target_view, color);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		swap_chain->Present(0U, 0U);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	if (swap_chain) {
		swap_chain->Release();
	}

	if (device_context) {
		device_context->Release();
	}

	if (device) {
		device->Release();
	}

	if (render_target_view) {
		render_target_view->Release();
	}

	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}