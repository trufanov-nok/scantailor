#ifndef FREEIMAGEFILTERS_H
#define FREEIMAGEFILTERS_H
#include <QMap>

enum FREE_IMAGE_FILTER {
    FILTER_BOX		  = 0,	//! Box, pulse, Fourier window, 1st order (constant) b-spline
    FILTER_BICUBIC	  = 1,	//! Mitchell & Netravali's two-param cubic filter
    FILTER_BILINEAR   = 2,	//! Bilinear filter
    FILTER_BSPLINE	  = 3,	//! 4th order (cubic) b-spline
    FILTER_CATMULLROM = 4,	//! Catmull-Rom spline, Overhauser spline
    FILTER_LANCZOS3	  = 5	//! Lanczos3 filter
};

struct FilterInfo
{
    FilterInfo();
    FilterInfo(const QString& id, const QString& title, const QString& description);
    QString id;
    QString title;
    QString description;
};

class ImageFilters {
public:
    typedef QMap<FREE_IMAGE_FILTER, FilterInfo> Info;
    static const Info info;
};

QString scale_filter2str(FREE_IMAGE_FILTER filter);
FREE_IMAGE_FILTER str2scale_filter(const QString & str);


#endif // FREEIMAGEFILTERS_H
