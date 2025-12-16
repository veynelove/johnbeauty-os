#include <drivers/vga.h>

namespace JLOS::Drivers {
VideoGraphicsArray::VideoGraphicsArray():
miscPort(0x3c2),crtcIndexPort(0x3d4),crtcDataPort(0x3d5), sequencerIndexPort(0x3c4), sequencerDataPort(0x3c5),
graphicsControllerIndexPort(0x3ce), graphicsControllerDataPort(0x3cf),attributeControllerIndexPort(0x3c0),
attributeControllerReadPort(0x3c0),attribiteControllerWritePort(0x3c1), attribiteControllerResetPort(0x3da)
{}

VideoGraphicsArray::~VideoGraphicsArray(){}

void VideoGraphicsArray::WriteRegisters(uint8_t *registers)
{

}
uint8_t *VideoGraphicsArray::GetFrameBufferSegment(uint8_t r, uint8_t g, uint8_t b)
{

}

void VideoGraphicsArray::PutPixel(uint32_t x, uint32_t y, uint8_t colorIndex)
{

}
uint8_t VideoGraphicsArray::GetColorIndex(uint32_t x, uint32_t y, uint8_t colorIndex)
{

}

bool VideoGraphicsArray::SupportMode(uint32_t width, uint32_t height, uint32_t colordepth)
{
     return width == 320 && height == 200 && colordepth == 8;
}

bool VideoGraphicsArray::SetMode(uint32_t width, uint32_t height, uint32_t colordepth)
{

}

void VideoGraphicsArray::PutPixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{

}
}
