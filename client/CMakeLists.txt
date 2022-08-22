set_app_name( PROGNAME qdigidoc4 )

add_executable( TSLDownload TSLDownload.cpp )
target_link_libraries(TSLDownload Qt${QT_VERSION_MAJOR}::Network)
get_target_property(qtCore_install_prefix Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
get_filename_component(qtCore_install_prefix ${qtCore_install_prefix} DIRECTORY)
add_custom_command(
	OUTPUT TSL.qrc
	DEPENDS TSLDownload
	COMMAND $<TARGET_FILE:TSLDownload> "${CMAKE_CURRENT_BINARY_DIR}" ${TSL_URL} ${TSL_INCLUDE}
	WORKING_DIRECTORY ${qtCore_install_prefix}
)

configure_file( translations/tr.qrc tr.qrc COPYONLY )
qt_add_translation(SOURCES translations/en.ts translations/et.ts translations/ru.ts)

add_executable( ${PROGNAME} WIN32 MACOSX_BUNDLE
	${PROGNAME}.rc
	${SOURCES}
	images/images.qrc
	fonts/fonts.qrc
	${CMAKE_CURRENT_BINARY_DIR}/tr.qrc
	${CMAKE_CURRENT_BINARY_DIR}/TSL.qrc
	main.cpp
	Application.cpp
	CheckConnection.cpp
	CryptoDoc.cpp
	DateTime.cpp
	DigiDoc.cpp
	Diagnostics.cpp
	DiagnosticsTask.cpp
	DocumentModel.cpp
	IKValidator.cpp
	MainWindow.ui
	MainWindow.cpp
	MainWindow_MyEID.cpp
	LdapSearch.cpp
	QCardLock.cpp
	QCryptoBackend.cpp
	QPKCS11.cpp
	QSigner.cpp
	QSmartCard.cpp
	sslConnect.cpp
	Styles.cpp
	PrintSheet.cpp
	SslCertificate.cpp
	TokenData.cpp
	dialogs/AccessCert.cpp
	dialogs/AddRecipients.ui
	dialogs/AddRecipients.cpp
	dialogs/CertificateDetails.ui
	dialogs/CertificateDetails.cpp
	dialogs/CertificateHistory.ui
	dialogs/CertificateHistory.cpp
	dialogs/FileDialog.cpp
	dialogs/FirstRun.ui
	dialogs/FirstRun.cpp
	dialogs/KeyDialog.ui
	dialogs/KeyDialog.cpp
	dialogs/MobileDialog.ui
	dialogs/MobileDialog.cpp
	dialogs/MobileProgress.ui
	dialogs/MobileProgress.cpp
	dialogs/PinPopup.ui
	dialogs/PinPopup.cpp
	dialogs/PinUnblock.ui
	dialogs/PinUnblock.cpp
	dialogs/RoleAddressDialog.ui
	dialogs/RoleAddressDialog.cpp
	dialogs/SettingsDialog.ui
	dialogs/SettingsDialog.cpp
	dialogs/SignatureDialog.ui
	dialogs/SignatureDialog.cpp
	dialogs/SmartIDDialog.ui
	dialogs/SmartIDDialog.cpp
	dialogs/SmartIDProgress.cpp
	dialogs/WaitDialog.ui
	dialogs/WaitDialog.cpp
	dialogs/WarningDialog.ui
	dialogs/WarningDialog.cpp
	effects/ButtonHoverFilter.cpp
	effects/FadeInNotification.cpp
	effects/Overlay.cpp
	widgets/Accordion.ui
	widgets/Accordion.cpp
	widgets/AccordionTitle.ui
	widgets/AccordionTitle.cpp
	widgets/AddressItem.ui
	widgets/AddressItem.cpp
	widgets/CardWidget.ui
	widgets/CardWidget.cpp
	widgets/CardPopup.cpp
	widgets/CheckBox.cpp
	widgets/ContainerPage.ui
	widgets/ContainerPage.cpp
	widgets/ComboBox.cpp
	widgets/DropdownButton.cpp
	widgets/FileItem.ui
	widgets/FileItem.cpp
	widgets/FileList.cpp
	widgets/IDSelector.cpp
	widgets/Item.cpp
	widgets/ItemList.ui
	widgets/ItemList.cpp
	widgets/InfoStack.ui
	widgets/InfoStack.cpp
	widgets/LabelButton.cpp
	widgets/LineEdit.cpp
	widgets/MainAction.ui
	widgets/MainAction.cpp
	widgets/NoCardInfo.ui
	widgets/NoCardInfo.cpp
	widgets/PageIcon.ui
	widgets/PageIcon.cpp
	widgets/RadioButton.cpp
	widgets/SignatureItem.ui
	widgets/SignatureItem.cpp
	widgets/StyledWidget.cpp
	widgets/VerifyCert.ui
	widgets/VerifyCert.cpp
	widgets/WarningItem.ui
	widgets/WarningItem.cpp
	widgets/WarningList.cpp
	widgets/WarningRibbon.ui
	widgets/WarningRibbon.cpp
)

target_link_libraries( ${PROGNAME}
	qdigidoccommon
	Qt${QT_VERSION_MAJOR}::PrintSupport
	Qt${QT_VERSION_MAJOR}::Svg
	${LIBDIGIDOCPP_LIBRARY}
	${LDAP_LIBRARIES}
)

if(NOT BUILD_DATE)
	string(TIMESTAMP BUILD_DATE "%d.%m.%Y")
endif()

if(${QT_VERSION_MAJOR} STREQUAL "6")
	find_package(Qt6 COMPONENTS Core5Compat SvgWidgets REQUIRED)
	target_link_libraries(${PROGNAME} Qt6::Core5Compat Qt6::SvgWidgets)
elseif(WIN32)
	find_package(Qt5 COMPONENTS WinExtras)
	target_compile_definitions(${PROGNAME} PRIVATE QT_WIN_EXTRAS)
	target_link_libraries(${PROGNAME} Qt5::WinExtras)
endif()

set_target_properties(${PROGNAME} PROPERTIES
	AUTOUIC ON
	AUTORCC ON
	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.cmake
)
target_include_directories(${PROGNAME} PRIVATE ${CMAKE_SOURCE_DIR} ${LIBDIGIDOCPP_INCLUDE_DIR} )
target_compile_definitions(${PROGNAME} PRIVATE
	MOBILEID_URL="${MOBILEID_URL}"
	SMARTID_URL="${SMARTID_URL}"
	BUILD_DATE="${BUILD_DATE}"
)

if( APPLE )
	file(GLOB_RECURSE RESOURCE_FILES
		${CMAKE_CURRENT_SOURCE_DIR}/mac/Resources/*.icns
		${CMAKE_CURRENT_SOURCE_DIR}/mac/Resources/*.strings)
	foreach(_file ${RESOURCE_FILES})
		get_filename_component(_file_dir ${_file} PATH)
		file(RELATIVE_PATH _file_dir ${CMAKE_CURRENT_SOURCE_DIR}/mac ${_file_dir})
		set_source_files_properties( ${_file} PROPERTIES MACOSX_PACKAGE_LOCATION ${_file_dir})
	endforeach(_file)

	target_sources( ${PROGNAME} PRIVATE ${RESOURCE_FILES} Application_mac.mm MacMenuBar.cpp dialogs/CertificateDetails_mac.mm Diagnostics_unix.cpp)
	set_source_files_properties( Application_mac.mm dialogs/CertificateDetails_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc" )
	set_source_files_properties( LdapSearch.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations" )
	target_link_libraries( ${PROGNAME} "-framework Security" "-framework Quartz" "-framework Cocoa" "-fobjc-arc")
	add_custom_command( TARGET ${PROGNAME} POST_BUILD
		COMMAND cp -a ${PKCS11_MODULE} $<TARGET_FILE_DIR:${PROGNAME}>
		COMMAND mkdir -p $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/PlugIns
		COMMAND cp -a $<TARGET_BUNDLE_DIR:DigiDocQL> $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/PlugIns
	)
	add_custom_target( macdeployqt DEPENDS ${PROGNAME}
		COMMAND ${qtCore_install_prefix}/macdeployqt $<TARGET_BUNDLE_DIR:${PROGNAME}>
		COMMAND rm -rf $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/PlugIns/bearer
		COMMAND cp -a /Library/Frameworks/digidocpp.framework $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks
		COMMAND rm $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks/digidocpp.framework/Resources/digidoc-tool
		COMMAND find $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks -name Headers | xargs rm -rf
		COMMAND install_name_tool -delete_rpath /Library/Frameworks $<TARGET_FILE:${PROGNAME}>
		COMMAND ln -sf digidocpp.framework/Libraries/libcrypto.dylib
			$<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks/libcrypto.1.1.dylib
		COMMAND ln -sf digidocpp.framework/Libraries/libssl.dylib
			$<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks/libssl.1.1.dylib
	)

	add_custom_target( codesign DEPENDS ${PROGNAME}
		COMMAND codesign -f -s \"$$SIGNCERT\"
			$<TARGET_FILE_DIR:${PROGNAME}>/*.*
			$<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks/digidocpp.framework/Libraries/*
			$<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/Frameworks/*.framework
			$<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/PlugIns/*/*.dylib
		COMMAND codesign -f -s \"$$SIGNCERT\" $<TARGET_BUNDLE_CONTENT_DIR:${PROGNAME}>/PlugIns/DigiDocQL.appex
			--entitlements ${CMAKE_SOURCE_DIR}/extensions/DigiDocQL/PreviewViewController.entitlements
		COMMAND if echo \"$$SIGNCERT\" | grep -q "Developer ID" \; then
				codesign -f -s \"$$SIGNCERT\" $<TARGET_BUNDLE_DIR:${PROGNAME}> --entitlements ${CMAKE_SOURCE_DIR}/${PROGNAME}.eToken.entitlements\;
			else
				codesign -f -s \"$$SIGNCERT\" $<TARGET_BUNDLE_DIR:${PROGNAME}> --entitlements ${CMAKE_SOURCE_DIR}/${PROGNAME}.entitlements\;
			fi
	)

	add_custom_target( productbuild DEPENDS ${PROGNAME}
		COMMAND productbuild --component $<TARGET_BUNDLE_DIR:${PROGNAME}> /Applications
			--sign \"$$INSTCERT\" ${CMAKE_BINARY_DIR}/qdigidoc4-store_${VERSION}$ENV{VER_SUFFIX}.pkg
	)
	add_custom_target( zip DEPENDS ${PROGNAME}
		COMMAND zip --symlinks -r -D ${CMAKE_BINARY_DIR}/qdigidoc4_${VERSION}$ENV{VER_SUFFIX}.zip ${PROGNAME}.app
	)
	add_custom_target( zipdebug DEPENDS ${PROGNAME}
		COMMAND dsymutil -o ${PROGNAME}.dSYM $<TARGET_FILE:${PROGNAME}>
		COMMAND zip -r ${CMAKE_BINARY_DIR}/qdigidoc4-dbg_${VERSION}$ENV{VER_SUFFIX}.zip ${PROGNAME}.dSYM
	)
elseif(WIN32)
	target_sources( ${PROGNAME} PRIVATE QCSP.cpp QCNG.cpp QWin.cpp CertStore.cpp Diagnostics_win.cpp)
	target_compile_options(${PROGNAME} PRIVATE "/guard:cf")
	target_link_options(${PROGNAME} PRIVATE "/guard:cf" $<$<BOOL:${CROSSSIGNCERT}>:/INTEGRITYCHECK>)
	target_link_libraries(${PROGNAME} NCrypt Crypt32 Cryptui)
	get_filename_component(LIBS_PATH ${LIBDIGIDOCPP_LIBRARIES} DIRECTORY)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(PLATFORM x64)
		set(OPENSSL_SUFFIX "-x64")
	else()
		set(PLATFORM x86)
		set(OPENSSL_SUFFIX "")
	endif()
	set(MSI_FILE "Digidoc4_Client-${VERSION}$ENV{VER_SUFFIX}.${PLATFORM}")
	list(APPEND CANDLE_CMD "$ENV{WIX}bin\\candle.exe" -nologo -arch ${PLATFORM} -dMSI_VERSION=${VERSION} -dPlatform=${PLATFORM}
		-dlibs_path="${LIBS_PATH}" -dclient_path=$<TARGET_FILE:${PROGNAME}> -dico_path=${CMAKE_CURRENT_SOURCE_DIR}/images/digidoc.ico
		-dqtconf=${CMAKE_SOURCE_DIR}/qt.conf -dschemasLocation=${LIBS_PATH}/schema SchemasFragment.wxs
		-dqt_ver_major=${QT_VERSION_MAJOR} -dqt_suffix=$<$<CONFIG:Debug>:d> ${CMAKE_SOURCE_DIR}/qdigidoc4.wxs
		${CMAKE_SOURCE_DIR}/cmake/modules/WelcomeDlg2.wxs ${CMAKE_SOURCE_DIR}/cmake/modules/WixUI_Minimal2.wxs)
	list(APPEND LIGHT_CMD "$ENV{WIX}bin\\light.exe" -nologo -ext WixUIExtension
		qdigidoc4.wixobj SchemasFragment.wixobj WelcomeDlg2.wixobj WixUI_Minimal2.wixobj
		-dWixUIDialogBmp=${CMAKE_SOURCE_DIR}/cmake/modules/dlgbmp.bmp
		-dWixUIBannerBmp=${CMAKE_SOURCE_DIR}/cmake/modules/banner.bmp)
	add_custom_target(msi DEPENDS ${PROGNAME}
		COMMAND "$ENV{WIX}bin\\heat.exe" dir ${LIBS_PATH}/schema -nologo -cg Schemas -gg -scom -sreg -sfrag -srd -dr SchemaFolder -var var.schemasLocation -out SchemasFragment.wxs
		COMMAND ${CANDLE_CMD}
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.en-US.wxl -cultures:en-US -o "${MSI_FILE}.en-US.msi"
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.et-EE.wxl -cultures:et-EE -o "${MSI_FILE}.et-EE.msi"
		#Build MSI with QT
		COMMAND ${CANDLE_CMD} -dqt_path=${qtCore_install_prefix}
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.en-US.wxl -cultures:en-US -o "${MSI_FILE}.en-US.qt.msi"
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.et-EE.wxl -cultures:et-EE -o "${MSI_FILE}.et-EE.qt.msi"
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	if(SIGNCERT)
		list(APPEND SIGNCMD signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256
			/du http://installer.id.ee /tr http://sha256timestamp.ws.symantec.com/sha256/timestamp /td SHA256)
		add_custom_command(TARGET ${PROGNAME} POST_BUILD
			COMMAND ${SIGNCMD} "$<$<BOOL:${CROSSSIGNCERT}>:/ph;/ac;${CROSSSIGNCERT}>" $<TARGET_FILE:${PROGNAME}>
			COMMAND_EXPAND_LISTS
		)
		add_custom_command(TARGET msi POST_BUILD
			COMMAND ${SIGNCMD} "${MSI_FILE}.en-US.msi" "${MSI_FILE}.et-EE.msi" "${MSI_FILE}.en-US.qt.msi" "${MSI_FILE}.et-EE.qt.msi"
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		)
	endif()
	configure_file(${CMAKE_SOURCE_DIR}/AppxManifest.xml.cmake ${CMAKE_BINARY_DIR}/AppxManifest.xml)
	add_custom_target(appx DEPENDS ${PROGNAME}
		COMMAND ${CMAKE_COMMAND} -E remove ${MSI_FILE}.appx
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx
		COMMAND ${CMAKE_COMMAND} -E make_directory appx
		COMMAND ${CMAKE_COMMAND} -E copy AppxManifest.xml appx
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROGNAME}> appx
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/qt.conf appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/798.p12 appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/libcrypto-1_1${OPENSSL_SUFFIX}.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/libssl-1_1${OPENSSL_SUFFIX}.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/zlib1.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/xerces-c_3_2.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/XalanMessages_1_12.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/Xalan-C_1_12.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/xsec_2_0.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/digidocpp.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/digidocpp.conf appx
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets appx/Assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBS_PATH}/schema appx/schema
		COMMAND ${qtCore_install_prefix}/windeployqt --no-translations --no-compiler-runtime --no-patchqt appx
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx/bearer
		COMMAND makeappx.exe pack -d appx -p ${MSI_FILE}.appx
		# https://msdn.microsoft.com/en-us/library/windows/desktop/jj835832(v=vs.85).aspx
		#Popups GUI
		#COMMAND MakeCert.exe /n "CN=8BBBE4D8-620A-4884-A12A-72F1A2030D8B" /r /h 0 /eku "1.3.6.1.5.5.7.3.3,1.3.6.1.4.1.311.10.3.13" /m 120 /a sha256 /sv qdigidoc4.pvk qdigidoc4.cer
		#COMMAND Pvk2Pfx.exe /pvk qdigidoc4.pvk /spc qdigidoc4.cer /pfx qdigidoc4.pfx /f
		COMMAND signtool.exe sign -f ${CMAKE_SOURCE_DIR}/qdigidoc4.pfx -fd SHA256 -v ${MSI_FILE}.appx
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
else()
	find_package( Threads REQUIRED )
	target_sources( ${PROGNAME} PRIVATE Diagnostics_unix.cpp)
	target_link_libraries( ${PROGNAME} Threads::Threads )
	configure_file( qdigidoc4.1.cmake qdigidoc4.1 )
	install( TARGETS ${PROGNAME} DESTINATION ${CMAKE_INSTALL_BINDIR} )
	install( FILES ${CMAKE_CURRENT_BINARY_DIR}/qdigidoc4.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )
	install( FILES qdigidoc4.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications )
	install( FILES qdigidoc4.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages )
	foreach(RES 16 22 32 48 128 256 512)
		install(
			FILES images/digidoc_${RES}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/apps/
			RENAME qdigidoc4.png
		)
		install(
			FILES images/qdigidoc_client_document_${RES}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/
			RENAME application-vnd.etsi.asic-e+zip.png
		)
		install(
			FILES images/qdigidoc_crypto_document_${RES}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/
			RENAME application-x-cdoc.png
		)
		foreach(TGT application-vnd.etsi.asic-s+zip.png application-x-ddoc.png application-vnd.lt.archyvai.adoc-2008.png)
			install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink application-vnd.etsi.asic-e+zip.png \
				\$ENV{DESTDIR}/${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/${TGT})")
		endforeach()
		# Workaround Ubuntu 21.10 Yaru icon theme issues
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory \
			\$ENV{DESTDIR}/${CMAKE_INSTALL_FULL_DATADIR}/icons/Yaru/${RES}x${RES}/mimetypes/)")
		foreach(TGT application-vnd.etsi.asic-e+zip.png application-vnd.etsi.asic-s+zip.png application-x-ddoc.png application-vnd.lt.archyvai.adoc-2008.png application-x-cdoc.png)
			install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \
				${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/${TGT} \
				\$ENV{DESTDIR}/${CMAKE_INSTALL_FULL_DATADIR}/icons/Yaru/${RES}x${RES}/mimetypes/${TGT})")
		endforeach()
	endforeach()
endif()
