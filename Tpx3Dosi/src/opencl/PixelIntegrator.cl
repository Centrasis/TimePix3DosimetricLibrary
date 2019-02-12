#include "OCLPixelTypes.clh"

void kernel main_kernel(__global FTpxPixel* buffer, ulong lowAddress, ulong highAddress, ulong BufferLen, __write_only image2d_t outImage)
{
	bool isFirst = (get_global_id(0) == 0);

	bool isOverfow = highAddress < lowAddress;

	
	ulong segLen = 0;
	if (!isOverfow)
	{
		segLen = highAddress - lowAddress;
	}
	else
	{
		segLen = BufferLen - lowAddress + highAddress;
	}

	int width = get_image_width(outImage);
	int height = get_image_height(outImage);

	ulong subSegmentLen = (ulong)(segLen) / get_global_size(0);
	ulong from = (lowAddress + (get_global_id(0) * subSegmentLen)) % BufferLen;

	ulong to = 0;
	if (get_global_id(0) + 1 == get_global_size(0))
	{
		to = highAddress;
	}
	else
	{
		to = (from + subSegmentLen) % BufferLen;
	}

	if (!isOverfow)
	{
		for (ulong i = from; i <= to; i++)
		{
			int2 pixelcoord = (int2) (buffer[i].coord.x, buffer[i].coord.y);

			write_imageui(outImage, pixelcoord, 255);
		}
	}
	else
	{
		if(from <= to)
		{
			for (ulong i = from; i <= to; i++)
			{
				int2 pixelcoord = (int2) (buffer[i].coord.x, buffer[i].coord.y);

				write_imageui(outImage, pixelcoord, 255);
			}
		}
		else
		{
			for (ulong i = from; i < BufferLen; i++)
			{
				int2 pixelcoord = (int2) (buffer[i].coord.x, buffer[i].coord.y);

				write_imageui(outImage, pixelcoord, 255);
			}

			for (ulong i = 0; i <= to; i++)
			{
				int2 pixelcoord = (int2) (buffer[i].coord.x, buffer[i].coord.y);

				write_imageui(outImage, pixelcoord, 255);
			}
		}
	}
}