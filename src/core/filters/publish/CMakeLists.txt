PROJECT("Publishing Filter")

INCLUDE_DIRECTORIES(BEFORE "${CMAKE_CURRENT_BINARY_DIR}")

SET(
        sources
        Filter.cpp Filter.h
        Task.cpp Task.h
        CacheDrivenTask.cpp CacheDrivenTask.h
        OptionsWidget.cpp OptionsWidget.h
	Params.cpp Params.h
        SourceImagesInfo.cpp SourceImagesInfo.h
        Settings.cpp Settings.h
	DjbzDispatcher.cpp DjbzDispatcher.h
	OutputParams.cpp OutputParams.h
        OrderByFileSize.cpp OrderByFileSize.h
        DjbzManagerDialog.cpp DjbzManagerDialog.h
        ContentsManagerDialog.cpp ContentsManagerDialog.h
        FreeImageFilters.cpp FreeImageFilters.h
        EncodingProgressWidget.cpp EncodingProgressWidget.h
        EncodingProgressInfo.h
        ClickableLabel.cpp ClickableLabel.h
        MetadataEditorDialog.cpp MetadataEditorDialog.h
        djview4/qdjvuwidget.cpp djview4/qdjvuwidget.h
        djview4/qdjvu.cpp djview4/qdjvu.h

)

SOURCE_GROUP("Sources" FILES ${sources})
FILE(GLOB ui_files "ui/*.ui")
SOURCE_GROUP("UI Files" FILES ${ui_files})

QT5_WRAP_UI(ui_sources ${ui_files})
QT5_ADD_RESOURCES(resource_files djview4/qdjvuwidget.qrc)

ADD_LIBRARY(publish STATIC ${sources} ${ui_sources} ${resource_files})
QT5_USE_MODULES(publish Widgets Xml)
TRANSLATION_SOURCES(scantailor-universal ${sources} ${ui_files})
