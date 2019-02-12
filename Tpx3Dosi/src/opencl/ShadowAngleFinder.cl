//#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

#include "shadowProjection.cl"
#include "OCLRandom.clh"

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
__constant int maxAmountOfAngleSteps = 100;

//Box-Mueller Trafo; normed til radius of 2
float2 getGaussianCoord(float2 normedRandoms)
{
    float2 retVal = (float2)(sqrt((-2) * log(normedRandoms.x)) * cos(2*M_PI*normedRandoms.y), sqrt((-2) * log(normedRandoms.x)) * sin(2*M_PI*normedRandoms.y));
	//[norm by 2]
	retVal /= 2;

    return retVal;
}

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
	return (pown((float)test.x - shadow->middle.x, 2) * pown((float)shadow->radiusY, 2) + pown((float)test.y - shadow->middle.y, 2) * pown((float)shadow->radiusX, 2) <= pown((float)shadow->radiusX, 2) * pown((float)shadow->radiusY, 2));
}

bool ellipseOverlapping(FShadow* ActiveTestedShadow, FShadow* PassiveShadow)
{
	int maxRadius = maxInt((int)ActiveTestedShadow->radiusX, (int)ActiveTestedShadow->radiusY);
	int minX = maxInt((int)ActiveTestedShadow->middle.x - maxRadius, (int)0);
	int minY = maxInt((int)ActiveTestedShadow->middle.y - maxRadius, (int)0);
    for(int x = minX; x < ActiveTestedShadow->middle.x + maxRadius; x++)
	{		
	    for(int y = minY; y < ActiveTestedShadow->middle.y + maxRadius; y++)
		{
			if (ellipseHitTest((int2)(x, y), ActiveTestedShadow) && ellipseHitTest((int2)(x, y), PassiveShadow))
				return true;
		}
	}

	return false;
}

int2 getMatchingGaussianPixel(pcg32_random_t* rng, __local FGaussianShadow* shadow, __local FImageInfo* imgInfo)
{
   float2 retPixel = (float2)(0.f,0.f);
  do
   {
	  float normedRand1 = pcg32_boundedrand_r(rng, 6000) / 6000.f;
	  float normedRand2 = pcg32_boundedrand_r(rng, 6000) / 6000.f;
      retPixel = getGaussianCoord((float2)(normedRand1, normedRand2));

	  /*float xPerc = ((float)pcg32_boundedrand_r(rng, imgInfo->width) - (imgInfo->width / 2)) / ((float)imgInfo->width / 2);
	  float yPerc = ((float)pcg32_boundedrand_r(rng, imgInfo->height) - (imgInfo->height / 2)) / ((float)imgInfo->height / 2);
	  retPixel = (float2)(xPerc, yPerc);*/


	  retPixel *= (float2)(shadow->xFactor * imgInfo->width / 2.f, shadow->yFactor * imgInfo->height / 2.f);//stretch over whole image weighted
	  retPixel += (float2)(shadow->mainShadow.middle.x, shadow->mainShadow.middle.y);//middle of gaussian at middle of shadow
	  //check if retPixel is normed onto [-1,1]
   } while(retPixel.x < 0.f || retPixel.y < 0.f || retPixel.x >= imgInfo->width || retPixel.y >= imgInfo->width);

   return (int2)((int)retPixel.x % imgInfo->width, (int)retPixel.y % imgInfo->height);
}

