/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "CommandLine.h"
#include "MainWindow.h"

#include "NewOpenProjectPanel.h"
#include "RecentProjects.h"
#include "WorkerThread.h"
#include "ProjectPages.h"
#include "PageSelectionAccessor.h"
#include "StageSequence.h"
#include "ThumbnailSequence.h"
#include "PageOrderOption.h"
#include "PageOrderProvider.h"
#include "ProcessingTaskQueue.h"
#include "FileNameDisambiguator.h"
#include "OutputFileNameGenerator.h"
#include "ImageInfo.h"
#include "PageInfo.h"
#include "ImageId.h"
#include "Utils.h"
#include "FilterOptionsWidget.h"
#include "ErrorWidget.h"
#include "AutoRemovingFile.h"
#include "DebugImages.h"
#include "DebugImageView.h"
#include "TabbedDebugImages.h"
#include "BasicImageView.h"
#include "ProjectWriter.h"
#include "ProjectReader.h"
#include "ThumbnailPixmapCache.h"
#include "ThumbnailFactory.h"
#include "ContentBoxPropagator.h"
#include "PageOrientationPropagator.h"
#include "ProjectCreationContext.h"
#include "ProjectOpeningContext.h"
#include "SkinnedButton.h"
#include "SystemLoadWidget.h"
#include "ProcessingIndicationWidget.h"
#include "ImageMetadataLoader.h"
#include "SmartFilenameOrdering.h"
#include "OrthogonalRotation.h"
#include "FixDpiDialog.h"
#include "LoadFilesStatusDialog.h"
#include "SettingsDialog.h"
#include "AbstractRelinker.h"
#include "RelinkingDialog.h"
#include "OutOfMemoryHandler.h"
#include "OutOfMemoryDialog.h"
#include "QtSignalForwarder.h"
#include "StartBatchProcessingDialog.h"
#include "ExportSuggestions.h"

#include "filters/fix_orientation/Filter.h"
#include "filters/fix_orientation/Task.h"
#include "filters/fix_orientation/CacheDrivenTask.h"
#include "filters/page_split/Filter.h"
#include "filters/page_split/Task.h"
#include "filters/page_split/CacheDrivenTask.h"
#include "filters/deskew/Filter.h"
#include "filters/deskew/Task.h"
#include "filters/deskew/CacheDrivenTask.h"
#include "filters/select_content/Filter.h"
#include "filters/select_content/Task.h"
#include "filters/select_content/CacheDrivenTask.h"
#include "filters/page_layout/Filter.h"
#include "filters/page_layout/Task.h"
#include "filters/page_layout/CacheDrivenTask.h"
#include "filters/output/Filter.h"
#include "filters/output/Task.h"
#include "filters/output/CacheDrivenTask.h"
#include "filters/publish/Filter.h"
#include "filters/publish/Task.h"
#include "filters/publish/CacheDrivenTask.h"
#include "filters/publish/EncodingProgressWidget.h"

#include "LoadFileTask.h"
#include "CompositeCacheDrivenTask.h"
#include "ScopedIncDec.h"
#include "ui_AboutDialog.h"
#include "ui_RemovePagesDialog.h"
#include "ui_BatchProcessingLowerPanel.h"
#include "ui_StartBatchProcessingDialog.h"
#include "config.h"
#include "version.h"
#include "settings/globalstaticsettings.h"
#include "StatusBarProvider.h"
#include "OpenWithMenuProvider.h"
#include <QApplication>
#include <QLineF>
#include <QPointer>
#include <QWidget>
#include <QDialog>
#include <QCloseEvent>
#include <QStackedLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLayoutItem>
#include <QScrollBar>
#include <QPushButton>
#include <QCheckBox>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QProcess>
#include <QClipboard>
#include <QMimeDatabase>
#include <QModelIndex>
#include <QFileDialog>
#include <QMessageBox>
#include <QPalette>
#include <QStyle>
#include "settings/ini_keys.h"
#include <QDomDocument>
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QResource>
#include <Qt>
#include <QDebug>
#include <QDate>
#include <algorithm>
#include <vector>
#include <stddef.h>
#include <math.h>
#include <assert.h>

#include <iostream>

MainWindow::MainWindow()
    :   m_ptrPages(new ProjectPages),
        m_ptrStages(new StageSequence(m_ptrPages, newPageSelectionAccessor())),
        m_ptrWorkerThread(new WorkerThread),
        m_ptrInteractiveQueue(new ProcessingTaskQueue(ProcessingTaskQueue::RANDOM_ORDER)),
        m_curFilter(0),
        m_ignoreSelectionChanges(0),
        m_ignorePageOrderingChanges(0),
        m_debug(false),
        m_closing(false),
        m_docking_enabled(true),
        m_autosave_timer(nullptr)
{
    GlobalStaticSettings::updateSettings();
    QSettings settings;
    m_maxLogicalThumbSize = settings.value(_key_thumbnails_max_thumb_size, _key_thumbnails_max_thumb_size_def).toSizeF();
    m_ptrThumbSequence.reset(new ThumbnailSequence(m_maxLogicalThumbSize));
    setupUi(this);
    setupStatusBar();

    sortOptionsWgt->setVisible(false);
    connect(sortOptions, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
    this, [this](int index) {
        resetSortingBtn->setVisible(index);
    });

    createBatchProcessingWidget();
    m_ptrProcessingIndicationWidget.reset(new ProcessingIndicationWidget);

    filterList->setStages(m_ptrStages);
    filterList->selectRow(0);

    setupThumbView(); // Expects m_ptrThumbSequence to be initialized.

    m_ptrTabbedDebugImages.reset(new TabbedDebugImages);

    m_pImageFrameLayout = new QVBoxLayout(imageViewFrame);
    m_pOptionsFrameLayout = new QStackedLayout(filterOptions);

    addAction(actionFirstPage);
    addAction(actionLastPage);
    addAction(actionNextPage);
    addAction(actionPrevPage);
    addAction(actionPrevPageQ);
    addAction(actionNextPageW);
    addAction(actionFirstSelectedPage);
    addAction(actionLastSelectedPage);
    addAction(actionNextSelectedPage);
    addAction(actionPrevSelectedPage);
    addAction(actionJumpPageB);
    addAction(actionJumpPageF);

    addAction(actionSwitchFilter1);
    addAction(actionSwitchFilter2);
    addAction(actionSwitchFilter3);
    addAction(actionSwitchFilter4);
    addAction(actionSwitchFilter5);
    addAction(actionSwitchFilter6);

    // Should be enough to save a project.
    OutOfMemoryHandler::instance().allocateEmergencyMemory(3 * 1024 * 1024);

    connect(actionFirstPage, SIGNAL(triggered(bool)), SLOT(goFirstPage()));
    connect(actionLastPage, SIGNAL(triggered(bool)), SLOT(goLastPage()));
    connect(actionPrevPage, SIGNAL(triggered(bool)), SLOT(goPrevPage()));
    connect(actionNextPage, SIGNAL(triggered(bool)), SLOT(goNextPage()));
    connect(actionFirstSelectedPage, &QAction::triggered, this, [this]() {
        goFirstPage(true);
    });
    connect(actionLastSelectedPage, &QAction::triggered, this, [this]() {
        goLastPage(true);
    });
    connect(actionPrevSelectedPage, &QAction::triggered, this, [this]() {
        goPrevPage(-1, true);
    });
    connect(actionNextSelectedPage, &QAction::triggered, this, [this]() {
        goNextPage(1, true);
    });
    connect(actionPrevPageQ, SIGNAL(triggered(bool)), this, SLOT(goPrevPage()));
    connect(actionNextPageW, SIGNAL(triggered(bool)), this, SLOT(goNextPage()));
    connect(actionAbout, SIGNAL(triggered(bool)), this, SLOT(showAboutDialog()));

    connect(actionInsertEmptyPgBefore, &QAction::triggered, this, [this]() {
        PageInfo page_info = m_ptrThumbSequence->selectionLeader();
        if (!page_info.imageId().filePath().isEmpty()) {
            showInsertEmptyPageDialog(BEFORE, page_info.id());
        }
    });
    addAction(actionInsertEmptyPgBefore);

    connect(actionInsertEmptyPgAfter, &QAction::triggered, this, [this]() {
        PageInfo page_info = m_ptrThumbSequence->selectionLeader();
        if (!page_info.imageId().filePath().isEmpty()) {
            showInsertEmptyPageDialog(AFTER, page_info.id());
        }
    });
    addAction(actionInsertEmptyPgAfter);

    connect(actionSwitchFilter1, SIGNAL(triggered(bool)), SLOT(switchFilter1()));
    connect(actionSwitchFilter2, SIGNAL(triggered(bool)), SLOT(switchFilter2()));
    connect(actionSwitchFilter3, SIGNAL(triggered(bool)), SLOT(switchFilter3()));
    connect(actionSwitchFilter4, SIGNAL(triggered(bool)), SLOT(switchFilter4()));
    connect(actionSwitchFilter5, SIGNAL(triggered(bool)), SLOT(switchFilter5()));
    connect(actionSwitchFilter6, SIGNAL(triggered(bool)), SLOT(switchFilter6()));

    connect(
        filterList->selectionModel(),
        SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
        this, SLOT(filterSelectionChanged(QItemSelection))
    );
    connect(
        filterList, SIGNAL(launchBatchProcessing()),
        this, SLOT(startBatchProcessing())
    );

    connect(
        m_ptrWorkerThread.get(),
        SIGNAL(taskResult(BackgroundTaskPtr,FilterResultPtr)),
        this, SLOT(filterResult(BackgroundTaskPtr,FilterResultPtr))
    );

    connect(
        m_ptrThumbSequence.get(),
        SIGNAL(newSelectionLeader(PageInfo,QRectF,ThumbnailSequence::SelectionFlags)),
        this, SLOT(currentPageChanged(PageInfo,QRectF,ThumbnailSequence::SelectionFlags))
    );
    connect(
        m_ptrThumbSequence.get(),
        SIGNAL(pageContextMenuRequested(PageInfo,QPoint,bool)),
        this, SLOT(pageContextMenuRequested(PageInfo,QPoint,bool))
    );
    connect(
        m_ptrThumbSequence.get(),
        SIGNAL(pastLastPageContextMenuRequested(QPoint)),
        SLOT(pastLastPageContextMenuRequested(QPoint))
    );

    connect(
        thumbView->verticalScrollBar(), SIGNAL(sliderMoved(int)),
        this, SLOT(thumbViewScrolled())
    );
    connect(
        thumbView->verticalScrollBar(), SIGNAL(valueChanged(int)),
        this, SLOT(thumbViewScrolled())
    );
    connect(
        focusButton, SIGNAL(clicked(bool)),
        this, SLOT(thumbViewFocusToggled(bool))
    );
    connect(
        sortOptions, SIGNAL(currentIndexChanged(int)),
        this, SLOT(pageOrderingChanged(int))
    );

    connect(actionFixDpi, SIGNAL(triggered(bool)), SLOT(fixDpiDialogRequested()));
    connect(actionRelinking, SIGNAL(triggered(bool)), SLOT(showRelinkingDialog()));
    connect(actionSettings, SIGNAL(triggered(bool)), SLOT(openSettingsDialog()));
//begin of modified by monday2000
//Export_Subscans
//added:
    connect(
        actionExport, SIGNAL(triggered(bool)),
        this, SLOT(openExportDialog())
    );
//end of modified by monday2000
    connect(
        actionNewProject, SIGNAL(triggered(bool)),
        this, SLOT(newProject())
    );
    connect(
        actionOpenProject, SIGNAL(triggered(bool)),
        this, SLOT(openProject())
    );
    connect(
        actionSaveProject, SIGNAL(triggered(bool)),
        this, SLOT(saveProjectTriggered())
    );
    connect(
        actionSaveProjectAs, SIGNAL(triggered(bool)),
        this, SLOT(saveProjectAsTriggered())
    );
    connect(
        actionCloseProject, SIGNAL(triggered(bool)),
        this, SLOT(closeProject())
    );
    connect(
        actionQuit, SIGNAL(triggered(bool)),
        this, SLOT(close())
    );

    updateProjectActions();
    updateWindowTitle();
    updateMainArea();

    //Process settings
    settingsChanged();
    QByteArray arr = settings.value(_key_app_state).toByteArray();
    if (!arr.isEmpty()) {
        restoreState(arr);
    }

    scrollArea->horizontalScrollBar()->setDisabled(true);
    filterOptions->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    destroyAutoSaveTimer();

    QSettings settings;
    settings.setValue(_key_app_state, saveState());

    m_ptrInteractiveQueue->cancelAndClear();
    if (m_ptrBatchQueue.get()) {
        m_ptrBatchQueue->cancelAndClear();
    }
    m_ptrWorkerThread->shutdown();

    removeWidgetsFromLayout(m_pImageFrameLayout);
    removeWidgetsFromLayout(m_pOptionsFrameLayout);
    m_ptrTabbedDebugImages->clear();
}

void
MainWindow::settingsChanged()
{
    QSettings settings;
    if (settings.value(_key_app_maximized, _key_app_maximized_def) == false) {
        QByteArray arr = settings.value(_key_app_geometry).toByteArray();
        if (arr.isEmpty() || !restoreGeometry(arr)) {
            resize(1014, 689); // A sensible value.
        }
    }

    m_maxLogicalThumbSize = settings.value(_key_thumbnails_max_thumb_size, _key_thumbnails_max_thumb_size_def).toSizeF();
    if (m_ptrThumbSequence.get()) {
        if (m_ptrThumbSequence->maxLogicalThumbSize() != m_maxLogicalThumbSize) {
            m_ptrThumbSequence->setMaxLogicalThumbSize(m_maxLogicalThumbSize);
            setupThumbView();
            resetThumbSequence(currentPageOrderProvider(), ThumbnailSequence::KEEP_SELECTION);
        }
    }

    setDockingPanels(settings.value(_key_app_docking_enabled, _key_app_docking_enabled_def).toBool());

    QString default_lang = QLocale::system().name().toLower();
    default_lang.truncate(default_lang.lastIndexOf('_'));
    changeLanguage(settings.value(_key_app_language, default_lang).toString());

    m_debug = settings.value(_key_debug_enabled, _key_debug_enabled_def).toBool();

    bool autoSaveIsOn = settings.value(_key_autosave_enabled, _key_autosave_enabled_def).toBool();

    if (autoSaveIsOn) {
        createAutoSaveTimer();
    } else {
        destroyAutoSaveTimer();
    }

    float old_val = GlobalStaticSettings::m_picture_detection_sensitivity;
    GlobalStaticSettings::updateSettings();
    GlobalStaticSettings::updateHotkeys();
    if (old_val != GlobalStaticSettings::m_picture_detection_sensitivity) {
        invalidateAllThumbnails();
    }

    emit settingsUpdateRequest();
    applyShortcutsSettings();
    updateMainArea(); // to invoke preUpdateUI in optionsWidget
    invalidateAllThumbnails();
}

PageSequence
MainWindow::allPages() const
{
    return m_ptrThumbSequence->toPageSequence();
}

std::set<PageId>
MainWindow::selectedPages() const
{
    return m_ptrThumbSequence->selectedItems();
}

std::vector<PageRange>
MainWindow::selectedRanges() const
{
    return m_ptrThumbSequence->selectedRanges();
}

