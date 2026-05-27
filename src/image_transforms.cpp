#include "image_transforms.h"
#include "error_handlers.h"
#include <cmath>
#include <algorithm>

void fillGapPixels(UncompressedImage& img, std::vector<std::vector<bool>>& is_gap_pixel) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < img.height; ++i) {
            for (size_t j = 0; j < img.width; ++j) {
                if (!is_gap_pixel[i][j]) continue;
                int r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
                int di[] = {-1, 0, 1, 0};
                int dj[] = {0, -1, 0, 1};
                for (int d = 0; d < 4; ++d) {
                    int ni = static_cast<int>(i) + di[d];
                    int nj = static_cast<int>(j) + dj[d];
                    if (ni >= 0 && ni < static_cast<int>(img.height) &&
                        nj >= 0 && nj < static_cast<int>(img.width) &&
                        !is_gap_pixel[ni][nj]) {
                        r_sum += img.image_data[ni][nj].r;
                        g_sum += img.image_data[ni][nj].g;
                        b_sum += img.image_data[ni][nj].b;
                        ++count;
                    }
                }
                if (count > 0) {
                    img.image_data[i][j] = {
                        static_cast<uint8_t>(r_sum / count),
                        static_cast<uint8_t>(g_sum / count),
                        static_cast<uint8_t>(b_sum / count)
                    };
                    is_gap_pixel[i][j] = false;
                    changed = true;
                }
            }
        }
    }
}

void rotate(UncompressedImage& img, int angle, ColorRGB fill_color, bool smart_gap_interpolation) {
    angle = ((angle % 360) + 360) % 360;

    double rad = angle * M_PI / 180.0;
    double cos_a = std::cos(rad);
    double sin_a = std::sin(rad);

    double cx = img.width / 2.0;
    double cy = img.height / 2.0;

    double corners_x[] = {0, static_cast<double>(img.width), static_cast<double>(img.width), 0};
    double corners_y[] = {0, 0, static_cast<double>(img.height), static_cast<double>(img.height)};

    double min_x = 1e18, max_x = -1e18, min_y = 1e18, max_y = -1e18;
    for (int i = 0; i < 4; ++i) {
        double x = corners_x[i] - cx;
        double y = corners_y[i] - cy;
        double rx = cos_a * x - sin_a * y;
        double ry = sin_a * x + cos_a * y;
        min_x = std::min(min_x, rx);
        max_x = std::max(max_x, rx);
        min_y = std::min(min_y, ry);
        max_y = std::max(max_y, ry);
    }

    uint32_t new_width = static_cast<uint32_t>(std::round(max_x - min_x));
    uint32_t new_height = static_cast<uint32_t>(std::round(max_y - min_y));

    double new_cx = new_width / 2.0;
    double new_cy = new_height / 2.0;

    std::vector<std::vector<ColorRGB>> new_data(new_height, std::vector<ColorRGB>(new_width, fill_color));
    std::vector<std::vector<bool>> is_gap(new_height, std::vector<bool>(new_width, true));

    double inv_cos = cos_a;   
    double inv_sin = -sin_a;  

    for (size_t ny = 0; ny < new_height; ++ny) {
        for (size_t nx = 0; nx < new_width; ++nx) {
            double x = static_cast<double>(nx) - new_cx;
            double y = static_cast<double>(ny) - new_cy;
            double ox = inv_cos * x - inv_sin * y + cx;
            double oy = inv_sin * x + inv_cos * y + cy;

            if (ox >= 0 && ox < img.width && oy >= 0 && oy < img.height) {
                if (!smart_gap_interpolation) {
                    int px = static_cast<int>(std::round(ox));
                    int py = static_cast<int>(std::round(oy));
                    px = std::clamp(px, 0, static_cast<int>(img.width) - 1);
                    py = std::clamp(py, 0, static_cast<int>(img.height) - 1);
                    new_data[ny][nx] = img.image_data[py][px];
                } else {
                    int x0 = static_cast<int>(std::floor(ox));
                    int y0 = static_cast<int>(std::floor(oy));
                    int x1 = x0 + 1;
                    int y1 = y0 + 1;
                    x0 = std::clamp(x0, 0, static_cast<int>(img.width) - 1);
                    y0 = std::clamp(y0, 0, static_cast<int>(img.height) - 1);
                    x1 = std::clamp(x1, 0, static_cast<int>(img.width) - 1);
                    y1 = std::clamp(y1, 0, static_cast<int>(img.height) - 1);

                    double fx = ox - std::floor(ox);
                    double fy = oy - std::floor(oy);

                    auto lerp_color = [&](ColorRGB c00, ColorRGB c10, ColorRGB c01, ColorRGB c11) {
                        double r = c00.r * (1-fx)*(1-fy) + c10.r * fx*(1-fy) + c01.r * (1-fx)*fy + c11.r * fx*fy;
                        double g = c00.g * (1-fx)*(1-fy) + c10.g * fx*(1-fy) + c01.g * (1-fx)*fy + c11.g * fx*fy;
                        double b = c00.b * (1-fx)*(1-fy) + c10.b * fx*(1-fy) + c01.b * (1-fx)*fy + c11.b * fx*fy;
                        return ColorRGB{
                            static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(r)), 0, 255)),
                            static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(g)), 0, 255)),
                            static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(b)), 0, 255))
                        };
                    };

                    new_data[ny][nx] = lerp_color(
                        img.image_data[y0][x0], img.image_data[y0][x1],
                        img.image_data[y1][x0], img.image_data[y1][x1]);
                }
                is_gap[ny][nx] = false;
            }
        }
    }

    if (smart_gap_interpolation) {
        UncompressedImage tmp_img;
        tmp_img.width = new_width;
        tmp_img.height = new_height;
        tmp_img.is_grayscale = img.is_grayscale;
        tmp_img.image_data = new_data;
        fillGapPixels(tmp_img, is_gap);
        new_data = tmp_img.image_data;
    }

    img.width = new_width;
    img.height = new_height;
    img.image_data = new_data;
}

