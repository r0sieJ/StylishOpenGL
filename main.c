#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <stdio.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <Uxtheme.h>

#define OPT_ENABLE_BORDER TRUE
#define OPT_ENABLE_BLUR TRUE

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Light mode

#define BUTTONACTIVECLOSE 7
#define BUTTONINACTIVECLOSE 8
#define BUTTONACTIVECAPTION 3
#define BUTTONINACTIVECAPTION 4

#define BUTTONCLOSEGLYPH96 11
#define BUTTONCLOSEGLYPH120 12
#define BUTTONCLOSEGLYPH144 13
#define BUTTONCLOSEGLYPH192 14
#define BUTTONHELPGLYPH96 15
#define BUTTONHELPGLYPH120 16
#define BUTTONHELPGLYPH144 17
#define BUTTONHELPGLYPH192 18
#define BUTTONMAXGLYPH96 19
#define BUTTONMAXGLYPH120 20
#define BUTTONMAXGLYPH144 21
#define BUTTONMAXGLYPH192 22
#define BUTTONMINGLYPH96 23
#define BUTTONMINGLYPH120 24
#define BUTTONMINGLYPH144 25
#define BUTTONMINGLYPH192 26
#define BUTTONRESTOREGLYPH96 27
#define BUTTONRESTOREGLYPH120 28
#define BUTTONRESTOREGLYPH144 29
#define BUTTONRESTOREGLYPH192 30

// Dark mode

#define BUTTONACTIVECAPTIONDARK 88
#define BUTTONINACTIVECAPTIONDARK 89

#define BUTTONCLOSEGLYPH96DARK 64
#define BUTTONCLOSEGLYPH120DARK 65
#define BUTTONCLOSEGLYPH144DARK 66
#define BUTTONCLOSEGLYPH192DARK 67
#define BUTTONHELPGLYPH96DARK 68
#define BUTTONHELPGLYPH120DARK 69
#define BUTTONHELPGLYPH144DARK 70
#define BUTTONHELPGLYPH192DARK 71
#define BUTTONMAXGLYPH96DARK 72
#define BUTTONMAXGLYPH120DARK 73
#define BUTTONMAXGLYPH144DARK 74
#define BUTTONMAXGLYPH192DARK 75
#define BUTTONMINGLYPH96DARK 76
#define BUTTONMINGLYPH120DARK 77
#define BUTTONMINGLYPH144DARK 78
#define BUTTONMINGLYPH192DARK 79
#define BUTTONRESTOREGLYPH96DARK 80
#define BUTTONRESTOREGLYPH120DARK 81
#define BUTTONRESTOREGLYPH144DARK 82
#define BUTTONRESTOREGLYPH192DARK 83

typedef struct
{
	float alpha;
	RECT bgRect;
	RECT iconRect;
} ButtonInfo_t;

const COLORREF g_defaultBorder = 0x232323;

SIZE g_atlasSize;
ButtonInfo_t g_buttons[3];
int g_hoveredId;
int g_lastHoveredId;
GLuint g_atlasTex;
COLORREF g_accentColor;

BOOL IsWindowMaximized(HWND hWnd)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);

	GetWindowPlacement(hWnd, &wp);

	return wp.showCmd == SW_MAXIMIZE;
}

