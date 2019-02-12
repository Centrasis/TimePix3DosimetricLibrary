#include "OCLGeometryTypes.clh"
#include "OCLShadowTypes.clh"
#include "Matrix.clh"

int2 rotatePointAroundOrigin(float angle, int2 px)
{
	int2 retVal;
	retVal = (int2)(round(cos(angle) * px.x - sin(angle) * px.y),
		round(cos(angle) * px.y + sin(angle) * px.x));

	return retVal;
}


//orthogonal projection of each vertex onto detector using a orthogonal projection matrix
void kernel calcShadows(__global float3* geom, __global FDetector* detector, __global FShadow* outShadows, float2 angle)
{
	__local float3 dir;
	uint id = get_global_id(0);
	uint size = get_local_size(1);
	uint vertexPos = id * size + get_local_id(1);
	__local float3 projectionV[%VERTEX_COUNT%];

	dir = (float3)(0.f, sin(angle.x), -1.f * cos(angle.x));

	float3 vertex = geom[vertexPos];
	float alpha = vertex.z/dir.z;
	projectionV[get_local_id(1)] = vertex - alpha * dir;

	barrier(CLK_LOCAL_MEM_FENCE);

	if (get_local_id(1) == 0)
	{
		float maxX;
		float minX;
		float maxY;
		float minY;

		maxX = -1.f*FLT_MAX;
		minX = FLT_MAX;
		maxY = -1.f*FLT_MAX;
		minY = FLT_MAX;

		for (uint i = 0; i < size; i++)
		{
			if (minX > projectionV[i].x)
				minX = projectionV[i].x;
			if (maxX < projectionV[i].x)
				maxX = projectionV[i].x;
			if (minY > projectionV[i].y)
				minY = projectionV[i].y;
			if (maxY < projectionV[i].y)
				maxY = projectionV[i].y;
		}

		FShadow SOut;
		SOut.radiusX = ((maxX - minX) / 2.f) * detector[0].pixelPer_mm.x;
		SOut.radiusY = ((maxY - minY) / 2.f)* detector[0].pixelPer_mm.y;
		/*SOut.middle = (int2)((int)(((maxX[0] - minX[0]) / 2.f) + minX[0]) * detector[0].pixelPer_mm.x, (int)((((maxY[0] - minY[0]) / 2.f) + minY[0]) * detector[0].pixelPer_mm.y));
		SOut.middle += (int2)(0, (int)((((maxX[1] - minX[1]) / 2.f) + minX[1]) * detector[0].pixelPer_mm.y));*/

		SOut.middle = (int2)((int)(((maxX - minX) / 2.f) + minX) * detector[0].pixelPer_mm.x, (int)((((maxY - minY) / 2.f) + minY) * detector[0].pixelPer_mm.y));
		SOut.middle = rotatePointAroundOrigin(angle.y, SOut.middle);
		SOut.middle += (int2)(detector[0].widthIn_px/2.f, detector[0].heightIn_px/2.f);
		SOut.direction = angle.y;

		outShadows[id] = SOut;
	}
}
