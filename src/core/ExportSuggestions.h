#ifndef EXPORTSUGGESTIONS_H
#define EXPORTSUGGESTIONS_H

#include <QMap>
#include <QDateTime>
#include "PageId.h"

class QDomDocument;
class QDomElement;
class QImage;

class ExportSuggestion
{
public:
    ExportSuggestion();
    ExportSuggestion(QDomElement const& el);
    ExportSuggestion(const QImage& image);


    bool hasBWLayer;
    bool hasColorLayer;
    bool isValid;
    uint width;
    uint height;
    uint dpi;

    bool operator !=(const ExportSuggestion &other) const;
    QDomElement toXml(QDomDocument& doc, QString const& name) const;
    void updateLastChanged();
private:
    int lastChanged;
    static QDateTime startTimerDate;
};

class ExportSuggestions: public QMap<PageId, ExportSuggestion>
{
public:
    ExportSuggestions():QMap<PageId, ExportSuggestion>(){};
};

#endif // EXPORTSUGGESTIONS_H
