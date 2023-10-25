// dllmain.h : Declaration of module class.

class CEsteidShellExtensionModule : public CAtlDllModuleT< CEsteidShellExtensionModule >
{
public :
	DECLARE_LIBID(LIBID_EsteidShellExtensionLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ESTEIDEXT, "{08BD50C2-0AFC-4ED0-BC78-78488EDF9E58}")
};

extern class CEsteidShellExtensionModule _AtlModule;
