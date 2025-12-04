#pragma once
#include "MeshModule.h"
#include "PowerFSM.h"
#include "configuration.h"
#include <string>

class AutoPowerCycleModule : public MeshModule {
private:
    bool wasShutdown = false;
    bool shutdownMsgSent = false;
    bool powerUpMsgSent = false;
    uint32_t lastCheck = 0;
    const uint32_t CHECK_INTERVAL_MS = 15 * 60 * 1000;

    uint8_t lowBatteryPercent = 20;
    uint8_t resumePercent = 60;
    bool moduleEnabled = true;

    std::string shutdownMessage = "Shutting down until battery recharges to acceptable level";
    std::string powerUpMessage = "Back online – battery sufficiently recharged";

    bool isDM = false;
    uint32_t targetChannelHash = 0x78aa97c2;  // LongFast

protected:
    virtual int32_t runOnce() override;

public:
    AutoPowerCycleModule();
    virtual void handleFromRadio(const meshtastic_MeshPacket &mp) override;
    virtual bool wantPacket(const meshtastic_MeshPacket &mp) override;
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_ModuleConfig_AutoPowerCycleConfig *p) override;
    void sendCustomMessage(const std::string &msg);
};
