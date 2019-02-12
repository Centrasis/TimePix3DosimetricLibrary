__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;


kernel void gaussianBlur(__read_only image2d_t In, __write_only image2d_t Out, __constant float * mask, int maskSize)
{
	const int2 pos = (int2)(get_global_id(0), get_global_id(1) - 1);

	float sum = 0.0f;
	for (int a = -maskSize; a <= maskSize; a++) {
		for (int b = -maskSize; b <= maskSize; b++) {
			sum += mask[a + maskSize + (b + maskSize)*(maskSize * 2 + 1)]
				* read_imagef(In, sampler, pos + (int2)(a, b)).x;
		}
	}

	//blurredImage[pos.x + pos.y*get_global_size(0)] = sum;
	write_imagef(Out, pos, sum);
}


kernel void copyImg2Img(__read_only image2d_t In, __write_only image2d_t Out)
{
	const int2 pos = (int2)(get_global_id(0), get_global_id(1) - 1);
	write_imagef(Out, pos, read_imagef(In, sampler, pos));
}