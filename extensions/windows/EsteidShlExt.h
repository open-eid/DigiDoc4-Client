// EsteidShlExt.h : Declaration of the CEsteidShlExt

#pragma once
#include "resource.h"       // main symbols

#include "EsteidShellExtension_i.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

class ATL_NO_VTABLE CEsteidShlExt :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CEsteidShlExt, &CLSID_EsteidShlExt>,
	public IShellExtInit,
	public IContextMenu
{
public:
	CEsteidShlExt();
	~CEsteidShlExt();

	DECLARE_REGISTRY_RESOURCEID(IDR_ESTEIDSHLEXT)
	DECLARE_NOT_AGGREGATABLE(CEsteidShlExt)

	BEGIN_COM_MAP(CEsteidShlExt)
		COM_INTERFACE_ENTRY(IShellExtInit)
		COM_INTERFACE_ENTRY(IContextMenu)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	// IShellExtInit
	STDMETHODIMP Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY) override;

	// IContextMenu
	STDMETHODIMP QueryContextMenu(HMENU, UINT, UINT, UINT, UINT) override;
	STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO) override;
	STDMETHODIMP GetCommandString(UINT_PTR, UINT, UINT*, LPSTR, UINT) override;

private:
	enum {
		MENU_SIGN = 0,
		MENU_ENCRYPT = 1,
	};

	using tstring = std::basic_string<TCHAR>;
	bool WINAPI FindRegistryInstallPath(tstring* path);
	STDMETHODIMP ExecuteDigidocclient(LPCMINVOKECOMMANDINFO pCmdInfo, bool crypto = false);

	HBITMAP m_DigidocBmp = nullptr;
	std::vector<tstring> m_Files;
};

OBJECT_ENTRY_AUTO(__uuidof(EsteidShlExt), CEsteidShlExt)
