#include "opencv2/core/core.hpp"
#include <string>

namespace cv
{
namespace mjpeg
{

class MJpegWriter
{
public:
    virtual ~MJpegWriter();
    virtual bool write(const Mat& img) = 0;
    virtual bool isOpened() const = 0;
};

Ptr<MJpegWriter> openMJpegWriter(const std::string& filename, Size size, double fps, bool rawdata);

}

namespace jpeg
{
void writeJpeg(const std::string& filename, const Mat& img);
Mat readJpeg(const std::string& filename);
}

}
