#include "OCLGeometryTypes.clh"
#include "OCLShadowTypes.clh"

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

int2 rotatePixelRelative(FShadow* shadow, int2 px)
{
	int2 retVal;
	//move px to origin
	px -= shadow->middle;
	//multiply negative because of the relativity to the shadow
	retVal = (int2)(round(cos(-shadow->direction) * px.x - sin(-shadow->direction) * px.y),
					  round(cos(-shadow->direction) * px.y + sin(-shadow->direction) * px.x));

	//translate back around middle
	return retVal + shadow->middle;
}

//computes if the given pixel is in ellipse
bool ellipseHitTest(int2 test, FShadow* shadow)
{
	test = rotatePixelRelative(shadow, test);
    if (test.x == shadow->middle.x && test.y == shadow->middle.y)
		return true;

	//powr =  x > 0; pow x <> 0
	return ((pown((float)test.x - shadow->middle.x, 2)/pown((float)shadow->radiusX, 2)) + (pown((float)test.y - shadow->middle.y, 2)/pown((float)shadow->radiusY, 2)) <= 1.0);
}

bool coordInSetUpShadow(int2 coord, __global FShadow* shadowSetup, uint count)
{ 
	for(uint i = 0; i < count; i++)
	{
		FShadow s = shadowSetup[i];
		if (ellipseHitTest(coord, &s))
			return true;
	}

	return false;
}

kernel void draw_shadows(__read_only image2d_t integratedImage, __write_only image2d_t coloredDebugOutput, __global FShadow* shadowSetup, uint shadowCount)
{
	int2 coord = (int2)(get_global_id(0), get_global_id(1) - 1);
	if (coordInSetUpShadow(coord, shadowSetup, shadowCount))
	{
		if (read_imageui(integratedImage, sampler, coord).x < 128)
			write_imageui(coloredDebugOutput, coord, (uint4)(128,0,0,255));
		else
			write_imageui(coloredDebugOutput, coord, (uint4)(0,255,0,255));
	}
	else
	{
		write_imageui(coloredDebugOutput, coord, (uint4)(0,0,255,255));
	}
}
