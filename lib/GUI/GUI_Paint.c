/******************************************************************************
* | File        :   GUI_Paint.c
* | Author      :   Waveshare electronics
* | Function    :       Achieve drawing: draw points, lines, boxes, circles and
*                   their size, solid dotted line, solid rectangle hollow
*                   rectangle, solid circle hollow circle.
* | Info        :
*   Achieve display characters: Display a single character, string, number
*   Achieve time display: adaptive size display time minutes and seconds
*----------------
* |     This version:   V1.0
* | Date        :   2019-07-11
* | Info        :
* -----------------------------------------------------------------------------
*
* Create library
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documnetation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to  whom the Software is
* furished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
******************************************************************************/
#include "GUI_Paint.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h> //memset()
#include <math.h>

PAINT Paint;

/******************************************************************************
function: Create Image
parameter:
    image   :   Pointer to the image cache
    width   :   The width of the picture
    Height  :   The height of the picture
    Color   :   Whether the picture is inverted
******************************************************************************/
void Paint_NewImage(UWORD *image, UWORD Width, UWORD Height, UWORD Rotate, UWORD Color, UWORD Depth)
{
    Paint.Image = NULL;
    Paint.Image = image;

    Paint.WidthMemory = Width;
    Paint.HeightMemory = Height;
    Paint.Color = Color;
    Paint.WidthByte = Width;
    Paint.HeightByte = Height;
    Paint.Depth = Depth;
//    printf("WidthByte = %d, HeightByte = %d\r\n", Paint.WidthByte, Paint.HeightByte);
//    printf(" EPD_WIDTH / 8 = %d\r\n",  122 / 8);

    Paint.Rotate = Rotate;
    Paint.Mirror = MIRROR_NONE;

    if(Rotate == ROTATE_0 || Rotate == ROTATE_180) {
        Paint.Width = Width;
        Paint.Height = Height;
    } else {
        Paint.Width = Height;
        Paint.Height = Width;
    }
}

/******************************************************************************
function: Select Image
parameter:
    image : Pointer to the image cache
******************************************************************************/
void Paint_SelectImage(UWORD *image)
{
    Paint.Image = image;
}

/******************************************************************************
function: Select Image Rotate
parameter:
    Rotate : 0,90,180,270
******************************************************************************/
void Paint_SetRotate(UWORD Rotate)
{
    if(Rotate == ROTATE_0 || Rotate == ROTATE_90 || Rotate == ROTATE_180 || Rotate == ROTATE_270) {
        DEBUG("Set image Rotate %d\r\n", Rotate);
        Paint.Rotate = Rotate;
    if(Rotate == ROTATE_0 || Rotate == ROTATE_180) {
        Paint.Width = Paint.WidthMemory;
        Paint.Height = Paint.HeightMemory;
    } else {
        Paint.Width = Paint.HeightMemory;
        Paint.Height = Paint.WidthMemory;
    }
    } else {
        DEBUG("rotate = 0, 90, 180, 270\r\n");
    }
}

/******************************************************************************
function:       Select Image mirror
parameter:
    mirror   :Not mirror,Horizontal mirror,Vertical mirror,Origin mirror
******************************************************************************/
void Paint_SetMirroring(UBYTE mirror)
{
    if(mirror == MIRROR_NONE || mirror == MIRROR_HORIZONTAL ||
        mirror == MIRROR_VERTICAL || mirror == MIRROR_ORIGIN) {
        DEBUG("mirror image x:%s, y:%s\r\n",(mirror & 0x01)? "mirror":"none", ((mirror >> 1) & 0x01)? "mirror":"none");
        Paint.Mirror = mirror;
    } else {
        DEBUG("mirror should be MIRROR_NONE, MIRROR_HORIZONTAL, \
        MIRROR_VERTICAL or MIRROR_ORIGIN\r\n");
    }
}

/******************************************************************************
function: Draw Pixels
parameter:
    Xpoint : At point X
    Ypoint : At point Y
    Color  : Painted colors
******************************************************************************/
void Paint_SetPixel(UWORD Xpoint, UWORD Ypoint, UWORD Color)
{
    if(Xpoint > Paint.Width || Ypoint > Paint.Height){
       // DEBUG("Exceeding display boundaries\r\n");
        return;
    }
    UWORD X, Y;

    switch(Paint.Rotate) {
    case 0:
        X = Xpoint;
        Y = Ypoint;
        break;
    case 90:
        X = Paint.WidthMemory - Ypoint - 1;
        Y = Xpoint;
        break;
    case 180:
        X = Paint.WidthMemory - Xpoint - 1;
        Y = Paint.HeightMemory - Ypoint - 1;
        break;
    case 270:
        X = Ypoint;
        Y = Paint.HeightMemory - Xpoint - 1;
        break;
    default:
        return;
    }

    switch(Paint.Mirror) {
    case MIRROR_NONE:
        break;
    case MIRROR_HORIZONTAL:
        X = Paint.WidthMemory - X - 1;
        break;
    case MIRROR_VERTICAL:
        Y = Paint.HeightMemory - Y - 1;
        break;
    case MIRROR_ORIGIN:
        X = Paint.WidthMemory - X - 1;
        Y = Paint.HeightMemory - Y - 1;
        break;
    default:
        return;
    }

    if(X > Paint.WidthMemory || Y > Paint.HeightMemory){
        DEBUG("Exceeding display boundaries\r\n");
        return;
    }


    if(Paint.Depth == 1){
        UDOUBLE Addr = X / 8 + Y * Paint.WidthByte;
        UBYTE Rdata = Paint.Image[Addr];
        if(Color == BLACK)
            Paint.Image[Addr] = Rdata & ~(0x80 >> (X % 8));
        else
            Paint.Image[Addr] = Rdata | (0x80 >> (X % 8));
    } else {
        Color = ((Color<<8)&0xff00)|(Color>>8);
        UDOUBLE Addr = X  + Y * Paint.WidthByte;
        Paint.Image[Addr] = Color;
    }
}

/******************************************************************************
function: Clear the color of the picture
parameter:
    Color : Painted colors
******************************************************************************/
void Paint_Clear(UWORD Color)
{
    for (UWORD Y = 0; Y < Paint.HeightByte; Y++) {
        for (UWORD X = 0; X < Paint.WidthByte; X++ ) {//8 pixel =  1 byte
            UDOUBLE Addr = X + Y*Paint.WidthByte;
            Paint.Image[Addr] = Color;
        }
    }
}

/******************************************************************************
function: Clear the color of a window
parameter:
    Xstart : x starting point
    Ystart : Y starting point
    Xend   : x end point
    Yend   : y end point
    Color  : Painted colors
******************************************************************************/
void Paint_ClearWindow(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color)
{
    UWORD X, Y;
    for (Y = Ystart; Y < Yend; Y++) {
        for (X = Xstart; X < Xend; X++) {//8 pixel =  1 byte
            Paint_SetPixel(X, Y, Color);
        }
    }
}