void
MainWindow::switchToNewProject(
    IntrusivePtr<ProjectPages> const& pages,
    QString const& out_dir, QString const& project_file_path,
    ProjectReader const* project_reader)
{
    stopBatchProcessing(CLEAR_MAIN_AREA);
    m_ptrInteractiveQueue->cancelAndClear();

    Utils::maybeCreateCacheDir(out_dir);

    m_ptrPages = pages;
    m_projectFile = project_file_path;

    if (project_reader) {
        m_selectedPage = project_reader->selectedPage();
    }

    IntrusivePtr<FileNameDisambiguator> disambiguator;
    if (project_reader) {
        disambiguator = project_reader->namingDisambiguator();
    } else {
        disambiguator.reset(new FileNameDisambiguator);
    }

    m_outFileNameGen = OutputFileNameGenerator(
                           disambiguator, out_dir, pages->layoutDirection()
                       );

    // These two need to go in this order.
    updateDisambiguationRecords(pages->toPageSequence(IMAGE_VIEW));

    // Recreate the stages and load their state.
    m_ptrStages.reset(new StageSequence(pages, newPageSelectionAccessor()));


    connect(m_ptrStages->publishFilter().get(), &publish::Filter::setProgressPanelVisible,
            this, &MainWindow::setPublishProgressPanelVisible);
    connect(m_ptrStages->publishFilter().get(), &publish::Filter::enableBundledDjVuButton,
            filterList, &StageListView::enableBundledDjVuButton);
    connect(m_ptrStages->publishFilter().get(), &publish::Filter::setBundledDjVuDoc,
            filterList, &StageListView::setBundledDjVuDoc);

    QString suggested_djvu_name;
    if (!m_projectFile.isEmpty()) {
        QFileInfo fi(m_projectFile);
        suggested_djvu_name = m_outFileNameGen.outDir() + "/" + fi.baseName() + ".djvu";
    } else {
        QSettings settings;
        suggested_djvu_name = settings.value(_key_project_last_input_dir).toString();
        // suggest folder name as default project name
        suggested_djvu_name += '/' + QDir(suggested_djvu_name).dirName() + ".djvu";
    }
    m_ptrStages->publishFilter()->setDjVuFilenameSuggestion(suggested_djvu_name);

    if (project_reader) {
        project_reader->readFilterSettings(m_ptrStages->filters());
    }

    // Connect the filter list model to the view and select
    // the first item.
    {
        ScopedIncDec<int> guard(m_ignoreSelectionChanges);
        filterList->setStages(m_ptrStages);
        filterList->selectRow(0);
        m_curFilter = 0;

        // Setting a data model also implicitly sets a new
        // selection model, so we have to reconnect to it.
        connect(
            filterList->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(filterSelectionChanged(QItemSelection))
        );
    }

    updateSortOptions();

    m_ptrContentBoxPropagator.reset(
        new ContentBoxPropagator(
            m_ptrStages->pageLayoutFilter(),
            m_ptrStages->createCompositeCacheDrivenTask(m_outFileNameGen, m_ptrStages->selectContentFilterIdx())
        )
    );

    m_ptrPageOrientationPropagator.reset(
        new PageOrientationPropagator(
            m_ptrStages->pageSplitFilter(),
            m_ptrStages->createCompositeCacheDrivenTask(m_outFileNameGen, m_ptrStages->fixOrientationFilterIdx())
        )
    );

    // Thumbnails are stored relative to the output directory,
    // so recreate the thumbnail cache.
    if (out_dir.isEmpty()) {
        m_ptrThumbnailCache.reset();
    } else {
        m_ptrThumbnailCache = Utils::createThumbnailCache(m_outFileNameGen.outDir());
    }

    m_ptrStages->setOutputFileNameGenerator(&m_outFileNameGen, m_ptrThumbnailCache.get());

    resetThumbSequence(currentPageOrderProvider());

    // restore saved project selection
    std::set<PageId> selection;
    if (project_reader && ! m_selectedPage.isNull()) {
        selection.insert(m_selectedPage.get(getCurrentView()));
    } else {
        selection.insert(m_ptrThumbSequence->firstPage().id());
    }

    ensurePageVisible(selection, m_ptrThumbSequence->selectionLeader().id());

    removeFilterOptionsWidget();
    updateProjectActions();
    updateWindowTitle();
    updateMainArea();

    if (!QDir(out_dir).exists()) {
        showRelinkingDialog();
    }
}

void
MainWindow::showNewOpenProjectPanel()
{
    std::unique_ptr<QWidget> outer_widget(new QWidget);
    QGridLayout* layout = new QGridLayout(outer_widget.get());
    outer_widget->setLayout(layout);

    NewOpenProjectPanel* nop = new NewOpenProjectPanel(outer_widget.get());

    // We use asynchronous connections because otherwise we
    // would be deleting a widget from its event handler, which
    // Qt doesn't like.
    connect(
        nop, SIGNAL(newProject()),
        this, SLOT(newProject()),
        Qt::QueuedConnection
    );
    connect(
        nop, SIGNAL(openProject()),
        this, SLOT(openProject()),
        Qt::QueuedConnection
    );
    connect(
        nop, SIGNAL(openRecentProject(QString)),
        this, SLOT(openProject(QString)),
        Qt::QueuedConnection
    );

    layout->addWidget(nop, 1, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(0, 1);
    layout->setRowStretch(2, 1);
    setImageWidget(outer_widget.release(), TRANSFER_OWNERSHIP);

    filterList->setBatchProcessingPossible(false);
    updateProjectActions();
    emit NewOpenProjectPanelShown();
}

void
MainWindow::createBatchProcessingWidget()
{
    m_ptrBatchProcessingWidget.reset(new QWidget);
    QGridLayout* layout = new QGridLayout(m_ptrBatchProcessingWidget.get());
    m_ptrBatchProcessingWidget->setLayout(layout);

    SkinnedButton* stop_btn = new SkinnedButton(
        ":/icons/stop-big.png",
        ":/icons/stop-big-hovered.png",
        ":/icons/stop-big-pressed.png",
        "",
        m_ptrBatchProcessingWidget.get()
    );
    stop_btn->setStatusTip(tr("Stop batch processing"));

    class LowerPanel : public QWidget
    {
    public:
        LowerPanel(QWidget* parent = 0) : QWidget(parent)
        {
            ui.setupUi(this);
        }

        Ui::BatchProcessingLowerPanel ui;
    };
    LowerPanel* lower_panel = new LowerPanel(m_ptrBatchProcessingWidget.get());
    m_checkBeepWhenFinished = [lower_panel]() {
        return lower_panel->ui.beepWhenFinished->isChecked();
    };

    int row = 0; // Row 0 is reserved.
    layout->addWidget(stop_btn, ++row, 1, Qt::AlignCenter);
    layout->addWidget(lower_panel, ++row, 0, 1, 3, Qt::AlignHCenter | Qt::AlignTop);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(0, 1);
    layout->setRowStretch(row, 1);

    connect(stop_btn, SIGNAL(clicked()), SLOT(stopBatchProcessing()));
}

void
MainWindow::setupThumbView()
{
    int const sb = thumbView->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int inner_width = thumbView->maximumViewportSize().width() - sb;
    if (thumbView->style()->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents, 0, thumbView)) {
        inner_width -= thumbView->frameWidth() * 2;
    }
    int const delta_x = thumbView->size().width() - inner_width;
    thumbView->setMinimumWidth((int)ceil(m_maxLogicalThumbSize.width() + delta_x));

//    thumbView->setBackgroundBrush(palette().color(QPalette::Window));
    m_ptrThumbSequence->attachView(thumbView);

    thumbView->removeEventFilter(this); // make sure installed once
    thumbView->installEventFilter(this);
    if (thumbView->verticalScrollBar()) {
        thumbView->verticalScrollBar()->removeEventFilter(this);
        thumbView->verticalScrollBar()->installEventFilter(this);
    }
}

static int _wheel_val_sum_thumbs = 0;
bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == filterOptions && ev->type() == QEvent::Resize) {
        scrollArea->setMinimumWidth(filterOptions->minimumSizeHint().width());
    }

    if (obj == thumbView) {
        if (ev->type() == QEvent::Resize) {
            invalidateAllThumbnails();
        }
    }

    if (!GlobalStaticSettings::m_fixedMaxLogicalThumbSize) {
        if (obj == thumbView || obj == thumbView->verticalScrollBar()) {
            if (ev->type() == QEvent::Wheel) {
                Qt::KeyboardModifiers mods = GlobalStaticSettings::m_hotKeyManager.get(ThumbSizeChange)->
                                             sequences().first().m_modifierSequence;
                QWheelEvent* wev = static_cast<QWheelEvent*>(ev);

                if ((wev->modifiers() & mods) == mods) {

                    const QPoint& angleDelta = wev->angleDelta();
                    _wheel_val_sum_thumbs += angleDelta.x() + angleDelta.y();

                    if (abs(_wheel_val_sum_thumbs) >= 30) {
                        wev->accept();

                        const int dy = (_wheel_val_sum_thumbs > 0) ? 10 : -10;
                        _wheel_val_sum_thumbs = 0;

                        m_maxLogicalThumbSize += QSizeF(dy, dy);
                        if (m_maxLogicalThumbSize.width() < 25.) {
                            m_maxLogicalThumbSize.setWidth(25.);
                        }
                        if (m_maxLogicalThumbSize.height() < 16.) {
                            m_maxLogicalThumbSize.setHeight(16.);
                        }

                        if (m_ptrThumbSequence.get()) {
                            m_ptrThumbSequence->setMaxLogicalThumbSize(m_maxLogicalThumbSize);
                        }

                        setupThumbView();
                        resetThumbSequence(currentPageOrderProvider(), ThumbnailSequence::KEEP_SELECTION);
                        return true;
                    }
                }
            }
        }
    }

    if (obj == statusLabelPhysSize && ev->type() == QEvent::MouseButtonRelease) {
        if (statusLabelPhysSize->selectedText().isEmpty()) {
            StatusBarProvider::toggleStatusLabelPhysSizeDisplayMode();
            // check if we need update units in page hints
            // this is required only on Content Selection stage + logical sortings chosen
            if (m_curFilter == m_ptrStages->selectContentFilterIdx() &&
                    // px and inches are displayed in inches and inches follow px so no need to invalidate if it was switched from px to inches
                    StatusBarProvider::statusLabelPhysSizeDisplayMode != StatusLabelPhysSizeDisplayMode::Inch) {
                FilterPtr const& filter =  m_ptrStages->filterAt(m_curFilter);
                if (filter->selectedPageOrder() >= filter->pageOrderOptions().size() - 2) {
                    invalidateAllThumbnails();
                }
            } else if (m_curFilter == m_ptrStages->pageSplitFilterIdx()) {
                FilterPtr const& filter =  m_ptrStages->filterAt(m_curFilter);
                if (filter->selectedPageOrder() == filter->pageOrderOptions().size() - 1) {
                    invalidateAllThumbnails();
                }
            }
        }
        displayStatusBarPageSize();
    }

    if (obj == statusLabelFileSize && ev->type() == QEvent::MouseButtonRelease) {
            if (statusLabelFileSize->selectedText().isEmpty()) {
                StatusBarProvider::toggleStatusLabelFileSizeDisplayMode();
            }
            displayStatusBarFileSize();
        }

    if (obj == statusBar() && ev->type() == StatusBarProvider::StatusBarEventType) {
        QStatusBarProviderEvent* sb_ev = static_cast<QStatusBarProviderEvent*>(ev);
        if (sb_ev->testFlag(QStatusBarProviderEvent::MousePosChanged)) {
            emit UpdateStatusBarMousePos();
        }

        if (sb_ev->testFlag(QStatusBarProviderEvent::PhysSizeChanged)) {
            emit UpdateStatusBarPageSize();
        }

        if (sb_ev->testFlag(QStatusBarProviderEvent::FileSizeChanged)) {
            emit UpdateStatusBarFileSize();
        }

        sb_ev->accept();
        return true;
    }

    return false;
}

void
MainWindow::closeEvent(QCloseEvent* const event)
{
    if (m_closing) {
        event->accept();
    } else {
        event->ignore();
        startTimer(0);
    }
}

void
MainWindow::timerEvent(QTimerEvent* const event)
{
    // We only use the timer event for delayed closing of the window.
    killTimer(event->timerId());

    pauseAutoSaveTimer();
    if (closeProjectInteractive()) {
    m_closing = true;
    saveWindowSettigns();
    close();
    }

    resumeAutoSaveTimer();
}

