#include "compressor_funcs.h"
#include "error_handlers.h"
#include "libbmp.h"

void saveAsBMP(const UncompressedImage& img, const std::string& filename) {
    BMP bmp(static_cast<int>(img.width), static_cast<int>(img.height));
    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            size_t bmp_y = img.height - 1 - y;
            const ColorRGB& c = img.image_data[y][x];
            bmp.set_pixel(static_cast<int>(x), static_cast<int>(bmp_y), c.r, c.g, c.b);
        }
    }
    bmp.write(filename.c_str());
}

UncompressedImage loadFromBMP(const std::string& filename) {
    BMP bmp(filename.c_str());
    UncompressedImage img;
    img.width = static_cast<uint32_t>(bmp.get_width());
    img.height = static_cast<uint32_t>(bmp.get_height());
    img.is_grayscale = false;
    img.image_data.resize(img.height, std::vector<ColorRGB>(img.width));

    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            size_t bmp_y = img.height - 1 - y;
            uint8_t r, g, b;
            bmp.get_pixel(static_cast<int>(x), static_cast<int>(bmp_y), r, g, b);
            img.image_data[y][x] = {r, g, b};
        }
    }
    return img;
}

UncompressedImage readUncompressedFile(const std::string& filename) {
    std::fstream file(filename, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        handleLogMessage("Cannot open file " + filename, Severity::CRITICAL, 1);
    }

    char sig[8];
    file.read(sig, 8);
    if (std::string(sig, 8) != "RAWIMAGE") {
        handleLogMessage("Invalid file signature", Severity::CRITICAL, 1);
    }

    uint8_t v[3];
    file.read(reinterpret_cast<char*>(v), 3);

    uint32_t width, height;
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);

    uint8_t is_gray;
    file.read(reinterpret_cast<char*>(&is_gray), 1);

    UncompressedImage img;
    img.width = width;
    img.height = height;
    img.is_grayscale = (is_gray == 1);
    img.image_data.resize(height, std::vector<ColorRGB>(width));

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            if (is_gray) {
                uint8_t gray;
                file.read(reinterpret_cast<char*>(&gray), 1);
                img.image_data[y][x] = {gray, gray, gray};
            } else {
                ColorRGB c = readFromFileStream(file);
                img.image_data[y][x] = c;
            }
        }
    }

    char end_sig[9];
    file.read(end_sig, 9);

    return img;
}

void writeUncompressedFile(const std::string& filename, const UncompressedImage& image) {
    std::fstream file(filename, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        handleLogMessage("Cannot open file " + filename, Severity::CRITICAL, 1);
    }

    file.write("RAWIMAGE", 8);
    uint8_t version[3] = {1, 0, 0};
    file.write(reinterpret_cast<char*>(version), 3);
    file.write(reinterpret_cast<const char*>(&image.width), 4);
    file.write(reinterpret_cast<const char*>(&image.height), 4);

    uint8_t is_gray = image.is_grayscale ? 1 : 0;
    file.write(reinterpret_cast<char*>(&is_gray), 1);

    for (size_t y = 0; y < image.height; ++y) {
        for (size_t x = 0; x < image.width; ++x) {
            const ColorRGB& c = image.image_data[y][x];
            if (image.is_grayscale) {
                file.write(reinterpret_cast<const char*>(&c.r), 1);
            } else {
                file.write(reinterpret_cast<const char*>(&c.r), 1);
                file.write(reinterpret_cast<const char*>(&c.g), 1);
                file.write(reinterpret_cast<const char*>(&c.b), 1);
            }
        }
    }

    file.write("RAWIMGEND", 9);
}

uint8_t findClosestColorId(const ColorRGB& color, const std::map<uint8_t, ColorRGB>& colorTable) {
    uint8_t best_id = colorTable.begin()->first;
    int64_t best_dist = INT64_MAX;
    for (const auto& [id, c] : colorTable) {
        int64_t dist = colorDistanceSq(color, c);
        if (dist < best_dist) {
            best_dist = dist;
            best_id = id;
        }
    }
    return best_id;
}

