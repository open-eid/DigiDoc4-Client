// EsteidShlExt.h : Declaration of the CEsteidShlExt

#pragma once

#include "targetver.h"

#include <unknwn.h>
#include <winrt/base.h>
#include <ShlObj.h>
#include <string>
#include <vector>

class
#ifdef _WIN64
	__declspec(uuid("5606A547-759D-43DA-AEEB-D3BF1D1E816D"))
#else
	__declspec(uuid("310AAB39-76FE-401B-8A7F-0F578C5F6AB5"))
#endif
	CEsteidShlExt : public winrt::implements<CEsteidShlExt, IShellExtInit, IContextMenu>
{
public:
	CEsteidShlExt();
	~CEsteidShlExt();

	// IShellExtInit
	STDMETHODIMP Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pdtobj, HKEY hkeyProgID) final;

	// IContextMenu
	STDMETHODIMP QueryContextMenu(HMENU, UINT, UINT, UINT, UINT) final;
	STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO) final;
	STDMETHODIMP GetCommandString(UINT_PTR, UINT, UINT *, LPSTR, UINT) final;

private:
	enum {
		MENU_SIGN = 0,
		MENU_ENCRYPT = 1,
	};

	bool WINAPI FindRegistryInstallPath(std::wstring &path);
	STDMETHODIMP ExecuteDigidocclient(LPCMINVOKECOMMANDINFO pCmdInfo, bool crypto = false);

	HBITMAP m_DigidocBmp = nullptr;
	std::vector<std::wstring> m_Files;
};