MainWindow::SavePromptResult
MainWindow::promptProjectSave()
{
    QMessageBox::StandardButton const res = QMessageBox::question(
            this, tr("Save Project"), tr("Save this project?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
                                            );

    switch (res) {
    case QMessageBox::Save:
        return SAVE;
    case QMessageBox::Discard:
        return DONT_SAVE;
    default:
        return CANCEL;
    }
}

bool
MainWindow::compareFiles(QString const& fpath1, QString const& fpath2)
{
    // rough comparision as order of elements in XML may vary
    QFile file1(fpath1);
    QFile file2(fpath2);

    if (!file1.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (!file2.open(QIODevice::ReadOnly)) {
        return false;
    }

    if (!file1.isSequential() && !file2.isSequential()) {
        if (file1.size() != file2.size()) {
            return false;
        }
    }

    int const chunk_size = 4096;
    for (;;) {
        QByteArray const chunk1(file1.read(chunk_size));
        QByteArray const chunk2(file2.read(chunk_size));
        if (chunk1.size() != chunk2.size()) {
            return false;
        } else if (chunk1.size() == 0) {
            return true;
        }
    }
}

IntrusivePtr<PageOrderProvider const>
MainWindow::currentPageOrderProvider() const
{
    int const idx = sortOptions->currentIndex();
    if (idx < 0) {
        return IntrusivePtr<PageOrderProvider const>();
    }

    IntrusivePtr<AbstractFilter> const filter(m_ptrStages->filterAt(m_curFilter));
    return filter->pageOrderOptions()[idx].provider();
}

IntrusivePtr<PageOrderProvider const>
MainWindow::defaultPageOrderProvider() const
{
    IntrusivePtr<AbstractFilter> const filter(m_ptrStages->filterAt(m_curFilter));
    if (filter->pageOrderOptions().size() > 0) {
        return filter->pageOrderOptions()[0].provider();
    } else {
        return IntrusivePtr<PageOrderProvider const>();
    }
}

void
MainWindow::updateSortOptions()
{
    ScopedIncDec<int> const guard(m_ignorePageOrderingChanges);

    IntrusivePtr<AbstractFilter> const filter(m_ptrStages->filterAt(m_curFilter));

    sortOptions->clear();

    for (PageOrderOption const& opt : filter->pageOrderOptions()) {
        sortOptions->addItem(opt.name());
    }

    sortOptionsWgt->setVisible(m_ptrPages.get() && m_ptrPages->numImages() && sortOptions->count());

    if (sortOptions->count() > 0) {
        sortOptions->setCurrentIndex(filter->selectedPageOrder());
    }
}

void
MainWindow::resetThumbSequence(
    IntrusivePtr<PageOrderProvider const> const& page_order_provider,
    ThumbnailSequence::SelectionAction const action)
{
    if (m_ptrThumbnailCache.get()) {
        IntrusivePtr<CompositeCacheDrivenTask> const task(
            m_ptrStages->createCompositeCacheDrivenTask(m_outFileNameGen, m_curFilter)
        );

        m_ptrThumbSequence->setThumbnailFactory(
            IntrusivePtr<ThumbnailFactory>(
                new ThumbnailFactory(
                    m_ptrThumbnailCache,
                    m_maxLogicalThumbSize, task
                )
            )
        );
    }

    std::set<PageId> _selection = m_ptrThumbSequence->selectedItems();
    const PageId _selectionLeader = m_ptrThumbSequence->selectionLeader().id();

    m_ptrThumbSequence->reset(
        m_ptrPages->toPageSequence(getCurrentView()),
        action, page_order_provider
    );

    if (!m_ptrThumbnailCache.get()) {
        // Empty project.
        assert(m_ptrPages->numImages() == 0);
        m_ptrThumbSequence->setThumbnailFactory(
            IntrusivePtr<ThumbnailFactory>()
        );
    }

    ensurePageVisible(_selection, _selectionLeader, action);
}

void
MainWindow::ensurePageVisible(const std::set<PageId>& _selectedPages, PageId selectionLeader, ThumbnailSequence::SelectionAction const action)
{

    QVector<PageId> to_be_selected;
    for (const PageId& page : _selectedPages) {
        to_be_selected.append(page);
    }

    if (!selectionLeader.isNull() &&
            !to_be_selected.contains(selectionLeader)) {
        to_be_selected.append(selectionLeader);
    }

    std::sort(to_be_selected.begin(), to_be_selected.end());

    QSet<PageId> maybe_selected;
    PageSequence const displayed_pages = m_ptrThumbSequence->toPageSequenceById();

    for (const PageId& page : qAsConst(to_be_selected)) {
        for (const PageInfo& displ_page : displayed_pages) {
            if (displ_page.id().imageId() == page.imageId()) {
                if (page.subPage() == displ_page.id().subPage()) {
                    maybe_selected += page;
                } else if ((displ_page.id().subPage() == PageId::SINGLE_PAGE) !=
                           (page.subPage() == PageId::SINGLE_PAGE)) {   // different levels
                    maybe_selected += displ_page.id();
                    if (selectionLeader.imageId() == page.imageId()) {
                        selectionLeader = displ_page.id();
                    }
                }
            }
            if (page.imageId() < displ_page.id().imageId()) {
                break;
            }
        }
    }

    m_ptrThumbSequence->setSelection(maybe_selected, action);
    m_ptrThumbSequence->setSelection(selectionLeader, ThumbnailSequence::KEEP_SELECTION);

    thumbView->ensureVisible(m_ptrThumbSequence->selectionLeaderSceneRect(), 0, 0);
}

void
MainWindow::setOptionsWidget(FilterOptionsWidget* widget, Ownership const ownership)
{
    if (isBatchProcessingInProgress()) {
        if (ownership == TRANSFER_OWNERSHIP) {
            delete widget;
        }
        return;
    }

    if (m_ptrOptionsWidget != widget) {
        removeWidgetsFromLayout(m_pOptionsFrameLayout);
    }

    // Delete the old widget we were owning, if any.
    m_optionsWidgetCleanup.clear();

    if (ownership == TRANSFER_OWNERSHIP) {
        m_optionsWidgetCleanup.add(widget);
    }

    if (m_ptrOptionsWidget == widget) {
        return;
    }

    if (m_ptrOptionsWidget) {
        disconnect(
            m_ptrOptionsWidget, SIGNAL(reloadRequested()),
            this, SLOT(reloadRequested())
        );
        disconnect(
            m_ptrOptionsWidget, SIGNAL(invalidateThumbnail(PageId)),
            this, SLOT(invalidateThumbnail(PageId))
        );
        disconnect(
            m_ptrOptionsWidget, SIGNAL(invalidateThumbnail(PageInfo)),
            this, SLOT(invalidateThumbnail(PageInfo))
        );
        disconnect(
            m_ptrOptionsWidget, SIGNAL(invalidateAllThumbnails()),
            this, SLOT(invalidateAllThumbnails())
        );
        disconnect(
            m_ptrOptionsWidget, SIGNAL(goToPage(PageId)),
            this, SLOT(goToPage(PageId))
        );
    }

    m_pOptionsFrameLayout->addWidget(widget);
    m_ptrOptionsWidget = widget;
    widget->show();

    // We use an asynchronous connection here, because the slot
    // will probably delete the options panel, which could be
    // responsible for the emission of this signal.  Qt doesn't
    // like when we delete an object while it's emitting a signal.
    connect(
        widget, SIGNAL(reloadRequested()),
        this, SLOT(reloadRequested()), Qt::QueuedConnection
    );
    connect(
        widget, SIGNAL(invalidateThumbnail(PageId)),
        this, SLOT(invalidateThumbnail(PageId))
    );
    connect(
        widget, SIGNAL(invalidateThumbnail(PageInfo)),
        this, SLOT(invalidateThumbnail(PageInfo))
    );
    connect(
        widget, SIGNAL(invalidateAllThumbnails()),
        this, SLOT(invalidateAllThumbnails())
    );
    connect(
        widget, SIGNAL(goToPage(PageId)),
        this, SLOT(goToPage(PageId))
    );
}

void
MainWindow::setImageWidget(
    QWidget* widget, Ownership const ownership,
    DebugImages* debug_images)
{
    if (isBatchProcessingInProgress() && widget != m_ptrBatchProcessingWidget.get()) {
        if (ownership == TRANSFER_OWNERSHIP) {
            delete widget;
        }
        return;
    }

    removeImageWidget();

    if (ownership == TRANSFER_OWNERSHIP) {
        m_imageWidgetCleanup.add(widget);
    }

    if (!debug_images || debug_images->empty()) {
        m_pImageFrameLayout->addWidget(widget);
    } else {
        m_ptrTabbedDebugImages->addTab(widget, "Main");
        AutoRemovingFile file;
        QString label;
        while (!(file = debug_images->retrieveNext(&label)).get().isNull()) {
            QWidget* widget = new DebugImageView(file);
            m_imageWidgetCleanup.add(widget);
            m_ptrTabbedDebugImages->addTab(widget, label);
        }
        m_pImageFrameLayout->addWidget(m_ptrTabbedDebugImages.get());
    }

    widget->show();
}

void
MainWindow::removeImageWidget()
{
    removeWidgetsFromLayout(m_pImageFrameLayout);

    m_ptrTabbedDebugImages->clear();

    // Delete the old widget we were owning, if any.
    m_imageWidgetCleanup.clear();
}

void
MainWindow::invalidateThumbnail(PageId const& page_id)
{
    m_ptrThumbSequence->invalidateThumbnail(page_id);
}

void
MainWindow::invalidateThumbnail(PageInfo const& page_info)
{
    m_ptrThumbSequence->invalidateThumbnail(page_info);
}

void
MainWindow::invalidateAllThumbnails()
{
    m_ptrThumbSequence->invalidateAllThumbnails();
}

IntrusivePtr<AbstractCommand0<void> >
MainWindow::relinkingDialogRequester()
{
    class Requester : public AbstractCommand0<void>
    {
    public:
        Requester(MainWindow* wnd) : m_ptrWnd(wnd) {}

        virtual void operator()()
        {
            if (MainWindow* wnd = m_ptrWnd) {
                wnd->showRelinkingDialog();
            }
        }
    private:
        QPointer<MainWindow> m_ptrWnd;
    };

    return IntrusivePtr<AbstractCommand0<void> >(new Requester(this));
}

void
MainWindow::showRelinkingDialog()
{
    if (!isProjectLoaded()) {
        return;
    }

    RelinkingDialog* dialog = new RelinkingDialog(m_projectFile, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);

    m_ptrPages->listRelinkablePaths(dialog->pathCollector());
    dialog->pathCollector()(RelinkablePath(m_outFileNameGen.outDir(), RelinkablePath::Dir));

    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        this->performRelinking(dialog->relinker());
    });

    dialog->show();
}

void
MainWindow::performRelinking(IntrusivePtr<AbstractRelinker> const& relinker)
{
    assert(relinker.get());

    if (!isProjectLoaded()) {
        return;
    }

    m_ptrPages->performRelinking(*relinker);
    m_ptrStages->performRelinking(*relinker);
    m_outFileNameGen.performRelinking(*relinker);

    Utils::maybeCreateCacheDir(m_outFileNameGen.outDir());

    m_ptrThumbnailCache->setThumbDir(Utils::outputDirToThumbDir(m_outFileNameGen.outDir()));
    resetThumbSequence(currentPageOrderProvider());
    m_selectedPage.set(m_ptrThumbSequence->selectionLeader().id(), getCurrentView());

    reloadRequested();
}

void
MainWindow::goFirstPage(bool in_selection)
{
    if (isBatchProcessingInProgress() || !isProjectLoaded()) {
        return;
    }

    PageInfo const first_page(m_ptrThumbSequence->firstPage(in_selection));
    if (!first_page.isNull()) {
        goToPage(first_page.id(), in_selection ? ThumbnailSequence::KEEP_SELECTION : ThumbnailSequence::RESET_SELECTION);
    }
}

void
MainWindow::goLastPage(bool in_selection)
{
    if (isBatchProcessingInProgress() || !isProjectLoaded()) {
        return;
    }

    PageInfo const last_page(m_ptrThumbSequence->lastPage(in_selection));
    if (!last_page.isNull()) {
        goToPage(last_page.id(), in_selection ? ThumbnailSequence::KEEP_SELECTION : ThumbnailSequence::RESET_SELECTION);
    }
}

void
MainWindow::jumpToPage(int cnt, bool in_selection)
{
    if (isBatchProcessingInProgress() || !isProjectLoaded() || cnt == 0) {
        return;
    }

    const bool jump_prev = cnt < 0;
    cnt = abs(cnt);

    PageId pg_id(m_ptrThumbSequence->selectionLeader().id());
    if (pg_id.isNull()) {
        return;
    }

    while (cnt-- > 0) {
        PageId _id = jump_prev ? m_ptrThumbSequence->prevPage(pg_id, in_selection).id() :
                     m_ptrThumbSequence->nextPage(pg_id, in_selection).id();
        if (!_id.isNull()) {
            pg_id = _id;
        } else {
            break;
        }
    }

    goToPage(pg_id, in_selection ? ThumbnailSequence::KEEP_SELECTION : ThumbnailSequence::RESET_SELECTION);
}

void
MainWindow::goNextPage(int cnt, bool in_selection)
{
    jumpToPage(cnt, in_selection);
}

void
MainWindow::goPrevPage(int cnt, bool in_selection)
{
    jumpToPage(cnt, in_selection);
}

void
MainWindow::goToPage(PageId const& page_id, ThumbnailSequence::SelectionAction const action)
{
    bool was_checked = focusButton->isChecked();
    focusButton->setChecked(true);

    if (qApp && qApp->keyboardModifiers() & Qt::ShiftModifier) {
        m_ptrThumbSequence->setSelectionWithShift(page_id);
    } else {
        m_ptrThumbSequence->setSelection(page_id, action);
    }

    // If the page was already selected, it will be reloaded.
    // That's by design.
    updateMainArea();
    if (!was_checked) {
        focusButton->setChecked(false);
    }

}

void
MainWindow::currentPageChanged(
    PageInfo const& page_info, QRectF const& thumb_rect,
    ThumbnailSequence::SelectionFlags const flags)
{
    m_selectedPage.set(page_info.id(), getCurrentView());

    if ((flags & ThumbnailSequence::SELECTED_BY_USER) || focusButton->isChecked()) {
        if (!(flags & ThumbnailSequence::AVOID_SCROLLING_TO)) {
            thumbView->ensureVisible(thumb_rect, 0, 0);
        }
    }

    if (flags & ThumbnailSequence::SELECTED_BY_USER) {
        if (isBatchProcessingInProgress()) {
            stopBatchProcessing();
        } else if (!(flags & ThumbnailSequence::REDUNDANT_SELECTION)) {
            // Start loading / processing the newly selected page.
            updateMainArea();
        }
    }

}

void getPageNumbersFromStr(const QString& str, QVector<int>& results)
{
    results.clear();

    QRegularExpression rx("(\\d+)([: -]*)");
    QRegularExpressionMatchIterator i = rx.globalMatch(str);
    int interval_start = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        int val = match.captured(1).toInt();
        const QString tail = match.captured(2);
        if (tail.contains('-') || tail.contains(':')) {
            if (interval_start == 0) {
                interval_start = val;
            }
        } else {
            while (interval_start > 0 && interval_start < val) {
                results.push_back(interval_start++);
            }
            results.push_back(val);
            interval_start = 0;
        }
    }
}

void
MainWindow::pageContextMenuRequested(
    PageInfo const& page_info_, QPoint const& screen_pos, bool selected)
{
    if (isBatchProcessingInProgress()) {
        return;
    }

    // Make a copy to prevent it from being invalidated.
    PageInfo const page_info(page_info_);

    if (!selected) {
        goToPage(page_info.id());
    }

    QMenu menu;

    // populate Open source with.. submenu
    bool open_with_menu_is_used = false;
    const QString source_image_file = page_info.imageId().filePath();
    if (!source_image_file.isEmpty()) {
        const QString mime = QMimeDatabase().mimeTypeForFile(source_image_file).name();
        QMenu* open_with_menu = OpenWithMenuProvider::getOpenWithMenu(mime);
        if (open_with_menu) {
            open_with_menu->setTitle(tr("Open source with..."));

            const QList<QAction*> actions = open_with_menu->actions();
            for (const QAction* const act : actions) {
                QObject::connect(act, &QAction::triggered, [source_image_file, act]() {
                    QString cmd = act->data().toString();
                    if (cmd.contains("%u", Qt::CaseInsensitive)) {
                        cmd = cmd.replace("%u", " \"" + source_image_file + "\" ", Qt::CaseInsensitive);
                    } else {
                        cmd += " \"" + source_image_file + "\" ";
                    }
                    QProcess::startDetached(cmd);
                });
            }

            menu.addMenu(open_with_menu);
            open_with_menu_is_used = true;
        }
    }

    menu.addSeparator();

    QMenu* menu_copy = menu.addMenu(
                                tr("Copy")
                            );

    const QString iconThemeName("edit-copy");
    if (QIcon::hasThemeIcon(iconThemeName)) {
        menu_copy->setIcon(QIcon::fromTheme(iconThemeName));
    }

    menu_copy->addAction(actionCopySourceFileName);
    menu_copy->addAction(actionCopyOutputFileName);
    menu_copy->addAction(actionCopyPageNumber);

    QMenu* menu_insert = menu.addMenu(
                                tr("Insert")
                            );

    QAction* ins_before = menu_insert->addAction(
                              QIcon(":/icons/insert-before-16.png"), tr("Insert before...")
                          );
    QAction* ins_after = menu_insert->addAction(
                             QIcon(":/icons/insert-after-16.png"), tr("Insert after...")
                         );
    QMenu* menu_ins_empty = menu_insert->addMenu(
                                tr("Insert empty page")
                            );
    menu_ins_empty->addAction(actionInsertEmptyPgBefore);
    menu_ins_empty->addAction(actionInsertEmptyPgAfter);

    QAction* rename_output = menu.addAction(
                                 tr("Rename result filename...")
                             );

    menu.addSeparator();

    QAction* remove = menu.addAction(
                          QIcon(":/icons/user-trash.png"), tr("Remove from project...")
                      );

    QAction* regenerate = nullptr;
    bool regenerate_action_added = m_debug &&
                                   (m_curFilter != m_ptrStages->fixOrientationFilterIdx()) &&
                                   (m_curFilter != m_ptrStages->pageLayoutFilterIdx());
    regenerate_action_added |= open_with_menu_is_used;

    if (regenerate_action_added) {
        menu.addSeparator();
        regenerate = menu.addAction(tr("Regenerate result"));
    }

    QAction* action = menu.exec(screen_pos);
    if (action == ins_before) {
        showInsertFileDialog(BEFORE, page_info.imageId());
    } else if (action == ins_after) {
        showInsertFileDialog(AFTER, page_info.imageId());
    } else if (action == rename_output) {
        showRenameResultFileDialog(page_info);
    } else if (action == remove) {
        showRemovePagesDialog(m_ptrThumbSequence->selectedItems());
    } else if (regenerate_action_added && action == regenerate) {
        m_ptrStages->filterAt(m_curFilter)->invalidateSetting(page_info.id());
        invalidateThumbnail(page_info.id());
        reloadRequested();
    }
}