CompressedImage toCompressed(
    const UncompressedImage& img, const std::map<uint8_t, ColorRGB>& color_table, bool approximate,
    bool allow_color_add) {
    CompressedImage result;
    result.width = img.width;
    result.height = img.height;
    result.id_to_color = color_table;
    for (const auto& [id, c] : color_table) {
        result.color_to_id[c] = id;
    }

    result.image_data.resize(img.height, std::vector<uint8_t>(img.width));

    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            const ColorRGB& c = img.image_data[y][x];

            if (!approximate && allow_color_add) {
                auto it = result.color_to_id.find(c);
                if (it != result.color_to_id.end()) {
                    result.image_data[y][x] = it->second;
                } else {
                    uint8_t new_id = static_cast<uint8_t>(result.id_to_color.size());
                    result.id_to_color[new_id] = c;
                    result.color_to_id[c] = new_id;
                    result.image_data[y][x] = new_id;
                }
            } else if (!approximate && !allow_color_add) {
                auto it = result.color_to_id.find(c);
                if (it != result.color_to_id.end()) {
                    result.image_data[y][x] = it->second;
                } else {
                    result.image_data[y][x] = findClosestColorId(c, result.id_to_color);
                }
            } else {
                result.image_data[y][x] = findClosestColorId(c, result.id_to_color);
            }
        }
    }

    return result;
}

UncompressedImage toUncompressed(const CompressedImage& img) {
    UncompressedImage result;
    result.width = img.width;
    result.height = img.height;
    result.is_grayscale = false;
    result.image_data.resize(img.height, std::vector<ColorRGB>(img.width));

    for (size_t y = 0; y < img.height; ++y) {
        for (size_t x = 0; x < img.width; ++x) {
            uint8_t id = img.image_data[y][x];
            result.image_data[y][x] = img.id_to_color.at(id);
        }
    }
    return result;
}

ColorRGB getColor(const CompressedImage& img, int x, int y) {
    uint8_t id = img.image_data[static_cast<size_t>(y)][static_cast<size_t>(x)];
    return img.id_to_color.at(id);
}

CompressedImage readCompressedFile(const std::string& filename) {
    std::fstream file(filename, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        handleLogMessage("Cannot open file " + filename, Severity::CRITICAL, 1);
    }

    char sig[10];
    file.read(sig, 10);
    std::string sig_str(sig, 9);
    if (sig_str != "CMPRIMAGE" || sig[9] != 0x00) {
        handleLogMessage("Invalid compressed file signature", Severity::CRITICAL, 1);
    }

    uint8_t v[3];
    file.read(reinterpret_cast<char*>(v), 3);

    uint32_t width, height;
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);

    uint8_t pow_val;
    file.read(reinterpret_cast<char*>(&pow_val), 1);

    size_t palette_size = static_cast<size_t>(1) << pow_val;

    CompressedImage img;
    img.width = width;
    img.height = height;

    for (size_t i = 0; i < palette_size; ++i) {
        uint8_t r, g, b;
        file.read(reinterpret_cast<char*>(&r), 1);
        file.read(reinterpret_cast<char*>(&g), 1);
        file.read(reinterpret_cast<char*>(&b), 1);
        ColorRGB c{r, g, b};
        img.id_to_color[static_cast<uint8_t>(i)] = c;
        img.color_to_id[c] = static_cast<uint8_t>(i);
    }

    img.image_data.resize(height, std::vector<uint8_t>(width));
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            file.read(reinterpret_cast<char*>(&img.image_data[y][x]), 1);
        }
    }

    char end_sig[10];
    file.read(end_sig, 10);

    return img;
}

void writeCompressedFile(const std::string& filename, const CompressedImage& image) {
    std::fstream file(filename, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        handleLogMessage("Cannot open file " + filename, Severity::CRITICAL, 1);
    }

    file.write("CMPRIMAGE", 9);
    uint8_t null_byte = 0x00;
    file.write(reinterpret_cast<char*>(&null_byte), 1);

    uint8_t version[3] = {6, 6, 6};
    file.write(reinterpret_cast<char*>(version), 3);

    file.write(reinterpret_cast<const char*>(&image.width), 4);
    file.write(reinterpret_cast<const char*>(&image.height), 4);

    size_t palette_size = image.id_to_color.size();
    uint8_t pow_val = 0;
    size_t ps = 1;
    while (ps < palette_size) {
        ps <<= 1;
        ++pow_val;
    }
    file.write(reinterpret_cast<char*>(&pow_val), 1);

    for (size_t i = 0; i < ps; ++i) {
        auto it = image.id_to_color.find(static_cast<uint8_t>(i));
        if (it != image.id_to_color.end()) {
            file.write(reinterpret_cast<const char*>(&it->second.r), 1);
            file.write(reinterpret_cast<const char*>(&it->second.g), 1);
            file.write(reinterpret_cast<const char*>(&it->second.b), 1);
        } else {
            uint8_t zero[3] = {0, 0, 0};
            file.write(reinterpret_cast<char*>(zero), 3);
        }
    }

    for (size_t y = 0; y < image.height; ++y) {
        for (size_t x = 0; x < image.width; ++x) {
            file.write(reinterpret_cast<const char*>(&image.image_data[y][x]), 1);
        }
    }

    file.write("CMPRIMGEND", 10);
}