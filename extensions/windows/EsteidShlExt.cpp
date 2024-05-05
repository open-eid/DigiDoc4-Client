// EsteidShlExt.cpp : Implementation of CEsteidShlExt
// http://msdn.microsoft.com/en-us/library/bb757020.aspx

#include "EsteidShlExt.h"
#include "resource.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>

extern HINSTANCE instanceHandle;

typedef DWORD ARGB;

bool HasAlpha(ARGB *pargb, const SIZE &sizeImage, int cxRow)
{
	ULONG cxDelta = cxRow - sizeImage.cx;
	for(ULONG y = sizeImage.cy; y; --y)
	{
		for(ULONG x = sizeImage.cx; x; --x)
		{
			if(*pargb++ & 0xFF000000)
				return true;
		}
		pargb += cxDelta;
	}
	return false;
}

BITMAPINFO InitBitmapInfo(const SIZE &sizeImage)
{
	BITMAPINFO pbmi{{sizeof(BITMAPINFOHEADER)}};
	pbmi.bmiHeader.biPlanes = 1;
	pbmi.bmiHeader.biCompression = BI_RGB;
	pbmi.bmiHeader.biWidth = sizeImage.cx;
	pbmi.bmiHeader.biHeight = sizeImage.cy;
	pbmi.bmiHeader.biBitCount = 32;
	return pbmi;
}

HRESULT ConvertToPARGB32(HDC hdc, ARGB *pargb, HBITMAP hbmp, const SIZE &sizeImage, int cxRow)
{
	BITMAPINFO bmi = InitBitmapInfo(sizeImage);
	HRESULT hr = E_OUTOFMEMORY;
	HANDLE hHeap = GetProcessHeap();
	if (void *pvBits = HeapAlloc(hHeap, 0, bmi.bmiHeader.biWidth * 4 * bmi.bmiHeader.biHeight))
	{
		hr = E_UNEXPECTED;
		if (GetDIBits(hdc, hbmp, 0, bmi.bmiHeader.biHeight, pvBits, &bmi, DIB_RGB_COLORS) == bmi.bmiHeader.biHeight)
		{
			ULONG cxDelta = cxRow - bmi.bmiHeader.biWidth;
			ARGB *pargbMask = static_cast<ARGB *>(pvBits);
			for (ULONG y = bmi.bmiHeader.biHeight; y; --y)
			{
				for (ULONG x = bmi.bmiHeader.biWidth; x; --x)
				{
					if (*pargbMask++) // transparent pixel
						*pargb++ = 0;
					else // opaque pixel
						*pargb++ |= 0xFF000000;
				}
				pargb += cxDelta;
			}
			hr = S_OK;
		}
		HeapFree(hHeap, 0, pvBits);
	}
	return hr;
}

HRESULT ConvertBufferToPARGB32(HPAINTBUFFER hPaintBuffer, HDC hdc, HICON hicon, const SIZE &sizeIcon)
{
	RGBQUAD *prgbQuad;
	int cxRow = 0;
	HRESULT hr = GetBufferedPaintBits(hPaintBuffer, &prgbQuad, &cxRow);
	if (SUCCEEDED(hr))
	{
		ARGB *pargb = reinterpret_cast<ARGB *>(prgbQuad);
		if (!HasAlpha(pargb, sizeIcon, cxRow))
		{
			ICONINFO info = {};
			if (GetIconInfo(hicon, &info))
			{
				if (info.hbmMask)
					hr = ConvertToPARGB32(hdc, pargb, info.hbmMask, sizeIcon, cxRow);
				DeleteObject(info.hbmColor);
				DeleteObject(info.hbmMask);
			}
		}
	}
	return hr;
}

