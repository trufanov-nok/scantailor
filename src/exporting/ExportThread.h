/*
    Scan Tailor Universal - Interactive post-processing tool for scanned
    pages. A fork of Scan Tailor by Joseph Artsimovich.
    Copyright (C) 2020 Alexander Trufanov <trufanovan@gmail.com>

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

#ifndef EXPORTTHREAD_H
#define EXPORTTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include "PageId.h"
#include "ExportSettings.h"
#include "ExportSuggestions.h"

namespace exporting {

class ExportThread : public QThread
{
    Q_OBJECT
public:

    struct ExportRec {
        ExportRec(): page_no(0) {}
        int page_no;
        PageId page_id;
        QString filename;
        QString override_background_filename;
        QString override_foreground_filename;
        QStringList zones_info;
    };

    ExportThread(const ExportSettings& settings, const QVector<ExportRec>& outpaths,
                 const QString& export_dir, const ExportSuggestions& export_suggestions, QObject *parent = nullptr);
    ~ExportThread() {
        if (isRunning()) {
            requestInterruption();
            wait();
        }
    }

    void run() override;

    void continueExecution() { m_wait.wakeAll(); }
public Q_SLOTS:
    void cancel() { requestInterruption(); };
Q_SIGNALS:
    void imageProcessed();
    void exportCanceled();
    void exportCompleted();
    void needReprocess(const PageId& page_id, QImage* fore_subscan);
    void error(const QString& errorStr);
private:
    bool isCancelRequested();
private:
    ExportSettings m_settings;
    QVector<ExportRec> m_outpaths_vector;
    QString m_export_dir;
    QMutex m_paused;
    QWaitCondition m_wait;
    QImage m_orig_fore_subscan;
    bool m_interrupted;
    const ExportSuggestions& m_export_suggestions;
};

}

#endif // EXPORTTHREAD_H
