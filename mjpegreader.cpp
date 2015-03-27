#include <stdint.h>
#include <fstream>
#include <iostream>

#include "opencv2/core/core.hpp"
#include "mjpegreader.hpp"

namespace cv
{
namespace mjpeg
{

#define fourCC(a,b,c,d)   ((int)((uchar(d)<<24) | (uchar(c)<<16) | (uchar(b)<<8) | uchar(a)))

struct avi_header
{
    uint32_t dwMicroSecPerFrame;    //  The period between video frames
    uint32_t dwMaxBytesPerSec;      //  Maximum data rate of the file
    uint32_t dwReserved1;           // 0
    uint32_t dwFlags;               //  0x10 AVIF_HASINDEX: The AVI file has an idx1 chunk containing an index at the end of the file.
    uint32_t dwTotalFrames;         // Field of the main header specifies the total number of frames of data in file.
    uint32_t dwInitialFrames;       // Is used for interleaved files
    uint32_t dwStreams;             // Specifies the number of streams in the file.
    uint32_t dwSuggestedBufferSize; // Field specifies the suggested buffer size forreading the file
    uint32_t dwWidth;               // Fields specify the width of the AVIfile in pixels.
    uint32_t dwHeight;              // Fields specify the height of the AVIfile in pixels.
    uint32_t dwReserved[4];         // 0, 0, 0, 0
};

class MJpegReaderImpl : public MJpegReader
{
public:
    MJpegReaderImpl(const std::string& filename, Size size, double fps, int colorspace)
    {
        std::fstream fs;
        fs.open (filename.c_str(), std::fstream::in | std::fstream::binary);
        int riff = 0;
        int avi = 0;
        char buf[32];

        //fs.read((char*)&riff, 4);
        //fs.read((char*)&avi, 4);
        fs.read(buf, 32);

        std::cout << "is opened " << fs.is_open() << std::endl;
        //std::cout << "riff " << riff << " " << fourCC('R', 'I', 'F', 'F') << std::endl;
        //std::cout << "avi " << avi << " " << fourCC('A', 'V', 'I', ' ') << std::endl;
        for(int i = 0; i < 8; ++i)
        {
            std::cout << buf[i];
        }

        std::cout << std::endl;
    }

    ~MJpegReaderImpl()
    {

    }

    bool read(const Mat& img)
    {

    }

    bool isOpened() const
    {
        return true;
    }
};

Ptr<MJpegReader> openMJpegReader(const std::string& filename, Size size, double fps, int colorspace)
{
    Ptr<MJpegReader> mjcodec = new MJpegReaderImpl(filename, size, fps, colorspace);
    //if( mjcodec->isOpened() )
    //    return mjcodec;
    return Ptr<MJpegReader>();
}

}
}