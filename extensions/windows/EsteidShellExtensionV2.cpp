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

extern "C" IMAGE_DOS_HEADER __ImageBase;

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
		case LANG_RUSSIAN:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Зашифровать с помощью DigiDoc4", ppszName);
			else return SHLocalStrDupW(L"Подписать с помощью DigiDoc4", ppszName);
		default:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Encrypt with DigiDoc4", ppszName);
			else return SHLocalStrDupW(L"Sign with DigiDoc4", ppszName);
		}
	}
	STDMETHODIMP GetIcon(IShellItemArray */*psiItemArray*/, LPWSTR *ppszIcon) final
	{
		auto p = digidocPath() + L",0";
		return SHLocalStrDupW(p.data(), ppszIcon);
	}
	STDMETHODIMP GetToolTip(IShellItemArray */*psiItemArray*/, LPWSTR *ppszInfotip) final
	{
		switch(PRIMARYLANGID(GetUserDefaultUILanguage()))
		{
		case LANG_ESTONIAN:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Krüpteeri valitud failid", ppszInfotip);
			else return SHLocalStrDupW(L"Allkirjasta valitud failid digitaalselt", ppszInfotip);
		case LANG_RUSSIAN:
			if constexpr (IsCrypto) return SHLocalStrDupW(L"Зашифровать выбранные файлы", ppszInfotip);
			else return SHLocalStrDupW(L"Цифровая подпись выбранных файлов", ppszInfotip);
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
	STDMETHODIMP Invoke(IShellItemArray *psiItemArray, IBindCtx */*pbc*/) final
	{
		if(!psiItemArray)
			return S_OK;

		DWORD count;
		if(auto hr = psiItemArray->GetCount(&count); FAILED(hr))
			return hr;
		if(count == 0)
			return S_OK;

		std::wstring digidoc = digidocPath();
		std::wstring parameters;
		if constexpr (IsCrypto) parameters += L"\"-crypto\" ";
		else parameters += L"\"-sign\" ";
		for (DWORD i = 0; i < count; ++i)
		{
			IShellItem* psi = nullptr;
			if(auto hr = psiItemArray->GetItemAt(i, &psi); FAILED(hr))
				return hr;
			LPWSTR path;
			auto hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
			psi->Release();
			if(FAILED(hr))
				return hr;
			parameters += L"\"";
			parameters += path;
			parameters += L"\" ";
			if(path)
				CoTaskMemFree(path);
		}
		SHELLEXECUTEINFO seInfo{
			.cbSize = sizeof(SHELLEXECUTEINFO),
			.lpFile = digidoc.c_str(),
			.lpParameters = parameters.c_str(),
			.nShow = SW_SHOW
		};
		return ShellExecuteEx(&seInfo) ? S_OK : S_FALSE;
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

	static std::wstring digidocPath()
	{
		std::wstring path(MAX_PATH, 0);
		if(auto size = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), path.data(), DWORD(path.size())); size > 0)
			path.resize(size);
		else
			path.clear();
		if(auto pos = path.find_last_of('\\'); pos != std::wstring::npos)
		{
			path.resize(pos);
			path += L"\\qdigidoc4.exe";
		}
		return path;
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
