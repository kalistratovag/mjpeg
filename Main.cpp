#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <string>
#include "mjpegwriter.hpp"

using namespace cv;
using namespace std;

int main(int argc, char** argv)
{
	Mat img = imread(std::string(argv[1]), 1);
    Mat img_yuv, img_yuv444p;
    Rect rect(0, 0, img.cols, img.rows);
    int nframes = 10;
    Ptr<cv::mjpeg::MJpegWriter> codec = cv::mjpeg::openMJpegWriter(std::string(argv[2]), img.size(), 30, false);
    VideoWriter outputVideo;
    
    /*cvtColor(img, img_yuv, COLOR_BGR2YUV);
    img_yuv444p.create(img.rows * 3 , img.cols, CV_8U);
    Mat planes[] =
    {
        img_yuv444p.rowRange(0, img.rows),
        img_yuv444p.rowRange(img.rows, img.rows * 2),
        img_yuv444p.rowRange(img.rows * 2, img.rows * 3)
    };
    split(img_yuv, planes);*/

    double ttotal = 0;

	for (int i = 0; i < nframes; i++)
	{
        double tstart = (double)getTickCount();
        codec->write(img);
        double tend = (double)getTickCount();
        ttotal += tend - tstart;
        
        putchar('.');
        fflush(stdout);
	}

    codec.release();
    //codec->write(img);
    //cv::jpeg::writeJpeg("out0.jpg", img);
    printf("time per frame (including file i/o)=%.1fms\n", (double)ttotal*1000./getTickFrequency()/nframes);
    //Mat img2 = cv::jpeg::readJpeg(argv[2]);
    //imshow("test", img2);
    //waitKey();

	return 0;
}
