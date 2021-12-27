#include "ppm.hxx"

namespace ppm {

std::size_t bitmap::blob_size(std::size_t width, std::size_t height) {
  return height * (width / 8 + !!(width % 8));
}

void bitmap::convert(std::byte *destination,
                     const matrix<std::uint8_t> &matrix) {
  for (std::size_t y = 0; y < matrix.height(); ++y) {
    std::uint8_t buffer = 0;
    std::size_t buffer_fill = 0;
    for (std::size_t x = 0; x < matrix.width(); ++x) {
      buffer = (buffer << 1) | (matrix(x, y) ? 1 : 0);
      buffer_fill += 1;
      if (buffer_fill == 8) {
        *destination++ = std::byte{buffer};
        buffer = 0;
        buffer_fill = 0;
      }
    }
    if (buffer_fill > 0) {
      *destination++ =
          std::byte{static_cast<std::uint8_t>(buffer << (8 - buffer_fill))};
    }
  }
}

bitmap::pixel bitmap::get_pixel(const std::byte *blob, std::size_t width,
                                std::size_t height, std::size_t x,
                                std::size_t y) {
  std::size_t line_width = width / 8 + !!(width % 8);
  return ((static_cast<std::uint8_t>(blob[line_width * y + x / 8])) >>
          (7 - x % 8)) &
         1;
}

std::size_t graymap::blob_size(std::size_t width, std::size_t height) {
  return width * height;
}

void graymap::convert(std::byte *destination,
                      const matrix<std::uint8_t> &matrix) {
  for (std::size_t y = 0; y < matrix.height(); ++y) {
    for (std::size_t x = 0; x < matrix.width(); ++x) {
      *destination++ = std::byte{matrix(x, y)};
    }
  }
}

graymap::pixel graymap::get_pixel(const std::byte *blob, std::size_t width,
                                  std::size_t height, std::size_t x,
                                  std::size_t y) {
  return static_cast<pixel>(blob[width * y + x]);
}

}; // namespace ppm
