#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Read-only battery protocol for the ATK/VXE 0xff05 HID interface.
// Frame layout follows the publicly documented ATK Hub V2 transport:
// [report id 08][checksum][marker 72][param len][reserved][target][command][command len][data...]
namespace AtkV2 {

constexpr unsigned char kReportId = 0x08;
constexpr unsigned char kMarker = 0x72;
constexpr unsigned char kEventStatus = 0x73;
constexpr std::size_t kFrameSize = 64;
constexpr unsigned char kTargetDongle = 0;
constexpr unsigned char kTargetMouse = 1;
constexpr unsigned char kBatteryCommand = 0x07;
constexpr unsigned char kChargingCommand = 0x0F;

inline std::vector<unsigned char> BuildFrame(unsigned char command,
                                              unsigned char paramLength,
                                              unsigned char commandLength,
                                              const std::vector<unsigned char>& payload = {},
                                              unsigned char target = kTargetMouse) {
    if (payload.size() > 56 || target > 1) return {};
    std::vector<unsigned char> frame(kFrameSize, 0);
    frame[0] = kReportId;
    frame[2] = kMarker;
    frame[3] = paramLength;
    frame[5] = target;
    frame[6] = command;
    frame[7] = commandLength;
    std::copy(payload.begin(), payload.end(), frame.begin() + 8);
    unsigned int sum = 0;
    for (std::size_t i = 2; i < frame.size(); ++i) sum += frame[i];
    frame[1] = static_cast<unsigned char>(sum & 0xFF);
    return frame;
}

// Returns the command payload only for a matching, successful response.
// HID reads may include report ID 08 or begin directly with marker 72.
inline bool DecodeReply(const std::vector<unsigned char>& raw,
                        unsigned char command,
                        std::vector<unsigned char>& payload) {
    std::size_t offset = 0;
    if (!raw.empty() && raw[0] == kReportId) offset = 1;
    if (raw.size() < offset + 7 || raw[offset] != kMarker) return false;
    const unsigned char status = raw[offset + 1];
    if (status == kEventStatus || status == 0xFF || raw[offset + 4] != command) return false;
    const std::size_t available = raw.size() - (offset + 6);
    const std::size_t length = std::min<std::size_t>(raw[offset + 5], available);
    payload.assign(raw.begin() + offset + 6, raw.begin() + offset + 6 + length);
    return true;
}

}  // namespace AtkV2
