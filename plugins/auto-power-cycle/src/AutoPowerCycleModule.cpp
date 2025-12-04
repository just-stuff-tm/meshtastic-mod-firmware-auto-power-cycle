#include "AutoPowerCycleModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "power.h"
#include "sleep.h"
#include "channels/ChannelSet.h"
#include "../mesh/generated/meshtastic/module.pb.h"

AutoPowerCycleModule::AutoPowerCycleModule() : MeshModule("AutoPowerCycle") {
    ourRequestId = moduleConfig.mutable_auto_power_cycle();
}

bool AutoPowerCycleModule::wantPacket(const meshtastic_MeshPacket &mp) {
    return mp.decoded.portnum == meshtastic_PortNum_MODULE_CONFIG;
}

void AutoPowerCycleModule::handleFromRadio(const meshtastic_MeshPacket &mp) {
    if (mp.decoded.portnum != meshtastic_PortNum_MODULE_CONFIG) return;
    auto &p = mp.decoded;
    if (p.payload_variant_tag != meshtastic_MeshPacket_Decoded_PayloadVariant_module_config_auto_power_cycle_config_tag) return;

    auto cfg = &p.payload_variant.module_config.auto_power_cycle_config;
    moduleEnabled = cfg->enabled;
    lowBatteryPercent = cfg->low_threshold;
    resumePercent = cfg->resume_threshold;
    if (cfg->shutdown_message.size()) shutdownMessage = cfg->shutdown_message;
    if (cfg->power_up_message.size()) powerUpMessage = cfg->power_up_message;
    isDM = cfg->is_dm;
    if (cfg->has_target) targetChannelHash = cfg->target;
}

void AutoPowerCycleModule::sendCustomMessage(const std::string &msg) {
    if (msg.empty()) return;
    meshtastic_MeshPacket *p = service->allocDataPacket();
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    if (isDM) {
        p->to = targetChannelHash;
    } else {
        int chIndex = channels.getChannelIndexByHash(targetChannelHash);
        if (chIndex < 0) chIndex = 0;
        p->channel = chIndex;
    }
    p->decoded.payload.size = msg.copy((char*)p->decoded.payload.bytes, msg.length());
    service->sendToMesh(p, true);
}

int32_t AutoPowerCycleModule::runOnce() {
    if (!moduleEnabled) return 5000;
    uint32_t now = millis();
    if (lastCheck && (now - lastCheck < CHECK_INTERVAL_MS) && !powerFSM.getState().isSleeping) return 5000;
    lastCheck = now;

    if (!powerStatus->isPowerGood() && !wasShutdown) {
        uint8_t percent = powerStatus->getBatteryChargePercent();
        if (percent <= lowBatteryPercent) {
            if (!shutdownMsgSent) { sendCustomMessage(shutdownMessage); shutdownMsgSent = true; powerUpMsgSent = false; }
            screen->startAlert("Low battery\nShutting down...");
            delay(3000);
            wasShutdown = true;
            powerFSM.trigger(EVENT_LOW_BATTERY);
            doDeepSleep(0, true);
        }
    }

    if (wasShutdown) {
        uint8_t percent = powerStatus->getBatteryChargePercent();
        if (percent >= resumePercent) {
            wasShutdown = false; shutdownMsgSent = false;
            if (!powerUpMsgSent) {
                screen->startAlert("Recharged!\nStarting up...");
                delay(1000);
                sendCustomMessage(powerUpMessage);
                powerUpMsgSent = true;
            }
            powerFSM.trigger(EVENT_POWERED);
        } else {
            doDeepSleep(CHECK_INTERVAL_MS / 1000, true);
        }
    }
    return 5000;
}

bool AutoPowerCycleModule::handleReceivedProtobuf(const meshtastic_MeshPacket &, meshtastic_ModuleConfig_AutoPowerCycleConfig *) { return false; }

AutoPowerCycleModule *autoPowerCycleModule;
void initAutoPowerCycleModule() { autoPowerCycleModule = new AutoPowerCycleModule(); }

#include "ModuleRegistry.h"
MESHTASTIC_REGISTER_MODULE(AutoPowerCycleModule);
