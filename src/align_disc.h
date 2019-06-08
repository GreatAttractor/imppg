/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2019 Filip Szczerek <ga.software@yahoo.com>

This file is part of ImPPG.

ImPPG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ImPPG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ImPPG.  If not, see <http://www.gnu.org/licenses/>.

File description:
    Disc detection code header.
*/

#ifndef IMPPG_DISC_HEADER
#define IMPPG_DISC_HEADER

#include <cstdint>
#include <vector>
#include "common.h"
#include "image.h"

typedef struct strPointVal
{
    Point_t point; ///< Pixel coordinates
    uint8_t value; ///< Pixel value

    strPointVal() { }
    strPointVal(int x, int y, uint8_t val) { point.x = x; point.y = y; value = val; }
} PointVal_t;

typedef std::vector<PointVal_t> Ray_t;

/// Difference size used for determining limb crossing; see FindLimbCrossing()
const int DIFF_SIZE = 20;

/// Calculates the centroid of a PIX_MONO8 image
Point_t CalcCentroid(const c_Image& img);

/// Returns subsequent points of a line from 'origin' along direction 'dir' to border of 'img'
void GetRayPoints(const Point_t& origin, const Point_t& dir, const c_Image& img, Ray_t& result);

/// Finds a point (`result`) where `ray` crosses the limb; returns steepness of the transition.
int FindLimbCrossing(Ray_t& ray, uint8_t threshold, Point_t& result);

/// Finds the brightness threshold separating the disc from the background
uint8_t FindDiscBackgroundThreshold(const c_Image& img,
    uint8_t* avgDisc = 0,  ///< If not null, receives average disc brightness
    uint8_t* avgBkgrnd = 0 ///< If not null, receives average background brightness
);

/// Removes elements from 'points' which do not belong to their convex hull
void CullToConvexHull(std::vector<Point_t>& points);

/// Uses Gauss-Newton method to fit a circle to specified points; returns 'true' on success
bool FitCircleToPoints(
    const std::vector<FloatPoint_t>& points,
    float* centerX, ///< If not null, receives circle center's X
    float* centerY, ///< If not null, receives circle center's Y
    float* radius,  ///< If not null and 'forceRadius' is zero, receives radius
    float forceRadius = 0, ///< If not zero, used as a predetermined radius; only the circle's center will be fitted
    /// If 'true', 'centerX' and 'centerY' are used as initial center; otherwise, the centroid of 'points' is used
    bool initialCenterSpecified = false
);

#endif