void
MainWindow::pastLastPageContextMenuRequested(QPoint const& screen_pos)
{
    if (!isProjectLoaded()) {
        return;
    }

    QMenu menu;
    menu.addAction(QIcon(":/icons/insert-here-16.png"), tr("Insert here..."));

    if (menu.exec(screen_pos)) {
        showInsertFileDialog(BEFORE, ImageId());
    }
}

void
MainWindow::thumbViewFocusToggled(bool const checked)
{
    QRectF const rect(m_ptrThumbSequence->selectionLeaderSceneRect());
    if (rect.isNull()) {
        // No selected items.
        return;
    }

    if (checked) {
        thumbView->ensureVisible(rect, 0, 0);
    }
}

void
MainWindow::thumbViewScrolled()
{
    QRectF const rect(m_ptrThumbSequence->selectionLeaderSceneRect());
    if (rect.isNull()) {
        // No items selected.
        return;
    }

    QRectF const viewport_rect(thumbView->viewport()->rect());
    QRectF const viewport_item_rect(
        thumbView->viewportTransform().mapRect(rect)
    );

    double const intersection_threshold = 0.5;
    if (viewport_item_rect.top() >= viewport_rect.top() &&
            viewport_item_rect.top() + viewport_item_rect.height()
            * intersection_threshold <= viewport_rect.bottom()) {
        // Item is visible.
    } else if (viewport_item_rect.bottom() <= viewport_rect.bottom() &&
               viewport_item_rect.bottom() - viewport_item_rect.height()
               * intersection_threshold >= viewport_rect.top()) {
        // Item is visible.
    } else {
        focusButton->setChecked(false);
    }
    invalidateAllThumbnails();
}

void
MainWindow::filterSelectionChanged(QItemSelection const& selected)
{
    if (m_ignoreSelectionChanges) {
        return;
    }

    if (selected.empty()) {
        return;
    }

    m_ptrInteractiveQueue->cancelAndClear();
    if (m_ptrBatchQueue.get()) {
        // Should not happen, but just in case.
        m_ptrBatchQueue->cancelAndClear();
    }

    bool const was_below_fix_orientation = isBelowFixOrientation(m_curFilter);
    bool const was_below_select_content = isBelowSelectContent(m_curFilter);
    m_curFilter = selected.front().top();
    bool const now_below_fix_orientation = isBelowFixOrientation(m_curFilter);
    bool const now_below_select_content = isBelowSelectContent(m_curFilter);

    m_ptrStages->filterAt(m_curFilter)->selected();

    updateSortOptions();

    // Propagate context boxes down the stage list, if necessary.
    if (!was_below_select_content && now_below_select_content) {
        // IMPORTANT: this needs to go before resetting thumbnails,
        // because it may affect them.
        if (m_ptrContentBoxPropagator.get()) {
            m_ptrContentBoxPropagator->propagate(*m_ptrPages);
        } // Otherwise probably no project is loaded.
    }

    // Propagate page orientations (that might have changed) to the "Split Pages" stage.
    if (!was_below_fix_orientation && now_below_fix_orientation) {
        // IMPORTANT: this needs to go before resetting thumbnails,
        // because it may affect them.
        if (m_ptrPageOrientationPropagator.get()) {
            m_ptrPageOrientationPropagator->propagate(*m_ptrPages);
        } // Otherwise probably no project is loaded.
    }

    focusButton->setChecked(true); // Should go before resetThumbSequence().
    resetThumbSequence(currentPageOrderProvider(), ThumbnailSequence::KEEP_SELECTION);

    updateMainArea();

    StatusBarProvider::changeFilterIdx(m_curFilter);
    const bool v = (m_curFilter == m_ptrStages->publishFilterIdx());
    statusLabelFileSize->setVisible(v);
    statusLabelPhysSize->setVisible(!v);

    const bool enabled = isProjectLoaded() && isPublishFilter();
    actionManageContents->setEnabled(enabled);
    actionSetMetadata->setEnabled(enabled);
    actionManageSharedDictionaries->setEnabled(enabled);
//    actionSetBundledFilename->setEnabled(enabled);
}

void MainWindow::switchFilter1()
{
    filterList->selectRow(0);
}

void MainWindow::switchFilter2()
{
    filterList->selectRow(1);
}

void MainWindow::switchFilter3()
{
    filterList->selectRow(2);
}

void MainWindow::switchFilter4()
{
    filterList->selectRow(3);
}

void MainWindow::switchFilter5()
{
    filterList->selectRow(4);
}

void MainWindow::switchFilter6()
{
    filterList->selectRow(5);
}

void
MainWindow::pageOrderingChanged(int idx)
{
    if (m_ignorePageOrderingChanges) {
        return;
    }

    focusButton->setChecked(true); // Keep the current page in view.

    FilterPtr const& cur_filter = m_ptrStages->filterAt(m_curFilter);
    cur_filter->selectPageOrder(idx);
    sortOptions->setToolTip(cur_filter->pageOrderOptions()[idx].toolTip());

    std::set<PageId> _selection = m_ptrThumbSequence->selectedItems();
    const PageId _selectionLeader = m_ptrThumbSequence->selectionLeader().id();

    m_ptrThumbSequence->reset(
        m_ptrPages->toPageSequence(getCurrentView()),
        ThumbnailSequence::KEEP_SELECTION,
        currentPageOrderProvider()
    );

    ensurePageVisible(_selection, _selectionLeader);
}

void
MainWindow::reloadRequested()
{
    // Start loading / processing the current page.
    updateMainArea();
}

void
MainWindow::startBatchProcessing()
{
    if (isBatchProcessingInProgress() || !isProjectLoaded()) {
        return;
    }

    m_ptrInteractiveQueue->cancelAndClear();

    QSettings settings;

    bool show_dlg = !settings.value(_key_batch_dialog_remember_choice, _key_batch_dialog_remember_choice_def).toBool();
    bool processAll = !settings.value(_key_batch_dialog_start_from_current, _key_batch_dialog_start_from_current_def).toBool();

    if (show_dlg) {
        StartBatchProcessingDialog dialog(this, processAll);

        dialog.show();
        if (! dialog.exec()) {
            return;
        }
        processAll = dialog.isAllPagesChecked();
        settings.setValue(_key_batch_dialog_remember_choice, dialog.isRememberChoiceChecked());
        settings.setValue(_key_batch_dialog_start_from_current, !processAll);
    }

    m_ptrBatchQueue.reset(
        new ProcessingTaskQueue(
            currentPageOrderProvider().get()
            ? ProcessingTaskQueue::RANDOM_ORDER
            : ProcessingTaskQueue::SEQUENTIAL_ORDER
        )
    );

    PageInfo start_page = processAll ? m_ptrThumbSequence->firstPage() : m_ptrThumbSequence->selectionLeader();
    PageInfo page = start_page;

    int pages_in_queue = 0;
    if (isPublishFilter()) {
        QVector<PageInfo> pages_to_process;
        for (; !page.isNull(); page = m_ptrThumbSequence->nextPage(page.id())) {
            pages_to_process += page;
        }
        pages_to_process = m_ptrStages->publishFilter()->filterBatchPages(pages_to_process);
        for (const PageInfo& page: qAsConst(pages_to_process)) {
            m_ptrBatchQueue->addProcessingTask(
                        page, createCompositeTask(page, m_curFilter, /*batch=*/true, m_debug)
                        );
        }
        pages_in_queue = pages_to_process.count();
    } else {
        for (; !page.isNull(); page = m_ptrThumbSequence->nextPage(page.id())) {
            m_ptrBatchQueue->addProcessingTask(
                        page, createCompositeTask(page, m_curFilter, /*batch=*/true, m_debug)
                        );
            pages_in_queue++;
        }
    }

    m_ptrBatchQueue->startProgressTracking(pages_in_queue);

    focusButton->setChecked(true);

    removeFilterOptionsWidget();
    filterList->setBatchProcessingInProgress(true);
    filterList->setEnabled(false);

    BackgroundTaskPtr const task(m_ptrBatchQueue->takeForProcessing());
    if (task) {
        m_ptrWorkerThread->performTask(task);
    } else {
        stopBatchProcessing();
    }

    page = m_ptrBatchQueue->selectedPage();
    if (!page.isNull()) {
        m_ptrThumbSequence->setSelection(page.id());
    }

    // Display the batch processing screen.
    updateMainArea();
}

void
MainWindow::stopBatchProcessing(MainAreaAction main_area)
{
    if (!isBatchProcessingInProgress()) {
        return;
    }

    PageInfo const page(m_ptrBatchQueue->selectedPage());
    if (!page.isNull()) {
        m_ptrThumbSequence->setSelection(page.id());
    }

    m_ptrBatchQueue->cancelAndClear();
    m_ptrBatchQueue.reset();

    filterList->setBatchProcessingInProgress(false);
    filterList->setEnabled(true);

    switch (main_area) {
    case UPDATE_MAIN_AREA:
        updateMainArea();
        break;
    case CLEAR_MAIN_AREA:
        removeImageWidget();
        break;
    }

    m_ptrStages->filterAt(m_curFilter)->updateStatistics();
    resetThumbSequence(currentPageOrderProvider());
}

void
MainWindow::filterResult(BackgroundTaskPtr const& task, FilterResultPtr const& result)
{
    // Cancelled or not, we must mark it as finished.
    m_ptrInteractiveQueue->processingFinished(task);
    if (m_ptrBatchQueue.get()) {
        m_ptrBatchQueue->processingFinished(task);
    }

    updateWindowTitle();

    if (task->isCancelled()) {
        return;
    }

    if (!isBatchProcessingInProgress()) {
        if (!result->filter()) {
            // Error loading file.  No special action is necessary.
        } else if (result->filter() != m_ptrStages->filterAt(m_curFilter)) {
            // Error from one of the previous filters.
            int const idx = m_ptrStages->findFilter(result->filter());
            assert(idx >= 0);
            m_curFilter = idx;

            ScopedIncDec<int> selection_guard(m_ignoreSelectionChanges);
            filterList->selectRow(idx);
        }
    }

    // This needs to be done even if batch processing is taking place,
    // for instance because thumbnail invalidation is done from here.
    result->updateUI(this);

    if (isBatchProcessingInProgress()) {
        if (m_ptrBatchQueue->allProcessed()) {
            stopBatchProcessing();

            QApplication::alert(this); // Flash the taskbar entry.
            if (m_checkBeepWhenFinished()) {
                QSettings settings;
                QString cmd = settings.value(_key_app_alert_cmd, _key_app_alert_cmd_def).toString();
                if (cmd.isEmpty()) {
#ifdef HAVE_CANBERRA
                    if (m_canberraPlayer.isWorking())
                        m_canberraPlayer.play();
                    else
#endif
                    QApplication::beep();
                } else {
                    std::system(cmd.toStdString().c_str());
                }
            }

            if (m_selectedPage.get(getCurrentView()) == m_ptrThumbSequence->lastPage().id()) {
                // If batch processing finished at the last page, jump to the first one.
                goFirstPage();
            }

            return;
        }

        BackgroundTaskPtr const task(m_ptrBatchQueue->takeForProcessing());
        if (task) {
            m_ptrWorkerThread->performTask(task);
        }

        PageInfo const page(m_ptrBatchQueue->selectedPage());
        if (!page.isNull()) {
            m_ptrThumbSequence->setSelection(page.id());
        }
    }
}

void
MainWindow::fixDpiDialogRequested()
{
    if (isBatchProcessingInProgress() || !isProjectLoaded()) {
        return;
    }

    assert(!m_ptrFixDpiDialog);
    m_ptrFixDpiDialog = new FixDpiDialog(m_ptrPages->toImageFileInfo(), this);
    m_ptrFixDpiDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_ptrFixDpiDialog->setWindowModality(Qt::WindowModal);

    connect(m_ptrFixDpiDialog, SIGNAL(accepted()), SLOT(fixedDpiSubmitted()));

    m_ptrFixDpiDialog->show();
}

void
MainWindow::fixedDpiSubmitted()
{
    assert(m_ptrFixDpiDialog);
    assert(m_ptrPages);
    assert(m_ptrThumbSequence.get());

    PageInfo const selected_page_before(m_ptrThumbSequence->selectionLeader());

    m_ptrPages->updateMetadataFrom(m_ptrFixDpiDialog->files());

    // The thumbnail list also stores page metadata, including the DPI.
    m_ptrThumbSequence->reset(
        m_ptrPages->toPageSequence(getCurrentView()),
        ThumbnailSequence::KEEP_SELECTION, m_ptrThumbSequence->pageOrderProvider()
    );

    PageInfo const selected_page_after(m_ptrThumbSequence->selectionLeader());

    // Reload if the current page was affected.
    // Note that imageId() isn't supposed to change - we check just in case.
    if (selected_page_before.imageId() != selected_page_after.imageId() ||
            selected_page_before.metadata() != selected_page_after.metadata()) {

        reloadRequested();
    }
}

void
MainWindow::saveProjectTriggered()
{
    if (m_projectFile.isEmpty()) {
        pauseAutoSaveTimer();
        saveProjectAsTriggered();
        resumeAutoSaveTimer();
        return;
    }

    pauseAutoSaveTimer();
    if (saveProjectWithFeedback(m_projectFile)) {
        updateWindowTitle();
    }
    resumeAutoSaveTimer();
}

void
MainWindow::saveProjectAsTriggered()
{
    // XXX: this function is duplicated in OutOfMemoryDialog.

    QString project_dir;
    if (!m_projectFile.isEmpty()) {
        project_dir = QFileInfo(m_projectFile).absolutePath();
    } else {
        QSettings settings;
        project_dir = settings.value(_key_project_last_input_dir).toString();
        // suggest folder name as default project name
        project_dir += '/' + QDir(project_dir).dirName();
    }

    QString project_file(
        QFileDialog::getSaveFileName(
            this, QString(), project_dir,
            tr("Scan Tailor Projects") + " (*.ScanTailor)"
        )
    );
    if (project_file.isEmpty()) {
        return;
    }

    if (!project_file.endsWith(".ScanTailor", Qt::CaseInsensitive)) {
        project_file += ".ScanTailor";
    }

    if (saveProjectWithFeedback(project_file)) {
        m_projectFile = project_file;
        updateWindowTitle();

        QSettings settings;
        settings.setValue(
            _key_project_last_dir,
            QFileInfo(m_projectFile).absolutePath()
        );

        RecentProjects rp;
        rp.read();
        rp.setMostRecent(m_projectFile);
        rp.write();
    }
}

