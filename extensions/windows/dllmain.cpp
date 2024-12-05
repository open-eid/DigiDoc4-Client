// dllmain.cpp : Implementation of DllMain.

#include "EsteidShlExt.h"

HINSTANCE instanceHandle{};

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		instanceHandle = hModule;
		DisableThreadLibraryCalls(hModule);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

struct CEsteidShlExtFactory : winrt::implements<CEsteidShlExtFactory, IClassFactory>
{
	STDMETHODIMP CreateInstance(
		IUnknown *pUnkOuter, REFIID riid, LPVOID *ppvObject) noexcept final try {
		return winrt::make<CEsteidShlExt>().as(riid, ppvObject);
	} catch (...) {
		return winrt::to_hresult();
	}

	STDMETHODIMP LockServer(BOOL fLock) noexcept final {
		return S_OK;
	}
};

// Used to determine whether the DLL can be unloaded by OLE
STDMETHODIMP DllCanUnloadNow()
{
	if (winrt::get_module_lock()) {
		return S_FALSE;
	}

	winrt::clear_factory_cache();
	return S_OK;
}

// Returns a class factory to create an object of the requested type
STDMETHODIMP DllGetClassObject(const GUID &clsid, const GUID &iid, LPVOID *result) try
{
	*result = nullptr;

	if (clsid == __uuidof(CEsteidShlExt)) {
		return winrt::make<CEsteidShlExtFactory>().as(iid, result);
	}

	return winrt::hresult_class_not_available().to_abi();
} catch (...) {
	return winrt::to_hresult();
}
