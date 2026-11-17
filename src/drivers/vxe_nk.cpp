// VXE R1 NK / ATK NK NANO receiver (373B:1216), 64-byte 0xff05 protocol.
// Read-only battery commands documented by the public ATK Hub V2 protocol research.
#include "drivers.h"
#include "atk_v2_protocol.h"

namespace {

bool QueryV2(HidDev& device, BYTE command, BYTE paramLength, BYTE commandLength,
             std::vector<BYTE>& payload, BYTE target = AtkV2::kTargetMouse) {
    const auto request = AtkV2::BuildFrame(command, paramLength, commandLength, {}, target);
    if (request.empty()) return false;

    for (int attempt = 0; attempt < 3; ++attempt) {
        device.Flush();
        if (!device.Write(request, 600)) continue;
        const ULONGLONG deadline = GetTickCount64() + 600;
        while (GetTickCount64() < deadline) {
            const int remaining = static_cast<int>(deadline - GetTickCount64());
            std::vector<BYTE> reply;
            if (device.Read(reply, remaining) < 0) break;
            if (AtkV2::DecodeReply(reply, command, payload)) return true;
        }
    }
    return false;
}

bool ReadBattery(HidDev& device, DeviceStatus& status) {
    std::vector<BYTE> battery;
    if (!QueryV2(device, AtkV2::kBatteryCommand, 2, 1, battery) || battery.empty() || battery[0] > 100)
        return false;

    std::vector<BYTE> charging;
    if (!QueryV2(device, AtkV2::kChargingCommand, 2, 1, charging) || charging.empty())
        return false;

    status.percent = battery[0];
    status.charging = charging[0] == 1 || charging[0] == 0x3B;
    if (battery.size() >= 3) {
        const int millivolts = battery[1] | (battery[2] << 8);
        if (millivolts >= 1000 && millivolts <= 6000) status.millivolts = millivolts;
    }
    return true;
}

}  // namespace

void PollVxeNk(PollContext& ctx) {
    constexpr USHORT kVid = 0x373B;
    constexpr USHORT kPid = 0x1216;

    for (const HidColl* collection : ctx.Find(kVid, kPid, [](const HidColl& c) {
             return c.page() == 0xFF05 && c.inLen() == AtkV2::kFrameSize &&
                    c.outLen() == AtkV2::kFrameSize;
         })) {
        HidDev device;
        if (!device.Open(*collection)) continue;

        DeviceStatus status;
        if (!ReadBattery(device, status)) continue;

        status.key = L"vxe-r1-nk:373B:1216";
        status.name = L"VXE R1 NK";
        status.kind = DeviceKind::Mouse;
        status.via = L"2.4 GHz receiver";
        ctx.Add(status, kVid, kPid);
        break;
    }
}