void
MainWindow::newProject()
{
    pauseAutoSaveTimer();
    if (!closeProjectInteractive()) {
        resumeAutoSaveTimer();
        return;
    }

    // It will delete itself when it's done.
    ProjectCreationContext* context = new ProjectCreationContext(this);
    connect(
        context, SIGNAL(done(ProjectCreationContext*)),
        this, SLOT(newProjectCreated(ProjectCreationContext*))
    );
}

void
MainWindow::newProjectCreated(ProjectCreationContext* context)
{
    IntrusivePtr<ProjectPages> pages(
        new ProjectPages(
            context->files(), ProjectPages::AUTO_PAGES,
            context->layoutDirection()
        )
    );
    switchToNewProject(pages, context->outDir());
    setAutoSaveInputDir(context->inputDir());
}

void
MainWindow::openProject()
{
    pauseAutoSaveTimer();

    if (!closeProjectInteractive()) {
        resumeAutoSaveTimer();
        return;
    }
    setAutoSaveInputDir("");

    QSettings settings;
    QString const project_dir(settings.value(_key_project_last_dir).toString());

    QString const project_file(
        QFileDialog::getOpenFileName(
            this, tr("Open Project"), project_dir,
            tr("Scan Tailor Projects") + " (*.ScanTailor)"
        )
    );
    if (project_file.isEmpty()) {
        // Cancelled by user.
        resumeAutoSaveTimer();
        return;
    }

    openProject(project_file);
}

void
MainWindow::openProject(QString const& project_file)
{
    QFile file(project_file);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(
            this, tr("Error"),
            tr("Unable to open the project file.")
        );
        resumeAutoSaveTimer();
        return;
    }

    QDomDocument doc;
    QString errorMsg; int errorLine; int errorColumn;
    if (!doc.setContent(&file, false, &errorMsg, &errorLine, &errorColumn)) {
        QMessageBox::warning(
            this, tr("Error"),
                    QString(tr("The project file is broken.\nError message: %1\nLine: %2, Column: %3")).arg(errorMsg).arg(errorLine).arg(errorColumn)
        );
        resumeAutoSaveTimer();
        return;
    }

    file.close();

    ProjectOpeningContext* context = new ProjectOpeningContext(this, project_file, doc);
    connect(context, SIGNAL(done(ProjectOpeningContext*)), SLOT(projectOpened(ProjectOpeningContext*)));
    context->proceed();
}

void
MainWindow::projectOpened(ProjectOpeningContext* context)
{
    RecentProjects rp;
    rp.read();
    rp.setMostRecent(context->projectFile());
    rp.write();

    QSettings settings;
    settings.setValue(
        _key_project_last_dir,
        QFileInfo(context->projectFile()).absolutePath()
    );

    switchToNewProject(
        context->projectReader()->pages(),
        context->projectReader()->outputDirectory(),
        context->projectFile(), context->projectReader()
    );

    setAutoSaveInputDir(context->projectReader()->inputDirectory());
}

void
MainWindow::closeProject()
{
    pauseAutoSaveTimer();
    closeProjectInteractive();
    resumeAutoSaveTimer();
}

void
MainWindow::openSettingsDialog()
{
    saveWindowSettigns();
    SettingsDialog* dialog = new SettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    connect(dialog, SIGNAL(settingsChanged()), this, SLOT(settingsChanged()));
    dialog->exec();
}

//begin of modified by monday2000
//Export_Subscans
//Original_Foreground_Mixed
//added:
void
MainWindow::openExportDialog()
{
    m_p_export_dialog = new exporting::ExportDialog(this, m_outFileNameGen.outDir());

    m_p_export_dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_p_export_dialog->setWindowModality(Qt::WindowModal);
    m_p_export_dialog->show();

    connect(m_p_export_dialog, &exporting::ExportDialog::ExportOutputSignal, this, &MainWindow::ExportOutput);
    connect(m_p_export_dialog, &exporting::ExportDialog::SetStartExportSignal, this, &MainWindow::SetStartExport);
}

void
MainWindow::ExportOutput(exporting::ExportSettings settings)
{
    if (isBatchProcessingInProgress()) {
        QMessageBox::critical(nullptr, tr("Error"), tr("Batch processing is in the progress."));

        return;
    }

    if (!isProjectLoaded()) {
        QMessageBox::critical(nullptr, tr("Error"), tr("No project is loaded."));

        return;
    }

    m_ptrInteractiveQueue->cancelAndClear();
    if (m_ptrBatchQueue.get()) { // Should not happen, but just in case.
        m_ptrBatchQueue->cancelAndClear();
    }

// Checking whether all the output thumbnails don't have a question mark on them

    m_ptrThumbSequence_export.reset(new ThumbnailSequence(m_maxLogicalThumbSize));

    if (m_ptrThumbnailCache.get()) {
        IntrusivePtr<CompositeCacheDrivenTask> const task(
            m_ptrStages->createCompositeCacheDrivenTask(m_outFileNameGen, m_ptrStages->outputFilterIdx())
        );

        m_ptrThumbSequence_export->setThumbnailFactory(
            IntrusivePtr<ThumbnailFactory>(
                new ThumbnailFactory(
                    m_ptrThumbnailCache,
                    m_maxLogicalThumbSize, task
                )
            )
        );
    }

    const PageSequence all_pages = m_ptrPages->toPageSequence(m_ptrStages->filterAt(m_ptrStages->outputFilterIdx())->getView());
    QSet<PageId> selected_pages;

    if (settings.export_selected_pages_only) {
        QVector<PageId> to_be_selected;
        for (PageId const& page : m_ptrThumbSequence->selectedItems()) {
            to_be_selected.append(page);
        }
        std::sort(to_be_selected.begin(), to_be_selected.end());

        for (PageId const& page : qAsConst(to_be_selected)) {
            for (PageInfo const& p_info : all_pages) {
                if (p_info.id().imageId() == page.imageId()) {
                    if (page.subPage() == p_info.id().subPage()) {
                        selected_pages += page;
                    } else if ((p_info.id().subPage() == PageId::SINGLE_PAGE) !=
                               (page.subPage() == PageId::SINGLE_PAGE)) {    // different levels (image/page/subpage)
                        selected_pages += p_info.id();
                    }
                }
            }
        }
    }

    m_ptrThumbSequence_export->reset(all_pages, ThumbnailSequence::RESET_SELECTION, defaultPageOrderProvider());
    m_ptrThumbSequence_export->setSelection(selected_pages, ThumbnailSequence::KEEP_SELECTION);

    if (!m_ptrThumbSequence_export->AllThumbnailsComplete(settings.export_selected_pages_only)) {
        m_p_export_dialog->reset();
        return;
    }

// Getting the output filenames

    QString export_dir = settings.default_out_dir ? m_outFileNameGen.outDir() :
                                                    settings.export_dir_path;
    export_dir += QString(QDir::separator()) + "export";

    std::vector<PageId> const output_pages = m_ptrThumbSequence_export->toPageSequence().asPageIdVector(); // get all the pages (input pages)

    QVector<exporting::ExportThread::ExportRec> outpaths_vector;

    int page_no = 0;
    for (const PageId& page_id : output_pages) {
        ++page_no;
        QString out_file_path = m_outFileNameGen.filePathFor(page_id);

        if (settings.export_selected_pages_only) {
            if (selected_pages.contains(page_id)) {
                selected_pages.remove(page_id);
            } else {
                continue;
            }
        }

        if (QFile::exists(out_file_path)) {
            exporting::ExportThread::ExportRec rec;
            rec.page_no = page_no;
            rec.filename = out_file_path;
            rec.page_id = page_id;
            rec.zones_info = m_ptrStages->outputFilter()->getZonesInfo(page_id);
            outpaths_vector.append(rec);
        }

        if (settings.export_selected_pages_only && selected_pages.isEmpty()) {
            break;
        }
    }

    m_p_export_dialog->setCount(outpaths_vector.size());

    // exporting pages

    m_p_export_thread = new exporting::ExportThread(settings,
                                                    outpaths_vector,
                                                    export_dir,
                                                    *m_ptrStages->outputFilter()->exportSuggestions(),
                                                    this);

    connect(m_p_export_thread, &exporting::ExportThread::finished, this, [=](){
        m_p_export_thread->deleteLater();
        m_p_export_thread = nullptr;
    });

    connect(m_p_export_thread, &exporting::ExportThread::exportCanceled,
            m_p_export_dialog, &exporting::ExportDialog::exportCanceled);

    connect(m_p_export_thread, &exporting::ExportThread::exportCompleted,
            m_p_export_dialog, &exporting::ExportDialog::exportCompleted);

    connect(m_p_export_thread, &exporting::ExportThread::imageProcessed,
            m_p_export_dialog, &exporting::ExportDialog::stepProgress);

    connect(m_p_export_thread, &exporting::ExportThread::needReprocess,
            this, &MainWindow::exportRequestedReprocessing);

    connect(m_p_export_dialog, &exporting::ExportDialog::ExportStopSignal,
            m_p_export_thread, &exporting::ExportThread::cancel);

    connect(m_p_export_dialog, &QDialog::destroyed,
            m_p_export_thread, &exporting::ExportThread::cancel);

    connect(m_p_export_thread, &exporting::ExportThread::error,
            m_p_export_dialog, &exporting::ExportDialog::exportError);

    m_p_export_thread->start();
}

void
MainWindow::exportRequestedReprocessing(const PageId& page_id, QImage* fore_subscan)
{

    assert(m_ptrThumbnailCache.get());
    m_ptrInteractiveQueue->cancelAndClear();

    {
    const PageInfo page_info = m_ptrThumbSequence_export->toPageSequence().pageAt(page_id);

    auto output_task = m_ptrStages->outputFilter()->createTask(
                page_id, m_ptrThumbnailCache, m_outFileNameGen, IntrusivePtr<publish::Task>(),
                false, m_debug, true, fore_subscan
                );

    auto page_layout_task = m_ptrStages->pageLayoutFilter()->createTask(
                page_id, output_task, false, false
                );
    auto select_content_task = m_ptrStages->selectContentFilter()->createTask(
                page_id, page_layout_task, false, false
                );
    auto deskew_task = m_ptrStages->deskewFilter()->createTask(
                page_id, select_content_task, false, false
                );
    auto page_split_task = m_ptrStages->pageSplitFilter()->createTask(
                page_info,
                deskew_task, false, false
                );
    auto fix_orientation_task = m_ptrStages->fixOrientationFilter()->createTask(
                page_id, page_split_task, false
                );
    assert(fix_orientation_task);

    BackgroundTaskPtr task = BackgroundTaskPtr(
                new LoadFileTask(
                    BackgroundTask::INTERACTIVE,
                    page_info, m_ptrThumbnailCache, m_ptrPages, fix_orientation_task
                    )
                );

    FilterResultPtr result = (*task)();
    }

    m_p_export_thread->continueExecution();
}

void
MainWindow::SetStartExport()
{
    if (isBatchProcessingInProgress()) {
        m_p_export_dialog->exportError(tr("Batch processing is in the progress."));
        return;
    }

    if (!isProjectLoaded()) {
        m_p_export_dialog->exportError(tr("No project is loaded."));
        return;
    }

    m_p_export_dialog->setStartExport();
}
//end of modified by monday2000

void
MainWindow::showAboutDialog()
{
    Ui::AboutDialog ui;
    QDialog* dialog = new QDialog(this);
    ui.setupUi(dialog);
    ui.tabWidget->setCurrentIndex(0); // in case I forget to switch it back in UI Designer
    ui.version->setText(QString::fromUtf8(VERSION) + "\n" + tr("build on ") +
                        QDate(BUILD_YEAR, BUILD_MONTH, BUILD_DAY).toString(Qt::SystemLocaleShortDate));

    QResource license(":/GPLv3.html");
    ui.licenseViewer->setHtml(QString::fromUtf8((char const*)license.data(), license.size()));

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->show();
}

/**
 * Note: the removed widgets are not deleted.
 */
void
MainWindow::removeWidgetsFromLayout(QLayout* layout)
{
    QLayoutItem* child;
    while ((child = layout->takeAt(0))) {
        if (child->widget()) {
            child->widget()->hide();
        }
        delete child;
    }
}

void
MainWindow::removeFilterOptionsWidget()
{
    removeWidgetsFromLayout(m_pOptionsFrameLayout);

    // Delete the old widget we were owning, if any.
    m_optionsWidgetCleanup.clear();

    m_ptrOptionsWidget = nullptr;
}

void
MainWindow::updateProjectActions()
{
    bool const loaded = isProjectLoaded();
    actionSaveProject->setEnabled(loaded);
    actionSaveProjectAs->setEnabled(loaded);
    actionFixDpi->setEnabled(loaded);
    actionRelinking->setEnabled(loaded);
    actionExport->setEnabled(loaded);
    actionGoToPage->setEnabled(loaded);
    actionSelectPages->setEnabled(loaded);
    if (!loaded) {
        actionManageContents->setEnabled(false);
        actionSetMetadata->setEnabled(false);
        actionManageSharedDictionaries->setEnabled(false);
//        actionSetBundledFilename->setEnabled(false);
    }
    if (m_ptrStages.get()) {
        // just in case it ever changes
        StatusBarProvider::setOutputFilterIdx(m_ptrStages->outputFilterIdx());
    }
}

bool
MainWindow::isBatchProcessingInProgress() const
{
    return m_ptrBatchQueue.get() != 0;
}

bool
MainWindow::isProjectLoaded() const
{
    return (!m_outFileNameGen.outDir().isEmpty()) && (m_ptrPages->numImages() > 0);
}

bool
MainWindow::isBelowSelectContent() const
{
    return isBelowSelectContent(m_curFilter);
}

bool
MainWindow::isBelowSelectContent(int const filter_idx) const
{
    return (filter_idx > m_ptrStages->selectContentFilterIdx());
}

bool
MainWindow::isBelowFixOrientation(int filter_idx) const
{
    return (filter_idx > m_ptrStages->fixOrientationFilterIdx());
}

bool
MainWindow::isOutputFilter() const
{
    return isOutputFilter(m_curFilter);
}

bool
MainWindow::isOutputFilter(int const filter_idx) const
{
    return (filter_idx == m_ptrStages->outputFilterIdx());
}

bool
MainWindow::isPublishFilter() const
{
    return isPublishFilter(m_curFilter);
}

bool
MainWindow::isPublishFilter(int const filter_idx) const
{
    return (filter_idx == m_ptrStages->publishFilterIdx());
}

PageView
MainWindow::getCurrentView() const
{
    return m_ptrStages->filterAt(m_curFilter)->getView();
}

