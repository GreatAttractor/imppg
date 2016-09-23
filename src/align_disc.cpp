/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015, 2016 Filip Szczerek <ga.software@yahoo.com>

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
    Disc detection code.
*/

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <cmath>
#include <cfloat>
#include <set>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/operation.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include "align_disc.h"
#include "gauss.h"


using namespace boost::numeric::ublas;

/// Calculates the centroid of a PIX_MONO8 image
Point_t CalcCentroid(c_Image &img)
{
    // We use 64-bit accumulators which is enough for an 8-bit image with 2^28 x 2^28 pixels.
    assert(img.GetPixelFormat() == PIX_MONO8);

    uint64_t sumX = 0, sumY = 0, sumVals = 0;
    for (unsigned y = 0; y < img.GetHeight(); y++)
        for (unsigned x = 0; x < img.GetWidth(); x++)
        {
            uint8_t pixval = ((uint8_t *)img.GetRow(y))[x];
            sumVals += pixval;
            sumX += (uint64_t)x * pixval;
            sumY += (uint64_t)y * pixval;
        }

    if (sumVals > 0)
        return Point_t(sumX/sumVals, sumY/sumVals);
    else
        return Point_t(0, 0);
}

/// Returns subsequent points of a line from 'origin' along direction 'dir' to border of 'img'
void GetRayPoints(Point_t origin, Point_t dir, c_Image &img, Ray_t &result)
{
    result.clear();

    // Handle vertical and horizontal direction
    if (dir.x == 0)
    {
        int delta = (dir.y > 0 ? 1 : -1);
        for (int y = origin.y; y >= 0 && y < (int)img.GetHeight(); y += delta)
            result.push_back(PointVal_t(origin.x, y, ((uint8_t *)img.GetRow(y))[origin.x]));
    }
    else if (dir.y == 0)
    {
        int delta = (dir.x > 0 ? 1 : -1);

        for (int x = origin.x; x >=0 && x < (int)img.GetWidth(); x += delta)
            result.push_back(PointVal_t(x, origin.y, ((uint8_t *)img.GetRow(origin.y))[x]));
    }
    // Handle slanted lines; high performance it not required, so use the naive algorithm with divisions
    else
    {
        if (std::abs(dir.x) > std::abs(dir.y))
        {
            int delta = (dir.x > 0 ? 1 : -1);
            int x = origin.x;
            int y = origin.y;
            while (true)
            {
                y = origin.y + dir.y * (x - origin.x) / dir.x;
                if (x < 0 || x >= (int)img.GetWidth() || y < 0 || y >= (int)img.GetHeight())
                    break;
                result.push_back(PointVal_t(x, y, ((uint8_t*)img.GetRow(y))[x]));
                x += delta;
            }
        }
        else
        {
            int delta = (dir.y > 0 ? 1 : -1);
            int x = origin.x;
            int y = origin.y;
            while (true)
            {
                x = origin.x + dir.x * (y - origin.y) / dir.y;
                if (x < 0 || x >= (int)img.GetWidth() || y < 0 || y >= (int)img.GetHeight())
                    break;
                result.push_back(PointVal_t(x, y, ((uint8_t*)img.GetRow(y))[x]));
                y += delta;
            }
        }
    }
}

/// Removes elements from 'points' which do not belong to their convex hull (found by "gift wrapping")
void CullToConvexHull(std::vector<Point_t> &points)
{
    // We use less than 100 limb points, so the gift-wrapping algorithm's speed is sufficient

    if (points.size() <= 3)
        return;

    unsigned minXidx = 0;
    int minX = -1000000;
    for (unsigned i = 0; i < points.size(); i++)
        if (points[i].x < minX)
        {
            minX = points[i].x;
            minXidx = i;
        }

    std::set<unsigned> convexHull;
    convexHull.insert(minXidx);

    Point_t lastHullPt = points[minXidx];
    Point_t wrapDir(0, 1);
    float wrapDirLen = 1.0f;
    bool nextConvHullPtFound = false;
    do
    {
        nextConvHullPtFound = false;

        // Choose the point which gives the vector with the smallest angle (highest cosine)
        // between it and the current wrapping direction.
        float maxCosine = -100.0f;
        Point_t nextVec;
        float nextVecLen = 0.0f;
        unsigned nextPtIdx;
        for (unsigned i = 0; i < points.size(); i++)
        {
            Point_t vec(points[i].x - lastHullPt.x, points[i].y - lastHullPt.y);
            float vecLen = std::sqrt(SQR(vec.x) + SQR(vec.y));
            if (vecLen == 0.0f)
            {
                continue;
            }

            float cosine = (wrapDir.x*vec.x + wrapDir.y*vec.y) / (wrapDirLen * vecLen);
            if (cosine > maxCosine)
            {
                maxCosine = cosine;
                nextConvHullPtFound = true;
                nextPtIdx = i;
                nextVec = vec;
                nextVecLen = vecLen;
            }
        }

        if (nextConvHullPtFound)
        {
            if (convexHull.find(nextPtIdx) != convexHull.end())
                break; // we found the hull's first point, work done

            wrapDir = nextVec;
            wrapDirLen = nextVecLen;
            convexHull.insert(nextPtIdx);
            lastHullPt = points[nextPtIdx];
        }

    } while (nextConvHullPtFound);

    std::vector<Point_t> allPoints = points;
    points.clear();
    for (std::set<unsigned>::const_iterator it = convexHull.begin(); it != convexHull.end(); it++)
        points.push_back(allPoints[*it]);
}

