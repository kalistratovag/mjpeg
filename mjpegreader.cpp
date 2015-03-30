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
#define fourCC_str(a)     ((int)((uchar(a[3])<<24) | (uchar(a[2])<<16) | (uchar(a[1])<<8) | uchar(a[0])))
#define RIFF_CC           fourCC_str("RIFF")
#define LIST_CC           fourCC_str("LIST")
#define HDRL_CC           fourCC_str("hdrl")
#define AVIH_CC           fourCC_str("avih")
#define STRL_CC           fourCC_str("strl")
#define STRH_CC           fourCC_str("strh")
#define VIDS_CC           fourCC_str("vids")
#define MJPG_CC           fourCC_str("MJPG")
#define STRF_CC           fourCC_str("strf")
#define AVI_CC            fourCC_str("AVI ")

typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef int32_t  LONG;


#pragma pack(push, 1)
struct AviMainHeader
{
    DWORD dwMicroSecPerFrame;    //  The period between video frames
    DWORD dwMaxBytesPerSec;      //  Maximum data rate of the file
    DWORD dwReserved1;           // 0
    DWORD dwFlags;               //  0x10 AVIF_HASINDEX: The AVI file has an idx1 chunk containing an index at the end of the file.
    DWORD dwTotalFrames;         // Field of the main header specifies the total number of frames of data in file.
    DWORD dwInitialFrames;       // Is used for interleaved files
    DWORD dwStreams;             // Specifies the number of streams in the file.
    DWORD dwSuggestedBufferSize; // Field specifies the suggested buffer size forreading the file
    DWORD dwWidth;               // Fields specify the width of the AVIfile in pixels.
    DWORD dwHeight;              // Fields specify the height of the AVIfile in pixels.
    DWORD dwReserved[4];         // 0, 0, 0, 0
};

struct AviStreamHeader
{
    uint32_t fccType;              // 'vids', 'auds', 'txts'...
    uint32_t fccHandler;           // "cvid", "DIB "
    DWORD dwFlags;               // 0
    DWORD dwPriority;            // 0
    DWORD dwInitialFrames;       // 0
    DWORD dwScale;               // 1
    DWORD dwRate;                // Fps (dwRate - frame rate for video streams)
    DWORD dwStart;               // 0
    DWORD dwLength;              // Frames number (playing time of AVI file as defined by scale and rate)
    DWORD dwSuggestedBufferSize; // For reading the stream
    DWORD dwQuality;             // -1 (encoding quality. If set to -1, drivers use the default quality value)
    DWORD dwSampleSize;          // 0 means that each frame is in its own chunk
    struct {
        short int left;
        short int top;
        short int right;
        short int bottom;
    } rcFrame;                // If stream has a different size than dwWidth*dwHeight(unused)
};

struct BitmapInfoHeader
{
    DWORD biSize;                // Write header size of BITMAPINFO header structure
    LONG  biWidth;               // width in pixels
    LONG  biHeight;              // heigth in pixels
    WORD  biPlanes;              // Number of color planes in which the data is stored
    WORD  biBitCount;            // Number of bits per pixel
    DWORD biCompression;         // Type of compression used (uncompressed: NO_COMPRESSION=0)
    DWORD biSizeImage;           // Image Buffer. Quicktime needs 3 bytes also for 8-bit png 
                                 //   (biCompression==NO_COMPRESSION)?0:xDim*yDim*bytesPerPixel;
    LONG  biXPelsPerMeter;       // Horizontal resolution in pixels per meter
    LONG  biYPelsPerMeter;       // Vertical resolution in pixels per meter
    DWORD biClrUsed;             // 256 (color table size; for 8-bit only)
    DWORD biClrImportant;        // Specifies that the first x colors of the color table. Are important to the DIB.
};

struct RiffChunk
{
    uint32_t m_four_cc;
    uint32_t m_size;
};

struct RiffList
{
    uint32_t m_riff_or_list_cc;
    uint32_t m_size;
    uint32_t m_list_type_cc;
};

#pragma pack(pop)

void printFourCc(const uint32_t& four_cc)
{
    char result[5];
    uint32_t* p_4cc = (uint32_t*)result;

    *p_4cc = four_cc;

    result[4] = 0;

    std::cout << result << std::endl;
}

std::istream& operator >> (std::istream& is, AviMainHeader& avih) 
{
    is.read((char*)(&avih), sizeof(AviMainHeader));
}

std::istream& operator >> (std::istream& is, AviStreamHeader& strh) 
{
    is.read((char*)(&strh), sizeof(AviStreamHeader));
}

std::istream& operator >> (std::istream& is, BitmapInfoHeader& bmph) 
{
    is.read((char*)(&bmph), sizeof(BitmapInfoHeader));
}

std::istream& operator >> (std::istream& is, RiffList& riff_list) 
{
    is.read((char*)(&riff_list), sizeof(riff_list));
}

std::istream& operator >> (std::istream& is, RiffChunk& riff_chunk) 
{
    is.read((char*)(&riff_chunk), sizeof(riff_chunk));
}

class MJpegReaderImpl : public MJpegReader
{
public:
    MJpegReaderImpl(const std::string& filename, Size size, double fps, int colorspace)
    {
        std::fstream fs;
        fs.open (filename.c_str(), std::fstream::in | std::fstream::binary);

        RiffList riff_list;

        fs >> riff_list;

        std::cout << (RIFF_CC == fourCC_str("RIFF")) << std::endl;;

        if(riff_list.m_riff_or_list_cc == RIFF_CC)
        {
            if(riff_list.m_list_type_cc == AVI_CC)
            {
                std::cout << "AVI" << std::endl;
                RiffList list;

                fs >> list;

                if(list.m_riff_or_list_cc == LIST_CC)
                {
                    std::cout << "LIST" << std::endl;
                    std::cout << list.m_size << std::endl;

                    printFourCc(list.m_list_type_cc);

                    RiffChunk hdrl;

                    fs >> hdrl;

                    printFourCc(hdrl.m_four_cc);

                    AviMainHeader avi_head;
                    fs >> avi_head;

                    fs >> list;

                    if(list.m_riff_or_list_cc == LIST_CC)
                    {
                        printFourCc(list.m_list_type_cc);
                    }

                    fs >> hdrl;
                    printFourCc(hdrl.m_four_cc);

                    AviStreamHeader strh;
                    fs >> strh;

                    printFourCc(strh.fccType);
                    printFourCc(strh.fccHandler);

                    fs >> hdrl;
                    printFourCc(hdrl.m_four_cc);

                    BitmapInfoHeader bmp_info_header;
                    fs >> bmp_info_header;

                    std::cout << bmp_info_header.biBitCount << std::endl;

                    printFourCc(bmp_info_header.biCompression);

                    fs >> list;

                    if(list.m_riff_or_list_cc == LIST_CC)
                    {
                        std::cout << "LIST" << std::endl;
                        std::cout << list.m_size << std::endl;

                        printFourCc(list.m_list_type_cc);

                        fs >> hdrl;
                        printFourCc(hdrl.m_four_cc);

                        std::cout << hdrl.m_size << std::endl;

                        fs.seekg(hdrl.m_size, fs.cur);

                        fs >> hdrl;
                        printFourCc(hdrl.m_four_cc);

                        fs.seekg(hdrl.m_size, fs.cur);

                        fs >> list;
                        if(list.m_riff_or_list_cc == LIST_CC)
                        {
                            std::cout << "LIST" << std::endl;
                            std::cout << list.m_size << std::endl;

                            printFourCc(list.m_list_type_cc);
                        }
                    }
                }
            }
        }
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