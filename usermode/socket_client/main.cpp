#include "driver.h"
#include <iostream>
#include <TlHelp32.h>
#include <future>
#include "nuklear/nuklear_d3d11.h"
#include "resource.h"
#include "manager.h"

#pragma comment(lib, "Winmm.lib")

#include <Windows.h>
#include <winternl.h>
#include <assert.h>
#include "xor.h"

typedef NTSTATUS(NTAPI* lpNtCreateThreadEx)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, HANDLE, LPTHREAD_START_ROUTINE, LPVOID, ULONG, ULONG_PTR, SIZE_T, SIZE_T, LPVOID);
typedef NTSTATUS(NTAPI* lpNtQueryInformationThread)(HANDLE, LONG, PVOID, ULONG, PULONG);

namespace globals {
	bool aimtab = true;
	bool visualtab = false;
	bool misctab = false;

	bool esp = false;
	bool clip = false;
	bool recoil = false;
	bool spread = false;
	bool speed = false;
	bool damage = false;
	bool fov = false;
	bool aimbot = false;
	bool glow = false;
	bool tp = false;

	bool debug = false;

	float aimbotfov = 30.0f;
	float spreadmultiplier = 0.0f;
	float recoilmultiplier = 0.0f;
	float customfov = 90.0f;

	char username[256];
	char password[256];

	bool login = false;

	SOCKET connection = NULL;

	uint64_t window_width = 315;
	uint64_t window_height = 250;
	bool     window_update = false;

	static IDXGISwapChain* swap_chain;
	static ID3D11Device* device;
	static ID3D11DeviceContext* context;
	static ID3D11RenderTargetView* rt_view;

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

DWORD get_process_id(LPCSTR lpExeName)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapShot == INVALID_HANDLE_VALUE)
		return NULL;

	PROCESSENTRY32 pe = { 0 };
	pe.dwSize = sizeof(PROCESSENTRY32);

	for (BOOL	success = Process32First(hSnapShot, &pe);
		success == TRUE;
		success = Process32Next(hSnapShot, &pe))
	{
		if (strcmp(lpExeName, pe.szExeFile) == 0)
		{
			CloseHandle(hSnapShot);
			return pe.th32ProcessID;
		}
	}

	CloseHandle(hSnapShot);
	return NULL;
}

