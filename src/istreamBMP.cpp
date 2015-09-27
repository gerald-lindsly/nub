#include <nub/istreamBMP.h>

HBITMAP getStreamBMP(std::istream* is)
{
    BITMAPINFO bmi = {};
    is->seekg(sizeof(BITMAPFILEHEADER));
    is->read((char*)&bmi, sizeof(BITMAPINFOHEADER));
    char* p;
    HBITMAP bmp = CreateDIBSection(0, &bmi, 0, (void**)&p, 0, 0);
    is->read(p, bmi.bmiHeader.biHeight * bmi.bmiHeader.biWidth * 4);
    return bmp;
}
