#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

#include "colors.h"
#include "compressor_funcs.h"

void rotate(UncompressedImage& img, int angle, ColorRGB fill_color={0, 0, 0},
bool smart_gap_interpolation = false);


void applyKernel(
    UncompressedImage& img, const std::vector<std::vector<int>>& kernel, int divisor = 1);

void sharpen(UncompressedImage& img);
void gaussianBlurApprox(UncompressedImage& img, bool hard_blur=false);
void edgeDetect(UncompressedImage& img);

void negative(UncompressedImage& img);
void negative(CompressedImage& img);

void toGrayscale(UncompressedImage& img);
void toGrayscale(CompressedImage& img);

// template methods below

template <typename Image>
void mirror(Image& img, bool horizontal = false) {
    if (horizontal) {
        // Flip left-right: reverse each row
        for (auto& row : img.image_data) {
            std::reverse(row.begin(), row.end());
        }
    } else {
        // Flip top-bottom: reverse the rows
        std::reverse(img.image_data.begin(), img.image_data.end());
    }
}