DWORD WINAPI FeaturesRoutine(LPVOID)
{
	printf(xorstr_("[+] Thread %u works!\n"), GetCurrentThreadId());

	driver::initialize();

	globals::connection = driver::connect();
	if (globals::connection == INVALID_SOCKET)
	{
		std::cout << xorstr_("[!] Connection failed.\n");
		MessageBoxA(NULL, xorstr_("Please load the driver before the cheeto"), xorstr_("Error"), MB_ICONERROR);
		driver::disconnect(globals::connection);
		driver::deinitialize();
		exit(-1);
		return 0;
	}

	//driver::hwid_spoofing(globals::connection);
	driver::clean_piddbcachetable(globals::connection);
	driver::clean_mmunloadeddrivers(globals::connection);

	DWORD pid = 0;

	while (pid == 0) {
		std::cout << xorstr_("[+] Waiting for Rainbow Six Siege to launch.") << std::endl;

		pid = get_process_id(xorstr_("RainbowSix.exe"));

		if (pid != 0) {
			manager::m_base = driver::get_process_base_address(globals::connection, pid);
			std::printf(xorstr_("[+] Process base: %p.\n"), (void*)manager::m_base);

			const auto dos_magic = driver::read<uint16_t>(globals::connection, pid, manager::m_base);
			std::printf(xorstr_("[+] Process DOS signature: %X.\n"), dos_magic);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	while (TRUE) {
		const auto entities = manager::get_entities();

		const auto entity_count = manager::get_entity_count();

		std::cout << xorstr_("Entity Count: ") << entity_count << std::endl;

		for (uint64_t entity : entities)
		{
			std::cout << xorstr_("Entity: ") << entity << std::endl;

			const auto health = manager::get_entity_health(entity);
			std::cout << xorstr_("\t[+] Entity health: ") << health << std::endl;

			const auto team = manager::get_entity_team(entity);
			std::cout << xorstr_("\t[+] Entity team: ") << team << std::endl;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	driver::disconnect(globals::connection);
	driver::deinitialize();
}

static void
set_swap_chain_size(int width, int height)
{
	ID3D11Texture2D* back_buffer;
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	HRESULT hr;

	if (globals::rt_view)
		globals::rt_view->Release();

	globals::context->OMSetRenderTargets(0, NULL, NULL);

	hr = globals::swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		/* to recover from this, you'll need to recreate device and all the resources */
		MessageBoxW(NULL, xorstr_(L"DXGI device is removed or reset!"), xorstr_(L"Error"), 0);
		exit(0);
	}
	assert(SUCCEEDED(hr));

	memset(&desc, 0, sizeof(desc));
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	hr = globals::swap_chain->GetBuffer(0, IID_ID3D11Texture2D, (void**)& back_buffer);
	assert(SUCCEEDED(hr));

	hr = globals::device->CreateRenderTargetView((ID3D11Resource*)back_buffer, &desc, &globals::rt_view);
	assert(SUCCEEDED(hr));

	back_buffer->Release();
}

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		if (globals::swap_chain)
		{
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			set_swap_chain_size(width, height);
			nk_d3d11_resize(globals::context, width, height);
		}
		break;
	}

	if (nk_d3d11_handle_event(wnd, msg, wparam, lparam))
		return 0;

	return DefWindowProcW(wnd, msg, wparam, lparam);
}

DWORD WINAPI GuiRoutine(LPVOID)
{
	printf(xorstr_("[+] Thread %u works!\n"), GetCurrentThreadId());

	struct nk_context* ctx;
	struct nk_colorf bg;

	WNDCLASSW wc;
	RECT rect = { 0, 0, globals::window_width, globals::window_height };
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exstyle = WS_EX_APPWINDOW;
	HWND wnd;
	int running = 1;
	HRESULT hr;
	D3D_FEATURE_LEVEL feature_level;
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;

	memset(&wc, 0, sizeof(wc));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandleW(0);
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = xorstr_(L"NuklearWindowClass");
	RegisterClassW(&wc);
	AdjustWindowRectEx(&rect, style, FALSE, exstyle);

	wnd = CreateWindowExW(exstyle, wc.lpszClassName, xorstr_(L"Its a Cheeto! No, It's a Chicken!"),
		style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, wc.hInstance, NULL);

	SetWindowLong(wnd, GWL_STYLE,
		GetWindowLong(wnd, GWL_STYLE) & ~WS_MINIMIZEBOX);

	SetWindowLong(wnd, GWL_STYLE,
		GetWindowLong(wnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);

	SetWindowLong(wnd, GWL_STYLE,
		GetWindowLong(wnd, GWL_STYLE) & ~WS_THICKFRAME);

	EnableMenuItem(GetSystemMenu(wnd, FALSE), SC_CLOSE,
		MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

	memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.OutputWindow = wnd;
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swap_chain_desc.Flags = 0;
	if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
		NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
		&globals::swap_chain, &globals::device, &feature_level, &globals::context)))
	{
		hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP,
			NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
			&globals::swap_chain, &globals::device, &feature_level, &globals::context);
		assert(SUCCEEDED(hr));
	}
	set_swap_chain_size(globals::window_width, globals::window_height);

	ctx = nk_d3d11_init(globals::device, globals::window_width, globals::window_height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);

	{
		struct nk_font_atlas* atlas;
		nk_d3d11_font_stash_begin(&atlas);
		nk_d3d11_font_stash_end();

		set_style(ctx, theme::THEME_RED);
	}

	bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
	while (running)
	{
		MSG msg;
		nk_input_begin(ctx);
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				running = 0;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		nk_input_end(ctx);

		if (nk_begin(ctx, xorstr_("Demo"), nk_rect(0, 0, 300, 425), NK_WINDOW_NO_SCROLLBAR))
		{
			// TO-DO: Add sql logic for multiple user logins.
			if (!globals::login) {
				if (!globals::window_update)
				{
					globals::window_update = true;

					RECT desktop;
					const HWND hDesktop = GetDesktopWindow();
					GetWindowRect(hDesktop, &desktop);
					int horizontal = desktop.right;
					int vertical = desktop.bottom;

					MoveWindow(wnd, ((horizontal / 2) - (globals::window_width / 2)), ((vertical / 2) - (globals::window_height / 2)), globals::window_width, globals::window_height, TRUE);
				}

				nk_layout_row_dynamic(ctx, 185, 1);
				if (nk_group_begin(ctx, xorstr_("column1"), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
					nk_layout_row_dynamic(ctx, 25, 1);

					nk_label_colored(ctx, xorstr_("Its a Cheeto! No, It's a Chicken!"), NK_TEXT_CENTERED, nk_color{ 209, 209, 209, 255 });

					nk_label(ctx, xorstr_("Username"), NK_TEXT_CENTERED);
					nk_flags eventUsername = nk_edit_string_zero_terminated(ctx,
						NK_EDIT_BOX,
						globals::username, sizeof(globals::username), nk_filter_ascii);

					nk_label(ctx, xorstr_("Password"), NK_TEXT_CENTERED);
					nk_flags eventPassword = nk_edit_string_zero_terminated(ctx,
						NK_EDIT_BOX,
						globals::password, sizeof(globals::password), nk_filter_ascii);

					std::string lusername = globals::username;
					std::string lpassword = globals::password;
					std::string gusername = xorstr_("user");
					std::string gpassword = xorstr_("password");

					if (eventPassword && nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
						if (lusername == gusername && lpassword == gpassword) {
							mciSendString(xorstr_("play cheeto.mp3"), NULL, 0, NULL);

							globals::login = true;
							globals::window_update = false;

							globals::window_width = 315;
							globals::window_height = 465;
						}
						else {
							mciSendString(xorstr_("play chicken.mp3"), NULL, 0, NULL);
						}
					}

					if (nk_button_label(ctx, xorstr_("LOGIN"))) {
						if (lusername == gusername && lpassword == gpassword) {
#pragma region FeaturesRoutine
							auto hNtdll = LoadLibraryA(xorstr_("ntdll"));
							assert(hNtdll);

							auto NtCreateThreadEx = (lpNtCreateThreadEx)GetProcAddress(hNtdll, xorstr_("NtCreateThreadEx"));
							assert(NtCreateThreadEx);

							auto NtQueryInformationThread = (lpNtQueryInformationThread)GetProcAddress(hNtdll, xorstr_("NtQueryInformationThread"));
							assert(NtQueryInformationThread);

							NTSTATUS ntStatFeatures = 0;

							HANDLE hThreadFeatures = INVALID_HANDLE_VALUE;
							ntStatFeatures = NtCreateThreadEx(&hThreadFeatures, THREAD_ALL_ACCESS, 0, GetCurrentProcess(), (LPTHREAD_START_ROUTINE)0 /* Start address */, 0, 0x1 /* Suspended */, 0, 0, 0, 0);
							assert(ntStatFeatures == 0);

							printf(xorstr_("[+] %p[%u] created. Func on %p\n"), hThreadFeatures, GetThreadId(hThreadFeatures), FeaturesRoutine);

							BOOL bContextRetFeatures = FALSE;

							CONTEXT ctxFeatures;
							ctxFeatures.ContextFlags = CONTEXT_ALL;
							bContextRetFeatures = GetThreadContext(hThreadFeatures, &ctxFeatures);
							assert(bContextRetFeatures);

#ifdef _WIN64
							ctxFeatures.Rcx = (DWORD64)FeaturesRoutine;
#else
							ctxFeatures.Eax = (DWORD)FeaturesRoutine;
#endif

							bContextRetFeatures = SetThreadContext(hThreadFeatures, &ctxFeatures);
							assert(bContextRetFeatures);

							printf(xorstr_("[+] Context changed!\n"));

							auto dwResumeRetFeatures = ResumeThread(hThreadFeatures);
							assert(dwResumeRetFeatures != -1);

							printf(xorstr_("[+] Resumed!\n"));

							DWORD_PTR dwStartAddressFeatures = 0;
							ntStatFeatures = NtQueryInformationThread(hThreadFeatures, 9 /* ThreadQuerySetWin32StartAddress */, &dwStartAddressFeatures, sizeof(dwStartAddressFeatures), NULL);
							assert(ntStatFeatures == 0);

							printf(xorstr_("[+] Started on %p\n"), (LPVOID)dwStartAddressFeatures);
#pragma endregion

							mciSendString(xorstr_("play cheeto.mp3"), NULL, 0, NULL);

							globals::login = true;
							globals::window_update = false;

							globals::window_width = 315;
							globals::window_height = 465;
						}
						else {
							mciSendString(xorstr_("play chicken.mp3"), NULL, 0, NULL);
						}
					}
					nk_group_end(ctx);
				}
			}
			else
			{
				if (!globals::window_update)
				{
					globals::window_update = true;

					RECT desktop;
					const HWND hDesktop = GetDesktopWindow();
					GetWindowRect(hDesktop, &desktop);
					int horizontal = desktop.right;
					int vertical = desktop.bottom;

					MoveWindow(wnd, ((horizontal / 2) - (globals::window_width / 2)), ((vertical / 2) - (globals::window_height / 2)), globals::window_width, globals::window_height, TRUE);
				}

				nk_layout_row_dynamic(ctx, 50, 1);
				if (nk_group_begin(ctx, xorstr_("group1"), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
					nk_layout_row_dynamic(ctx, 50, 3);
					if (nk_group_begin(ctx, xorstr_("column1"), NULL)) {
						nk_layout_row_dynamic(ctx, 30, 1);
						if (nk_button_label(ctx, xorstr_("aim"))) {
							globals::aimtab = true;
							globals::visualtab = false;
							globals::misctab = false;

							std::cout << xorstr_("[+] Switched tab to: Aim") << std::endl;
						}
						nk_group_end(ctx);
					}
					if (nk_group_begin(ctx, xorstr_("column2"), NULL)) {
						nk_layout_row_dynamic(ctx, 30, 1);
						if (nk_button_label(ctx, xorstr_("visual"))) {
							globals::aimtab = false;
							globals::visualtab = true;
							globals::misctab = false;

							std::cout << xorstr_("[+] Switched tab to: Visual") << std::endl;
						}
						nk_group_end(ctx);
					}
					if (nk_group_begin(ctx, xorstr_("column3"), NULL)) {
						nk_layout_row_dynamic(ctx, 30, 1);
						if (nk_button_label(ctx, xorstr_("misc"))) {
							globals::aimtab = false;
							globals::visualtab = false;
							globals::misctab = true;

							std::cout << xorstr_("[+] Switched tab to: Misc") << std::endl;
						}
						nk_group_end(ctx);
					}
					nk_group_end(ctx);
				}

				nk_layout_row_dynamic(ctx, 315, 1);

				if (nk_group_begin(ctx, xorstr_("group2"), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
					nk_layout_row_static(ctx, 30, 265, 1);

					if (globals::aimtab) {
						if (globals::aimbot) {
							if (nk_button_label(ctx, xorstr_("Aimbot: ON"))) {
								globals::aimbot = false;

								std::cout << xorstr_("[-] Disabled: Aimbot") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Aimbot: OFF"))) {
								globals::aimbot = true;

								std::cout << xorstr_("[+] Enabled: Aimbot") << std::endl;
							}
						}

						if (globals::aimbot) {
							nk_layout_row_static(ctx, 15, 270, 1);

							char buffer[64];
							int ret = snprintf(buffer, sizeof buffer, "%f", globals::aimbotfov);

							std::string test = xorstr_("Aimbot Fov (");
							test += buffer;
							test += xorstr_(")");

							char cstr[64];
							strcpy(cstr, test.c_str());

							nk_label(ctx, cstr, NK_TEXT_ALIGN_LEFT);

							if (nk_slider_float(ctx, 0.0f, &globals::aimbotfov, 360.0f, 1.0f)) {
								std::cout << xorstr_("[+] Aimbot Fov Changed: ") << buffer << std::endl;
							}

							enum { m5, m4, m2, m1 };
							static int op = m2;
							static int prev_op = m2;
							nk_layout_row_dynamic(ctx, 30, 2);
							if (nk_option_label(ctx, xorstr_("Mouse 5"), op == m5)) {
								if (prev_op != op)
								{
									prev_op = op;

									std::cout << xorstr_("[+] Aimbot Mouse Changed: Mouse 5") << std::endl;
								}

								op = m5;

							}
							if (nk_option_label(ctx, xorstr_("Mouse 4"), op == m4)) {
								if (prev_op != op)
								{
									prev_op = op;

									std::cout << xorstr_("[+] Aimbot Mouse Changed: Mouse 4") << std::endl;
								}

								op = m4;
							}
							if (nk_option_label(ctx, xorstr_("Mouse 2"), op == m2)) {
								if (prev_op != op)
								{
									prev_op = op;

									std::cout << xorstr_("[+] Aimbot Mouse Changed: Mouse 2") << std::endl;
								}

								op = m2;
							}
							if (nk_option_label(ctx, xorstr_("Mouse 1"), op == m1)) {
								if (prev_op != op)
								{
									prev_op = op;

									std::cout << xorstr_("[+] Aimbot Mouse Changed: Mouse 1") << std::endl;
								}

								op = m1;
							}
							nk_layout_row_dynamic(ctx, 30, 1);
						}

						if (globals::recoil) {
							if (nk_button_label(ctx, xorstr_("No Recoil: ON"))) {
								globals::recoil = false;
								std::cout << xorstr_("[-] Disabled: No Recoil") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("No Recoil: OFF"))) {
								globals::recoil = true;
								std::cout << xorstr_("[+] Enabled: No Recoil") << std::endl;
							}
						}

						if (globals::recoil) {
							nk_layout_row_static(ctx, 15, 270, 1);

							char buffer[64];
							int ret = snprintf(buffer, sizeof buffer, "%f", globals::recoilmultiplier);

							std::string test = xorstr_("Recoil Multiplier (");
							test += buffer;
							test += xorstr_(")");

							char cstr[64];
							strcpy(cstr, test.c_str());

							nk_label(ctx, cstr, NK_TEXT_ALIGN_LEFT);

							if (nk_slider_float(ctx, 0.0f, &globals::recoilmultiplier, 1.0f, 0.01f)) {
								std::cout << xorstr_("[+] Recoil Multiplier Changed: ") << buffer << std::endl;
							}

							nk_layout_row_dynamic(ctx, 30, 1);
						}

						if (globals::spread) {
							if (nk_button_label(ctx, xorstr_("No Spread: ON"))) {
								globals::spread = false;
								std::cout << xorstr_("[-] Disabled: No Spread") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("No Spread: OFF"))) {
								globals::spread = true;
								std::cout << xorstr_("[+] Enabled: No Spread") << std::endl;
							}
						}

						if (globals::spread) {
							nk_layout_row_static(ctx, 15, 270, 1);

							char buffer[64];
							int ret = snprintf(buffer, sizeof buffer, "%f", globals::spreadmultiplier);

							std::string test = xorstr_("Spread Multiplier (");
							test += buffer;
							test += xorstr_(")");

							char cstr[64];
							strcpy(cstr, test.c_str());

							nk_label(ctx, cstr, NK_TEXT_ALIGN_LEFT);

							if (nk_slider_float(ctx, 0.0f, &globals::spreadmultiplier, 1.0f, 0.01f)) {
								std::cout << xorstr_("[+] Spread Multiplier Changed: ") << buffer << std::endl;
							}

							nk_layout_row_dynamic(ctx, 30, 1);
						}
					}

					if (globals::visualtab) {
						if (globals::esp) {
							if (nk_button_label(ctx, xorstr_("Cav Esp: ON"))) {
								globals::esp = false;
								std::cout << xorstr_("[-] Disabled: Cav Esp") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Cav Esp: OFF"))) {
								globals::esp = true;
								std::cout << xorstr_("[+] Enabled: Cav Esp") << std::endl;
							}
						}
						if (globals::glow) {
							if (nk_button_label(ctx, xorstr_("Glow Esp: ON"))) {
								globals::glow = false;
								std::cout << xorstr_("[-] Disabled: Glow Esp") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Glow Esp: OFF"))) {
								globals::glow = true;
								std::cout << xorstr_("[+] Enabled: Glow Esp") << std::endl;
							}
						}

						if (globals::glow) {
							nk_layout_row_dynamic(ctx, 20, 1);
							nk_label(ctx, xorstr_("Glow Color"), NK_TEXT_LEFT);
							nk_layout_row_dynamic(ctx, 120, 1);
							bg = nk_color_picker(ctx, bg, NK_RGBA);
							nk_layout_row_dynamic(ctx, 25, 1);
							bg.r = nk_propertyf(ctx, xorstr_("#R:"), 0, bg.r, 1.0f, 0.01f, 0.005f);
							bg.g = nk_propertyf(ctx, xorstr_("#G:"), 0, bg.g, 1.0f, 0.01f, 0.005f);
							bg.b = nk_propertyf(ctx, xorstr_("#B:"), 0, bg.b, 1.0f, 0.01f, 0.005f);
						}
					}

					if (globals::misctab) {
						if (globals::tp) {
							if (nk_button_label(ctx, xorstr_("Tp: ON"))) {
								globals::tp = false;
								std::cout << xorstr_("[-] Disabled: Tp") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Tp: OFF"))) {
								globals::tp = true;
								std::cout << xorstr_("[+] Enabled: Tp") << std::endl;
							}
						}

						if (globals::speed) {
							if (nk_button_label(ctx, xorstr_("Speed: ON"))) {
								globals::speed = false;
								std::cout << xorstr_("[-] Disabled: Speed") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Speed: OFF"))) {
								globals::speed = true;
								std::cout << xorstr_("[+] Enabled: Speed") << std::endl;
							}
						}

						if (globals::clip) {
							if (nk_button_label(ctx, xorstr_("No-Clip: ON"))) {
								globals::clip = false;
								std::cout << xorstr_("[-] Disabled: No-Clip") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("No-Clip: OFF"))) {
								globals::clip = true;
								std::cout << xorstr_("[+] Enabled: No-Clip") << std::endl;
							}
						}
						if (globals::damage) {
							if (nk_button_label(ctx, xorstr_("Damage Multiplier: ON"))) {
								globals::damage = false;
								std::cout << xorstr_("[-] Disabled: Damage Multiplier") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Damage Multiplier: OFF"))) {
								globals::damage = true;
								std::cout << xorstr_("[+] Enabled: Damage Multiplier") << std::endl;
							}
						}
						if (globals::fov) {
							if (nk_button_label(ctx, xorstr_("Custom Fov: ON"))) {
								globals::fov = false;
								std::cout << xorstr_("[-] Disabled: Custom Fov") << std::endl;
							}
						}
						else {
							if (nk_button_label(ctx, xorstr_("Custom Fov: OFF"))) {
								globals::fov = true;
								std::cout << xorstr_("[+] Enabled: Custom Fov") << std::endl;
							}
						}

						if (globals::fov) {
							nk_layout_row_static(ctx, 15, 270, 1);

							char buffer[64];
							int ret = snprintf(buffer, sizeof buffer, "%f", globals::customfov);

							std::string test = xorstr_("Fov Multiplier (");
							test += buffer;
							test += xorstr_(")");

							char cstr[64];
							strcpy(cstr, test.c_str());

							nk_label(ctx, cstr, NK_TEXT_ALIGN_LEFT);

							if (nk_slider_float(ctx, 0.0f, &globals::customfov, 180.0f, 1.0f)) {
								std::cout << xorstr_("[+] Custom Fov Changed: ") << buffer << std::endl;
							}

							nk_layout_row_dynamic(ctx, 30, 1);
						}
					}

					nk_group_end(ctx);
				}

				nk_layout_row_dynamic(ctx, 30, 2); // wrapping row

				if (nk_group_begin(ctx, xorstr_("column1"), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) { // column 1
					nk_layout_row_static(ctx, 20, 120, 1);

					if (nk_button_label(ctx, xorstr_("help"))) {
						mciSendString(xorstr_("play table.mp3"), NULL, 0, NULL);

						std::cout << xorstr_("[?] https://yourwebsitehere.com/support") << std::endl;
					}

					nk_group_end(ctx);
				}

				if (nk_group_begin(ctx, xorstr_("column2"), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) { // column 2
					nk_layout_row_static(ctx, 20, 120, 1);

					if (globals::debug) {
						if (nk_button_label(ctx, xorstr_("Debug: ON"))) {
							globals::debug = false;
							::ShowWindow(::GetConsoleWindow(), SW_HIDE);
						}
					}
					else {
						if (nk_button_label(ctx, xorstr_("Debug: OFF"))) {
							globals::debug = true;
							::ShowWindow(::GetConsoleWindow(), SW_SHOW);
						}
					}

					nk_group_end(ctx);
				}
			}
			nk_layout_row_static(ctx, 30, 275, 1);

			nk_label(ctx, xorstr_("Press END to close the cheeto."), NK_TEXT_ALIGN_CENTERED);
		}
		nk_end(ctx);

		/* Draw */
		globals::context->ClearRenderTargetView(globals::rt_view, &bg.r);
		globals::context->OMSetRenderTargets(1, &globals::rt_view, NULL);
		nk_d3d11_render(globals::context, NK_ANTI_ALIASING_ON);
		hr = globals::swap_chain->Present(1, 0);
		if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
			MessageBoxW(NULL, xorstr_(L"D3D11 device is lost or removed!"), xorstr_(L"Error"), 0);
			break;
		}
		else if (hr == DXGI_STATUS_OCCLUDED) {

		}
		assert(SUCCEEDED(hr));
	}

	globals::context->ClearState();
	nk_d3d11_shutdown();
	globals::rt_view->Release();
	globals::context->Release();
	globals::device->Release();
	globals::swap_chain->Release();
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}

