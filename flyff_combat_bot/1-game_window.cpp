// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com


#include "game_window.h"

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <cwctype>
#include <cwchar>

namespace
{
	bool title_is_flyff(const std::wstring &title)
	{
		// exact: old-style title. suffix: new-style "<character name> - Flyff Universe ...".
		// comparison is case-insensitive to match FindWindowW behavior.
		static const wchar_t *exact_titles[] = { L"flyff universe - google chrome", L"flyff universe - chromium" };
		static const wchar_t *title_suffixes[] = { L" - flyff universe - google chrome", L" - flyff universe - chromium" };
		std::wstring lowered;
		lowered.reserve(title.size());
		for (wchar_t ch : title)
			lowered.push_back(static_cast<wchar_t>(towlower(static_cast<wint_t>(ch))));
		for (const wchar_t *exact : exact_titles)
		{
			if (lowered == exact)
				return true;
		}
		for (const wchar_t *suffix : title_suffixes)
		{
			const size_t len = wcslen(suffix);
			if (lowered.size() > len && lowered.compare(lowered.size() - len, len, suffix) == 0)
				return true;
		}
		return false;
	}
}

HWND find_flyff_window()
{
	// Fail closed on ambiguity: proceed only when exactly one window matches.
	// Returning nullptr on 0 or 2+ matches keeps every existing caller on its
	// current not-found path, and the bot can never switch targets mid-run.
	std::vector<std::pair<HWND, std::wstring>> matches;
	EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
		wchar_t title[512];
		if (GetWindowTextW(hwnd, title, 512) > 0 && title_is_flyff(title))
		{
			reinterpret_cast<std::vector<std::pair<HWND, std::wstring>> *>(lparam)->emplace_back(hwnd, title);
		}
		return TRUE; // always enumerate everything so multiple matches are detected
	}, reinterpret_cast<LPARAM>(&matches));
	if (matches.size() > 1)
	{
		std::cout << "Found more than one Flyff Universe window:" << std::endl;
		for (const auto &match : matches)
		{
			std::wcout << L" - " << match.second << std::endl;
		}
		std::cout << "Keep only the one you want the bot to use (close the other Flyff windows or tabs), then press enter in this window to try again." << std::endl;
		return nullptr;
	}
	return matches.empty() ? nullptr : matches[0].first;
}


x_y_cords check_x_y_cords(int windowxcord, int windowycord, int windowwidth, int windowheight, RECT supportwindowrect_size)
{
	if (windowxcord < 0)
		windowxcord = 0;
	if (windowycord < 0)
		windowycord = 0;
	if (windowxcord + windowwidth > supportwindowrect_size.right)
		windowwidth = supportwindowrect_size.right - windowxcord;
	if (windowycord + windowheight > supportwindowrect_size.bottom)
		windowheight = supportwindowrect_size.bottom - windowycord;
	x_y_cords item{};
	item.windowxcord = windowxcord;
	item.windowycord = windowycord;
	item.windowwidth = windowwidth;
	item.windowheight = windowheight;
	item.supportwindowrect_size = supportwindowrect_size;
	return item;
}



