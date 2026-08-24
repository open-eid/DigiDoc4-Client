/*
 * EsteidShellExtension
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <unknwn.h>
#include <winrt/base.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <shlwapi.h>
#include <uxtheme.h>

#include <memory>

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct CoTaskMemDeleter
{
	void operator()(void *value) const noexcept
	{
		CoTaskMemFree(value);
	}
};

template<bool IsCrypto>
struct EsteidShellExtension : public winrt::implements<EsteidShellExtension<IsCrypto>, IExplorerCommand>
{
	// IExplorerCommand
	STDMETHODIMP GetTitle(IShellItemArray */*psiItemArray*/, LPWSTR *ppszName) final
	{
		switch(PRIMARYLANGID(GetUserDefaultUILanguage()))
		{
		case LANG_ESTONIAN:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Krüpteeri DigiDoc4-ga", ppszName);
			else return SHLocalStrDupW(L"Allkirjasta DigiDoc4-ga", ppszName);
		default:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Encrypt with DigiDoc4", ppszName);
			else return SHLocalStrDupW(L"Sign with DigiDoc4", ppszName);
		}
	}
	STDMETHODIMP GetIcon(IShellItemArray */*psiItemArray*/, LPWSTR *ppszIcon) final try
	{
		auto p = digidocPath();
		if(p.empty())
			return E_FAIL;
		p += L",0";
		return SHLocalStrDupW(p.data(), ppszIcon);
	}
	catch(...)
	{
		return winrt::to_hresult();
	}

	STDMETHODIMP GetToolTip(IShellItemArray */*psiItemArray*/, LPWSTR *ppszInfotip) final
	{
		switch(PRIMARYLANGID(GetUserDefaultUILanguage()))
		{
		case LANG_ESTONIAN:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Krüpteeri valitud failid", ppszInfotip);
			else return SHLocalStrDupW(L"Allkirjasta valitud failid digitaalselt", ppszInfotip);
		default:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Encrypt selected files", ppszInfotip);
			else return SHLocalStrDupW(L"Digitally sign selected files", ppszInfotip);
		}
	}

	STDMETHODIMP GetCanonicalName(GUID *pguidCommandName) final
	{
		*pguidCommandName = GUID_NULL;
		return S_OK;
	}

	STDMETHODIMP GetState(IShellItemArray */*psiItemArray*/, BOOL /*fOkToBeSlow*/, EXPCMDSTATE *pCmdState) final
	{
		*pCmdState = ECS_ENABLED;
		return S_OK;
	}

	STDMETHODIMP Invoke(IShellItemArray *psiItemArray, IBindCtx */*pbc*/) final try
	{
		if(!psiItemArray)
			return S_OK;

		DWORD count;
		if(auto hr = psiItemArray->GetCount(&count); FAILED(hr))
			return hr;
		if(count == 0)
			return S_OK;

		std::wstring digidoc = digidocPath();
		if(digidoc.empty())
			return E_FAIL;
		std::wstring parameters;
		if constexpr (IsCrypto) parameters += L"\"-crypto\" ";
		else parameters += L"\"-sign\" ";
		for (DWORD i = 0; i < count; ++i)
		{
			winrt::com_ptr<IShellItem> item;
			if(auto hr = psiItemArray->GetItemAt(i, item.put()); FAILED(hr))
				return hr;
			LPWSTR value{};
			auto hr = item->GetDisplayName(SIGDN_FILESYSPATH, &value);
			if(FAILED(hr))
				return hr;
			std::unique_ptr<wchar_t, CoTaskMemDeleter> path(value);
			if(!path)
				return E_UNEXPECTED;
			parameters += L"\"";
			parameters += path.get();
			parameters += L"\" ";
		}
		SHELLEXECUTEINFO seInfo{
			.cbSize = sizeof(SHELLEXECUTEINFO),
			.lpFile = digidoc.c_str(),
			.lpParameters = parameters.c_str(),
			.nShow = SW_SHOW
		};
		if(ShellExecuteEx(&seInfo))
			return S_OK;
		if(auto error = GetLastError(); error != ERROR_SUCCESS)
			return HRESULT_FROM_WIN32(error);
		return E_FAIL;
	}
	catch(...)
	{
		return winrt::to_hresult();
	}

	STDMETHODIMP GetFlags(EXPCMDFLAGS *pFlags) final
	{
		*pFlags = ECF_DEFAULT;
		return S_OK;
	}

	STDMETHODIMP EnumSubCommands(IEnumExplorerCommand **ppEnum) final
	{
		*ppEnum = {};
		return E_NOTIMPL;
	}

	static std::wstring digidocPath() try
	{
		// The maximum extended-length path is 32,767 characters plus the null terminator.
		std::wstring path(32768, 0);
		auto size = GetModuleFileNameW(
			reinterpret_cast<HMODULE>(&__ImageBase), path.data(), DWORD(path.size()));
		if(size == 0 || size >= path.size())
			return {};
		path.resize(size);
		auto pos = path.find_last_of(L"\\/");
		if(pos == std::wstring::npos)
			return {};
		path.resize(pos + 1);
		path += L"qdigidoc4.exe";
		return path;
	}
	catch (...) {
		return {};
	}
};

struct __declspec(uuid("4ef3a5aa-125c-45f5-b5fd-d4c478050afa"))
EsteidShellExtensionSign : public EsteidShellExtension<false> {};

struct __declspec(uuid("bb67aa19-089b-4ec9-a059-13d985987cdc"))
EsteidShellExtensionEnc : public EsteidShellExtension<true> {};

template<typename T>
struct EsteidShellExtensionFactory : winrt::implements<EsteidShellExtensionFactory<T>, IClassFactory>
{
	STDMETHODIMP CreateInstance(
		IUnknown *pUnkOuter, REFIID riid, LPVOID *ppvObject) noexcept final try {
		if(!ppvObject)
			return E_POINTER;
		*ppvObject = nullptr;
		if(pUnkOuter)
			return CLASS_E_NOAGGREGATION;
		return winrt::make<T>().as(riid, ppvObject);
	} catch (...) {
		return winrt::to_hresult();
	}

	STDMETHODIMP LockServer(BOOL /*fLock*/) noexcept final {
		return S_OK;
	}
};

STDMETHODIMP DllCanUnloadNow()
{
	if (winrt::get_module_lock())
		return S_FALSE;
	winrt::clear_factory_cache();
	return S_OK;
}

STDMETHODIMP DllGetClassObject(const GUID &clsid, const GUID &iid, LPVOID *result) try
{
	*result = nullptr;
	if (clsid == __uuidof(EsteidShellExtensionSign))
		return winrt::make<EsteidShellExtensionFactory<EsteidShellExtensionSign>>().as(iid, result);
	if (clsid == __uuidof(EsteidShellExtensionEnc))
		return winrt::make<EsteidShellExtensionFactory<EsteidShellExtensionEnc>>().as(iid, result);
	return winrt::hresult_class_not_available().to_abi();
} catch (...) {
	return winrt::to_hresult();
}