//TODO: add header comment
float GetSumSqrDiffsFromHistogram(
        uint32_t histogram[], ///< 256 elements
        size_t iMin, size_t iMax)
{
    float avg = 0.0f;
    int numPix = 0;
    for (size_t i = iMin; i <= iMax; i++)
    {
        avg += histogram[i] * i;
        numPix += histogram[i];
    }
    avg /= numPix;

    float sumSqrDiffs = 0.0f;
    for (size_t i = iMin; i <= iMax; i++)
        sumSqrDiffs += histogram[i] * SQR(i - avg);

    return sumSqrDiffs;
}

/// Finds the brightness threshold separating the disc from the background
uint8_t FindDiscBackgroundThreshold(c_Image &img,
    uint8_t *avgDisc,  ///< If not null, receives average disc brightness
    uint8_t *avgBkgrnd ///< If not null, receives average background brightness
)
{
    assert(img.GetPixelFormat() == PIX_MONO8);

    // Determine the histogram
    uint32_t histogram[256] = { 0 };
    for (unsigned i = 0; i < img.GetHeight(); i++)
        for (unsigned j = 0; j < img.GetWidth(); j++)
            histogram[((uint8_t *)img.GetRow(i))[j]] += 1;

    // Use bisection to find the value 'currDivPos' in histogram which has
    // the lowest sum of squared pixel value differences from the average
    // for all pixels darker and brighter than 'currDivPos'.
    //
    // This identifies the most significant "dip" in the histogram
    // which separates two groups of "similar" values: those belonging
    // to the disc and to the background.

    unsigned iLow = 0, iHigh = 255;
    unsigned currDivPos = (iHigh - iLow) / 2;

    while (iHigh - iLow > 1)
    {
        unsigned divPosLeft = (iLow + currDivPos) / 2, divPosRight = (iHigh
                + currDivPos) / 2;

        float varSumLeft = GetSumSqrDiffsFromHistogram(histogram, 0, divPosLeft)
                + GetSumSqrDiffsFromHistogram(histogram, divPosLeft, 255);

        float varSumRight = GetSumSqrDiffsFromHistogram(histogram, 0, divPosRight)
                + GetSumSqrDiffsFromHistogram(histogram, divPosRight, 255);

        if (varSumLeft < varSumRight)
        {
            iHigh = currDivPos;
            currDivPos = divPosLeft;
        }
        else
        {
            iLow = currDivPos;
            currDivPos = divPosRight;
        }
    }

    // determine average brightness of pixels below and above the threshold
    unsigned totalDisc = 0, discCount = 0,
             totalBkgrnd = 0, bkgrndCount = 0;
    for (unsigned i = 0; i < currDivPos; i++)
    {
        totalBkgrnd += i*histogram[i];
        bkgrndCount += histogram[i];
    }
    for (unsigned i = currDivPos; i < 256; i++)
    {
        totalDisc += i*histogram[i];
        discCount += histogram[i];
    }

    if (avgDisc)
        *avgDisc = totalDisc/discCount;
    if (avgBkgrnd)
        *avgBkgrnd = totalBkgrnd/bkgrndCount;

    return (uint8_t) currDivPos;
}

