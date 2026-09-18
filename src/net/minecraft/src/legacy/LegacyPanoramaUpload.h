#pragma once

#include <memory>
#include <string>

class BufferedImage;

std::unique_ptr<BufferedImage> legacyPreparePanoramaForUpload(
    const std::string &name, std::unique_ptr<BufferedImage> image);