int main()
{
	SetConsoleTextAttribute(globals::hConsole, 14);
	std::cout << xorstr_("[+] Debugging started.") << std::endl;
	::ShowWindow(::GetConsoleWindow(), SW_HIDE);

#pragma region GuiRoutine
	auto hNtdll = LoadLibraryA(xorstr_("ntdll"));
	assert(hNtdll);

	auto NtCreateThreadEx = (lpNtCreateThreadEx)GetProcAddress(hNtdll, xorstr_("NtCreateThreadEx"));
	assert(NtCreateThreadEx);

	auto NtQueryInformationThread = (lpNtQueryInformationThread)GetProcAddress(hNtdll, xorstr_("NtQueryInformationThread"));
	assert(NtQueryInformationThread);

	NTSTATUS ntStatGui = 0;

	HANDLE hThreadGui = INVALID_HANDLE_VALUE;
	ntStatGui = NtCreateThreadEx(&hThreadGui, THREAD_ALL_ACCESS, 0, GetCurrentProcess(), (LPTHREAD_START_ROUTINE)0 /* Start address */, 0, 0x1 /* Suspended */, 0, 0, 0, 0);
	assert(ntStatGui == 0);

	printf(xorstr_("[+] %p[%u] created. Func on %p\n"), hThreadGui, GetThreadId(hThreadGui), GuiRoutine);

	BOOL bContextRetGui = FALSE;

	CONTEXT ctxGui;
	ctxGui.ContextFlags = CONTEXT_ALL;
	bContextRetGui = GetThreadContext(hThreadGui, &ctxGui);
	assert(bContextRetGui);

#ifdef _WIN64
	ctxGui.Rcx = (DWORD64)GuiRoutine;
#else
	ctxGui.Eax = (DWORD)GuiRoutine;
#endif

	bContextRetGui = SetThreadContext(hThreadGui, &ctxGui);
	assert(bContextRetGui);

	printf(xorstr_("[+] Context changed!\n"));

	auto dwResumeRetGui = ResumeThread(hThreadGui);
	assert(dwResumeRetGui != -1);

	printf(xorstr_("[+] Resumed!\n"));

	DWORD_PTR dwStartAddressGui = 0;
	ntStatGui = NtQueryInformationThread(hThreadGui, 9 /* ThreadQuerySetWin32StartAddress */, &dwStartAddressGui, sizeof(dwStartAddressGui), NULL);
	assert(ntStatGui == 0);

	printf(xorstr_("[+] Started on %p\n"), (LPVOID)dwStartAddressGui);
#pragma endregion

	while (TRUE) {
		if (GetAsyncKeyState(VK_END)) {
			driver::disconnect(globals::connection);
			driver::deinitialize();
			exit(0);
		}
	}

	return 0;
}