void
MainWindow::updateMainArea()
{
    if (m_ptrPages->numImages() == 0) {
        filterList->setBatchProcessingPossible(false);
        showNewOpenProjectPanel();
    } else if (isBatchProcessingInProgress()) {
        filterList->setBatchProcessingPossible(false);
        setImageWidget(m_ptrBatchProcessingWidget.get(), KEEP_OWNERSHIP);
    } else {
        PageInfo const page(m_ptrThumbSequence->selectionLeader());
        if (page.isNull()) {
            filterList->setBatchProcessingPossible(false);
            removeImageWidget();
            removeFilterOptionsWidget();
        } else {
            // Note that loadPageInteractive may reset it to false.
            filterList->setBatchProcessingPossible(true);
            loadPageInteractive(page);
        }
    }
}

bool
MainWindow::checkReadyForOutput(PageId const* ignore) const
{
    return m_ptrStages->pageLayoutFilter()->checkReadyForOutput(
               *m_ptrPages, ignore
           );
}

bool
MainWindow::checkReadyToPublish(PageId const* ignore) const
{
    return m_ptrStages->outputFilter()->checkReadyToPublish(
                m_outFileNameGen, *m_ptrPages, ignore
    );
}

void
MainWindow::loadPageInteractive(PageInfo const& page)
{
    assert(!isBatchProcessingInProgress());

    m_ptrInteractiveQueue->cancelAndClear();

    if ( ( isOutputFilter() && !checkReadyForOutput(&page.id()) ) ||
         ( isPublishFilter() && !checkReadyToPublish(&page.id()) ) ) {
        filterList->setBatchProcessingPossible(false);

        // Switch to the first page - the user will need
        // to process all pages in batch mode.
        m_ptrThumbSequence->setSelection(m_ptrThumbSequence->firstPage().id());

        QString err_text;
        if (isOutputFilter()) {
            err_text = tr("Output is not yet possible, as the final size"
                                  " of pages is not yet known.\nTo determine it,"
                                  " run batch processing at \"Select Content\" or"
                                  " \"Page Layout\".");
        } else {
            err_text = tr("Publishing is not yet possible, as all pages aren't"
                                  " processed and outputted yet.\nTo process them,"
                                  " run batch processing at \"Output\" step.");
        }

        removeFilterOptionsWidget();
        setImageWidget(new ErrorWidget(err_text), TRANSFER_OWNERSHIP);
        return;
    }

    if (!isBatchProcessingInProgress()) {
        if (m_pImageFrameLayout->indexOf(m_ptrProcessingIndicationWidget.get()) != -1) {
            m_ptrProcessingIndicationWidget->processingRestartedEffect();
        }
        setImageWidget(m_ptrProcessingIndicationWidget.get(), KEEP_OWNERSHIP);
        m_ptrStages->filterAt(m_curFilter)->preUpdateUI(this, page.id());
    }

    assert(m_ptrThumbnailCache.get());

    m_ptrInteractiveQueue->cancelAndClear();
    m_ptrInteractiveQueue->addProcessingTask(
        page, createCompositeTask(page, m_curFilter, /*batch=*/false, m_debug)
    );
    m_ptrWorkerThread->performTask(m_ptrInteractiveQueue->takeForProcessing());
}

void
MainWindow::updateWindowTitle()
{
    QString project_name;
    CommandLine cli = CommandLine::get();

    if (m_projectFile.isEmpty()) {
        project_name = tr("Unnamed");
    } else if (cli.hasWindowTitle()) {
        project_name = cli.getWindowTitle();
    } else {
        project_name = QFileInfo(m_projectFile).baseName();
    }
    QString const version(QString::fromUtf8(VERSION));
    QString title = tr("%2 - Scan Tailor \"Universal\" %3 [%1bit]").arg(sizeof(void*) * 8).arg(project_name, version);
    if (m_ptrBatchQueue.get()) {
        const double progress = m_ptrBatchQueue->getProgress();
        if (progress < 100.) {
            title = tr("%1% - %2").arg(progress, 0, 'f', 1).arg(title);
        }
    }
    setWindowTitle(title);
}

/**
 * \brief Closes the currently project, prompting to save it if necessary.
 *
 * \return true if the project was closed, false if the user cancelled the process.
 */
bool
MainWindow::closeProjectInteractive()
{
    if (!isProjectLoaded()) {
        return true;
    }

    if (m_projectFile.isEmpty()) {
        switch (promptProjectSave()) {
        case SAVE:
            saveProjectTriggered();
        // fall through
        case DONT_SAVE:
            break;
        case CANCEL:
            return false;
        }
        closeProjectWithoutSaving();
        return true;
    }

    QFileInfo const project_file(m_projectFile);
    QFileInfo const backup_file(
        project_file.absoluteDir(),
        QLatin1String("Backup.") + project_file.fileName()
    );
    QString const backup_file_path(backup_file.absoluteFilePath());

    ProjectWriter writer(m_ptrPages, m_selectedPage, m_outFileNameGen);

    if (!writer.write(backup_file_path, m_ptrStages->filters())) {
        // Backup file could not be written???
        QFile::remove(backup_file_path);
        switch (promptProjectSave()) {
        case SAVE:
            saveProjectTriggered();
        // fall through
        case DONT_SAVE:
            break;
        case CANCEL:
            return false;
        }
        closeProjectWithoutSaving();
        return true;
    }

    if (compareFiles(m_projectFile, backup_file_path)) {
        // The project hasn't really changed.
        QFile::remove(backup_file_path);
        closeProjectWithoutSaving();
        return true;
    }

    switch (promptProjectSave()) {
    case SAVE:
        if (!Utils::overwritingRename(
                    backup_file_path, m_projectFile)) {
            QMessageBox::warning(
                this, tr("Error"),
                tr("Error saving the project file!")
            );
            return false;
        }
    // fall through
    case DONT_SAVE:
        QFile::remove(backup_file_path);
        break;
    case CANCEL:
        return false;
    }

    closeProjectWithoutSaving();
    return true;
}

void
MainWindow::closeProjectWithoutSaving()
{
    IntrusivePtr<ProjectPages> pages(new ProjectPages());
    switchToNewProject(pages, QString());
}

bool
MainWindow::saveProjectWithFeedback(QString const& project_file)
{
    ProjectWriter writer(m_ptrPages, m_selectedPage, m_outFileNameGen);

    if (QStatusBar* sb = statusBar()) {
        sb->showMessage(tr("Saving project..."), 1000);
    }

    if (!writer.write(project_file, m_ptrStages->filters())) {
        QMessageBox::warning(
            this, tr("Error"),
            tr("Error saving the project file!")
        );
        return false;
    }

    return true;
}

/**
 * Note: showInsertFileDialog(BEFORE, ImageId()) is legal and means inserting at the end.
 */
void
MainWindow::showInsertFileDialog(BeforeOrAfter before_or_after, ImageId const& existing)
{
    if (isBatchProcessingInProgress() || !isProjectLoaded()) {
        return;
    }

    // We need to filter out files already in project.
    class ProxyModel : public QSortFilterProxyModel
    {
    public:
        ProxyModel(ProjectPages const& pages)
        {
            setDynamicSortFilter(true);

            PageSequence const sequence(pages.toPageSequence(IMAGE_VIEW));
            for (PageInfo const& page : sequence) {
                m_inProjectFiles.push_back(QFileInfo(page.imageId().filePath()));
            }
        }
    protected:
        virtual bool filterAcceptsRow(int source_row, QModelIndex const& source_parent) const override
        {
            QModelIndex const idx(sourceModel()->index(source_row, 0, source_parent));
            QVariant const data(idx.data(QFileSystemModel::FilePathRole));
            if (data.isNull()) {
                return true;
            }
            return !m_inProjectFiles.contains(QFileInfo(data.toString()));
        }

        virtual bool lessThan(QModelIndex const& left, QModelIndex const& right) const override
        {
            return left.row() < right.row();
        }
    private:
        QFileInfoList m_inProjectFiles;
    };

    std::unique_ptr<QFileDialog> dialog(
        new QFileDialog(
            this, tr("Files to insert"),
            QFileInfo(existing.filePath()).absolutePath()
        )
    );
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setOption(QFileDialog::DontUseNativeDialog, GlobalStaticSettings::m_DontUseNativeDialog); // must be called before setProxyModel
    dialog->setProxyModel(new ProxyModel(*m_ptrPages));
    QString filter = QSettings().value(_key_app_open_filetype_filter, _key_app_open_filetype_filter_def).toString();
    dialog->setNameFilter(tr("Images not in project (%1)").arg(filter));
    // XXX: Adding individual pages from a multi-page TIFF where
    // some of the pages are already in project is not supported right now.

    if (dialog->exec() != QDialog::Accepted) {
        return;
    }

    QStringList files(dialog->selectedFiles());
    if (files.empty()) {
        return;
    }

    // The order of items returned by QFileDialog is platform-dependent,
    // so we enforce our own ordering.
    std::sort(files.begin(), files.end(), SmartFilenameOrdering());

    // I suspect on some platforms it may be possible to select the same file twice,
    // so to be safe, remove duplicates.
    files.erase(std::unique(files.begin(), files.end()), files.end());

    std::vector<ImageFileInfo> new_files;
    std::vector<QString> loaded_files;
    std::vector<QString> failed_files; // Those we failed to read metadata from.

    // dialog->selectedFiles() returns file list in reverse order.
    for (int i = files.size() - 1; i >= 0; --i) {
        QFileInfo const file_info(files[i]);
        ImageFileInfo image_file_info(file_info, std::vector<ImageMetadata>());

        ImageMetadataLoader::Status const status = ImageMetadataLoader::load(
                    files.at(i),[&](ImageMetadata const& metadata) {
                image_file_info.imageInfo().push_back(metadata);
            } );

        if (status == ImageMetadataLoader::LOADED) {
            new_files.push_back(image_file_info);
            loaded_files.push_back(file_info.absoluteFilePath());
        } else {
            failed_files.push_back(file_info.absoluteFilePath());
        }
    }

    if (!failed_files.empty()) {
        std::unique_ptr<LoadFilesStatusDialog> err_dialog(new LoadFilesStatusDialog(this));
        err_dialog->setLoadedFiles(loaded_files);
        err_dialog->setFailedFiles(failed_files);
        err_dialog->setOkButtonName(QString(" %1 ").arg(tr("Skip failed files")));
        if (err_dialog->exec() != QDialog::Accepted || loaded_files.empty()) {
            return;
        }
    }

    // Check if there is at least one DPI that's not OK.
    bool not_ok = false;
    for (ImageFileInfo const& file : new_files) {
         if (!file.isDpiOK()) {
             not_ok = true;
             break;
         }
    }
    if (not_ok) {
        std::unique_ptr<FixDpiDialog> dpi_dialog(new FixDpiDialog(new_files, this));
        dpi_dialog->setWindowModality(Qt::WindowModal);
        if (dpi_dialog->exec() != QDialog::Accepted) {
            return;
        }

        new_files = dpi_dialog->files();
    }

    // Actually insert the new pages.
    for (ImageFileInfo const& file : new_files) {
        int image_num = -1; // Zero-based image number in a multi-page TIFF.

        for (ImageMetadata const& metadata : file.imageInfo()) {
            ++image_num;

            int const num_sub_pages = ProjectPages::adviseNumberOfLogicalPages(
                                          metadata, OrthogonalRotation()
                                      );
            ImageInfo const image_info(
                ImageId(file.fileInfo(), image_num), metadata, num_sub_pages, false, false
            );
            insertImage(image_info, before_or_after, existing);
        }
    }
}

/**
 * Note: showInsertEmptyPageDialog(BEFORE, ImageId()) is legal and means inserting at the end.
 */

void
MainWindow::showInsertEmptyPageDialog(BeforeOrAfter before_or_after, const PageId& existing_page)
{
    if (isBatchProcessingInProgress() || !isProjectLoaded()) {
        return;
    }

    QString const empty_page_filename(":/images/empty-page.png");
    QFileInfo const file_info(empty_page_filename);
    ImageFileInfo image_file_info(file_info, std::vector<ImageMetadata>());

    ImageMetadataLoader::Status const status = ImageMetadataLoader::load(
                empty_page_filename, [&](const ImageMetadata& m) { image_file_info.imageInfo().push_back(m); }
            );

    if (status != ImageMetadataLoader::LOADED) {
        return;
    }

    assert(image_file_info.imageInfo().size() > 0);

    int next_page_num = m_ptrPages->getMaxPageNum(empty_page_filename) + 1;

    ImageInfo const image_info(
        ImageId(image_file_info.fileInfo(), next_page_num), image_file_info.imageInfo().front(), 1, false, false
    );

    // sugest an overriden_filename for empty_page.png to stick into right place of alphabetically sorted output files
    PageSequence all_pages = m_ptrPages->toPageSequence(IMAGE_VIEW);
    QStringList insert_to_filenames(QFileInfo(m_outFileNameGen.filePathFor(existing_page)).baseName());
    std::vector<PageInfo>::const_iterator it = all_pages.begin();
    for (; it != all_pages.end() && it->id() != existing_page; ++it) { }
    if (it != all_pages.end()) {
        if ((before_or_after == AFTER) && (++it != all_pages.end())) {
            insert_to_filenames.append(QFileInfo(m_outFileNameGen.filePathFor(it->id())).baseName());
        } else if ((before_or_after == BEFORE) && (it != all_pages.begin())) {
            insert_to_filenames.prepend(QFileInfo(m_outFileNameGen.filePathFor((--it)->id())).baseName());
        }
    }

    qDebug() << "filelist = " << insert_to_filenames;
    QString suggested_name = m_outFileNameGen.suggestOverridenFileName(insert_to_filenames, before_or_after == AFTER);
    qDebug() << "overriden " << suggested_name;

    insertImage(image_info, before_or_after, existing_page.imageId(), suggested_name);

}

void
MainWindow::showRenameResultFileDialog(PageInfo const& page_info)
{
    QString old_filepath = m_outFileNameGen.filePathFor(page_info.id());
    QFileInfo fi(old_filepath);
    old_filepath = fi.absoluteFilePath();
    const QString old_filename = fi.fileName();
    const QString old_base_filename = fi.completeBaseName();

    QInputDialog dlg(this);
    const QString title(tr("Overwrite default file name for resulting image"));
    dlg.setWindowTitle(title);
    dlg.setLabelText(tr("Here you may overwrite default resulting image file name\nthat will be generated for this page. It may be\nhelpful to keep the right alphabetical order of files in out subfolder."));
    dlg.setInputMode(QInputDialog::InputMode::TextInput);

    dlg.setTextValue(old_base_filename);

    QLineEdit* le = dlg.findChild<QLineEdit*>(QString(), Qt::FindChildrenRecursively);
    if (le) {
        le->setPlaceholderText(old_base_filename);
    }
    if (dlg.exec() == QDialog::Accepted) {
        const QString new_base_filename = dlg.textValue();
        const QString new_filename = new_base_filename + ".tif";
        fi.setFile(fi.path() + QDir::separator() + new_filename);
        const QString new_filepath = fi.absoluteFilePath();
        if (old_filepath != new_filepath) {
            QFile nf(new_filepath);
            if (nf.exists()) {
                if (QMessageBox::question(this, title, tr("File %1 already exists in out subfolder.\nWould you like to replace it?").arg(new_filename)) == QMessageBox::Yes) {
                    if (!nf.remove()) {
                        QMessageBox::critical(this, title, tr("Can't remove file %1!\nCancelling...").arg(new_filename));
                        return;
                    };
                }
            }
            QFile of(old_filepath);
            if (of.exists()) {
                if (!of.rename(new_filepath)) {
                    QMessageBox::critical(this, title, tr("Can't rename file %1!\nCancelling...").arg(old_filename));
                    return;
                }
            }
        }

        ImageId imgid = page_info.imageId();
        m_outFileNameGen.disambiguator()->registerFile(imgid.filePath(), imgid.page(), &new_base_filename);
    }
}