CEsteidShlExt::CEsteidShlExt()
{
	const SIZE sizeIcon { GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) };
	if(HICON hIcon = (HICON)LoadImage(instanceHandle, MAKEINTRESOURCE(IDB_DIGIDOCICO), IMAGE_ICON, sizeIcon.cx, sizeIcon.cy, LR_DEFAULTCOLOR|LR_CREATEDIBSECTION))
	{
		if(HDC hdcDest = CreateCompatibleDC(nullptr))
		{
			BITMAPINFO bmi = InitBitmapInfo(sizeIcon);
			if((m_DigidocBmp = CreateDIBSection(hdcDest, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0)))
			{
				if(HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcDest, m_DigidocBmp))
				{
					RECT rcIcon = { 0, 0, sizeIcon.cx, sizeIcon.cy };
					BLENDFUNCTION bfAlpha = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
					BP_PAINTPARAMS paintParams = { sizeof(paintParams), BPPF_ERASE, nullptr, &bfAlpha };
					HDC hdcBuffer;
					if(HPAINTBUFFER hPaintBuffer = BeginBufferedPaint(hdcDest, &rcIcon, BPBF_DIB, &paintParams, &hdcBuffer))
					{
						if(DrawIconEx(hdcBuffer, 0, 0, hIcon, sizeIcon.cx, sizeIcon.cy, 0, nullptr, DI_NORMAL))
						{
							// If icon did not have an alpha channel, we need to convert buffer to PARGB.
							ConvertBufferToPARGB32(hPaintBuffer, hdcDest, hIcon, sizeIcon);
						}
						EndBufferedPaint(hPaintBuffer, TRUE);
					}
					SelectObject(hdcDest, hbmpOld);
				}
			}
			DeleteDC(hdcDest);
		}
		DestroyIcon(hIcon);
	}
}

CEsteidShlExt::~CEsteidShlExt()
{
	DeleteObject(m_DigidocBmp);
}

STDMETHODIMP CEsteidShlExt::Initialize(
	LPCITEMIDLIST /* pidlFolder */, LPDATAOBJECT pDataObj, HKEY /* hProgID */)
{
	FORMATETC fmt{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stg{ TYMED_HGLOBAL };
	m_Files.clear();

	// Look for CF_HDROP data in the data object.
	if (FAILED(pDataObj->GetData(&fmt, &stg))) {
		return E_INVALIDARG;
	}

	// Get a pointer to the actual data.
	HDROP hDrop = HDROP(GlobalLock(stg.hGlobal));
	if (!hDrop) {
		ReleaseStgMedium(&stg);
		return E_INVALIDARG;
	}

	for (UINT i = 0, nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0); i < nFiles; i++) {
		// Get path length in chars
		UINT len = DragQueryFile(hDrop, i, nullptr, 0);
		if (len == 0 || len >= MAX_PATH)
			continue;

		// Get the name of the file
		auto &szFile = m_Files.emplace_back(len, 0);
		if (DragQueryFile(hDrop, i, szFile.data(), len + 1) != len)
			m_Files.pop_back();
	}

	GlobalUnlock(stg.hGlobal);
	ReleaseStgMedium(&stg);

	return m_Files.empty() ? E_INVALIDARG : S_OK;
}

STDMETHODIMP CEsteidShlExt::QueryContextMenu(
	HMENU hmenu, UINT uMenuIndex, UINT uidFirstCmd,
	UINT /* uidLastCmd */, UINT uFlags)
{
	// If the flags include CMF_DEFAULTONLY then we shouldn't do anything.
	if (uFlags & CMF_DEFAULTONLY)
		return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);

	LPCWSTR sign = L"Sign digitally";
	LPCWSTR encrypt = L"Encrypt";
	switch (PRIMARYLANGID(GetUserDefaultUILanguage()))
	{
	case LANG_ESTONIAN:
		sign = L"Allkirjasta digitaalselt";
		encrypt = L"Krüpteeri";
		break;
	case LANG_RUSSIAN:
		sign = L"Подписать дигитально";
		encrypt = L"Зашифровать";
		break;
	default: break;
	}

	InsertMenu(hmenu, uMenuIndex, MF_STRING | MF_BYPOSITION, uidFirstCmd, sign);
	if (m_DigidocBmp)
		SetMenuItemBitmaps(hmenu, uMenuIndex, MF_BYPOSITION, m_DigidocBmp, nullptr);
	InsertMenu(hmenu, uMenuIndex + MENU_ENCRYPT, MF_STRING | MF_BYPOSITION, uidFirstCmd + MENU_ENCRYPT, encrypt);
	if (m_DigidocBmp)
		SetMenuItemBitmaps(hmenu, uMenuIndex + MENU_ENCRYPT, MF_BYPOSITION, m_DigidocBmp, nullptr);

	return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 2);
}

