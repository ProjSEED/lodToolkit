#pragma once

#include <osg/Vec3>
#include <osg/Vec3b>

#include "core.h" 

namespace seed
{
	class PointCI
	{
	public:
		PointCI() : P(0,0,0), C(0, 0, 0), I(255) {}

		osg::Vec3 P;
		osg::Vec3b C;
		unsigned char I;
	};
}