void
MainWindow::showRemovePagesDialog(std::set<PageId> const& pages)
{
    std::unique_ptr<QDialog> dialog(new QDialog(this));
    Ui::RemovePagesDialog ui;
    ui.setupUi(dialog.get());
    ui.icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(48, 48));

    ui.text->setText(QObject::tr("Remove %n page(s) from project?", "", (int) pages.size()));

    QPushButton* remove_btn = ui.buttonBox->button(QDialogButtonBox::Ok);
    remove_btn->setText(tr("Remove"));

    dialog->setWindowModality(Qt::WindowModal);
    if (dialog->exec() == QDialog::Accepted) {
        removeFromProject(pages);
        eraseOutputFiles(pages);
        if (ui.cbRemoveInputFiles->isChecked()) {
            eraseInputFiles(pages);
        }
    }
}

/**
 * Note: insertImage(..., BEFORE, ImageId()) is legal and means inserting at the end.
 */
void
MainWindow::insertImage(ImageInfo const& new_image,
                        BeforeOrAfter before_or_after, ImageId existing, QString const& overriden_filename)
{
    std::vector<PageInfo> pages(
        m_ptrPages->insertImage(
            new_image, before_or_after, existing, getCurrentView()
        )
    );

    if (before_or_after == BEFORE) {
        // The second one will be inserted first, then the first
        // one will be inserted BEFORE the second one.
        std::reverse(pages.begin(), pages.end());
    }

    for (PageInfo const& page_info : pages) {
        ImageId imgid = page_info.imageId();
        m_outFileNameGen.disambiguator()->registerFile(imgid.filePath(), imgid.page(), &overriden_filename);
        m_ptrThumbSequence->insert(page_info, before_or_after, existing);
        existing = page_info.imageId();
    }
}

void
MainWindow::removeFromProject(std::set<PageId> const& pages)
{
    emit toBeRemoved(pages); // direct signal call

    m_ptrInteractiveQueue->cancelAndRemove(pages);
    if (m_ptrBatchQueue.get()) {
        m_ptrBatchQueue->cancelAndRemove(pages);
    }

    m_ptrPages->removePages(pages);

    const PageSequence itemsInOrder = m_ptrThumbSequence->toPageSequence();
    std::set<PageId> new_selection;

    bool select_first_non_deleted = false;
    if (itemsInOrder.numPages() > 0) {
        // if first page was deleted select first not deleted page
        // otherwise select last not deleted page from beginning
        select_first_non_deleted = pages.find(itemsInOrder.pageAt(0).id()) != pages.end();

        PageId last_non_deleted;
        for (const PageInfo& page : itemsInOrder) {

            const PageId& id = page.id();
            const bool was_deleted = (pages.find(id) != pages.end());

            if (!was_deleted) {
                if (select_first_non_deleted) {
                    m_ptrThumbSequence->setSelection(id);
                    new_selection.insert(id);
                    break;
                } else {
                    last_non_deleted = id;
                }
            } else if (!select_first_non_deleted && !last_non_deleted.isNull()) {
                m_ptrThumbSequence->setSelection(last_non_deleted);
                new_selection.insert(last_non_deleted);
                break;
            }

        }

        m_ptrThumbSequence->removePages(pages);

        if (new_selection.empty()) {
            // fallback to old behaviour
            if (m_ptrThumbSequence->selectionLeader().isNull()) {
                m_ptrThumbSequence->setSelection(m_ptrThumbSequence->firstPage().id());
            }
        } else {
            ensurePageVisible(new_selection, m_ptrThumbSequence->selectionLeader().id());
        }

    }

    updateMainArea();
}

void
MainWindow::eraseOutputFiles(std::set<PageId> const& pages)
{
    std::vector<PageId::SubPage> erase_variations;
    erase_variations.reserve(3);

    for (PageId const& page_id : pages) {
        erase_variations.clear();
        switch (page_id.subPage()) {
        case PageId::SINGLE_PAGE:
            erase_variations.push_back(PageId::SINGLE_PAGE);
            erase_variations.push_back(PageId::LEFT_PAGE);
            erase_variations.push_back(PageId::RIGHT_PAGE);
            break;
        case PageId::LEFT_PAGE:
            erase_variations.push_back(PageId::SINGLE_PAGE);
            erase_variations.push_back(PageId::LEFT_PAGE);
            break;
        case PageId::RIGHT_PAGE:
            erase_variations.push_back(PageId::SINGLE_PAGE);
            erase_variations.push_back(PageId::RIGHT_PAGE);
            break;
        }

        for (PageId::SubPage subpage : erase_variations) {
            QFile::remove(m_outFileNameGen.filePathFor(PageId(page_id.imageId(), subpage)));
        }
    }
}

void
MainWindow::eraseInputFiles(std::set<PageId> const& pages)
{
    // Must be called after removeFromProject()
    // remove input image files in case their pages were selected and
    // file isn't used in the project anymore.

    QStringList src_files = m_ptrPages->toImageFilePaths();
    for (PageId const& page_id : pages) {
        QString const& img_to_remove = page_id.imageId().filePath();
        if (!src_files.contains(img_to_remove)) {
            QFile::remove(img_to_remove);
        }
    }
}

BackgroundTaskPtr
MainWindow::createCompositeTask(
    PageInfo const& page, int const last_filter_idx, bool const batch, bool debug)
{
    IntrusivePtr<fix_orientation::Task> fix_orientation_task;
    IntrusivePtr<page_split::Task> page_split_task;
    IntrusivePtr<deskew::Task> deskew_task;
    IntrusivePtr<select_content::Task> select_content_task;
    IntrusivePtr<page_layout::Task> page_layout_task;
    IntrusivePtr<output::Task> output_task;
    IntrusivePtr<publish::Task> publish_task;

    if (batch) {
        debug = false;
    }

    if (last_filter_idx >= m_ptrStages->publishFilterIdx()) {
        publish_task = m_ptrStages->publishFilter()->createTask(
                    page.id(), m_ptrThumbnailCache, m_outFileNameGen, batch
                    );
        debug = false;
    }
    
    if (last_filter_idx >= m_ptrStages->outputFilterIdx()) {
        output_task = m_ptrStages->outputFilter()->createTask(
                          page.id(), m_ptrThumbnailCache, m_outFileNameGen, publish_task, batch, debug
                      );
        debug = false;
        disconnect(output_task->getSettingsListener(), SLOT(settingsChanged()));
        connect(this, SIGNAL(settingsUpdateRequest()), output_task->getSettingsListener(), SLOT(settingsChanged()));
    }
    if (last_filter_idx >= m_ptrStages->pageLayoutFilterIdx()) {
        page_layout_task = m_ptrStages->pageLayoutFilter()->createTask(
                               page.id(), output_task, batch, debug
                           );
        debug = false;
    }
    if (last_filter_idx >= m_ptrStages->selectContentFilterIdx()) {
        select_content_task = m_ptrStages->selectContentFilter()->createTask(
                                  page.id(), page_layout_task, batch, debug
                              );
        debug = false;
    }
    if (last_filter_idx >= m_ptrStages->deskewFilterIdx()) {
        deskew_task = m_ptrStages->deskewFilter()->createTask(
                          page.id(), select_content_task, batch, debug
                      );
        debug = false;
    }
    if (last_filter_idx >= m_ptrStages->pageSplitFilterIdx()) {
        page_split_task = m_ptrStages->pageSplitFilter()->createTask(
                              page, deskew_task, batch, debug
                          );
    }
    if (last_filter_idx >= m_ptrStages->fixOrientationFilterIdx()) {
        fix_orientation_task = m_ptrStages->fixOrientationFilter()->createTask(
                                   page.id(), page_split_task, batch
                               );
    }
    assert(fix_orientation_task);

    return BackgroundTaskPtr(
               new LoadFileTask(
                   batch ? BackgroundTask::BATCH : BackgroundTask::INTERACTIVE,
                   page, m_ptrThumbnailCache, m_ptrPages, fix_orientation_task
               )
           );
}

void
MainWindow::updateDisambiguationRecords(PageSequence const& pages)
{
    for (const PageInfo& page : pages) {
        ImageId imgid = page.imageId();
        m_outFileNameGen.disambiguator()->registerFile(imgid.filePath(), imgid.page());
    }
}

PageSelectionAccessor
MainWindow::newPageSelectionAccessor()
{
    IntrusivePtr<PageSelectionProvider const> provider(new PageSelectionProviderImpl(this));
    return PageSelectionAccessor(provider);
}

void setupDockingPanel(QDockWidget* panel, bool enabled)
{
    panel->setFeatures(enabled ? QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable :
                       QDockWidget::NoDockWidgetFeatures);

    if (!enabled) {
        Q_ASSERT(panel->titleBarWidget() == nullptr);

        QWidget* titleWidget = new QWidget(panel);
        titleWidget->setVisible(false);
        panel->setTitleBarWidget(titleWidget);

        panel->setFloating(false);
    } else {
        QWidget* titleWidget = panel->titleBarWidget();
        Q_ASSERT(titleWidget != nullptr);
        panel->setTitleBarWidget(nullptr);
        titleWidget->deleteLater();
    }

}

void
MainWindow::setDockingPanels(bool enabled)
{
    if (enabled != m_docking_enabled) {
        setupDockingPanel(dockWidgetThumbnails, enabled);
        setupDockingPanel(dockWidget_4, enabled);

        m_docking_enabled = enabled;
    }
}

bool
MainWindow::loadLanguage(const QString& dir, const QString& lang)
{
    const QString translation("scantailor-universal_" + lang);
    bool loaded = m_translator.load(dir + translation);

#if defined(unix) || defined(__unix__) || defined(__unix)
    // will load it from /usr/share later
#else
    // we distribute all Qt Fraeworks files with ST installer for Win
    const QString qt_translation("qtbase_" + lang);
    if (loaded) {
        m_qt_translator.load(dir + qt_translation);
    }
#endif
    return loaded;
}

void
MainWindow::changeLanguage(QString lang, bool dont_store)
{
    lang = lang.toLower();
    if (lang == m_current_lang) {
        return;
    }

    bool loaded = loadLanguage("", lang);
    if (!loaded) {
        loaded = loadLanguage(qApp->applicationDirPath() + "/", lang);
        if (!loaded) {
            loaded = loadLanguage(QString::fromUtf8(TRANSLATIONS_DIR_ABS) + "/", lang);
            if (!loaded) {
                loaded = loadLanguage(QString::fromUtf8(TRANSLATIONS_DIR_REL) + "/", lang);
                if (!loaded) {
                    loaded = loadLanguage(QApplication::applicationDirPath() + "/" +
                                          QString::fromUtf8(TRANSLATIONS_DIR_REL) + "/", lang);
                }
            }
        }
    }

    if (loaded || lang == "en") {
        qApp->removeTranslator(&m_translator);
        qApp->installTranslator(&m_translator);
        if (!dont_store) {
            QSettings settings;
            settings.setValue(_key_app_language, lang);
        }
        m_current_lang = lang;

        // additionally load Qt's own localization to translate QDialogButtonBox buttons etc.
        if (!m_qt_translator.isEmpty()) {
            qApp->removeTranslator(&m_qt_translator);
        }

        if (lang != "en") { // Qt's "en" is built in

#if defined(unix) || defined(__unix__) || defined(__unix)
            if (m_qt_translator.isEmpty()) {
                m_qt_translator.load(QString("qt_%1").arg(lang), "/usr/share/qt5/translations/");
            }
#endif
            if (!m_qt_translator.isEmpty()) {
                qApp->installTranslator(&m_qt_translator);
            }
        }

    } else {
        changeLanguage("en"); // fallback to EN
    }
}