/// Finds a point ('result') where 'ray' crosses the limb; returns steepness of the transition
int FindLimbCrossing(Ray_t &ray, c_Image &img, uint8_t threshold, Point_t &result)
{
    // If the image was wavelet-sharpened, there may be a bright border left. To avoid
    // its influence, replace the first NUM_BORDER_AVG pixels on each end of the ray
    // by their average.
    const int NUM_BORDER_AVG = 16;
    int avgStart = 0, avgEnd = 0;

    for (int i = 0; i < NUM_BORDER_AVG && i < (int)ray.size(); i++)
        avgStart += ray[i].value;
    for (int i = (int)ray.size() - 1; i >= (int)ray.size() - NUM_BORDER_AVG && i >= 0; i--)
        avgEnd += ray[i].value;

    avgStart /= NUM_BORDER_AVG;
    avgEnd /= NUM_BORDER_AVG;

    for (int i = 0; i < NUM_BORDER_AVG; i++)
    {
        if (i < (int)ray.size())
            ray[i].value = avgStart;

        if ((int)ray.size() - i - 1 >= 0)
            ray[ray.size() - i - 1].value = avgEnd;
    }

    // The rays are tracked from the image's centroid (which lies inside the solar disc)
    // outwards toward the background. This is why we start our scanning from the ray's
    // last point (that is, after skipping SKIP_BORDER points) toward the first element.

    const int SKIP_BORDER = 6;
    if (ray.size() <= SKIP_BORDER)
        return 0;

    int currPos = ray.size() - SKIP_BORDER;

    // Find the first pixel >= threshold
    while (ray[currPos].value < threshold && currPos >=0)
        currPos -= 1;

    /*
      It is possible that the background threshold found by FindDiscBackgroundThreshold()
      is slightly too low, causing the 'currPos' to point at e.g. extended bright halo
      around the disc or a bright part of prominence. Let us move 'currPos' a bit more
      towards the inside of the disc (ray's first point) before scanning for the actual
      disc-background transition.
    */

    const int BACK_OFFSET = 20; //TODO: make it adaptive
    currPos -= std::max(BACK_OFFSET, currPos/10);
    if (currPos < 0)
        currPos = 0;

    // Move towards the ray's end and look for the highest absolute difference
    // of DIFF_SIZE sum of pixel values around 'currPos'.

    int iMaxDiff = 0;
    int maxDiff = 0;
    for (int i = currPos; i < (int)ray.size(); i++)
    {
        int sumLo = 0;
        for (int j = -DIFF_SIZE; j <= 0; j++)
            if (i+j >=0)
                sumLo += ray[i+j].value;
            else
                sumLo += ray[0].value;

        int sumHi = 0;
        for (int j = 0; j < DIFF_SIZE; j++)
            if (i+j < (int)ray.size())
                sumHi += ray[i+j].value;
            else
                sumHi += ray.back().value;

        int diff = std::abs(sumHi - sumLo);
        if (diff > maxDiff)
        {
            maxDiff = diff;
            iMaxDiff = i;
        }
    }

    result = ray[iMaxDiff].point;
    return maxDiff;
}

void Invert3x3(matrix<double> &M)
{
    matrix<double> Minv(3, 3);

    double detM = +M(0, 0)*M(1, 1)*M(2, 2)
        +M(1, 0)*M(2, 1)*M(0, 2)
        +M(2, 0)*M(0, 1)*M(1, 2)
        -M(0, 2)*M(1, 1)*M(2, 0)
        -M(1, 2)*M(2, 1)*M(0, 0)
        -M(2, 2)*M(0, 1)*M(1, 0);

    Minv(0, 0) = M(1, 1)*M(2, 2)-M(1, 2)*M(2, 1);
    Minv(0, 1) = M(0, 2)*M(2, 1)-M(0, 1)*M(2, 2);
    Minv(0, 2) = M(0, 1)*M(1, 2)-M(0, 2)*M(1, 1);

    Minv(1, 0) = M(1, 2)*M(2, 0)-M(1, 0)*M(2, 2);
    Minv(1, 1) = M(0, 0)*M(2, 2)-M(0, 2)*M(2, 0);
    Minv(1, 2) = M(0, 2)*M(1, 0)-M(0, 0)*M(1, 2);

    Minv(2, 0) = M(1, 0)*M(2, 1)-M(1, 1)*M(2, 0);
    Minv(2, 1) = M(0, 1)*M(2, 0)-M(0, 0)*M(2, 1);
    Minv(2, 2) = M(0, 0)*M(1, 1)-M(0, 1)*M(1, 0);

    Minv /= detM;

    M = Minv;
}

void Invert2x2(matrix<double> &M)
{
    matrix<double> Minv(2, 2);
    Minv(0, 0) = M(1, 1);
    Minv(0, 1) = -M(0, 1);
    Minv(1, 0) = -M(1, 0);
    Minv(1, 1) = M(0, 0);
    Minv /= M(0, 0) * M(1, 1) - M(0, 1) * M(1, 0);

    M = Minv;
}

