#ifndef __DRIVERS_VGA_H
#define __DRIVERS_VGA_H

#include <common/types.h>
#include <hdc/interrupts.h>
#include <hdc/port.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
class VideoGraphicsArray {
private:
     Hdc::Port8Bit miscPort;
     Hdc::Port8Bit crtcIndexPort;
     Hdc::Port8Bit crtcDataPort;
     Hdc::Port8Bit sequencerIndexPort;
     Hdc::Port8Bit sequencerDataPort;
     Hdc::Port8Bit graphicsControllerIndexPort;
     Hdc::Port8Bit graphicsControllerDataPort;
     Hdc::Port8Bit attributeControllerIndexPort;
     Hdc::Port8Bit attributeControllerReadPort;
     Hdc::Port8Bit attribiteControllerWritePort;
     Hdc::Port8Bit attribiteControllerResetPort;

     void WriteRegisters(uint8_t *registers);
     uint8_t *GetFrameBufferSegment(uint8_t r, uint8_t g, uint8_t b);
     uint8_t *GetFrameBufferSegment();

     virtual uint8_t GetColorIndex(uint32_t r, uint32_t g, uint8_t b);

public:
     VideoGraphicsArray();
     ~VideoGraphicsArray();

     virtual bool SupportMode(uint32_t width, uint32_t height, uint32_t colordepth);
     virtual bool SetMode(uint32_t width, uint32_t height, uint32_t colordepth);
     virtual void PutPixel(int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b);
     virtual void PutPixel(int32_t x, int32_t y, uint8_t colorIndex);

     virtual void FillRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b);
};
}
}
#endif
