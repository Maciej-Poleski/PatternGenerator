#include "ppm.hxx"

std::size_t ppm::bitmap::blob_size(std::size_t width, std::size_t height) {
  return height * (width / 8 + !!(width % 8));
}

void ppm::bitmap::convert(std::byte *destination,
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

std::size_t ppm::graymap::blob_size(std::size_t width, std::size_t height) {
  return width * height;
}

void ppm::graymap::convert(std::byte *destination,
                           const matrix<std::uint8_t> &matrix) {
  for (std::size_t y = 0; y < matrix.height(); ++y) {
    for (std::size_t x = 0; x < matrix.width(); ++x) {
      *destination++ = std::byte{matrix(x, y)};
    }
  }
}