float calcMatchingScore(__read_only image2d_t image, FShadow* shadow)
{
	//float A_Ellipse = M_PI * shadow->radiusX * shadow->radiusY;
	int notHittedPixels = 0;
	int pixelsInEllipse = 0;

	int maxRad = maxInt(shadow->radiusY, shadow->radiusX);
	int minX = maxInt((int)shadow->middle.x - maxRad, (int)0);
	int w = get_image_width(image);
	int h = get_image_height(image);
	
	for(int x = minX; x < shadow->middle.x + maxRad; x++)
	{
		int minY = maxInt((int)shadow->middle.y - maxRad, 0);
	    for(int y = minY; y < shadow->middle.y + maxRad; y++)
		{
			if (ellipseHitTest((int2)(x, y), shadow))
			{
				 pixelsInEllipse++;
				 if (y <= h && x <= w && read_imageui(image, sampler, (int2)(x, y)).x == 0)
					notHittedPixels++;
			}
		}
	}

	//return quotient not hitted pixels : max hittable pixels
	//return ((float)notHittedPixels) / A_Ellipse;
	return ((float)notHittedPixels) / (float)pixelsInEllipse;
}

bool isOverlapping(FShadow* shadow, __global FShadow * shadowArray)
{
	if (get_global_id(0) == 0)
		return false;

    float myarea = shadow->radiusX * shadow->radiusY;
	for(int i = 0; i < get_global_id(0); i++)
	{
	   FShadow s = shadowArray[i];
	   float otherArea = s.radiusX * s.radiusY;
	   if(myarea > otherArea)
	   { 
		  
	      if (ellipseOverlapping(&s, shadow))
			return true;
	   }
	   else
	   {
	      if (ellipseOverlapping(shadow, &s))
			return true;
	   }
	}

	return false;
}

/** calc the shadow ratio with width * height kernels */
void kernel calcOverallShadowRatio(__read_only image2d_t image, __global unsigned int* shadowPixelCount)
{
   	if(read_imageui(image, sampler, (int2)(get_global_id(0), get_global_id(1) - 1)).x < 50)
		atomic_inc(shadowPixelCount);
}

bool coordInSetUpShadow(int2 coord, __global FShadow* shadowSetup, uint ShadowCount)
{ 
	for(uint i = 0; i < ShadowCount; i++)
	{
		FShadow s = shadowSetup[i];
		if (ellipseHitTest(coord, &s))
			return true;
	}

	return false;
}

void kernel calcRatioOfFilterSetup(__read_only image2d_t image, __global FShadow* shadowSetup, uint ShadowCount, __global uint* pixelInSetupCount, __global uint* shadowedPixelInSetupCount, __global float* overallShadowRatio, __global uint* theoreticalArea, __global uint* passedItems)
{ 
	if(get_global_id(0) == 0 && get_global_id(1) == 1)
	{
		for (uint i = 0; i < ShadowCount; i++)
		{
			
			FShadow s = shadowSetup[i];
			*theoreticalArea += round(s.radiusX * s.radiusY * M_PI);
			/*if(!(s.middle.x - s.radiusX >= 0 && s.middle.x + s.radiusX <= get_image_width(image) && s.middle.y - s.radiusY >= 0 && s.middle.y + s.radiusY <= get_image_height(image)))
			{
				float len = fmin(length(s.middle), length((int2)(get_image_width(image), get_image_height(image)) - s.middle));
				if (len > )
				*shadowedPixelInSetupCount += round(s.radiusX * s.radiusY * M_PI * lOverallShadowRatio);
			}*/
		}
		//*pixelInSetupCount = localCount;
	}

	const int2 coord = (int2)(get_global_id(0), get_global_id(1) - 1);

	if (coordInSetUpShadow(coord, shadowSetup, ShadowCount))
	{
		atomic_inc(pixelInSetupCount);

		if (read_imageui(image, sampler, coord).x < 50)
			atomic_inc(shadowedPixelInSetupCount);
	}

	atomic_inc(passedItems);
	if(*passedItems >= get_global_size(0) * get_global_size(1))
	{
		float lOverallShadowRatio = *overallShadowRatio;
		if(*theoreticalArea > *pixelInSetupCount)
		{
			uint AreaDiff = *theoreticalArea - *pixelInSetupCount;
			*shadowedPixelInSetupCount += (uint)(AreaDiff * lOverallShadowRatio);
			*pixelInSetupCount = *theoreticalArea;
		}
	}	
}