void applyKernel(UncompressedImage& img, const std::vector<std::vector<int>>& kernel, int divisor) {
    int kh = static_cast<int>(kernel.size());
    int kw = static_cast<int>(kernel[0].size());
    int kcy = kh / 2;
    int kcx = kw / 2;

    std::vector<std::vector<ColorRGB>> new_data(img.height, std::vector<ColorRGB>(img.width));

    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            int r_sum = 0, g_sum = 0, b_sum = 0;
            for (int ky = 0; ky < kh; ++ky) {
                for (int kx = 0; kx < kw; ++kx) {
                    int sy = static_cast<int>(y) + ky - kcy;
                    int sx = static_cast<int>(x) + kx - kcx;
                    sy = std::clamp(sy, 0, static_cast<int>(img.height) - 1);
                    sx = std::clamp(sx, 0, static_cast<int>(img.width) - 1);
                    r_sum += kernel[ky][kx] * img.image_data[sy][sx].r;
                    g_sum += kernel[ky][kx] * img.image_data[sy][sx].g;
                    b_sum += kernel[ky][kx] * img.image_data[sy][sx].b;
                }
            }
            int div = (divisor == 1) ? 1 : divisor;
            new_data[y][x] = {
                static_cast<uint8_t>(std::clamp(r_sum / div, 0, 255)),
                static_cast<uint8_t>(std::clamp(g_sum / div, 0, 255)),
                static_cast<uint8_t>(std::clamp(b_sum / div, 0, 255))
            };
        }
    }
    img.image_data = new_data;
}

void sharpen(UncompressedImage& img) {
    const std::vector<std::vector<int>> kernel = {
        {0, -1, 0},
        {-1, 5, -1},
        {0, -1, 0}
    };
    applyKernel(img, kernel, 1);
}

void gaussianBlurApprox(UncompressedImage& img, bool hard_blur) {
    if (!hard_blur) {
        const std::vector<std::vector<int>> kernel = {
            {1, 2, 1},
            {2, 4, 2},
            {1, 2, 1}
        };
        applyKernel(img, kernel, 16);
    } else {
        const std::vector<std::vector<int>> kernel = {
            {1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1}
        };
        applyKernel(img, kernel, 25);
    }
}

void edgeDetect(UncompressedImage& img) {
    const std::vector<std::vector<int>> kernel = {
        {-1, -1, -1},
        {-1,  8, -1},
        {-1, -1, -1}
    };
    applyKernel(img, kernel, 1);
}

void negative(UncompressedImage& img) {
    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            ColorRGB& c = img.image_data[y][x];
            c.r = 255 - c.r;
            c.g = 255 - c.g;
            c.b = 255 - c.b;
        }
    }
}

void negative(CompressedImage& img) {
    for (auto& [id, c] : img.id_to_color) {
        c.r = 255 - c.r;
        c.g = 255 - c.g;
        c.b = 255 - c.b;
    }
    img.color_to_id.clear();
    for (auto& [id, c] : img.id_to_color) {
        img.color_to_id[c] = id;
    }
}

void toGrayscale(UncompressedImage& img) {
    if (img.is_grayscale) return;
    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            uint8_t gray = colorToGrayscale(img.image_data[y][x]);
            img.image_data[y][x] = {gray, gray, gray};
        }
    }
    img.is_grayscale = true;
}

void toGrayscale(CompressedImage& img) {
    for (auto& [id, c] : img.id_to_color) {
        uint8_t gray = colorToGrayscale(c);
        c = {gray, gray, gray};
    }
    img.color_to_id.clear();
    for (auto& [id, c] : img.id_to_color) {
        img.color_to_id[c] = id;
    }
}