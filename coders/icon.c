/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%                        IIIII   CCCC   OOO   N   N                           %
%                          I    C      O   O  NN  N                           %
%                          I    C      O   O  N N N                           %
%                          I    C      O   O  N  NN                           %
%                        IIIII   CCCC   OOO   N   N                           %
%                                                                             %
%                                                                             %
%                   Read Microsoft Windows Icon Format                        %
%                                                                             %
%                              Software Design                                %
%                                   Cristy                                    %
%                                 July 1992                                   %
%                                                                             %
%                                                                             %
%  Copyright 1999 ImageMagick Studio LLC, a non-profit organization           %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    https://imagemagick.org/script/license.php                               %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
*/

/*
  Include declarations.
*/
#include "magick/studio.h"
#include "magick/artifact.h"
#include "magick/blob.h"
#include "magick/blob-private.h"
#include "magick/cache.h"
#include "magick/colormap.h"
#include "magick/colorspace.h"
#include "magick/colorspace-private.h"
#include "magick/exception.h"
#include "magick/exception-private.h"
#include "magick/image.h"
#include "magick/image-private.h"
#include "magick/list.h"
#include "magick/log.h"
#include "magick/magick.h"
#include "magick/memory_.h"
#include "magick/monitor.h"
#include "magick/monitor-private.h"
#include "magick/nt-base-private.h"
#include "magick/nt-feature.h"
#include "magick/option.h"
#include "magick/pixel-accessor.h"
#include "magick/quantize.h"
#include "magick/quantum-private.h"
#include "magick/static.h"
#include "magick/string_.h"
#include "magick/module.h"

/*
  Define declarations.
*/
#define IconRgbCompression (size_t) 0
#define MaxIcons  1024

/*
  Typedef declarations.
*/
typedef struct _IconEntry
{
  unsigned char
    width,
    height,
    colors,
    reserved;

  unsigned short int
    planes,
    bits_per_pixel;

  size_t
    size,
    offset;
} IconEntry;

typedef struct _IconFile
{
  short
    reserved,
    resource_type,
    count;

  IconEntry
    directory[MaxIcons];
} IconFile;

typedef struct _IconInfo
{
  size_t
    file_size,
    ba_offset,
    offset_bits,
    size;

  ssize_t
    width,
    height;

  unsigned short
    planes,
    bits_per_pixel;

  size_t
    compression,
    image_size,
    x_pixels,
    y_pixels,
    number_colors,
    red_mask,
    green_mask,
    blue_mask,
    alpha_mask,
    colors_important;

  ssize_t
    colorspace;
} IconInfo;

/*
  Forward declarations.
*/
static Image
  *AutoResizeImage(const Image *,const char *,MagickOffsetType *,
    ExceptionInfo *);

static MagickBooleanType
  WriteICONImage(const ImageInfo *,Image *);

