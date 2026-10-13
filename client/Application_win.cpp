// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Application.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLibrary>
#include <QtCore/QSettings>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QWidget>

#include <qt_windows.h>
#include <MAPI.h>
#include <shobjidl_core.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

using namespace Qt::StringLiterals;

namespace
{
winrt::fire_and_forget showShareSheet(QUrl url, HWND window) try
{
	using namespace winrt::Windows::ApplicationModel::DataTransfer;
	using namespace winrt::Windows::Storage;

	QUrlQuery query(url);
	const QString path = QDir::toNativeSeparators(
		query.queryItemValue(QStringLiteral("attachment"), QUrl::FullyDecoded));
	const QString subject = query.queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded);
	auto file = co_await StorageFile::GetFileFromPathAsync(winrt::hstring(PWSTR(path.utf16()), uint32_t(path.size())));
	auto interop = winrt::get_activation_factory<DataTransferManager, IDataTransferManagerInterop>();
	constexpr winrt::guid dataTransferManagerId{
		0xa5caee9b, 0x8708, 0x49d1, {0x8d, 0x36, 0x67, 0xd2, 0x5a, 0x8d, 0xa0, 0x0c}};
	DataTransferManager manager{nullptr};
	winrt::check_hresult(interop->GetForWindow(window, dataTransferManagerId, winrt::put_abi(manager)));
	manager.DataRequested([file = std::move(file), subject](DataTransferManager const &, DataRequestedEventArgs const &args) {
		auto data = args.Request().Data();
		data.Properties().Title(winrt::hstring(PWSTR(subject.utf16()), uint32_t(subject.size())));
		data.Properties().Description(L"DigiDoc4 Client");
		data.SetStorageItems(winrt::single_threaded_vector<winrt::Windows::Storage::IStorageItem>({file}));
		data.RequestedOperation(DataPackageOperation::Copy);
	});
	winrt::check_hresult(interop->ShowShareUIForWindow(window));
}
catch(...)
{
	if(auto *app = QCoreApplication::instance())
		QMetaObject::invokeMethod(app,
			[url = std::move(url)] { QDesktopServices::openUrl(url); }, Qt::QueuedConnection);
}

bool sendWithMapi(const QUrl &url)
{
	QSettings userClients(u"HKEY_CURRENT_USER\\Software\\Clients\\Mail"_s, QSettings::NativeFormat);
	QSettings machineClients(u"HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail"_s, QSettings::NativeFormat);
	QString client = userClients.value(QStringLiteral("Default")).toString();
	if(client.isEmpty())
		client = machineClients.value(QStringLiteral("Default")).toString();
	if(client.isEmpty())
		return false;
	auto hasProvider = [&client](QSettings &clients) {
		clients.beginGroup(client);
		return clients.contains(QStringLiteral("DLLPath"))
			|| clients.contains(QStringLiteral("MSIComponentID"));
	};
	if(!hasProvider(userClients) && !hasProvider(machineClients))
		return false;

	QUrlQuery query(url);
	QString file = query.queryItemValue(QStringLiteral("attachment"), QUrl::FullyDecoded);
	QString filePath = QDir::toNativeSeparators(file);
	QString fileName = QFileInfo(file).fileName();
	QString subject = query.queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded);
	MapiFileDescW document {
		.nPosition = 0,
		.lpszPathName = PWSTR(filePath.utf16()),
		.lpszFileName = PWSTR(fileName.utf16()),
	};
	MapiMessageW message {
		.lpszSubject = PWSTR(subject.utf16()),
		.lpszNoteText = PWSTR(L""),
		.nFileCount = 1,
		.lpFiles = &document,
	};
	if(QLibrary lib("mapi32"); auto mapi = LPMAPISENDMAILW(lib.resolve("MAPISendMailW")))
	{
		switch(mapi({}, 0, &message, MAPI_LOGON_UI | MAPI_DIALOG, 0))
		{
		case SUCCESS_SUCCESS:
		case MAPI_E_USER_ABORT:
		case MAPI_E_LOGIN_FAILURE:
			return true;
		default: return false;
		}
	}
	return false;
}

}

void Application::mailTo(const QUrl &url)
{
	if(sendWithMapi(url))
		return;
	if(auto *window = mainWindow())
		showShareSheet(url, reinterpret_cast<HWND>(window->winId()));
	else
		QDesktopServices::openUrl(url);
}