cv::Mat flyff::window::get_mat(bool cursor)
{
	HWND supportwindow = find_flyff_window();
	if (!supportwindow)
	{
		std::cout << "Unable to find a Flyff Chrome Window \n"
		<< "Log in and press enter in this window to try again \n";
		std::cin.ignore();
		supportwindow = find_flyff_window();
		if (supportwindow)
		{
			std::cout << "Found the window, switch to the GUI to continue. \n";
		}
	}
	HDC hwindowDC, hwindowCompatibleDC;
	HBITMAP hbwindow;
	BITMAPINFOHEADER  bi;
	hwindowDC = GetDC(NULL);
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);

	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);
	// needs error handling
	RECT supportwindowrect;
	GetWindowRect(supportwindow, &supportwindowrect);

	int h = supportwindowrect.bottom;
	int w = supportwindowrect.right;
	int height = h, width = w, srcheight = h, srcwidth = w;
	if (width < 0 && height < 0)
	{
		std::cout << "Unable to use the Flyff window, it should NOT be minimized \n"
		<< "Fix it and press enter to try again \n";
		std::cin.ignore();
		GetWindowRect(supportwindow, &supportwindowrect);

		h = supportwindowrect.bottom;
		w = supportwindowrect.right;
		height = h, width = w, srcheight = h, srcwidth = w;
	}
	window_.create(height, width, CV_8UC4);

	// create a bitmap
	hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);
	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !

	if (cursor == true)
	{
		// Draw the cursor
		CURSORINFO cinfo;
		cinfo.cbSize = sizeof(cinfo);
		GetCursorInfo(&cinfo);
		if (cinfo.hCursor && (cinfo.flags & CURSOR_SHOWING))
		{
			ICONINFO iinfo;
			if (GetIconInfo((HICON)cinfo.hCursor, &iinfo))
			{
				int x = cinfo.ptScreenPos.x - iinfo.xHotspot;
				int y = cinfo.ptScreenPos.y - iinfo.yHotspot;
				HICON hIcon = CopyIcon((HICON)cinfo.hCursor);
				DrawIcon(hwindowCompatibleDC, x, y, hIcon);
				DestroyIcon(hIcon);
				DeleteObject(iinfo.hbmMask);
				DeleteObject(iinfo.hbmColor);
			}
		}
	}

	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, window_.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(supportwindow, hwindowDC);
	return window_;
}


cv::Mat flyff::window::get_and_convert_mat(cv::ColorConversionCodes color)
{
	flyff::window::get_mat();
	cv::cvtColor(window_, window_, color);
	return window_;
}


void flyff::window::free()
{
	window_.release();
}


Flyff_screen_cords get_flyff_char_screen_cords()
{
	RECT supportwindowrect_size{};
	HWND supportwindow_size = find_flyff_window();
	GetWindowRect(supportwindow_size, &supportwindowrect_size);
	int end_xcord = static_cast<int>(supportwindowrect_size.right * 0.2);
	int end_ycord = static_cast<int>(supportwindowrect_size.bottom * 0.26);
	Flyff_screen_cords cords{};
	cords.start_x = 0;
	cords.start_y = 0;
	cords.end_x = end_xcord;
	cords.end_y = end_ycord;
	return cords;
}

Flyff_screen_cords get_flyff_map_cords()
{
	RECT supportwindowrect_size{};
	HWND supportwindow_size = find_flyff_window();
	GetWindowRect(supportwindow_size, &supportwindowrect_size);
	int xoffset = static_cast<int>(supportwindowrect_size.right * 0.2);
	int start_xcord = static_cast<int>(supportwindowrect_size.right - xoffset);
	int end_xcord = supportwindowrect_size.right;
	int end_ycord = static_cast<int>(supportwindowrect_size.bottom * 0.3);

	Flyff_screen_cords cords{};
	cords.start_x = start_xcord;
	cords.start_y = 0;
	cords.end_x = end_xcord;
	cords.end_y = end_ycord;
	return cords;
}

int get_bottom_of_screen_y_cord()
{
	RECT supportwindowrect_size;
	HWND supportwindow_size = find_flyff_window();
	GetWindowRect(supportwindow_size, &supportwindowrect_size);
	int windowycord = static_cast<int>(supportwindowrect_size.bottom * 0.9);
	return windowycord;
}


center_window_data flyff::window::get_mat_from_center_of_window(const double x, const double y, const double w, const double h)
{
	cv::Mat img, img2;
	RECT supportwindowrect_size;
	HWND supportwindow_size = find_flyff_window();
	GetWindowRect(supportwindow_size, &supportwindowrect_size);
	int windowxcord = static_cast<int>(supportwindowrect_size.right * x);
	int windowycord = static_cast<int>(supportwindowrect_size.bottom * y);
	int windowwidth = static_cast<int>(supportwindowrect_size.right * w);
	int windowheight = static_cast<int>(supportwindowrect_size.bottom * h);
	x_y_cords cords = check_x_y_cords(windowxcord, windowycord, windowwidth, windowheight, supportwindowrect_size);
	center_window_data_.win_xcord = cords.windowxcord;
	center_window_data_.win_ycord = cords.windowycord;
	img = window_;
	img2 = img(cv::Rect(cords.windowxcord, cords.windowycord, cords.windowwidth, cords.windowheight));
	cv::cvtColor(img2, img2, cv::COLOR_BGRA2BGR);
	center_window_data_.img = img2;
	return center_window_data_;
}