/// Uses Gauss-Newton method to fit a circle to specified points; returns 'true' on success
bool FitCircleToPoints(
    std::vector<FloatPoint_t> &points,
    float *centerX, ///< If not null, receives circle center's X
    float *centerY, ///< If not null, receives circle center's Y
    float *radius,  ///< If not null and 'forceRadius' is zero, receives radius
    float forceRadius, ///< If not zero, used as a predetermined radius; only the circle's center will be fitted
    /// If 'true', 'centerX' and 'centerY' are used as initial center; otherwise, the centroid of 'points' is used
    bool initialCenterSpecified
    )
{
    const int ITERATIONS = 8;

    unsigned i;

    // The current approximation
    matrix<double> B = (forceRadius == 0.0) ?
        matrix<double>(3, 1) : // cx, cy, radius
        matrix<double>(2, 1);  // cx, cy


    // Vector of residuals; i-th row a residual corresponding with points[i]
    matrix<double> Rs(points.size(), 1);

    // Jacobian; i-th row contains 3 partial derivatives of the i-th residual with respect to 'cx', 'cy' and 'radius' (if we are fitting radius)
    matrix<double> J = (forceRadius == 0.0) ?
        matrix<double>(points.size(), 3) :
        matrix<double>(points.size(), 2);

    // Set the initial approximation of circle's center to the average of all points
    // and circle's radius to the average of their set's axis-aligned bounding box X and Y extents.

    B(0, 0) = 0.0;
    B(1, 0) = 0.0;
    int xmin = INT_MAX, xmax = INT_MIN, ymin = INT_MAX, ymax = INT_MIN;
    for (i = 0; i < points.size(); i++)
    {
        FloatPoint_t &p = points[i];
        B(0, 0) += p.x;
        B(1, 0) += p.y;

        if (p.x < xmin)
            xmin = p.x;
        if (p.x > xmax)
            xmax = p.x;
        if (p.y < ymin)
            ymin = p.y;
        if (p.y > ymax)
            ymax = p.y;
    }
    if (!initialCenterSpecified)
    {
        B(0, 0) /= points.size();
        B(1, 0) /= points.size();
    }
    else
    {
        B(0, 0) = *centerX;
        B(1, 0) = *centerY;
    }

    if (forceRadius == 0.0)
        B(2, 0) = ((xmax-xmin)/2 + (ymax-ymin)/2) / 2;

    // Perform Gauss-Newton iterations
    for (int iter = 0; iter < ITERATIONS; iter++)
    {
        // Calculate residuals and Jacobian for the current approximation
        for (i = 0; i < points.size(); i++)
        {
            /*
            Functions (residuals) 'r_i' (which we seek to minimize) are the differences between the radius
            and the i-th point distance from the circle center:

            r_i(cx, cy, r) = sqrt((x_i - cx)^2 + (y_i - cy)^2) - r

            The Jacobian matrix entries are defined as:

            J(i, j) = partial derivative of r_i(cx, cy, r) with respect to j-th parameter
            (where parameters cx, cy, r are numbered 0, 1, 2)

            which gives:

            J(i, 0) = (cx - x_i) / sqrt((x_i - cx)^2 + (y_i - cy)^2)
            J(i, 1) = (cy - y_i) / sqrt((x_i - cx)^2 + (y_i - cy)^2)
            J(i, 2) = -1          (only if we are also fitting the radius)
            */

            double pdist = sqrt(SQR(B(0, 0) - points[i].x) + SQR(B(1, 0) - points[i].y));

            if (forceRadius == 0.0)
                Rs(i, 0) = pdist - B(2, 0);
            else
                Rs(i, 0) = pdist - forceRadius;

            J(i, 0) = (B(0, 0) - points[i].x) / pdist;
            J(i, 1) = (B(1, 0) - points[i].y) / pdist;
            if (forceRadius == 0.0)
                J(i, 2) = -1.0;
        }

        // Calculate new approximation

        matrix<double> Jt = trans(J);

        matrix<double> JtJinv = prod(Jt, J);
        if (forceRadius == 0.0)
            Invert3x3(JtJinv);
        else
            Invert2x2(JtJinv);

        matrix<double> JtRs = prod(Jt, Rs);

        B -= prod(JtJinv, JtRs);
    }

    if (!boost::math::isfinite(B(0, 0)) ||
        !boost::math::isfinite(B(1, 0)) ||
        forceRadius == 0.0 && (!boost::math::isfinite(B(2, 0)) || B(2, 0) <= 0.0f))
    {
        return false;
    }
    else
    {
        if (centerX)
            *centerX = B(0, 0);
        if (centerY)
            *centerY = B(1, 0);
        if (forceRadius == 0.0 && radius)
            *radius = B(2, 0);

        return true;
    }
}