void LoadTheme(HWND hWnd)
{
	HTHEME theme = OpenThemeData(hWnd, L"DWMWindow");
	HMODULE themeLib = LoadLibraryEx(L"C:\\Windows\\Resources\\Themes\\aero\\aero.msstyles",
		0, LOAD_LIBRARY_AS_DATAFILE);

	void* buf;
	DWORD bufSize;
	GetThemeStream(theme, 0, 0, TMT_DISKSTREAM, &buf, &bufSize, themeLib);

	int width, height, comp;
	void* data = stbi_load_from_memory(buf, bufSize, &width, &height, &comp, 4);

	g_atlasSize.cx = width;
	g_atlasSize.cy = height;

	glGenTextures(1, &g_atlasTex);
	glBindTexture(GL_TEXTURE_2D, g_atlasTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	g_buttons[0].alpha = 0.f;
	GetThemeRect(theme, BUTTONACTIVECLOSE, 1, TMT_ATLASRECT, &g_buttons[0].bgRect);
	GetThemeRect(theme, BUTTONCLOSEGLYPH96DARK, 1, TMT_ATLASRECT, &g_buttons[0].iconRect);

	g_buttons[1].alpha = 0.f;
	GetThemeRect(theme, BUTTONACTIVECAPTIONDARK, 1, TMT_ATLASRECT, &g_buttons[1].bgRect);
	GetThemeRect(theme, BUTTONMAXGLYPH96DARK, 1, TMT_ATLASRECT, &g_buttons[1].iconRect);

	g_buttons[2].alpha = 0.f;
	GetThemeRect(theme, BUTTONACTIVECAPTIONDARK, 1, TMT_ATLASRECT, &g_buttons[2].bgRect);
	GetThemeRect(theme, BUTTONMINGLYPH96DARK, 1, TMT_ATLASRECT, &g_buttons[2].iconRect);

	typedef DWORD (WINAPI *pFnGetImmersiveColorFromColorSetEx)
		(DWORD dwImmersiveColorSet, DWORD dwImmersiveColorType,
		BOOL bIgnoreHighContrast, BOOL dwHighContrastCacheMode);

	typedef DWORD(WINAPI* pFnGetImmersiveColorTypeFromName)(LPCWSTR pName);

	typedef DWORD(WINAPI* pFnGetImmersiveUserColorSetPreference)
		(BOOL bForceCheckRegistry, BOOL bSkipCheckOnFail);

	HMODULE module = LoadLibrary(L"uxtheme.dll");

	pFnGetImmersiveColorFromColorSetEx getImmersiveColorFromColorSetEx
		= GetProcAddress(module, "GetImmersiveColorFromColorSetEx");

	// This function is missing a symbol
	pFnGetImmersiveColorTypeFromName getImmersiveColorTypeFromName
		= GetProcAddress(module, 96);

	pFnGetImmersiveUserColorSetPreference getImmersiveUserColorSetPreference
		= GetProcAddress(module, "GetImmersiveUserColorSetPreference");

	HKEY key;
	RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, KEY_READ, &key);

	DWORD size = sizeof(DWORD);
	DWORD value;
	RegQueryValueEx(key, L"ColorPrevalence", 0, NULL, &value, &size);

	RegCloseKey(key);

	if (value)
	{
		g_accentColor = getImmersiveColorFromColorSetEx(
			getImmersiveUserColorSetPreference(FALSE, FALSE),
			getImmersiveColorTypeFromName(L"ImmersiveStartSelectionBackground"),
			FALSE, 0
		);
	}
	else
	{
		g_accentColor = g_defaultBorder;
	}

	typedef struct
	{
		int nAccentState;
		int nFlags;
		int nColor;
		int nAnimationId;
	} ACCENTPOLICY_t;

	typedef struct
	{
		int nAttribute;
		PVOID pData;
		ULONG ulDataSize;
	} WINCOMPATTRDATA_t;

	typedef BOOL(WINAPI* pFnSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA_t*);
	const pFnSetWindowCompositionAttribute SetWindowCompositionAttribute
		= GetProcAddress(LoadLibrary(L"user32.dll"), "SetWindowCompositionAttribute");

	if (OPT_ENABLE_BLUR)
	{
		ACCENTPOLICY_t policy = { 3, 2, 0xAA000000, 0 }; // ACCENT_ENABLE_BLURBEHIND=3...
		WINCOMPATTRDATA_t data = { 19, &policy, sizeof(ACCENTPOLICY_t) }; // WCA_ACCENT_POLICY=19
		SetWindowCompositionAttribute(hWnd, &data);
	}

	CloseThemeData(theme);
	FreeLibrary(themeLib);
}

RECT GetCaptionButtonRect(HWND hWnd, int order, int buttonId)
{
	RECT rect;
	GetClientRect(hWnd, &rect);

	RECT capt;
	DwmGetWindowAttribute(hWnd, DWMWA_CAPTION_BUTTON_BOUNDS, &capt, sizeof(RECT));

	ButtonInfo_t info = g_buttons[buttonId];

	int buttonWidth = info.bgRect.right - info.bgRect.left - 5;

	int btn_x1 = rect.right - buttonWidth * (order + 1);
	int btn_x2 = rect.right - buttonWidth * order;

	int bgHeight = (info.bgRect.bottom - info.bgRect.top) / 4;

	int btn_y1 = capt.top;
	int btn_y2 = capt.bottom;

	return (RECT) { btn_x1, btn_y1, btn_x2, btn_y2 };
}

