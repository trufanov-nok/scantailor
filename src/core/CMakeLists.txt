SET(core_headers .)

# for version.h
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}")

INCLUDE_DIRECTORIES(.)

ADD_SUBDIRECTORY(ui)
INCLUDE_DIRECTORIES("${core_ui_sources_BINARY_DIR}")

ADD_SUBDIRECTORY(interaction)
ADD_SUBDIRECTORY(zones)
ADD_SUBDIRECTORY(tests)

ADD_SUBDIRECTORY(filters/fix_orientation)
ADD_SUBDIRECTORY(filters/page_split)
ADD_SUBDIRECTORY(filters/deskew)
ADD_SUBDIRECTORY(filters/select_content)
ADD_SUBDIRECTORY(filters/page_layout)
ADD_SUBDIRECTORY(filters/output)
ADD_SUBDIRECTORY(filters/publish)

SET(
        common_sources
        BackgroundExecutor.cpp BackgroundExecutor.h
        OpenGLSupport.cpp OpenGLSupport.h
        PixmapRenderer.cpp PixmapRenderer.h
        BubbleAnimation.cpp BubbleAnimation.h
        ProcessingIndicationWidget.cpp ProcessingIndicationWidget.h
        NonOwningWidget.cpp NonOwningWidget.h
        Dpi.cpp Dpi.h Dpm.cpp Dpm.h
        SmartFilenameOrdering.cpp SmartFilenameOrdering.h
        AbstractRelinker.h
        RelinkablePath.cpp RelinkablePath.h
        ImageInfo.cpp ImageInfo.h
        ImageFileInfo.cpp ImageFileInfo.h
        ImageMetadata.cpp ImageMetadata.h
        RecentProjects.cpp RecentProjects.h
        OutOfMemoryHandler.cpp OutOfMemoryHandler.h
        CommandLine.cpp CommandLine.h
        PageSelectionAccessor.cpp PageSelectionAccessor.h
        PageSelectionProvider.h
        ContentSpanFinder.cpp ContentSpanFinder.h
        PhysicalTransformation.cpp PhysicalTransformation.h
        ImageTransformation.cpp ImageTransformation.h
        ImagePixmapUnion.h
        ImageViewBase.cpp ImageViewBase.h
        BasicImageView.cpp BasicImageView.h
        DebugImageView.cpp DebugImageView.h
        TabbedDebugImages.cpp TabbedDebugImages.h
        ThumbnailLoadResult.h
        ThumbnailPixmapCache.cpp ThumbnailPixmapCache.h
        ThumbnailBase.cpp ThumbnailBase.h
        ThumbnailFactory.cpp ThumbnailFactory.h
        ThumbnailSequence.cpp ThumbnailSequence.h
        IncompleteThumbnail.cpp IncompleteThumbnail.h
        ContentBoxPropagator.cpp ContentBoxPropagator.h
        PageOrientationPropagator.cpp PageOrientationPropagator.h
        DebugImages.cpp DebugImages.h
        ImageId.cpp ImageId.h
        PageId.cpp PageId.h
        PageInfo.cpp PageInfo.h
        BackgroundTask.cpp BackgroundTask.h
        ProcessingTaskQueue.cpp ProcessingTaskQueue.h
        PageSequence.cpp PageSequence.h
        PageRangeSelectorWidget.cpp PageRangeSelectorWidget.h
        ApplyToDialog.cpp ApplyToDialog.h
        ErrorWidget.cpp ErrorWidget.h
        StageSequence.cpp StageSequence.h
        ProjectPages.cpp ProjectPages.h
        FilterData.cpp FilterData.h
        ImageMetadataLoader.cpp ImageMetadataLoader.h
        TiffReader.cpp TiffReader.h
        TiffWriter.cpp TiffWriter.h
        DjVuReader.cpp DjVuReader.h
        PngMetadataLoader.cpp PngMetadataLoader.h
        TiffMetadataLoader.cpp TiffMetadataLoader.h
        JpegMetadataLoader.cpp JpegMetadataLoader.h
        GenericMetadataLoader.cpp GenericMetadataLoader.h
        ImageLoader.cpp ImageLoader.h
        OrthogonalRotation.cpp OrthogonalRotation.h
        WorkerThread.cpp WorkerThread.h
        LoadFileTask.cpp LoadFileTask.h
        FilterOptionsWidget.cpp FilterOptionsWidget.h
        TaskStatus.h FilterUiInterface.h
        ProjectReader.cpp ProjectReader.h
        ProjectWriter.cpp ProjectWriter.h
        XmlMarshaller.cpp XmlMarshaller.h
        XmlUnmarshaller.cpp XmlUnmarshaller.h
        AtomicFileOverwriter.cpp AtomicFileOverwriter.h
        EstimateBackground.cpp EstimateBackground.h
        Despeckle.cpp Despeckle.h
        ThreadPriority.cpp ThreadPriority.h
        FileNameDisambiguator.cpp FileNameDisambiguator.h
        OutputFileNameGenerator.cpp OutputFileNameGenerator.h
        PageRange.cpp PageRange.h
        SelectedPage.cpp SelectedPage.h
        Utils.cpp Utils.h
        StatusBarProvider.cpp StatusBarProvider.h
        ExportSuggestions.cpp ExportSuggestions.h
        PageView.h
        AutoManualMode.h
        AbstractCommand.h
        AbstractFilter.h
        BeforeOrAfter.h
        FilterResult.h
        CompositeCacheDrivenTask.h
        Margins.h
        ChangedStateItemDelegate.h
        PageOrderProvider.h
        PageOrderOption.h
        PayloadEvent.h
        RegenParams.h
        AbstractFilterDataCollector.h
        ThumbnailCollector.h
        ContentBoxCollector.h
        PageOrientationCollector.h
        
        settings/globalstaticsettings.cpp settings/globalstaticsettings.h
        settings/hotkeysmanager.cpp settings/hotkeysmanager.h
        settings/TiffCompressionInfo.cpp settings/TiffCompressionInfo.h
        settings/ini_keys.h
)

IF (ENABLE_OPENJPEG)
LIST(APPEND common_sources
        Jp2Reader.cpp Jp2Reader.h
        Jp2MetadataLoader.cpp Jp2MetadataLoader.h
    )
ENDIF(ENABLE_OPENJPEG)


SOURCE_GROUP("Sources" FILES ${common_sources} ${core_ui_sources})

QT5_ADD_RESOURCES(core_resource_sources resources/core_resources.qrc)
SOURCE_GROUP("Generated" FILES ${core_resource_sources})
SOURCE_GROUP("Resources" FILES ${core_resource_sources})

ADD_LIBRARY(stcore STATIC ${core_resource_sources} ${common_sources} ${core_ui_sources})
QT5_USE_MODULES(stcore Widgets Xml ${MAYBE_QT_OPENGL_MODULE})
ADD_DEPENDENCIES(stcore core_ui_sources) 