STDMETHODIMP CEsteidShlExt::GetCommandString(
	UINT_PTR idCmd, UINT uFlags, UINT * /* pwReserved */, LPSTR pszName, UINT cchMax)
{
	// Check idCmd, it must be 0 or 1 since we have only two menu items.
	if (idCmd > MENU_ENCRYPT)
		return E_INVALIDARG;

	// If Explorer is asking for a help string, copy our string into the
	// supplied buffer.
	if (uFlags & GCS_HELPTEXT) {
		if (uFlags & GCS_UNICODE) {
			LPCWSTR szText = idCmd == MENU_SIGN
								 ? L"Allkirjasta valitud failid digitaalselt"
								 : L"Krüpteeri valitud failid";
			// We need to cast pszName to a Unicode string, and then use the
			// Unicode string copy API.
			lstrcpynW(LPWSTR(pszName), szText, int(cchMax));
		} else {
			LPCSTR szText = idCmd == MENU_SIGN
								 ? "Allkirjasta valitud failid digitaalselt"
								 : "Krüpteeri valitud failid";
			// Use the ANSI string copy API to return the help string.
			lstrcpynA(pszName, szText, int(cchMax));
		}

		return S_OK;
	}

	return E_INVALIDARG;
}

bool WINAPI CEsteidShlExt::FindRegistryInstallPath(std::wstring &path)
{
	HKEY hkey{};
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\RIA\\Open-EID", 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
		return false;
	DWORD dwSize = path.size() * sizeof(TCHAR);
	bool result = true;
	if(RegQueryValueEx(hkey, L"Installed", nullptr, nullptr, LPBYTE(path.data()), &dwSize) == ERROR_SUCCESS)
		path.resize(dwSize / sizeof(TCHAR) - 1); // size includes any terminating null
	else
		result = false;
	RegCloseKey(hkey);
	return result;
}

STDMETHODIMP CEsteidShlExt::ExecuteDigidocclient(LPCMINVOKECOMMANDINFO /* pCmdInfo */, bool crypto)
{
	if (m_Files.empty())
		return E_INVALIDARG;

	std::wstring path(MAX_PATH, 0);

	// Read the location of the installation from registry
	if (!FindRegistryInstallPath(path)) {
		// .. and fall back to directory where shellext resides if not found from registry
		GetModuleFileName(instanceHandle, path.data(), path.size());
		path.resize(path.find_last_of(L'\\') + 1);
	}

	path += L"qdigidoc4.exe";
	// Construct command line arguments to pass to qdigidocclient.exe
	std::wstring parameters = crypto ? L"\"-crypto\" " : L"\"-sign\" ";
	for (const auto &file: m_Files)
		parameters += L"\"" + file + L"\" ";

	SHELLEXECUTEINFO seInfo{
		.cbSize = sizeof(SHELLEXECUTEINFO),
		.lpFile = path.c_str(),
		.lpParameters = parameters.c_str(),
		.nShow = SW_SHOW
	};
	return ShellExecuteEx(&seInfo) ? S_OK : S_FALSE;
}

STDMETHODIMP CEsteidShlExt::InvokeCommand(LPCMINVOKECOMMANDINFO pCmdInfo)
{
	// If lpVerb really points to a string, ignore this function call and bail out.
	if (HIWORD(pCmdInfo->lpVerb) != 0)
		return E_INVALIDARG;

	// Get the command index - the valid ones are 0 and 1.
	switch (LOWORD(pCmdInfo->lpVerb)) {
	case MENU_SIGN:
		return ExecuteDigidocclient(pCmdInfo);
	case MENU_ENCRYPT:
		return ExecuteDigidocclient(pCmdInfo, true);
	default:
		return E_INVALIDARG;
	}
}