void
MainWindow::changeEvent(QEvent* event)
{
    if (0 != event) {
        switch (event->type()) {
        // this event is send if a translator is loaded
        case QEvent::LanguageChange:
            retranslateUi(this);
            updateWindowTitle();
            break;

        // this event is send, if the system, language changes
        case QEvent::LocaleChange: {
            QString locale = QLocale::system().name();
            locale.truncate(locale.lastIndexOf('_'));
            changeLanguage(locale);
        }
        break;
        default:
            break;
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void
MainWindow::pauseAutoSaveTimer()
{
    if (m_autosave_timer != nullptr) {
        m_autosave_timer->blockSignals(true);
    }
}

void
MainWindow::resumeAutoSaveTimer()
{
    if (m_autosave_timer != nullptr) {
        m_autosave_timer->blockSignals(false);
    }
}

void
MainWindow::createAutoSaveTimer()
{
    int time = 60 * 1000 * abs(QSettings().value(_key_autosave_time_period_min, _key_autosave_time_period_min_def).toInt());

    if (m_autosave_timer == nullptr) {
        m_autosave_timer = new QAutoSaveTimer(this);
        m_autosave_timer->start(time);
    } else {
        if (m_autosave_timer->interval() != time) {
            m_autosave_timer->start(time);
        }
    }

}

void
MainWindow::destroyAutoSaveTimer()
{
    if (m_autosave_timer != nullptr) {
        m_autosave_timer->stop();
        m_autosave_timer->deleteLater();
        m_autosave_timer = nullptr;
    }
}

void
MainWindow::setAutoSaveInputDir(const QString dir)
{
    QSettings().setValue(_key_autosave_inputdir, dir);
}

void
MainWindow::setupStatusBar()
{
    QStatusBar* sb = statusBar();
    if (!sb) {
        return;
    }

    StatusBarProvider::registerStatusBar(sb);
    sb->installEventFilter(this);
    sb->setMaximumHeight(statusBarPanel->height() + 6);
    sb->addPermanentWidget(statusBarPanel);
    connect(m_ptrThumbSequence.get(), &ThumbnailSequence::newSelectionLeader, this,
    [ = ](PageInfo const & page) {
        PageSequence ps = m_ptrThumbSequence->toPageSequence();
        if (ps.numPages() <= 0) {
            statusLabelPageNo->clear();
        } else {
            statusLabelPageNo->setText(tr("p. %1").arg(ps.pageNo(page.id()) + 1));
        }
    });

    connect(this, &MainWindow::NewOpenProjectPanelShown, statusLabelPageNo, &QLabel::clear);

    connect(this, &MainWindow::UpdateStatusBarPageSize, this, &MainWindow::displayStatusBarPageSize);
    connect(this, &MainWindow::UpdateStatusBarMousePos, this, &MainWindow::displayStatusBarMousePos);
    connect(this, &MainWindow::UpdateStatusBarFileSize, this, &MainWindow::displayStatusBarFileSize);

    statusLabelPhysSize->installEventFilter(this);
    statusLabelFileSize->installEventFilter(this);
}

void
MainWindow::applyUnitsSettingToCoordinates(qreal& x, qreal& y)
{
    StatusLabelPhysSizeDisplayMode mode = StatusBarProvider::statusLabelPhysSizeDisplayMode;
    Dpi dpi = StatusBarProvider::getOriginalDpi();

    if (isOutputFilter()) {
        Dpi outputDpi = StatusBarProvider::getSettingsDpi();
        if (dpi != outputDpi) {
            // the image will be scaled
            y = y * outputDpi.vertical() / dpi.vertical();
            x = x * outputDpi.horizontal() / dpi.horizontal();
        }
    }

    Dpm dpm = Dpm(dpi);
    switch (mode) {
    case Inch:
        y /= dpi.vertical();
        x /= dpi.horizontal();
        break;
    case MM:
        y = y / dpm.vertical() * 1000.;
        x = x / dpm.horizontal() * 1000.;
        break;
    case SM:
        y = y / dpm.vertical() * 100.;
        x = x / dpm.horizontal() * 100.;
        break;
    default:
        break;
    }

}

void
MainWindow::displayStatusBarPageSize()
{
    QSizeF page_size = StatusBarProvider::getPageSize();

    if (isBatchProcessingInProgress() || !isProjectLoaded() || !page_size.isValid()) {
        statusLabelPhysSize->clear();
        return;
    }

    qreal x = page_size.width();
    qreal y = page_size.height();
    applyUnitsSettingToCoordinates(x, y);
    statusLabelPhysSize->setText(QObject::tr("%1 x %2 %3").arg(x).arg(y).arg(StatusBarProvider::getStatusLabelPhysSizeDisplayModeSuffix()));
}

void
MainWindow::displayStatusBarMousePos()
{
    QPointF pos = StatusBarProvider::getMousePos();

    if (isBatchProcessingInProgress() || !isProjectLoaded() || pos.isNull()) {
        statusLabelMousePos->clear();
        return;
    }

    qreal x = pos.x();
    qreal y = pos.y();
    applyUnitsSettingToCoordinates(x, y);
    statusLabelMousePos->setText(tr("%1, %2").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1));
}

void
MainWindow::displayStatusBarFileSize()
{
    int sz = StatusBarProvider::getFileSize();

    if (isBatchProcessingInProgress() || !isProjectLoaded() || sz == 0) {
        statusLabelFileSize->clear();
        return;
    }

    statusLabelFileSize->setText(StatusBarProvider::getStatusLabelFileSizeText());
}

const QList<QKeySequence> getPageActionShortcuts(const HotKeysId& id, int idx = 0)
{
    // Allows to catch Shift+ any shortcut combination
    // Reqired for Shift key processing in QAction based page navigation functions
    QKeySequence sc = GlobalStaticSettings::createShortcut(id, idx);
    QString s = sc.toString();
    QKeySequence sc2 = QKeySequence::fromString("Shift+" + s);
    QList<QKeySequence> ks;
    ks.append(sc);
    ks.append(sc2);
    return ks;
}

void
MainWindow::applyShortcutsSettings()
{
    actionNewProject->setShortcut(GlobalStaticSettings::createShortcut(ProjectNew));
    actionOpenProject->setShortcut(GlobalStaticSettings::createShortcut(ProjectOpen));
    actionSaveProject->setShortcut(GlobalStaticSettings::createShortcut(ProjectSave));
    actionSaveProjectAs->setShortcut(GlobalStaticSettings::createShortcut(ProjectSaveAs));

    actionPrevPage->setShortcuts(getPageActionShortcuts(PagePrev));
    actionPrevPageQ->setShortcuts(getPageActionShortcuts(PagePrev, 1));
    actionFirstPage->setShortcuts(getPageActionShortcuts(PageFirst));
    actionLastPage->setShortcuts(getPageActionShortcuts(PageLast));
    actionNextPage->setShortcuts(getPageActionShortcuts(PageNext));
    actionNextPageW->setShortcuts(getPageActionShortcuts(PageNext, 1));
    actionFirstSelectedPage->setShortcuts(getPageActionShortcuts(PageFirstSelected));
    actionLastSelectedPage->setShortcuts(getPageActionShortcuts(PageLastSelected));
    actionPrevSelectedPage->setShortcuts(getPageActionShortcuts(PagePrevSelected));
    actionNextSelectedPage->setShortcuts(getPageActionShortcuts(PageNextSelected));
    actionJumpPageF->setShortcuts(getPageActionShortcuts(PageJumpForward));
    actionJumpPageB->setShortcuts(getPageActionShortcuts(PageJumpBackward));

    actionCloseProject->setShortcut(GlobalStaticSettings::createShortcut(ProjectClose));
    actionQuit->setShortcut(GlobalStaticSettings::createShortcut(AppQuit));

    actionInsertEmptyPgBefore->setText(tr("Insert before"));
    QKeySequence k_seq = GlobalStaticSettings::createShortcut(InsertEmptyPageBefore);
    if (actionInsertEmptyPgBefore->property("shortcutVisibleInContextMenu").isValid()) {
        // shortcutVisibleInContextMenu available in Qt 5.10+
        actionInsertEmptyPgBefore->setProperty("shortcutVisibleInContextMenu", QVariant(true));
    } else {
        actionInsertEmptyPgBefore->setText(actionInsertEmptyPgBefore->text() + "/t" + k_seq.toString());
    }
    actionInsertEmptyPgBefore->setShortcut(k_seq);

    actionInsertEmptyPgAfter->setText(tr("Insert after"));
    k_seq = GlobalStaticSettings::createShortcut(InsertEmptyPageAfter);
    if (actionInsertEmptyPgAfter->property("shortcutVisibleInContextMenu").isValid()) {
        // shortcutVisibleInContextMenu available in Qt 5.10+
        actionInsertEmptyPgAfter->setProperty("shortcutVisibleInContextMenu", QVariant(true));
    } else {
        actionInsertEmptyPgAfter->setText(actionInsertEmptyPgAfter->text() + "/t" + k_seq.toString());
    }
    actionInsertEmptyPgAfter->setShortcut(k_seq);

    actionSwitchFilter1->setShortcut(GlobalStaticSettings::createShortcut(StageFixOrientation));
    actionSwitchFilter2->setShortcut(GlobalStaticSettings::createShortcut(StageSplitPages));
    actionSwitchFilter3->setShortcut(GlobalStaticSettings::createShortcut(StageDeskew));
    actionSwitchFilter4->setShortcut(GlobalStaticSettings::createShortcut(StageSelectContent));
    actionSwitchFilter5->setShortcut(GlobalStaticSettings::createShortcut(StageMargins));
    actionSwitchFilter6->setShortcut(GlobalStaticSettings::createShortcut(StageOutput));

    thumbView->setStatusTip(tr("Use %1, %2, %3 (or %4), %5 (or %6) to navigate between pages.")
                            .arg(GlobalStaticSettings::getShortcutText(PageFirst),
                            GlobalStaticSettings::getShortcutText(PageLast),
                            GlobalStaticSettings::getShortcutText(PagePrev),
                            GlobalStaticSettings::getShortcutText(PagePrev, 1),
                            GlobalStaticSettings::getShortcutText(PageNext),
                            GlobalStaticSettings::getShortcutText(PageNext, 1))
                           );
}

void
MainWindow::saveWindowSettigns()
{
    QSettings settings;
    settings.setValue(_key_app_maximized, isMaximized());
    if (!isMaximized()) {
        settings.setValue(
            _key_app_geometry, saveGeometry()
        );
    }
}

void MainWindow::on_actionJumpPageF_triggered()
{
    QSettings settings;
    int pg_jmp = settings.value(_key_hot_keys_jump_forward_pg_num, _key_hot_keys_jump_forward_pg_num_def).toUInt();
    jumpToPage(pg_jmp);
}

void MainWindow::on_actionJumpPageB_triggered()
{
    QSettings settings;
    int pg_jmp = settings.value(_key_hot_keys_jump_backward_pg_num, _key_hot_keys_jump_backward_pg_num_def).toUInt();
    jumpToPage(-1 * pg_jmp);
}

void MainWindow::on_multiselectButton_toggled(bool checked)
{
    GlobalStaticSettings::m_simulateSelectionModifier = checked;
}

void MainWindow::on_inverseOrderButton_toggled(bool checked)
{
    GlobalStaticSettings::m_inversePageOrder = checked;
    inverseOrderButton->setArrowType(checked ? Qt::ArrowType::UpArrow : Qt::ArrowType::DownArrow);
    if (!currentPageOrderProvider().get() && !checked) {
        // reset is required here as we don't have a fair order provider for natural order
        // that's why currentPageOrderProvider is empty
        // and once it's reversed invalidateAllThumbnails() can't restore natural order back
        // so call full reset
        m_ptrThumbSequence->reset(
            m_ptrPages->toPageSequence(getCurrentView()),
            ThumbnailSequence::KEEP_SELECTION,
            currentPageOrderProvider()
        );
    } else {
        invalidateAllThumbnails();
    }
    if (!m_selectedPage.isNull()) {
        std::set<PageId> selection;
        selection.insert(m_selectedPage.get(getCurrentView()));
        ensurePageVisible(selection, m_ptrThumbSequence->selectionLeader().id());
    }
}

void MainWindow::on_resetSortingBtn_clicked()
{
    sortOptions->setCurrentIndex(0);
    resetSortingBtn->hide();
}

void MainWindow::on_actionCopySourceFileName_triggered()
{
    const std::set<PageId> sel_pages = selectedPages();
    QStringList filenames;
    for (PageId const &page: sel_pages) {
        filenames.append(page.imageId().filePath());
    }
    QGuiApplication::clipboard()->setText(filenames.join('\n'));
}

void MainWindow::on_actionCopyOutputFileName_triggered()
{
    const std::set<PageId> sel_pages = selectedPages();
    QStringList filenames;
    for (PageId const &page: sel_pages) {
        filenames.append(m_outFileNameGen.filePathFor(page));
    }
    QGuiApplication::clipboard()->setText(filenames.join('\n'));
}

void MainWindow::on_actionCopyPageNumber_triggered()
{
    const std::set<PageId> sel_pages = selectedPages();
    const PageSequence pages = m_ptrPages->toPageSequence(getCurrentView());
    QStringList numbers;
    for (PageId const &page: sel_pages) {
        numbers.append(QString::number(pages.pageNo(page)));
    }

    QGuiApplication::clipboard()->setText(numbers.join('\n'));
}


void MainWindow::on_actionGoToPage_triggered()
{
    bool ok;
    const PageSequence pages = m_ptrPages->toPageSequence(getCurrentView());
    int page_no = QInputDialog::getInt(this, tr("Go to page"), tr("Page number:"),
                                       1, 1, pages.numPages(), 1, &ok,
                                       Qt::Dialog);
    if (ok) {
        goToPage(pages.pageAt(page_no - 1).id());
    }
}

void MainWindow::on_actionSelectPages_triggered()
{
    QInputDialog dlg(this, Qt::Dialog);
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setOption(QInputDialog::UsePlainTextEditForTextInput);
    dlg.setWindowTitle(tr("Select pages by number"));
    dlg.setToolTip(tr("Numbers should start from 1\n Line ends are ignored\n" \
                      "Any non digit symbols are interpreted as number separators\n" \
                      "Number followed by '-' or ':' treated as a start of page sequence\n"));
    const QString default_label_text(tr("Input page numbers:"));
    dlg.setLabelText(default_label_text);
    QVector<int> res;
    connect(&dlg, &QInputDialog::textValueChanged, [&dlg, &default_label_text, &res](const QString & text) {
        getPageNumbersFromStr(text, res);
        if (!res.isEmpty()) {
            dlg.setLabelText(tr("Pages to be selected: %1").arg(res.count()));
        } else {
            dlg.setLabelText(default_label_text);
        }
    });

    if (dlg.exec() == QDialog::Accepted && !res.isEmpty()) {
        const PageSequence pages = m_ptrPages->toPageSequence(getCurrentView());

        QSet<PageId> page_ids;
        for (int page_no : qAsConst(res)) {
            page_ids += pages.pageAt(page_no - 1).id();
        }

        m_ptrThumbSequence->setSelection(page_ids, ThumbnailSequence::RESET_SELECTION);

//            const int pages_cnt = pages.numPages();
//            for (int i = 0; i < res.count(); i++) {
//                const int page_no = res[i];
//                if (page_no > 0 && page_no <= pages_cnt) {
//                    const PageId id = pages.pageAt(page_no-1).id();
//                    if (!id.isNull()) {
//                        m_ptrThumbSequence->setSelection(id, (i>0) ? ThumbnailSequence::KEEP_SELECTION :
//                                                                     ThumbnailSequence::RESET_SELECTION);
//                    }
//                }
//            }
        updateMainArea();
    }
}

void
MainWindow::setPublishProgressPanelVisible(bool visible)
{
    if (visible) {
        if (m_ptrPublishProgressWidget) { //should be nullptr, but just in case
            m_ptrPublishProgressWidget->reset();
        } else {
            m_ptrPublishProgressWidget.reset(new publish::EncodingProgressWidget(this));
        }
        if (m_pImageFrameLayout->indexOf(m_ptrPublishProgressWidget.get()) == -1) {
            m_pImageFrameLayout->addWidget(m_ptrPublishProgressWidget.get());
            connect(m_ptrStages->publishFilter().get(), &publish::Filter::displayProgressInfo,
                    m_ptrPublishProgressWidget.get(), &publish::EncodingProgressWidget::displayInfo);
        }
    } else {
        if (m_ptrPublishProgressWidget) {
            if (m_pImageFrameLayout->indexOf(m_ptrPublishProgressWidget.get()) != -1) {
                m_pImageFrameLayout->removeWidget(m_ptrPublishProgressWidget.get());
            }
            m_ptrPublishProgressWidget.reset(nullptr);
        }
    }
}

void MainWindow::on_actionManageSharedDictionaries_triggered()
{
    m_ptrStages->publishFilter()->displayDbjzManagerDlg();
}

void MainWindow::on_actionManageContents_triggered()
{
    m_ptrStages->publishFilter()->displayContentsManagerDlg();
}

void MainWindow::on_actionSetBundledFilename_triggered()
{
    IntrusivePtr<publish::Filter> filter = m_ptrStages->publishFilter();
    QString current_fname = filter->bundledFilename();
    if (current_fname.isEmpty()) {
        current_fname = filter->djVuFilenameSuggestion();
    }

    const QString new_fname = QFileDialog::getSaveFileName(this, tr("Set bundled document filename"),
                                                           current_fname,
                                                           tr("DjVu document (*.djvu *.djv)") );

    if (new_fname != current_fname) {
        filter->setBundledFilename(new_fname);
        if (filter->checkReadyToBundle()) {
            filter->makeBundledDjVu();
            QMessageBox::information(this, tr("Bundled document saved"),
                                     tr("Bundled document is saved to:\n%1\n"
                                        "This file will be automatically updated if pages will be changed in the project.").arg(new_fname));
        } else {
            QMessageBox::information(this, tr("Bundled document filename set"),
                                     tr("Bundled document filename is set to:\n%1\n"
                                        "But its not created as some pages are not processed yet.\n"
                                        "This file will be automatically created when all pages are processed and updated in case of any changes afterwards.").arg(new_fname));
        }
    }
}
