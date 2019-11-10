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
    OpenGL shader uniform names declaration.
*/

#ifndef IMPPG_GL_UNIFORMS_H
#define IMPPG_GL_UNIFORMS_H

/// Correspond to uniform identifiers in shaders.
namespace imppg::backend::uniforms
{
    extern const char* ScrollPos;
    extern const char* ViewportSize;
    extern const char* ZoomFactor;
    extern const char* NumPoints;
    extern const char* CurvePoints;
    extern const char* Splines;
    extern const char* Image;
    extern const char* Smooth;
    extern const char* IsGamma;
    extern const char* Gamma;
    extern const char* KernelRadius;
    extern const char* GaussianKernel;
    extern const char* BlurredImage;
    extern const char* SelectionPos;
    extern const char* AmountMin;
    extern const char* AmountMax;
    extern const char* InputArray1;
    extern const char* InputArray2;
}

#endif // IMPPG_GL_UNIFORMS_H
