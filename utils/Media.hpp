#pragma once
#include "settings/Settings.h"
#include <string>

inline std::string imageUrl(const std::string& publicId) {
    return "https://res.cloudinary.com/" + getSettings()->cloudinary.cloudName + "/image/upload/f_auto,q_auto/" + publicId;
}
