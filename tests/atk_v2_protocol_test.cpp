#include "../src/drivers/atk_v2_protocol.h"

#include <cassert>
#include <iostream>

int main() {
    const auto request = AtkV2::BuildFrame(AtkV2::kBatteryCommand, 2, 1);
    assert(request.size() == 64);
    assert(request[0] == 0x08);
    assert(request[2] == 0x72);
    assert(request[3] == 2);
    assert(request[5] == AtkV2::kTargetMouse);
    assert(request[6] == AtkV2::kBatteryCommand);
    assert(request[7] == 1);

    unsigned int sum = 0;
    for (std::size_t i = 2; i < request.size(); ++i) sum += request[i];
    assert(request[1] == (sum & 0xFF));

    // Synthetic successful 82% / 4038 mV response.
    std::vector<unsigned char> reply(64, 0);
    reply[0] = 0x08;
    reply[2] = 0x72;
    reply[3] = 0x00;  // success
    reply[6] = AtkV2::kBatteryCommand;
    reply[7] = 3;
    reply[8] = 82;
    reply[9] = 0xC6;
    reply[10] = 0x0F;

    std::vector<unsigned char> payload;
    assert(AtkV2::DecodeReply(reply, AtkV2::kBatteryCommand, payload));
    assert(payload.size() == 3);
    assert(payload[0] == 82);
    assert((payload[1] | (payload[2] << 8)) == 4038);

    // Unsolicited events and mismatched commands must not be accepted.
    reply[3] = AtkV2::kEventStatus;
    assert(!AtkV2::DecodeReply(reply, AtkV2::kBatteryCommand, payload));
    reply[3] = 0;
    assert(!AtkV2::DecodeReply(reply, AtkV2::kChargingCommand, payload));

    // Decoder also accepts a HID backend that strips report ID 08.
    reply.erase(reply.begin());
    assert(AtkV2::DecodeReply(reply, AtkV2::kBatteryCommand, payload));
    assert(payload[0] == 82);

    std::cout << "ATK/VXE V2 protocol tests passed\n";
    return 0;
}
