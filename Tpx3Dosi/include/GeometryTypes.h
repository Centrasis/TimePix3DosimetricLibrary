#pragma once
#include "MultiplattformTypes.h"
#include <vector>
#include "OpenCLTypes.h"

typedef struct FVector2D
{
	float X;
	float Y;

	FVector2D(float x, float y)
	{
		X = x;
		Y = y;
	};
} FVector2D;

PACK(
typedef struct FVector3D 
{
	float X;
	float Y;
	float Z;

	FVector3D(float x = 0.f, float y = 0.f, float z = 0.f)
	{
		X = x;
		Y = y;
		Z = z;
	}

	void Normalize()
	{
		float len = sqrt(X*X + Y*Y + Z*Z);

		X /= len;
		Y /= len;
		Z /= len;
	}

	static float dot(FVector3D& vec1, FVector3D& vec2)
	{
		return vec1.X * vec2.X + vec1.Y * vec2.Y + vec1.Z * vec2.Z;
	}

	static FVector3D Normalize(FVector3D& vec)
	{
		FVector3D ret = vec;
		ret.Normalize();
		return ret;
	}

	/** Get Angle beetween vec1 and vec2
	@Returns angle in radians */
	static float getAngleBetween(FVector3D& vec1, FVector3D& vec2)
	{
		return acos(vec1.X * vec2.X + vec1.Y * vec2.Y + vec1.Z * vec2.Z);
	}

	FVector3D operator+ (const FVector3D& rhs) const
	{
		return FVector3D(X + rhs.X, Y + rhs.Y, Z + rhs.Z);
	}

	FVector3D& operator+= (const FVector3D& rhs)
	{
		X += rhs.X;
		Y += rhs.Y;
		Z += rhs.Z;
		return *this;
	}

	FVector3D operator- (const FVector3D& rhs) const
	{
		return FVector3D(X - rhs.X, Y - rhs.Y, Z - rhs.Z);
	}

	FVector3D operator* (const float& rhs) const
	{
		return FVector3D(X * rhs, Y * rhs, Z * rhs);
	}

	FVector3D operator/ (const float& rhs) const
	{
		return FVector3D(X / rhs, Y / rhs, Z / rhs);
	}

	FVector3D operator/= (const float& rhs)
	{
		X /= rhs;
		Y /= rhs;
		Z /= rhs;
		return *this;
	}

	FVector3D operator/= (const size_t& rhs)
	{
		X /= rhs;
		Y /= rhs;
		Z /= rhs;
		return *this;
	}

	FVector3D operator/= (const FVector3D& rhs)
	{
		X /= rhs.X;
		Y /= rhs.Y;
		Z /= rhs.Z;
		return *this;
	}

	FVector3D operator-= (const FVector3D& rhs)
	{
		X -= rhs.X;
		Y -= rhs.Y;
		Z -= rhs.Z;
		return *this;
	}

	FVector3D operator/ (const size_t& rhs) const
	{
		return FVector3D(X / (float)rhs, Y / (float)rhs, Z / (float)rhs);
	}
}) FFVector3D;

typedef std::vector<FVector3D> FGeometry;

typedef struct FMaterial
{
	std::string materialName;
	bool bUsedForEnergyCalc;
	double density;
	FMaterial()
	{
		materialName = "None";
		bUsedForEnergyCalc = false;
		density = 0.0;
	}

	FMaterial(std::string name, double density, bool usedForEnergyCalc = true)
	{
		materialName = name;
		this->density = density;
		bUsedForEnergyCalc = usedForEnergyCalc;
	}

	explicit operator bool() const
	{
		return (((int)bUsedForEnergyCalc == 0 || (int)bUsedForEnergyCalc == 1) && density >= 0.0);
	}
} FMaterial;

typedef struct FFilter
{
	std::string name;
	FGeometry geometry;
	FMaterial material;

	FFilter()
	{
		name = "";
		material = FMaterial();
		geometry = FGeometry();
	}

	FFilter(FGeometry& geom, FMaterial mat, std::string name = "NoName")
	{
		this->name = name;
		this->geometry = geom;
		this->material = material;
	}

	explicit operator bool() const
	{
		return (bool)material;
	}
} FFilter;

