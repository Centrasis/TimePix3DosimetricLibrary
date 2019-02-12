void kernel simple_add(global const int* A, global const int* B, global int* C){
       C[get_global_id(0)]=A[get_global_id(0)]+B[get_global_id(0)];
}

typedef struct __attribute__((packed)) FTpxPixel
{
	struct __attribute__((packed)) Coord { uchar x, y; } coord;
	uchar fToA;
	ulong ToA;
	ushort ToT;
} FTpxPixel;


void kernel image_test(__global FTpxPixel* buffer, ulong lowAddress, ulong highAddress, __write_only image2d_t outImage)
{
	printf("Work in segment: [%u, %u] using %u per elem\n", lowAddress, highAddress, sizeof(FTpxPixel));
	bool isFirst = (get_global_id(0) == 0);

	ulong segLen = highAddress - lowAddress;

	int width = get_image_width(outImage);
	int height = get_image_height(outImage);

	ulong subSegmentLen = (ulong)(segLen) / get_global_size(0);
	ulong from = lowAddress + (get_global_id(0) * subSegmentLen);
	ulong to = 0;
	if (get_global_id(0) + 1 == get_global_size(0))
	{
		to = highAddress;
	}
	else
	{
		to = lowAddress + ((get_global_id(0) + 1) * subSegmentLen);
	}

	for (ulong i = from; i < to; i++)
	{
		int2 pixelcoord = (int2) (buffer[i].coord.x, buffer[i].coord.y);

		printf("[x:%i,y:%i] ToT:%i   \n", pixelcoord.x, pixelcoord.y, buffer[i].ToT);

		write_imageui(outImage, pixelcoord, 255);
	}
}