set( RESOURCES ../../client/mac/Resources/asic.icns )
add_library( DigiDocQL MODULE
	${RESOURCES}
	Info.plist
	main.c
	GenerateThumbnailForURL.c
	GeneratePreviewForURL.mm
)
set_source_files_properties( GeneratePreviewForURL.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc" )
set_source_files_properties( Info.plist PROPERTIES MACOSX_PACKAGE_LOCATION . )
set_target_properties( DigiDocQL PROPERTIES
	BUNDLE YES
	BUNDLE_EXTENSION qlgenerator
	RESOURCE "${RESOURCES}"
	XCODE_ATTRIBUTE_WRAPPER_EXTENSION qlgenerator
	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
	AUTOMOC OFF
	INCLUDE_DIRECTORIES "${LIBDIGIDOCPP_INCLUDE_DIR}"
	COMPILE_FLAGS "-Wno-unused-parameter"
	LINK_LIBRARIES "-framework QuickLook;-framework digidocpp;-framework Cocoa"
	LINK_FLAGS "-F/Library/Frameworks -fobjc-arc"
	BUILD_WITH_INSTALL_RPATH YES
	INSTALL_RPATH "@loader_path/../../../../../Frameworks"
)
