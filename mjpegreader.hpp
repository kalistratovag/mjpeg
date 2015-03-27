#pragma once

#include "opencv2/core/core.hpp"
#include <string>

namespace cv
{
namespace mjpeg
{

class MJpegReader
{
public:
    enum { COLORSPACE_GRAY=0, COLORSPACE_RGBA=1, COLORSPACE_BGR=2, COLORSPACE_YUV444P=3 };
    virtual ~MJpegReader() {};
    virtual bool read(const Mat& img) = 0;
    virtual bool isOpened() const = 0;
};

Ptr<MJpegReader> openMJpegReader(const std::string& filename, Size size, double fps, int colorspace);

}

}
