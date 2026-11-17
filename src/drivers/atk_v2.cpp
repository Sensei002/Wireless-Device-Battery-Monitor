// VXE R1 NK / ATK NK NANO NearLink receiver (373B:1216).
// This model exposes a dedicated 64-byte 0xff05 HID collection. Its battery
// commands are not compatible with the older 17-byte Kysona/0xff02 protocol.
#include "drivers.h"
#include "atk_v2_protocol.h"

namespace {

bool QueryV2(HidDev& dev, BYTE command, std::vector<BYTE>& payload) {
    dev.Flush();
    const auto request = AtkV2::BuildFrame(command, 2, 1, AtkV2::kTargetMouse);
    if (!dev.Write(request, 500)) return false;

    const ULONGLONG deadline = GetTickCount64() + 700;
    while (GetTickCount64() < deadline) {
        const int left = static_cast<int>(deadline - GetTickCount64());
        std::vector<BYTE> reply;
        if (dev.Read(reply, left) < 0) return false;
        if (AtkV2::DecodeReply(reply, command, payload)) return true;
        // Ignore unsolicited battery/event reports and replies to other commands.
    }
    return false;
}

}  // namespace

void PollAtkV2(PollContext& ctx) {
    constexpr USHORT kVid = 0x373B;
    constexpr USHORT kPid = 0x1216;

    for (const HidColl* collection : ctx.Find(kVid, kPid, [](const HidColl& c) {
             return c.iface == 2 && c.page() == 0xFF05 && c.usage() == 0x0001 &&
                    c.inLen() == AtkV2::kReportSize && c.outLen() == AtkV2::kReportSize;
         })) {
        // Claim this receiver as soon as its 0xff05 interface is identified. If the
        // new protocol does not answer (for example, the mouse is asleep), never
        // fall through and send legacy 17-byte Kysona commands to its vendor HID interfaces.
        ctx.Claim(kVid, kPid);
        HidDev dev;
        if (!dev.Open(*collection)) continue;

        std::vector<BYTE> battery;
        if (!QueryV2(dev, AtkV2::kBatteryCommand, battery) || battery.empty()) continue;

        std::vector<BYTE> charging;
        if (!QueryV2(dev, AtkV2::kChargingCommand, charging) || charging.empty()) continue;

        DeviceStatus status;
        status.key = L"atk-v2:373B:1216";
        status.name = L"VXE R1 NK";
        status.kind = DeviceKind::Mouse;
        status.via = L"2.4 GHz receiver";
        if (battery[0] <= 100) status.percent = battery[0];
        if (battery.size() >= 3) {
            const int mv = battery[1] | (battery[2] << 8);
            if (mv >= 1000 && mv <= 6000) status.millivolts = mv;
        }
        status.charging = charging[0] == 1 || charging[0] == 0x3B;
        ctx.Add(status, kVid, kPid);
        return;
    }
}
