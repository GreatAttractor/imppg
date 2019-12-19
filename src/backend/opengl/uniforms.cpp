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
    OpenGL shader uniform names definition.
*/

namespace imppg::backend::uniforms
{
    const char* ScrollPos = "ScrollPos";
    const char* ViewportSize = "ViewportSize";
    const char* ZoomFactor = "ZoomFactor";
    const char* NumPoints = "NumPoints";
    const char* CurvePoints = "CurvePoints";
    const char* Splines = "Splines";
    const char* Image = "Image";
    const char* Smooth = "Smooth";
    const char* IsGamma = "IsGamma";
    const char* Gamma = "Gamma";
    const char* KernelRadius = "KernelRadius";
    const char* GaussianKernel = "GaussianKernel";
    const char* BlurredImage = "BlurredImage";
    const char* SelectionPos = "SelectionPos";
    const char* AmountMin = "AmountMin";
    const char* AmountMax = "AmountMax";
    const char* InputArray1 = "InputArray1";
    const char* InputArray2 = "InputArray2";
    const char* InputImageBlurred = "InputImageBlurred";
    const char* TransitionCurve = "TransitionCurve";
    const char* Adaptive = "Adaptive";
    const char* Threshold = "Threshold";
    const char* Width = "Width";
}
