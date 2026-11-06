#pragma once
#include <Arduino.h>
#include <NetworkClient.h>

// InfoTAB / PowerWalker Phoenixtec Modbus TCP: read-only function code 0x03.
// Register map cross-checked on the real UPS 192.168.1.102 (2026-11-06).
// Never send function 0x05/0x06/0x0F/0x10, which can alter UPS operation.
namespace infotab_modbus {
static constexpr uint16_t kPort = 502;
static constexpr uint8_t kUnit = 1;

struct Sample {
    bool valid = false;
    uint8_t modeCode = 0;
    uint8_t loadPercent = 0;
    uint16_t activePowerRegister = 0; // FC03 reg156; scale unverified on this UPS model.
    uint16_t apparentPowerRegister = 0; // FC03 reg159; diagnostic only.
    bool powerRegisterValid = false;
    uint8_t batteryPercent = 0;
    uint16_t inputVolts10 = 0;   // /10 -> V
    uint16_t outputVolts10 = 0;  // /10 -> V
    uint16_t outputHertz10 = 0;  // /10 -> Hz
    uint16_t batteryVolts10 = 0; // /10 -> V
    uint32_t runtimeSeconds = 0;
};

inline bool readExact(NetworkClient &client, uint8_t *buffer, size_t n,
                      uint32_t timeoutMs = 1800) {
    const uint32_t started = millis();
    size_t pos = 0;
    while (pos < n && millis() - started < timeoutMs) {
        if (client.available() <= 0) {
            if (!client.connected()) return false;
            delay(2);
            continue;
        }
        const int bytes = client.read(buffer + pos, n - pos);
        if (bytes > 0) pos += static_cast<size_t>(bytes);
    }
    return pos == n;
}

// Modbus TCP MBAP + 0x03 Read Holding Registers only.
inline bool readHolding(NetworkClient &client, uint16_t address, uint16_t count,
                        uint16_t *output, uint16_t txid) {
    if (count < 1 || count > 32) return false;
    uint8_t request[12] = {
        static_cast<uint8_t>(txid >> 8), static_cast<uint8_t>(txid),
        0, 0, 0, 6, kUnit, 3,
        static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address),
        static_cast<uint8_t>(count >> 8), static_cast<uint8_t>(count)
    };
    if (client.write(request, sizeof(request)) != sizeof(request)) return false;
    uint8_t header[9]{};
    if (!readExact(client, header, sizeof(header))) return false;
    const uint16_t rxid = static_cast<uint16_t>(header[0] << 8 | header[1]);
    const uint16_t protocol = static_cast<uint16_t>(header[2] << 8 | header[3]);
    const uint16_t payloadLen = static_cast<uint16_t>(header[4] << 8 | header[5]);
    if (rxid != txid || protocol != 0 || header[6] != kUnit ||
        header[7] != 3 || header[8] != count * 2 ||
        payloadLen != 3 + count * 2) return false;
    uint8_t data[64]{};
    if (!readExact(client, data, count * 2)) return false;
    for (uint16_t i = 0; i < count; ++i) {
        output[i] = static_cast<uint16_t>(
            static_cast<uint16_t>(data[2 * i]) << 8 | data[2 * i + 1]);
    }
    return true;
}

inline bool poll(Sample &result) {
    result = Sample{};
    NetworkClient client;
    if (!client.connect("192.168.1.102", kPort, 1500)) return false;
    uint16_t mode[5]{}, input[7]{}, output[19]{}, battery[8]{};
    const bool ok = readHolding(client, 112, 5, mode, 0x101) &&
                    readHolding(client, 128, 7, input, 0x102) &&
                    readHolding(client, 144, 19, output, 0x103) &&
                    readHolding(client, 165, 8, battery, 0x104);
    client.stop();
    if (!ok) return false;

    // 0xFFFF = unsupported / unknown in this Phoenixtec register family.
    if (battery[4] == 0xFFFF || battery[4] > 100 ||
        output[18] == 0xFFFF || output[18] > 120 ||
        battery[0] == 0xFFFF || battery[1] == 0xFFFF ||
        battery[1] > 59 || input[4] == 0xFFFF || output[3] == 0xFFFF ||
        mode[0] == 0xFFFF) return false;
    const uint32_t runtime = uint32_t(battery[0]) * 60u + battery[1];
    if (runtime > 7u * 24u * 3600u) return false;

    result.modeCode = static_cast<uint8_t>(mode[0] & 0xFF);
    result.inputVolts10 = input[4];
    result.outputVolts10 = output[3];
    result.outputHertz10 = output[0];
    result.loadPercent = static_cast<uint8_t>(output[18]);
    if (output[12] != 0xFFFF && output[15] != 0xFFFF &&
        output[12] <= output[15] && output[15] < 5000) {
        result.activePowerRegister = output[12];  // holding register 156
        result.apparentPowerRegister = output[15]; // holding register 159
        result.powerRegisterValid = true;
    }
    result.batteryPercent = static_cast<uint8_t>(battery[4]);
    result.runtimeSeconds = runtime;
    result.batteryVolts10 = battery[5] == 0xFFFF ? 0 : battery[5];
    result.valid = true;
    return true;
}

} // namespace infotab_modbus