void DrawCaptionButton(HWND hWnd, int order, int buttonId, int state)
{
	ButtonInfo_t info = g_buttons[buttonId];

	RECT btnRect = GetCaptionButtonRect(hWnd, order, buttonId);

	int btn_x1 = btnRect.left;
	int btn_x2 = btnRect.right;
	int btn_y1 = btnRect.top;
	int btn_y2 = btnRect.bottom;

	int bgHeight = (info.bgRect.bottom - info.bgRect.top) / 4;
	
	float bg_u1 = (info.bgRect.left + 4) / (float)g_atlasSize.cx;
	float bg_u2 = (info.bgRect.right - 4) / (float)g_atlasSize.cx;
	float bg_v1 = (info.bgRect.top + bgHeight * state + 4) / (float)g_atlasSize.cy;
	float bg_v2 = (info.bgRect.top + bgHeight * (state + 1) - 4) / (float)g_atlasSize.cy;

	int iconHeight = (info.iconRect.bottom - info.iconRect.top) / 4;
	int iconWidth = info.iconRect.right - info.iconRect.left;

	int btnWidth = info.bgRect.right - info.bgRect.left - 6;
	int btnHeight = btn_y2 - btn_y1;

	float i_u1 = info.iconRect.left / (float)g_atlasSize.cx;
	float i_u2 = info.iconRect.right / (float)g_atlasSize.cx;
	float i_v1 = info.iconRect.top / (float)g_atlasSize.cy;
	float i_v2 = (info.iconRect.top + iconHeight) / (float)g_atlasSize.cy;

	int i_x1 = btn_x1 + btnWidth / 2 - iconWidth / 2;
	int i_y1 = btn_y1 + btnHeight / 2 - iconHeight / 2;
	int i_x2 = btn_x1 + btnWidth / 2 + iconWidth / 2;
	int i_y2 = btn_y1 + btnHeight / 2 + iconHeight / 2;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, g_atlasTex);

	glBegin(GL_QUADS);

	glColor4f(1.f, 1.f, 1.f, 1.f);

	// BG

	glTexCoord2f(bg_u1, bg_v1);
	glVertex2i(btn_x1, btn_y1);

	glTexCoord2f(bg_u2, bg_v1);
	glVertex2i(btn_x2, btn_y1);

	glTexCoord2f(bg_u2, bg_v2);
	glVertex2i(btn_x2, btn_y2);

	glTexCoord2f(bg_u1, bg_v2);
	glVertex2i(btn_x1, btn_y2);

	// ICON

	glColor4f(1.f, 1.f, 1.f, (state != 0 || GetActiveWindow() == hWnd) ? 1.f : 0.5f);

	glTexCoord2f(i_u1, i_v1);
	glVertex2i(i_x1, i_y1);

	glTexCoord2f(i_u2, i_v1);
	glVertex2i(i_x2, i_y1);

	glTexCoord2f(i_u2, i_v2);
	glVertex2i(i_x2, i_y2);

	glTexCoord2f(i_u1, i_v2);
	glVertex2i(i_x1, i_y2);

	glEnd();

	glDisable(GL_TEXTURE_2D);
}

void Repaint(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(hWnd, &pt);

	if ((GetKeyState(VK_LBUTTON) & 0x100) == 0)
	{
		g_hoveredId = -1;

		for (int i = 0; i < 3; i++)
		{
			RECT rect = GetCaptionButtonRect(hWnd, i, i);
			if (PtInRect(&rect, pt))
				g_hoveredId = i;
		}

		g_lastHoveredId = g_hoveredId;
	}
	else
	{
		g_hoveredId = -1;

		RECT rect = GetCaptionButtonRect(hWnd, g_lastHoveredId, g_lastHoveredId);
		if (PtInRect(&rect, pt))
			g_hoveredId = g_lastHoveredId;
	}

	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	RECT wr;
	GetClientRect(hWnd, &wr);

	glBegin(GL_TRIANGLES);

	glColor4f(1.f, 1.f, 1.f, 1.f);

	glVertex2i(wr.right / 2, wr.bottom / 3);
	glVertex2i(wr.right / 3 * 2, wr.bottom / 3 * 2);
	glVertex2i(wr.right / 3, wr.bottom / 3 * 2);

	glEnd();

	DrawCaptionButton(hWnd, 0, 0, g_hoveredId == 0 ? (((GetKeyState(VK_LBUTTON) & 0x100) != 0) ? 2 : 1) : 0);
	DrawCaptionButton(hWnd, 1, 1, g_hoveredId == 1 ? (((GetKeyState(VK_LBUTTON) & 0x100) != 0) ? 2 : 1) : 0);
	DrawCaptionButton(hWnd, 2, 2, g_hoveredId == 2 ? (((GetKeyState(VK_LBUTTON) & 0x100) != 0) ? 2 : 1) : 0);

	COLORREF color = GetActiveWindow() == hWnd ? g_accentColor : g_defaultBorder;

	int red = GetRValue(color);
	int green = GetGValue(color);
	int blue = GetBValue(color);

	glColor4f(red / 255.f, green / 255.f, blue / 255.f, 1.f);

	if (!IsWindowMaximized(hWnd) && OPT_ENABLE_BORDER)
	{
		glRecti(wr.left, wr.top, wr.left + 1, wr.bottom);
		glRecti(wr.right - 1, wr.top, wr.right, wr.bottom);
		glRecti(wr.left, wr.top, wr.right, wr.top + 1);
		glRecti(wr.left, wr.bottom - 1, wr.right, wr.bottom);
	}

	SwapBuffers(GetDC(hWnd));
}

LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wPar, LPARAM lPar)
{
	if (uMsg == WM_CREATE)
	{
		PIXELFORMATDESCRIPTOR pfd = { 0 };
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER | PFD_SUPPORT_COMPOSITION;
		pfd.cColorBits = 24;
		pfd.cStencilBits = 8;
		pfd.cDepthBits = 24;
		pfd.cAlphaBits = 8;

		HDC hdc = GetDC(hWnd);
		int pf = ChoosePixelFormat(hdc, &pfd);
		SetPixelFormat(hdc, pf, &pfd);

		HGLRC ctx = wglCreateContext(hdc);
		BOOL success = wglMakeCurrent(hdc, ctx);

		((BOOL(WINAPI*)(int))wglGetProcAddress("wglSwapIntervalEXT"))(1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		MARGINS margins = { 0, 0, 0, 1 };
		DwmExtendFrameIntoClientArea(hWnd, &margins);

		RECT rcClient;
		GetWindowRect(hWnd, &rcClient);

		SetWindowPos(hWnd,
			NULL,
			rcClient.left, rcClient.top,
			rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
			SWP_FRAMECHANGED);

		return 0;
	}

	if (uMsg == WM_SIZE)
	{
		glLoadIdentity();

		RECT rect;
		GetClientRect(hWnd, &rect);
		glViewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
		gluOrtho2D(rect.left, rect.right, rect.bottom, rect.top);
	}

	if (uMsg == WM_NCCALCSIZE && wPar == TRUE)
	{
		if (IsWindowMaximized(hWnd))
		{
			NCCALCSIZE_PARAMS* params = lPar;
			params->rgrc[0].left += 8;
			params->rgrc[0].right -= 8;
			params->rgrc[0].bottom -= 8;
			params->rgrc[0].top += 8;
		}

		return 0;
	}

	if (uMsg == WM_MOUSELEAVE)
		g_hoveredId = -1;

	if (uMsg == WM_NCHITTEST)
	{
		POINT pt = { GET_X_LPARAM(lPar), GET_Y_LPARAM(lPar) };
		ScreenToClient(hWnd, &pt);

		RECT capt;
		DwmGetWindowAttribute(hWnd, DWMWA_CAPTION_BUTTON_BOUNDS, &capt, sizeof(RECT));

		RECT wr;
		GetClientRect(hWnd, &wr);

		if (!IsWindowMaximized(hWnd))
		{
			const int border = 8;

			if (pt.x < border && pt.y < border)
				return HTTOPLEFT;

			if (pt.x < border && pt.y > wr.bottom - border)
				return HTBOTTOMLEFT;

			if (pt.x > wr.right - border && pt.y < border)
				return HTTOPRIGHT;

			if (pt.x > wr.right - border && pt.y > wr.bottom - border)
				return HTBOTTOMRIGHT;

			if (pt.x < border)
				return HTLEFT;

			if (pt.y < border)
				return HTTOP;

			if (pt.x > wr.right - border)
				return HTRIGHT;

			if (pt.y > wr.bottom - border)
				return HTBOTTOM;
		}

		if (pt.y < capt.bottom)
		{
			for (int i = 0; i < 3; i++)
			{
				RECT rect = GetCaptionButtonRect(hWnd, i, i);
				if (PtInRect(&rect, pt))
					return HTCLIENT;
			}

			return HTCAPTION;
		}

		return HTCLIENT;
	}

	if (uMsg == WM_PAINT)
	{
		Repaint(hWnd);

		return 0;
	}

	if (uMsg == WM_LBUTTONUP)
	{
		if (g_hoveredId == 0)
			DestroyWindow(hWnd);
		else if (g_hoveredId == 1)
			ShowWindow(hWnd, IsWindowMaximized(hWnd) ? SW_RESTORE : SW_MAXIMIZE);
		else if (g_hoveredId == 2)
			ShowWindow(hWnd, SW_MINIMIZE);
	}

	if (uMsg == WM_QUIT || uMsg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}

	if (uMsg == WM_SETFOCUS) {
		InvalidateRect(hWnd, NULL, FALSE);
		UpdateWindow(hWnd);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wPar, lPar);
}

int WinMain(HINSTANCE hInst, HINSTANCE prevInst, LPSTR lpCmd, int cmdShow)
{
	WNDCLASS wc = { 0 };
	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"MyClass";
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);

	RegisterClass(&wc);

	HWND hWnd = CreateWindow(L"MyClass", L"Window", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, wc.hInstance, NULL);

	LoadTheme(hWnd);

	ShowWindow(hWnd, SW_SHOW);

	MSG msg;
	while (TRUE) {
		BOOL ret = GetMessage(&msg, NULL, 0, 0);

		if (ret == 0 || ret == -1)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}