Image *AutoResizeImage(const Image *image,const char *option,
  MagickOffsetType *count,ExceptionInfo *exception)
{
  #define MAX_SIZES 16

  char
    *q;

  const char
    *p;

  Image
    *resized,
    *images;

  size_t
    sizes[MAX_SIZES]={256, 192, 128, 96, 64, 48, 40, 32, 24, 16};

  ssize_t
    i;

  images=NULL;
  *count=0;
  i=0;
  p=option;
  while (*p != '\0' && i < MAX_SIZES)
  {
    size_t
      size;

    while ((isspace((int) ((unsigned char) *p)) != 0))
      p++;
    size=(size_t)strtol(p,&q,10);
    if ((p == q) || (size < 16) || (size > 256))
        return((Image *) NULL);
    p=q;
    sizes[i++]=size;
    while ((isspace((int) ((unsigned char) *p)) != 0) || (*p == ','))
      p++;
  }
  if (i == 0)
    i=10;
  *count=i;
  for (i=0; i < *count; i++)
  {
    resized=ResizeImage(image,sizes[i],sizes[i],image->filter,image->blur,
      exception);
    if (resized == (Image *) NULL)
      return(DestroyImageList(images));
    if (images == (Image *) NULL)
      images=resized;
    else
      AppendImageToList(&images,resized);
  }
  return(images);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e a d I C O N I m a g e                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  ReadICONImage() reads a Microsoft icon image file and returns it.  It
%  allocates the memory necessary for the new Image structure and returns a
%  pointer to the new image.
%
%  The format of the ReadICONImage method is:
%
%      Image *ReadICONImage(const ImageInfo *image_info,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image_info: the image info.
%
%    o exception: return any errors or warnings in this structure.
%
*/

static Image *Read1XImage(Image *image,ExceptionInfo *exception)
{
  size_t
    columns,
    rows;

  ssize_t
    i;

  /*
    Read Windows 1.0 Icon.
  */
  (void) ReadBlobLSBLong(image);  /* hot spot X/Y */
  columns=(size_t) ReadBlobLSBShort(image);
  rows=(size_t) (ReadBlobLSBShort(image));
  (void) ReadBlobLSBShort(image);  /* width of bitmap in bytes */
  (void) ReadBlobLSBShort(image);  /* cursor color */
  if ((rows != 32 && rows != 64) || (columns != 32 && columns != 64))
    ThrowImageException(CorruptImageError,"ImproperImageHeader");
  /*
    Convert bitmap scanline.
  */
  if (SetImageExtent(image,columns,rows) == MagickFalse)
    return(DestroyImageList(image));
  image->matte=MagickTrue;
  if (AcquireImageColormap(image,3) == MagickFalse)
    return(DestroyImageList(image));
  image->colormap[1].opacity=TransparentOpacity;
  for (i=0; i < 2; i++)
  {
    ssize_t
      y;

    for (y=0; y < (ssize_t) image->columns; y++)
    {
      IndexPacket
        *indexes;

      PixelPacket
        *q;

      ssize_t
        x;

      q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
      if (q == (PixelPacket *) NULL)
        break;
      indexes=GetAuthenticIndexQueue(image);
      for (x=0; x < (ssize_t) (image->columns-7); x+=8)
      {
        size_t
          bit,
          byte;

        byte=(size_t) ReadBlobByte(image);
        for (bit=0; bit < 8; bit++)
        {
          Quantum
            index;

          index=((byte & (0x80 >> bit)) != 0 ? (i == 0 ? 0x01 : 0x02) : 0x00);
          if (i == 0)
            SetPixelIndex(indexes+x+bit,index);
          else
            if (GetPixelIndex(indexes+x+bit) != 0x01)
              SetPixelIndex(indexes+x+bit,index);
          q++;
        }
      }
      if (SyncAuthenticPixels(image,exception) == MagickFalse)
        break;
    }
  }
  (void) SyncImage(image);
  if (CloseBlob(image) == MagickFalse)
    return(DestroyImageList(image));
  return(GetFirstImageInList(image));
}

static Image *ReadICONImage(const ImageInfo *image_info,
  ExceptionInfo *exception)
{
  IconFile
    icon_file;

  IconInfo
    icon_info;

  Image
    *image;

  MagickBooleanType
    status;

  MagickSizeType
    extent;

  IndexPacket
    *indexes;

  ssize_t
    i,
    x;

  PixelPacket
    *q;

  unsigned char
    *p;

  size_t
    bit,
    byte,
    bytes_per_line,
    one,
    scanline_pad;

  ssize_t
    count,
    offset,
    y;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  (void) LogMagickEvent(CoderEvent,GetMagickModule(),"%s",image_info->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  image=AcquireImage(image_info);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  (void) memset(&icon_file,0,sizeof(icon_file));
  icon_file.reserved=(short) ReadBlobLSBShort(image);
  if ((icon_file.reserved == 0x0001) || (icon_file.reserved == 0x0101) ||
      (icon_file.reserved == 0x0201))
    return(Read1XImage(image,exception));
  icon_file.resource_type=(short) ReadBlobLSBShort(image);
  icon_file.count=(short) ReadBlobLSBShort(image);
  if ((icon_file.reserved != 0) ||
      ((icon_file.resource_type != 1) && (icon_file.resource_type != 2)) ||
      (icon_file.count > MaxIcons))
    ThrowReaderException(CorruptImageError,"ImproperImageHeader");
  extent=0;
  for (i=0; i < icon_file.count; i++)
  {
    icon_file.directory[i].width=(unsigned char) ReadBlobByte(image);
    icon_file.directory[i].height=(unsigned char) ReadBlobByte(image);
    icon_file.directory[i].colors=(unsigned char) ReadBlobByte(image);
    icon_file.directory[i].reserved=(unsigned char) ReadBlobByte(image);
    icon_file.directory[i].planes=(unsigned short) ReadBlobLSBShort(image);
    icon_file.directory[i].bits_per_pixel=(unsigned short)
      ReadBlobLSBShort(image);
    icon_file.directory[i].size=ReadBlobLSBLong(image);
    icon_file.directory[i].offset=ReadBlobLSBLong(image);
    if (EOFBlob(image) != MagickFalse)
      break;
    extent=MagickMax(extent,icon_file.directory[i].size);
  }
  if ((EOFBlob(image) != MagickFalse) || (extent > GetBlobSize(image)))
    ThrowReaderException(CorruptImageError,"UnexpectedEndOfFile");
  one=1;
  for (i=0; i < icon_file.count; i++)
  {
    /*
      Verify Icon identifier.
    */
    offset=(ssize_t) SeekBlob(image,(MagickOffsetType)
      icon_file.directory[i].offset,SEEK_SET);
    if (offset < 0)
      ThrowReaderException(CorruptImageError,"ImproperImageHeader");
    icon_info.size=ReadBlobLSBLong(image);
    icon_info.width=(unsigned char) ReadBlobLSBSignedLong(image);
    icon_info.height=(unsigned char) (ReadBlobLSBSignedLong(image)/2);
    icon_info.planes=ReadBlobLSBShort(image);
    icon_info.bits_per_pixel=ReadBlobLSBShort(image);
    if (EOFBlob(image) != MagickFalse)
      {
        ThrowFileException(exception,CorruptImageError,"UnexpectedEndOfFile",
          image->filename);
        break;
      }
    if (((icon_info.planes == 18505) && (icon_info.bits_per_pixel == 21060)) || 
        (icon_info.size == 0x474e5089))
      {
        Image
          *icon_image;

        ImageInfo
          *read_info;

        size_t
          length;

        unsigned char
          *png;

        /*
          Icon image encoded as a compressed PNG image.
        */
        length=icon_file.directory[i].size;
        if ((length < 16) || (~length < 16))
          ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
        png=(unsigned char *) AcquireQuantumMemory(length,sizeof(*png));
        if (png == (unsigned char *) NULL)
          ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
        (void) memcpy(png,"\211PNG\r\n\032\n\000\000\000\015",12);
        png[12]=(unsigned char) icon_info.planes;
        png[13]=(unsigned char) (icon_info.planes >> 8);
        png[14]=(unsigned char) icon_info.bits_per_pixel;
        png[15]=(unsigned char) (icon_info.bits_per_pixel >> 8);
        count=ReadBlob(image,length-16,png+16);
        if (count != (ssize_t) (length-16))
          {
            png=(unsigned char *) RelinquishMagickMemory(png);
            ThrowReaderException(CorruptImageError,
              "InsufficientImageDataInFile");
          }
        read_info=CloneImageInfo(image_info);
        (void) CopyMagickString(read_info->magick,"PNG",MaxTextExtent);
        icon_image=BlobToImage(read_info,png,length,exception);
        read_info=DestroyImageInfo(read_info);
        png=(unsigned char *) RelinquishMagickMemory(png);
        if (icon_image == (Image *) NULL)
          return(DestroyImageList(image));
        DestroyBlob(icon_image);
        icon_image->blob=ReferenceBlob(image->blob);
        ReplaceImageInList(&image,icon_image);
        icon_image->scene=i;
      }
    else
      {
        if (icon_info.bits_per_pixel > 32)
          ThrowReaderException(CorruptImageError,"ImproperImageHeader");
        icon_info.compression=ReadBlobLSBLong(image);
        icon_info.image_size=ReadBlobLSBLong(image);
        icon_info.x_pixels=ReadBlobLSBLong(image);
        icon_info.y_pixels=ReadBlobLSBLong(image);
        icon_info.number_colors=ReadBlobLSBLong(image);
        if (icon_info.number_colors > GetBlobSize(image))
          ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
        icon_info.colors_important=ReadBlobLSBLong(image);
        image->matte=MagickTrue;
        image->columns=(size_t) icon_file.directory[i].width;
        if ((ssize_t) image->columns > icon_info.width)
          image->columns=(size_t) icon_info.width;
        if (image->columns == 0)
          image->columns=256;
        image->rows=(size_t) icon_file.directory[i].height;
        if ((ssize_t) image->rows > icon_info.height)
          image->rows=(size_t) icon_info.height;
        if (image->rows == 0)
          image->rows=256;
        image->depth=icon_info.bits_per_pixel;
        if (image->depth > 16)
          image->depth=8;
        if (image->debug != MagickFalse)
          {
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              " scene    = %.20g",(double) i);
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              "   size   = %.20g",(double) icon_info.size);
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              "   width  = %.20g",(double) icon_file.directory[i].width);
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              "   height = %.20g",(double) icon_file.directory[i].height);
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              "   colors = %.20g",(double ) icon_info.number_colors);
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              "   planes = %.20g",(double) icon_info.planes);
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
              "   bpp    = %.20g",(double) icon_info.bits_per_pixel);
          }
      if ((icon_info.number_colors != 0) || (icon_info.bits_per_pixel <= 16U))
        {
          image->storage_class=PseudoClass;
          image->colors=icon_info.number_colors;
          if ((image->colors == 0) || (image->colors > 256))
            image->colors=one << icon_info.bits_per_pixel;
        }
      if (image->storage_class == PseudoClass)
        {
          ssize_t
            i;

          unsigned char
            *icon_colormap;

          /*
            Read Icon raster colormap.
          */
          if (image->colors > GetBlobSize(image))
            ThrowReaderException(CorruptImageError,
              "InsufficientImageDataInFile");
          if (AcquireImageColormap(image,image->colors) == MagickFalse)
            ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
          icon_colormap=(unsigned char *) AcquireQuantumMemory((size_t)
            image->colors,4UL*sizeof(*icon_colormap));
          if (icon_colormap == (unsigned char *) NULL)
            ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
          count=ReadBlob(image,(size_t) (4*image->colors),icon_colormap);
          if (count != (ssize_t) (4*image->colors))
            {
              icon_colormap=(unsigned char *) RelinquishMagickMemory(
                icon_colormap);
              ThrowReaderException(CorruptImageError,
                "InsufficientImageDataInFile");
            }
          p=icon_colormap;
          for (i=0; i < (ssize_t) image->colors; i++)
          {
            image->colormap[i].blue=(Quantum) ScaleCharToQuantum(*p++);
            image->colormap[i].green=(Quantum) ScaleCharToQuantum(*p++);
            image->colormap[i].red=(Quantum) ScaleCharToQuantum(*p++);
            p++;
          }
          icon_colormap=(unsigned char *) RelinquishMagickMemory(icon_colormap);
        }
        /*
          Convert Icon raster image to pixel packets.
        */
        if ((image_info->ping != MagickFalse) &&
            (image_info->number_scenes != 0))
          if (image->scene >= (image_info->scene+image_info->number_scenes-1))
            break;
        status=SetImageExtent(image,image->columns,image->rows);
        if (status == MagickFalse)
          {
            InheritException(exception,&image->exception);
            return(DestroyImageList(image));
          }
        bytes_per_line=(((image->columns*icon_info.bits_per_pixel)+31) &
          ~31) >> 3;
        (void) bytes_per_line;
        scanline_pad=((((image->columns*icon_info.bits_per_pixel)+31) & ~31)-
          (image->columns*icon_info.bits_per_pixel)) >> 3;
        switch (icon_info.bits_per_pixel)
        {
          case 1:
          {
            /*
              Convert bitmap scanline.
            */
            for (y=(ssize_t) image->rows-1; y >= 0; y--)
            {
              q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
              if (q == (PixelPacket *) NULL)
                break;
              indexes=GetAuthenticIndexQueue(image);
              for (x=0; x < (ssize_t) (image->columns-7); x+=8)
              {
                byte=(size_t) ReadBlobByte(image);
                for (bit=0; bit < 8; bit++)
                  SetPixelIndex(indexes+x+bit,((byte & (0x80 >> bit)) != 0 ?
                    0x01 : 0x00));
              }
              if ((image->columns % 8) != 0)
                {
                  byte=(size_t) ReadBlobByte(image);
                  for (bit=0; bit < (image->columns % 8); bit++)
                    SetPixelIndex(indexes+x+bit,((byte & (0x80 >> bit)) != 0 ?
                      0x01 : 0x00));
                }
              for (x=0; x < (ssize_t) scanline_pad; x++)
                (void) ReadBlobByte(image);
              if (SyncAuthenticPixels(image,exception) == MagickFalse)
                break;
              if (image->previous == (Image *) NULL)
                {
                  status=SetImageProgress(image,LoadImageTag,image->rows-y-1,
                    image->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 4:
          {
            /*
              Read 4-bit Icon scanline.
            */
            for (y=(ssize_t) image->rows-1; y >= 0; y--)
            {
              q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
              if (q == (PixelPacket *) NULL)
                break;
              indexes=GetAuthenticIndexQueue(image);
              for (x=0; x < ((ssize_t) image->columns-1); x+=2)
              {
                byte=(size_t) ReadBlobByte(image);
                SetPixelIndex(indexes+x,((byte >> 4) & 0xf));
                SetPixelIndex(indexes+x+1,((byte) & 0xf));
              }
              if ((image->columns % 2) != 0)
                {
                  byte=(size_t) ReadBlobByte(image);
                  SetPixelIndex(indexes+x,((byte >> 4) & 0xf));
                }
              for (x=0; x < (ssize_t) scanline_pad; x++)
                (void) ReadBlobByte(image);
              if (SyncAuthenticPixels(image,exception) == MagickFalse)
                break;
              if (image->previous == (Image *) NULL)
                {
                  status=SetImageProgress(image,LoadImageTag,image->rows-y-1,
                    image->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 8:
          {
            /*
              Convert PseudoColor scanline.
            */
            for (y=(ssize_t) image->rows-1; y >= 0; y--)
            {
              q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
              if (q == (PixelPacket *) NULL)
                break;
              indexes=GetAuthenticIndexQueue(image);
              for (x=0; x < (ssize_t) image->columns; x++)
              {
                byte=(size_t) ReadBlobByte(image);
                SetPixelIndex(indexes+x,byte);
              }
              for (x=0; x < (ssize_t) scanline_pad; x++)
                (void) ReadBlobByte(image);
              if (SyncAuthenticPixels(image,exception) == MagickFalse)
                break;
              if (image->previous == (Image *) NULL)
                {
                  status=SetImageProgress(image,LoadImageTag,image->rows-y-1,
                    image->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 16:
          {
            /*
              Convert PseudoColor scanline.
            */
            for (y=(ssize_t) image->rows-1; y >= 0; y--)
            {
              q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
              if (q == (PixelPacket *) NULL)
                break;
              indexes=GetAuthenticIndexQueue(image);
              for (x=0; x < (ssize_t) image->columns; x++)
              {
                byte=(size_t) ReadBlobByte(image);
                byte|=((size_t) ReadBlobByte(image) << 8);
                SetPixelIndex(indexes+x,byte);
              }
              for (x=0; x < (ssize_t) scanline_pad; x++)
                (void) ReadBlobByte(image);
              if (SyncAuthenticPixels(image,exception) == MagickFalse)
                break;
              if (image->previous == (Image *) NULL)
                {
                  status=SetImageProgress(image,LoadImageTag,image->rows-y-1,
                    image->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 24:
          case 32:
          {
            /*
              Convert DirectColor scanline.
            */
            for (y=(ssize_t) image->rows-1; y >= 0; y--)
            {
              q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
              if (q == (PixelPacket *) NULL)
                break;
              for (x=0; x < (ssize_t) image->columns; x++)
              {
                SetPixelBlue(q,ScaleCharToQuantum((unsigned char)
                  ReadBlobByte(image)));
                SetPixelGreen(q,ScaleCharToQuantum((unsigned char)
                  ReadBlobByte(image)));
                SetPixelRed(q,ScaleCharToQuantum((unsigned char)
                  ReadBlobByte(image)));
                if (icon_info.bits_per_pixel == 32)
                  SetPixelAlpha(q,ScaleCharToQuantum((unsigned char)
                    ReadBlobByte(image)));
                q++;
              }
              if (icon_info.bits_per_pixel == 24)
                for (x=0; x < (ssize_t) scanline_pad; x++)
                  (void) ReadBlobByte(image);
              if (SyncAuthenticPixels(image,exception) == MagickFalse)
                break;
              if (image->previous == (Image *) NULL)
                {
                  status=SetImageProgress(image,LoadImageTag,image->rows-y-1,
                    image->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          default:
            ThrowReaderException(CorruptImageError,"ImproperImageHeader");
        }
        if ((image_info->ping == MagickFalse) &&
            (icon_info.bits_per_pixel <= 16))
          (void) SyncImage(image);
        if (icon_info.bits_per_pixel != 32)
          {
            /*
              Read the ICON alpha mask.
            */
            image->storage_class=DirectClass;
            for (y=(ssize_t) image->rows-1; y >= 0; y--)
            {
              q=GetAuthenticPixels(image,0,y,image->columns,1,exception);
              if (q == (PixelPacket *) NULL)
                break;
              for (x=0; x < ((ssize_t) image->columns-7); x+=8)
              {
                byte=(size_t) ReadBlobByte(image);
                for (bit=0; bit < 8; bit++)
                  SetPixelOpacity(q+x+bit,(((byte & (0x80 >> bit)) != 0) ?
                    TransparentOpacity : OpaqueOpacity));
              }
              if ((image->columns % 8) != 0)
                {
                  byte=(size_t) ReadBlobByte(image);
                  for (bit=0; bit < (image->columns % 8); bit++)
                    SetPixelOpacity(q+x+bit,(((byte & (0x80 >> bit)) != 0) ?
                      TransparentOpacity : OpaqueOpacity));
                }
              if ((image->columns % 32) != 0)
                for (x=0; x < (ssize_t) ((32-(image->columns % 32))/8); x++)
                  (void) ReadBlobByte(image);
              if (SyncAuthenticPixels(image,exception) == MagickFalse)
                break;
            }
          }
        if (EOFBlob(image) != MagickFalse)
          {
            ThrowFileException(exception,CorruptImageError,
              "UnexpectedEndOfFile",image->filename);
            break;
          }
      }
    /*
      Proceed to next image.
    */
    if (image_info->number_scenes != 0)
      if (image->scene >= (image_info->scene+image_info->number_scenes-1))
        break;
    if (i < (ssize_t) (icon_file.count-1))
      {
        /*
          Allocate next image structure.
        */
        AcquireNextImage(image_info,image);
        if (GetNextImageInList(image) == (Image *) NULL)
          {
            status=MagickFalse;
            break;
          }
        image=SyncNextImageInList(image);
        status=SetImageProgress(image,LoadImagesTag,TellBlob(image),
          GetBlobSize(image));
        if (status == MagickFalse)
          break;
      }
  }
  (void) CloseBlob(image);
  if (status == MagickFalse)
    return(DestroyImageList(image));
  return(GetFirstImageInList(image));
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e g i s t e r I C O N I m a g e                                         %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  RegisterICONImage() adds attributes for the Icon image format to
%  the list of supported formats.  The attributes include the image format
%  tag, a method to read and/or write the format, whether the format
%  supports the saving of more than one frame to the same file or blob,
%  whether the format supports native in-memory I/O, and a brief
%  description of the format.
%
%  The format of the RegisterICONImage method is:
%
%      size_t RegisterICONImage(void)
%
*/
ModuleExport size_t RegisterICONImage(void)
{
  MagickInfo
    *entry;

  entry=SetMagickInfo("CUR");
  entry->decoder=(DecodeImageHandler *) ReadICONImage;
  entry->encoder=(EncodeImageHandler *) WriteICONImage;
  entry->adjoin=MagickFalse;
  entry->seekable_stream=MagickTrue;
  entry->description=ConstantString("Microsoft icon");
  entry->magick_module=ConstantString("ICON");
  (void) RegisterMagickInfo(entry);
  entry=SetMagickInfo("ICO");
  entry->decoder=(DecodeImageHandler *) ReadICONImage;
  entry->encoder=(EncodeImageHandler *) WriteICONImage;
  entry->adjoin=MagickTrue;
  entry->seekable_stream=MagickTrue;
  entry->description=ConstantString("Microsoft icon");
  entry->magick_module=ConstantString("ICON");
  (void) RegisterMagickInfo(entry);
  entry=SetMagickInfo("ICON");
  entry->decoder=(DecodeImageHandler *) ReadICONImage;
  entry->encoder=(EncodeImageHandler *) WriteICONImage;
  entry->adjoin=MagickFalse;
  entry->seekable_stream=MagickTrue;
  entry->description=ConstantString("Microsoft icon");
  entry->magick_module=ConstantString("ICON");
  (void) RegisterMagickInfo(entry);
  return(MagickImageCoderSignature);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   U n r e g i s t e r I C O N I m a g e                                     %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  UnregisterICONImage() removes format registrations made by the
%  ICON module from the list of supported formats.
%
%  The format of the UnregisterICONImage method is:
%
%      UnregisterICONImage(void)
%
*/
ModuleExport void UnregisterICONImage(void)
{
  (void) UnregisterMagickInfo("CUR");
  (void) UnregisterMagickInfo("ICO");
  (void) UnregisterMagickInfo("ICON");
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   W r i t e I C O N I m a g e                                               %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  WriteICONImage() writes an image in Microsoft Windows bitmap encoded
%  image format, version 3 for Windows or (if the image has a matte channel)
%  version 4.
%
%  It encodes any subimage as a compressed PNG image ("BI_PNG)", only when its
%  dimensions are 256x256 and image->compression is undefined or is defined as
%  ZipCompression.
%
%  The format of the WriteICONImage method is:
%
%      MagickBooleanType WriteICONImage(const ImageInfo *image_info,
%        Image *image)
%
%  A description of each parameter follows.
%
%    o image_info: the image info.
%
%    o image:  The image.
%
*/
static MagickBooleanType WriteICONImage(const ImageInfo *image_info,
  Image *image)
{
  const char
    *option;

  const IndexPacket
    *indexes;

  const PixelPacket
    *p;

  IconFile
    icon_file;

  IconInfo
    icon_info;

  Image
    *images,
    *next;

  MagickBooleanType
    status;

  MagickOffsetType
    offset,
    scene;

  size_t
    bytes_per_line,
    number_scenes,
    scanline_pad;

  ssize_t
    i,
    x,
    y;

  unsigned char
    bit,
    byte,
    *pixels,
    *q;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
    (void) LogMagickEvent(CoderEvent,GetMagickModule(),"%s",image->filename);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,&image->exception);
  if (status == MagickFalse)
    return(status);
  images=(Image *) NULL;
  option=GetImageOption(image_info,"icon:auto-resize");
  if (option != (const char *) NULL)
    {
      images=AutoResizeImage(image,option,&scene,&image->exception);
      if (images == (Image *) NULL)
        ThrowWriterException(ImageError,"InvalidDimensions");
    }
  else
    {
      scene=0;
      next=image;
      do
      {
        if ((image->columns > 256L) || (image->rows > 256L))
          ThrowWriterException(ImageError,"WidthOrHeightExceedsLimit");
        scene++;
        next=SyncNextImageInList(next);
      } while ((next != (Image *) NULL) && (image_info->adjoin != 
          MagickFalse));
    }
  /*
    Dump out a ICON header template to be properly initialized later.
  */
  (void) WriteBlobLSBShort(image,0);
  (void) WriteBlobLSBShort(image,1);
  (void) WriteBlobLSBShort(image,(unsigned char) scene);
  (void) memset(&icon_file,0,sizeof(icon_file));
  (void) memset(&icon_info,0,sizeof(icon_info));
  scene=0;
  next=(images != (Image *) NULL) ? images : image;
  do
  {
    (void) WriteBlobByte(image,icon_file.directory[scene].width);
    (void) WriteBlobByte(image,icon_file.directory[scene].height);
    (void) WriteBlobByte(image,icon_file.directory[scene].colors);
    (void) WriteBlobByte(image,icon_file.directory[scene].reserved);
    (void) WriteBlobLSBShort(image,icon_file.directory[scene].planes);
    (void) WriteBlobLSBShort(image,icon_file.directory[scene].bits_per_pixel);
    (void) WriteBlobLSBLong(image,(unsigned int)
      icon_file.directory[scene].size);
    (void) WriteBlobLSBLong(image,(unsigned int)
      icon_file.directory[scene].offset);
    scene++;
    next=SyncNextImageInList(next);
  } while ((next != (Image *) NULL) && (image_info->adjoin != MagickFalse));
  scene=0;
  next=(images != (Image *) NULL) ? images : image;
  number_scenes=GetImageListLength(image);
  do
  {
    if ((next->columns > 255L) && (next->rows > 255L) &&
        ((next->compression == UndefinedCompression) ||
        (next->compression == ZipCompression)))
      {
        Image
          *write_image;

        ImageInfo
          *write_info;

        size_t
          length;

        unsigned char
          *png;

        write_image=CloneImage(next,0,0,MagickTrue,&image->exception);
        if (write_image == (Image *) NULL)
          {
            images=DestroyImageList(images);
            return(MagickFalse);
          }
        write_info=CloneImageInfo(image_info);
        (void) CopyMagickString(write_info->magick,"PNG32",MagickPathExtent);
        length=0;

        /* Don't write any ancillary chunks except for gAMA */
        (void) SetImageArtifact(write_image,"png:include-chunk","none,gama");

        /* Only write PNG32 formatted PNG (32-bit RGBA), 8 bits per channel */
        (void) SetImageArtifact(write_image,"png:IHDR.color-type-orig","6");
        (void) SetImageArtifact(write_image,"png:IHDR.bit-depth-orig","8");
        png=(unsigned char *) ImageToBlob(write_info,write_image,&length,
          &image->exception);
        write_image=DestroyImageList(write_image);
        write_info=DestroyImageInfo(write_info);
        if (png == (unsigned char *) NULL)
          {
            images=DestroyImageList(images);
            return(MagickFalse);
          }
        icon_file.directory[scene].width=0;
        icon_file.directory[scene].height=0;
        icon_file.directory[scene].colors=0;
        icon_file.directory[scene].reserved=0;
        icon_file.directory[scene].planes=1;
        icon_file.directory[scene].bits_per_pixel=32;
        icon_file.directory[scene].size=(size_t) length;
        icon_file.directory[scene].offset=(size_t) TellBlob(image);
        (void) WriteBlob(image,(size_t) length,png);
        png=(unsigned char *) RelinquishMagickMemory(png);
      }
    else
      {
        /*
          Initialize ICON raster file header.
        */
        if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
      (void) TransformImageColorspace(image,sRGBColorspace);
        icon_info.file_size=14+12+28;
        icon_info.offset_bits=icon_info.file_size;
        icon_info.compression=IconRgbCompression;
        if ((next->storage_class != DirectClass) && (next->colors > 256))
          (void) SetImageStorageClass(next,DirectClass);
        if (next->storage_class == DirectClass)
          {
            /*
              Full color ICON raster.
            */
            icon_info.number_colors=0;
            icon_info.bits_per_pixel=32;
            icon_info.compression=IconRgbCompression;
          }
        else
          {
            size_t
              one;

            /*
              Colormapped ICON raster.
            */
            icon_info.bits_per_pixel=8;
            if (next->colors <= 16)
              icon_info.bits_per_pixel=4;
            if (next->colors <= 2)
              icon_info.bits_per_pixel=1;
            one=1;
            icon_info.number_colors=one << icon_info.bits_per_pixel;
            if (icon_info.number_colors < next->colors)
              {
                (void) SetImageStorageClass(next,DirectClass);
                icon_info.number_colors=0;
                icon_info.bits_per_pixel=(unsigned short) 24;
                icon_info.compression=IconRgbCompression;
              }
            else
              {
                size_t
                  one;

                one=1;
                icon_info.file_size+=3*(one << icon_info.bits_per_pixel);
                icon_info.offset_bits+=3*(one << icon_info.bits_per_pixel);
                icon_info.file_size+=(one << icon_info.bits_per_pixel);
                icon_info.offset_bits+=(one << icon_info.bits_per_pixel);
              }
          }
        bytes_per_line=(((next->columns*icon_info.bits_per_pixel)+31) &
          ~31) >> 3;
        icon_info.ba_offset=0;
        icon_info.width=(ssize_t) next->columns;
        icon_info.height=(ssize_t) next->rows;
        icon_info.planes=1;
        icon_info.image_size=bytes_per_line*next->rows;
        icon_info.size=40;
        icon_info.size+=(4*icon_info.number_colors);
        icon_info.size+=icon_info.image_size;
        icon_info.size+=(((icon_info.width+31) & ~31) >> 3)*icon_info.height;
        icon_info.file_size+=icon_info.image_size;
        icon_info.x_pixels=0;
        icon_info.y_pixels=0;
        switch (next->units)
        {
          case UndefinedResolution:
          case PixelsPerInchResolution:
          {
            icon_info.x_pixels=(size_t) (100.0*next->x_resolution/2.54);
            icon_info.y_pixels=(size_t) (100.0*next->y_resolution/2.54);
            break;
          }
          case PixelsPerCentimeterResolution:
          {
            icon_info.x_pixels=(size_t) (100.0*next->x_resolution);
            icon_info.y_pixels=(size_t) (100.0*next->y_resolution);
            break;
          }
        }
        icon_info.colors_important=icon_info.number_colors;
        /*
          Convert MIFF to ICON raster pixels.
        */
        pixels=(unsigned char *) AcquireQuantumMemory((size_t)
          icon_info.image_size,sizeof(*pixels));
        if (pixels == (unsigned char *) NULL)
          {
            images=DestroyImageList(images);
            ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
          }
        (void) memset(pixels,0,(size_t) icon_info.image_size);
        switch (icon_info.bits_per_pixel)
        {
          case 1:
          {
            size_t
              bit,
              byte;

            /*
              Convert PseudoClass image to a ICON monochrome image.
            */
            for (y=0; y < (ssize_t) next->rows; y++)
            {
              p=GetVirtualPixels(next,0,y,next->columns,1,&next->exception);
              if (p == (const PixelPacket *) NULL)
                break;
              indexes=GetVirtualIndexQueue(next);
              q=pixels+(next->rows-y-1)*bytes_per_line;
              bit=0;
              byte=0;
              for (x=0; x < (ssize_t) next->columns; x++)
              {
                byte<<=1;
                byte|=GetPixelIndex(indexes+x) != 0 ? 0x01 : 0x00;
                bit++;
                if (bit == 8)
                  {
                    *q++=(unsigned char) byte;
                    bit=0;
                    byte=0;
                  }
               }
              if (bit != 0)
                *q++=(unsigned char) (byte << (8-bit));
              if (next->previous == (Image *) NULL)
                {
                  status=SetImageProgress(next,SaveImageTag,y,next->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 4:
          {
            size_t
              nibble,
              byte;

            /*
              Convert PseudoClass image to a ICON monochrome image.
            */
            for (y=0; y < (ssize_t) next->rows; y++)
            {
              p=GetVirtualPixels(next,0,y,next->columns,1,&next->exception);
              if (p == (const PixelPacket *) NULL)
                break;
              indexes=GetVirtualIndexQueue(next);
              q=pixels+(next->rows-y-1)*bytes_per_line;
              nibble=0;
              byte=0;
              for (x=0; x < (ssize_t) next->columns; x++)
              {
                byte<<=4;
                byte|=((size_t) GetPixelIndex(indexes+x) & 0x0f);
                nibble++;
                if (nibble == 2)
                  {
                    *q++=(unsigned char) byte;
                    nibble=0;
                    byte=0;
                  }
               }
              if (nibble != 0)
                *q++=(unsigned char) (byte << 4);
              if (next->previous == (Image *) NULL)
                {
                  status=SetImageProgress(next,SaveImageTag,y,next->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 8:
          {
            /*
              Convert PseudoClass packet to ICON pixel.
            */
            for (y=0; y < (ssize_t) next->rows; y++)
            {
              p=GetVirtualPixels(next,0,y,next->columns,1,&next->exception);
              if (p == (const PixelPacket *) NULL)
                break;
              indexes=GetVirtualIndexQueue(next);
              q=pixels+(next->rows-y-1)*bytes_per_line;
              for (x=0; x < (ssize_t) next->columns; x++)
                *q++=(unsigned char) ((size_t) GetPixelIndex(indexes+x));
              if (next->previous == (Image *) NULL)
                {
                  status=SetImageProgress(next,SaveImageTag,y,next->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
          case 24:
          case 32:
          {
            /*
              Convert DirectClass packet to ICON BGR888 or BGRA8888 pixel.
            */
            for (y=0; y < (ssize_t) next->rows; y++)
            {
              p=GetVirtualPixels(next,0,y,next->columns,1,&next->exception);
              if (p == (const PixelPacket *) NULL)
                break;
              q=pixels+(next->rows-y-1)*bytes_per_line;
              for (x=0; x < (ssize_t) next->columns; x++)
              {
                *q++=ScaleQuantumToChar(GetPixelBlue(p));
                *q++=ScaleQuantumToChar(GetPixelGreen(p));
                *q++=ScaleQuantumToChar(GetPixelRed(p));
                if (next->matte == MagickFalse)
                  *q++=ScaleQuantumToChar(QuantumRange);
                else
                  *q++=ScaleQuantumToChar(GetPixelAlpha(p));
                p++;
              }
              if (icon_info.bits_per_pixel == 24)
                for (x=3L*(ssize_t) next->columns; x < (ssize_t) bytes_per_line; x++)
                  *q++=0x00;
              if (next->previous == (Image *) NULL)
                {
                  status=SetImageProgress(next,SaveImageTag,y,next->rows);
                  if (status == MagickFalse)
                    break;
                }
            }
            break;
          }
        }
        /*
          Write 40-byte version 3+ bitmap header.
        */
        icon_file.directory[scene].width=(unsigned char) icon_info.width;
        icon_file.directory[scene].height=(unsigned char) icon_info.height;
        icon_file.directory[scene].colors=(unsigned char)
          icon_info.number_colors;
        icon_file.directory[scene].reserved=0;
        icon_file.directory[scene].planes=icon_info.planes;
        icon_file.directory[scene].bits_per_pixel=icon_info.bits_per_pixel;
        icon_file.directory[scene].size=icon_info.size;
        icon_file.directory[scene].offset=(size_t) TellBlob(image);
        (void) WriteBlobLSBLong(image,(unsigned int) 40);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.width);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.height*2);
        (void) WriteBlobLSBShort(image,icon_info.planes);
        (void) WriteBlobLSBShort(image,icon_info.bits_per_pixel);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.compression);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.image_size);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.x_pixels);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.y_pixels);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.number_colors);
        (void) WriteBlobLSBLong(image,(unsigned int) icon_info.colors_important);
        if (next->storage_class == PseudoClass)
          {
            unsigned char
              *icon_colormap;

            /*
              Dump colormap to file.
            */
            icon_colormap=(unsigned char *) AcquireQuantumMemory((size_t)
              (1UL << icon_info.bits_per_pixel),4UL*sizeof(*icon_colormap));
            if (icon_colormap == (unsigned char *) NULL)
              {
                images=DestroyImageList(images);
                ThrowWriterException(ResourceLimitError,
                  "MemoryAllocationFailed");
              }
            q=icon_colormap;
            for (i=0; i < (ssize_t) next->colors; i++)
            {
              *q++=ScaleQuantumToChar(next->colormap[i].blue);
              *q++=ScaleQuantumToChar(next->colormap[i].green);
              *q++=ScaleQuantumToChar(next->colormap[i].red);
              *q++=(unsigned char) 0x0;
            }
            for ( ; i < (ssize_t) (1UL << icon_info.bits_per_pixel); i++)
            {
              *q++=(unsigned char) 0x00;
              *q++=(unsigned char) 0x00;
              *q++=(unsigned char) 0x00;
              *q++=(unsigned char) 0x00;
            }
            (void) WriteBlob(image,(size_t) (4UL*(1UL <<
              icon_info.bits_per_pixel)),icon_colormap);
            icon_colormap=(unsigned char *) RelinquishMagickMemory(
              icon_colormap);
          }
        (void) WriteBlob(image,(size_t) icon_info.image_size,pixels);
        pixels=(unsigned char *) RelinquishMagickMemory(pixels);
        /*
          Write matte mask.
        */
        scanline_pad=(((next->columns+31) & ~31)-next->columns) >> 3;
        for (y=((ssize_t) next->rows - 1); y >= 0; y--)
        {
          p=GetVirtualPixels(next,0,y,next->columns,1,&next->exception);
          if (p == (const PixelPacket *) NULL)
            break;
          bit=0;
          byte=0;
          for (x=0; x < (ssize_t) next->columns; x++)
          {
            byte<<=1;
            if ((next->matte != MagickFalse) &&
                (GetPixelOpacity(p) == (Quantum) TransparentOpacity))
              byte|=0x01;
            bit++;
            if (bit == 8)
              {
                (void) WriteBlobByte(image,(unsigned char) byte);
                bit=0;
                byte=0;
              }
            p++;
          }
          if (bit != 0)
            (void) WriteBlobByte(image,(unsigned char) (byte << (8-bit)));
          for (i=0; i < (ssize_t) scanline_pad; i++)
            (void) WriteBlobByte(image,(unsigned char) 0);
        }
      }
    if (GetNextImageInList(next) == (Image *) NULL)
      break;
    status=SetImageProgress(next,SaveImagesTag,scene++,number_scenes);
    if (status == MagickFalse)
      break;
    next=SyncNextImageInList(next);
  } while ((next != (Image *) NULL) && (image_info->adjoin != MagickFalse));
  offset=SeekBlob(image,0,SEEK_SET);
  (void) offset;
  (void) WriteBlobLSBShort(image,0);
  (void) WriteBlobLSBShort(image,1);
  (void) WriteBlobLSBShort(image,(unsigned short) (scene+1));
  scene=0;
  next=(images != (Image *) NULL) ? images : image;
  do
  {
    (void) WriteBlobByte(image,icon_file.directory[scene].width);
    (void) WriteBlobByte(image,icon_file.directory[scene].height);
    (void) WriteBlobByte(image,icon_file.directory[scene].colors);
    (void) WriteBlobByte(image,icon_file.directory[scene].reserved);
    (void) WriteBlobLSBShort(image,icon_file.directory[scene].planes);
    (void) WriteBlobLSBShort(image,icon_file.directory[scene].bits_per_pixel);
    (void) WriteBlobLSBLong(image,(unsigned int)
      icon_file.directory[scene].size);
    (void) WriteBlobLSBLong(image,(unsigned int)
      icon_file.directory[scene].offset);
    scene++;
    next=SyncNextImageInList(next);
  } while ((next != (Image *) NULL) && (image_info->adjoin != MagickFalse));
  (void) CloseBlob(image);
  images=DestroyImageList(images);
  return(MagickTrue);
}
