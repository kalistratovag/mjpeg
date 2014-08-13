#include "opencv2/core/core.hpp"
#include <string>

namespace cv
{
namespace mjpeg
{

class MJpegWriter
{
public:
    enum { COLORSPACE_GRAY=0, COLORSPACE_RGBA=1, COLORSPACE_BGR=2, COLORSPACE_YUV444P=3 };
    virtual ~MJpegWriter();
    virtual bool write(const Mat& img) = 0;
    virtual bool isOpened() const = 0;
};

Ptr<MJpegWriter> openMJpegWriter(const std::string& filename, Size size, double fps, int colorspace);

}

namespace jpeg
{
void writeJpeg(const std::string& filename, const Mat& img);
Mat readJpeg(const std::string& filename);
}

}