typedef struct FFilterSetup
{
	PACK(
	typedef struct FDetector
	{
		unsigned int widthIn_mm;
		unsigned int heightIn_mm;
		unsigned int widthIn_px;
		unsigned int heightIn_px;
		double pixelPer_mm[2];

		FDetector()
		{
			widthIn_mm = 0;
			heightIn_mm = 0;
			widthIn_px = 0;
			heightIn_px = 0;
			pixelPer_mm[0] = 0.0;
			pixelPer_mm[1] = 0.0;
		}

		void calcPixelsPer_mm()
		{
			pixelPer_mm[0] = (double)widthIn_px / (double)widthIn_mm;
			pixelPer_mm[1] = (double)heightIn_px / (double)heightIn_mm;
		}

		explicit operator bool() const
		{
			//fast check if values are making sense
			return (pixelPer_mm[0] > 0.0 && pixelPer_mm[1] > 0.0);
		}
	}) FDetector;

	PACK(
	typedef struct CLFilterGeometry
	{
		OCLDynamicTypedBuffer<cl_float3> verticies;
		OCLTypedVariable<FDetector> detector;
	}) CLFilterGeometry;

	std::vector<FFilter> Filters;
	FDetector Detector;

	FFilterSetup()
	{
		Filters = std::vector<FFilter>();
		Detector = FDetector();
	}

	FFilterSetup(FDetector detector, std::vector<FFilter> filters)
	{
		Filters = filters;
		Detector = detector;
	}

	explicit operator bool() const
	{
		if (Filters.size() == 0 || !Detector)
			return false;
		
		for (size_t i = 0; i < Filters.size(); i++)
		{
			if (!Filters[i])
				return false;
		}

		return true;
	}

	std::shared_ptr<CLFilterGeometry> generateCLFilterGeometry()
	{
		std::shared_ptr<CLFilterGeometry> fg = std::make_shared<CLFilterGeometry>();
		fg->detector[0] = Detector;
		fg->verticies.resizeBuffer(Filters.size() * Filters[0].geometry.size());
		size_t i = 0;
		for (size_t j = 0; j < Filters.size(); j++)
			for (size_t v = 0; v < Filters[0].geometry.size(); v++)
			{
				fg->verticies[i].x = Filters[j].geometry[v].X;
				fg->verticies[i].y = Filters[j].geometry[v].Y;
				fg->verticies[i].z = Filters[j].geometry[v].Z;
				i++;
			}

		return fg;
	}

	size_t getFilterIndexByName(std::string name)
	{
		size_t i = 0;
		for (auto c : Filters)
		{
			if (c.name == name)
				return i;
			i++;
		};

		return 0;
	}
} FFilterSetup;

PACK(
typedef struct FShadow
{
	unsigned int radiusX;
	unsigned int radiusY;
	int middle[2];
	float direction; 

	FShadow()
	{

	}

	FShadow(unsigned int radiusX, unsigned int radiusY, int middle[2])
	{
		this->radiusX = radiusX;
		this->radiusY = radiusY;
		this->middle[0] = middle[0];
		this->middle[1] = middle[1];
	}

	explicit operator bool() const
	{
		return (middle[0] >= 0.f && middle[1] >= 0.f);
	}
}) FShadow;

PACK(
typedef struct FShadowSetup
{
	std::vector<FShadow> singleShadows;
	float rotationAngle2D;
	float score;

	FShadowSetup()
	{
		singleShadows = std::vector<FShadow>();
		rotationAngle2D = 0.f;
		score = 0.f;
	}

	~FShadowSetup()
	{
		//delete openCLData;
	}

	explicit operator bool() const
	{
		if (score <= 0.f || abs(rotationAngle2D) > 360.f)
			return false;

		for (size_t i = 0; i < singleShadows.size(); i++)
			if (!singleShadows[0])
				return false;

		return true;
	}
}) FShadowSetup;