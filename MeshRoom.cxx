/*
 * MeshRoom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
#include <stdarg.h>
#include <pico/stdlib.h>
#include <pico/flash.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <sstream>
#include <iomanip>
#include <PicoPlatform.hxx>
#include <meshroom.h>
#include <MeshRoom.hxx>

#define BUTTON_EVENT_QUEUE_LEN  5

static QueueHandle_t button_event_queue = NULL;

static uint32_t crc32_le(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffu;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }

    return ~crc;
}

MeshRoom::MeshRoom()
    : SimpleClient(), HomeChat(), BaseNvm(), MorseBuzzer()
    , _irTx(IR_BLAST_PIN)
{
    bzero(&_main_body, sizeof(_main_body));
    _main_body.ir_flags =
        MESHROOM_IR_SONY_BRAVIA |
        MESHROOM_IR_PANASONIC_AC;
    _watchdogEnabled = true;
    _tvOnOff = false;
    _tvVol = 10;
    _tvChan = 1;
    _tvMute = false;
    _acOnOff = false;
    _acMode = AC_AC;
    _acTemp = 24;
    _acFanSpeed = 0;
    _acFanDir = 0;
    _acPowerful = false;
    _acQuiet = false;
    _resetCount = 1;
    _lastReset = time(NULL);

    if (button_event_queue == NULL) {
        button_event_queue = xQueueCreate(BUTTON_EVENT_QUEUE_LEN,
                                          sizeof(struct button_event));
    }

    gpio_init(PUSHBUTTON_PIN);
    gpio_set_dir(PUSHBUTTON_PIN, GPIO_IN);
    gpio_pull_up(PUSHBUTTON_PIN);
    gpio_set_irq_enabled_with_callback(PUSHBUTTON_PIN,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true,
                                       MeshRoom::gpio_callback);

    gpio_init(OUTRESET_PIN);
    gpio_set_dir(OUTRESET_PIN, GPIO_OUT);
    gpio_put(OUTRESET_PIN, false);
    for (volatile uint32_t counter = 0; counter < 0xffff; counter++);
    gpio_put(OUTRESET_PIN, true);

    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, false);

    _irTx.init();

    gpio_init(ALERT_LED_PIN);
    gpio_set_dir(ALERT_LED_PIN, GPIO_OUT);
    setAlertLed(false);
}

MeshRoom::~MeshRoom()
{

}

void MeshRoom::gpio_callback(uint gpio, uint32_t events)
{
    static uint64_t t0 = 0;
    struct button_event event = {
        .ts = 0,
        .tdur = 0,
    };
    BaseType_t woken = pdFALSE;

    if (gpio != PUSHBUTTON_PIN) {
        goto done;
    }

    event.ts = time_us_64();

    if (events & GPIO_IRQ_EDGE_FALL) {
        t0 = event.ts;
        goto done;
    }

    if (events & GPIO_IRQ_EDGE_RISE) {
        if (t0 == 0) {
            goto done;
        }

        event.tdur = event.ts - t0;
        t0 = 0;

        if (event.tdur < PUSHBUTTON_DURATION_THRESHOLD_US) {
            goto done;
        }
    }

    if (button_event_queue != NULL) {
        xQueueSendFromISR(button_event_queue, &event, &woken);
        portYIELD_FROM_ISR(woken);
    }

done:

    return;
}

bool MeshRoom::getButtonEvent(struct button_event &event, bool clearOld)
{
    bool result = false;
    struct button_event latest;

    if (button_event_queue == NULL) {
        goto done;
    }

    if (clearOld) {
        while (xQueueReceive(button_event_queue, &latest, 0) == pdTRUE) {
            event = latest;
            result = true;
        }
    } else if (xQueueReceive(button_event_queue, &event, 0) == pdTRUE) {
        result = true;
    }

done:

    return result;
}

void MeshRoom::blastTvCommand(enum TvCommand cmd)
{
    if (_main_body.ir_flags & MESHROOM_IR_SONY_BRAVIA) {
        _irTx.sendSonyTv(cmd);
    }
    if (_main_body.ir_flags & MESHROOM_IR_SAMSUNG_TV) {
        _irTx.sendSamsungTv(cmd);
    }
    if (_main_body.ir_flags & MESHROOM_IR_PANASONIC_TV) {
        _irTx.sendPanasonicTv(cmd);
    }
}

void MeshRoom::blastAcState(void)
{
    if (_main_body.ir_flags & MESHROOM_IR_PANASONIC_AC) {
        struct IrHvacState state;
        memset(&state, 0, sizeof(state));
        state.power = _acOnOff;
        switch (_acMode) {
        case AC_AC:
            state.mode = 1; // Cool
            break;
        case AC_HEATER:
            state.mode = 3; // Heat
            break;
        case AC_DEHUMIDIFIER:
            state.mode = 2; // Dry
            break;
        case AC_FAN:
            state.mode = 4; // Fan
            break;
        case AC_AUTO:
        default:
            state.mode = 0; // Auto
            break;
        }
        state.targetTemp = (float)_acTemp;
        state.fanSpeed = (uint8_t)_acFanSpeed;
        state.swingV = (uint8_t)_acFanDir;
        state.swingH = 0;
        state.powerful = _acPowerful;
        state.quiet = _acQuiet;
        _irTx.sendPanasonicHvac(state);
    }
}

void MeshRoom::tvOnOff(bool onOff)
{
    _tvOnOff = onOff;
    blastTvCommand(onOff ? TV_CMD_POWER_ON : TV_CMD_POWER_OFF);
}

bool MeshRoom::tvOnOff(void) const
{
    return _tvOnOff;
}

void MeshRoom::tvVol(unsigned int volume)
{
    if (volume > 100) {
        return;
    }

    if (volume > _tvVol) {
        blastTvCommand(TV_CMD_VOL_UP);
    } else if (volume < _tvVol) {
        blastTvCommand(TV_CMD_VOL_DOWN);
    }
    _tvVol = volume;
}

unsigned int MeshRoom::tvVol(void) const
{
    return _tvVol;
}

void MeshRoom::tvChan(unsigned int chan)
{
    if (chan > 999) {
        return;
    }

    if (chan > _tvChan) {
        blastTvCommand(TV_CMD_CHAN_UP);
    } else if (chan < _tvChan) {
        blastTvCommand(TV_CMD_CHAN_DOWN);
    }
    _tvChan = chan;
}

unsigned int MeshRoom::tvChan(void) const
{
    return _tvChan;
}

void MeshRoom::tvMute(bool mute)
{
    _tvMute = mute;
    blastTvCommand(TV_CMD_MUTE);
}

bool MeshRoom::tvMute(void) const
{
    return _tvMute;
}

void MeshRoom::toggleTvMute(void)
{
    tvMute(!_tvMute);
}

void MeshRoom::tvInput(void)
{
    blastTvCommand(TV_CMD_INPUT);
}

void MeshRoom::tvDigit(unsigned int digit)
{
    if (digit <= 9) {
        blastTvCommand((enum TvCommand)(TV_CMD_DIGIT_0 + digit));
    }
}

string MeshRoom::tvIrProtocolStr(void) const
{
    string s;
    if (_main_body.ir_flags & MESHROOM_IR_SONY_BRAVIA) {
        s = "sony_bravia";
    } else if (_main_body.ir_flags & MESHROOM_IR_SAMSUNG_TV) {
        s = "samsung_tv";
    } else if (_main_body.ir_flags & MESHROOM_IR_PANASONIC_TV) {
        s = "panasonic_tv";
    } else {
        s = "none";
    }
    return s;
}

void MeshRoom::acOnOff(bool onOff)
{
    _acOnOff = onOff;
    blastAcState();
}

bool MeshRoom::acOnOff(void) const
{
    return _acOnOff;
}

void MeshRoom::acMode(enum AcMode mode)
{
    if ((mode >= AC_AC) && (mode <= AC_FAN)) {
        _acMode = mode;
        blastAcState();
    }
}

enum MeshRoom::AcMode MeshRoom::acMode(void) const
{
    return _acMode;
}

string MeshRoom::acModeStr(void) const
{
    string s;

    switch (_acMode) {
    case AC_AC:
        s = "cool";
        break;
    case AC_HEATER:
        s = "heat";
        break;
    case AC_DEHUMIDIFIER:
        s = "dry";
        break;
    case AC_AUTO:
        s = "auto";
        break;
    case AC_FAN:
        s = "fan";
        break;
    default:
        break;
    }

    return s;
}

void MeshRoom::acTemp(unsigned int temp)
{
    if ((temp >= 16) && (temp <= 30)) {
        _acTemp = temp;
        blastAcState();
    }
}

unsigned int MeshRoom::acTemp(void) const
{
    return _acTemp;
}

void MeshRoom::acFanSpeed(unsigned int speed)
{
    if (speed <= 5) {
        _acFanSpeed = speed;
        blastAcState();
    }
}

unsigned int MeshRoom::acFanSpeed(void) const
{
    return _acFanSpeed;
}

string MeshRoom::acFanSpeedStr(void) const
{
    string s;
    switch (_acFanSpeed) {
    case 0: s = "auto"; break;
    case 1: s = "1 (quiet)"; break;
    case 2: s = "2 (low)"; break;
    case 3: s = "3 (med)"; break;
    case 4: s = "4 (high)"; break;
    case 5: s = "5 (max)"; break;
    default: s = to_string(_acFanSpeed); break;
    }
    return s;
}

void MeshRoom::acFanDir(unsigned int dir)
{
    if (dir <= 6) {
        _acFanDir = dir;
        blastAcState();
    }
}

unsigned int MeshRoom::acFanDir(void) const
{
    return _acFanDir;
}

string MeshRoom::acFanDirStr(void) const
{
    string s;
    switch (_acFanDir) {
    case 0: s = "auto"; break;
    case 1: s = "1 (up)"; break;
    case 2: s = "2 (up-mid)"; break;
    case 3: s = "3 (mid)"; break;
    case 4: s = "4 (down-mid)"; break;
    case 5: s = "5 (down)"; break;
    default: s = to_string(_acFanDir); break;
    }
    return s;
}

void MeshRoom::acPowerful(bool powerful)
{
    _acPowerful = powerful;
    if (_acPowerful) {
        _acQuiet = false;
    }
    blastAcState();
}

bool MeshRoom::acPowerful(void) const
{
    return _acPowerful;
}

void MeshRoom::acQuiet(bool quiet)
{
    _acQuiet = quiet;
    if (_acQuiet) {
        _acPowerful = false;
    }
    blastAcState();
}

bool MeshRoom::acQuiet(void) const
{
    return _acQuiet;
}

string MeshRoom::acIrProtocolStr(void) const
{
    if (_main_body.ir_flags & MESHROOM_IR_PANASONIC_AC) {
        return "panasonic_ac";
    }
    return "none";
}

void MeshRoom::reset(void)
{
    _resetCount++;

    gpio_put(OUTRESET_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(OUTRESET_PIN, true);

    _lastReset = time(NULL);
}

unsigned int MeshRoom::getResetCount(void) const
{
    return _resetCount;
}

time_t MeshRoom::getLastReset(void) const
{
    return _lastReset;
}

unsigned int MeshRoom::getLastResetSecsAgo(void) const
{
    time_t now;

    now = time(NULL);

    return now - _lastReset;
}

void MeshRoom::buzz(unsigned int ms)
{
    gpio_put(BUZZER_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(ms));
    gpio_put(BUZZER_PIN, false);
}

void MeshRoom::buzzMorseCode(const string &text, bool clearPrevious)
{
    if (clearPrevious) {
        this->clearMorseText();
    }

    this->addMorseText(text);
}

bool MeshRoom::isAlertLedOn(void) const
{
    return _alertLed;
}

void MeshRoom::setAlertLed(bool onOff)
{
    _alertLed = onOff;
    gpio_put(ALERT_LED_PIN, onOff);
}

void MeshRoom::flipAlertLed(void)
{
    _alertLed = !_alertLed;
    gpio_put(ALERT_LED_PIN, _alertLed);
}

void MeshRoom::flipOnboardLed(void)
{
    PicoPlatform::get()->flipOnboardLed();
}

float MeshRoom::getOnboardTempC(void) const
{
    return PicoPlatform::get()->getOnboardTempC();
}

bool MeshRoom::isWatchdogEnabled(void) const
{
    return _watchdogEnabled;
}

void MeshRoom::setWatchdogEnabled(bool enable)
{
    _watchdogEnabled = enable;
    if (enable) {
        watchdog_enable(5000, true);
    } else {
        watchdog_disable();
    }
}

void MeshRoom::gotTextMessage(const meshtastic_MeshPacket &packet,
                              const string &message)
{
    bool result = false;
    SimpleClient::gotTextMessage(packet, message);

    result = handleTextMessage(packet, message);
    if (result) {
        return;
    }
}

void MeshRoom::gotTelemetry(const meshtastic_MeshPacket &packet,
                            const meshtastic_Telemetry &telemetry)
{
    if (packet.from == whoami()) {
        SimpleClient::gotTelemetry(packet, telemetry);
    } else {
        // Ignore telemetry from other nodes
    }
}

void MeshRoom::gotRouting(const meshtastic_MeshPacket &packet,
                          const meshtastic_Routing &routing)
{
    SimpleClient::gotRouting(packet, routing);
    if ((routing.which_variant == meshtastic_Routing_error_reason_tag) &&
        (routing.error_reason == meshtastic_Routing_Error_NONE) &&
        (packet.from != packet.to)) {
        consoles_printf("traceroute from %s -> %s[%.2fdB]\n",
                        getDisplayName(packet.from).c_str(),
                        getDisplayName(packet.to).c_str(),
                        packet.rx_snr);
    }
}

void MeshRoom::gotTraceRoute(const meshtastic_MeshPacket &packet,
                             const meshtastic_RouteDiscovery &routeDiscovery)
{
    SimpleClient::gotTraceRoute(packet, routeDiscovery);
    if ((routeDiscovery.route_count > 0) &&
        (routeDiscovery.route_back_count == 0)) {
        float rx_snr;
        consoles_printf("traceroute from %s -> ",
                        getDisplayName(packet.from).c_str());
        for (unsigned int i = 0; i < routeDiscovery.route_count; i++) {
            if (i > 0) {
                consoles_printf(" -> ");
            }
            consoles_printf("%s",
                            getDisplayName(routeDiscovery.route[i]).c_str());
            if (routeDiscovery.snr_towards[i] != INT8_MIN) {
                rx_snr = routeDiscovery.snr_towards[i];
                rx_snr /= 4.0;
                consoles_printf("[%.2fdB]", rx_snr);
            } else {
                consoles_printf("[???dB]");
            }
        }
        rx_snr = packet.rx_snr;
        consoles_printf(" -> %s[%.2fdB]\n",
                        getDisplayName(packet.to).c_str(), rx_snr);
    }
}

string MeshRoom::handleUnknown(uint32_t node_num, uint32_t dest,
                               uint8_t channel, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);
    (void)(dest);
    (void)(channel);

    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);
    message = message.substr(first_word.size());
    trimWhitespace(message);

    if (first_word == "tv") {
        reply = handleTv(node_num, message);
    } else if (first_word == "ac") {
        reply = handleAc(node_num, message);
    } else if (first_word == "reset") {
        reply = handleReset(node_num, message);
    } else if (first_word == "buzz") {
        reply = handleBuzz(node_num, message);
    } else if (first_word == "morse") {
        reply = handleMorse(node_num, message);
    }

    return reply;
}

string MeshRoom::handleStatus(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    reply = "operational";

    return reply;
}

string MeshRoom::handleEnv(uint32_t node_num, string &message)
{
    stringstream ss;

    ss << HomeChat::handleEnv(node_num, message);
    if (!ss.str().empty()) {
        ss << endl;
    }

    ss << "board temperature: ";
    ss <<  setprecision(3) << getOnboardTempC();

    return ss.str();
}

string MeshRoom::handleTv(uint32_t node_num, string &message)
{
    string reply;
    (void)(node_num);

    stringstream ss(message);
    string cmd;
    ss >> cmd;
    toLowercase(cmd);

    if (cmd == "on") {
        tvOnOff(true);
        reply = "TV turned ON";
    } else if (cmd == "off") {
        tvOnOff(false);
        reply = "TV turned OFF";
    } else if (cmd == "toggle") {
        tvOnOff(!tvOnOff());
        reply = string("TV toggled ") + (tvOnOff() ? "ON" : "OFF");
    } else if (cmd == "mute") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "on") {
            tvMute(true);
            reply = "TV muted";
        } else if (sub == "off") {
            tvMute(false);
            reply = "TV unmuted";
        } else {
            toggleTvMute();
            reply = string("TV mute ") + (tvMute() ? "ON" : "OFF");
        }
    } else if (cmd == "input" || cmd == "source") {
        tvInput();
        reply = "TV input switched";
    } else if (cmd == "key" || (cmd.length() == 1 && isdigit((unsigned char)cmd[0]))) {
        unsigned int d = 0;
        if (cmd == "key") {
            string sub;
            ss >> sub;
            d = (unsigned int)strtoul(sub.c_str(), NULL, 10);
        } else {
            d = (unsigned int)(cmd[0] - '0');
        }
        tvDigit(d);
        reply = "TV sent key " + to_string(d);
    } else if (cmd == "vol") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "up") {
            tvVol(tvVol() + 1);
            reply = "TV volume UP to " + to_string(tvVol());
        } else if (sub == "down") {
            tvVol(tvVol() > 0 ? tvVol() - 1 : 0);
            reply = "TV volume DOWN to " + to_string(tvVol());
        } else if (!sub.empty()) {
            tvVol((unsigned int)strtoul(sub.c_str(), NULL, 10));
            reply = "TV volume set to " + to_string(tvVol());
        } else {
            reply = "TV volume: " + to_string(tvVol());
        }
    } else if (cmd == "chan") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "up") {
            tvChan(tvChan() + 1);
            reply = "TV channel UP to " + to_string(tvChan());
        } else if (sub == "down") {
            tvChan(tvChan() > 1 ? tvChan() - 1 : 1);
            reply = "TV channel DOWN to " + to_string(tvChan());
        } else if (!sub.empty()) {
            tvChan((unsigned int)strtoul(sub.c_str(), NULL, 10));
            reply = "TV channel set to " + to_string(tvChan());
        } else {
            reply = "TV channel: " + to_string(tvChan());
        }
    } else {
        reply = string("TV: ") + (tvOnOff() ? "ON" : "OFF") +
                ", Vol: " + to_string(tvVol()) +
                ", Chan: " + to_string(tvChan()) +
                ", Mute: " + (tvMute() ? "ON" : "OFF") +
                ", IR: " + tvIrProtocolStr();
    }

    return reply;
}

string MeshRoom::handleAc(uint32_t node_num, string &message)
{
    string reply;
    (void)(node_num);

    stringstream ss(message);
    string cmd;
    ss >> cmd;
    toLowercase(cmd);

    if (cmd == "on") {
        acOnOff(true);
        reply = "AC turned ON";
    } else if (cmd == "off") {
        acOnOff(false);
        reply = "AC turned OFF";
    } else if (cmd == "tx" || cmd == "blast") {
        blastAcState();
        reply = "AC state re-transmitted";
    } else if (cmd == "temp") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "up") {
            acTemp(acTemp() + 1);
            reply = "AC temp UP to " + to_string(acTemp()) + "C";
        } else if (sub == "down") {
            acTemp(acTemp() - 1);
            reply = "AC temp DOWN to " + to_string(acTemp()) + "C";
        } else if (!sub.empty()) {
            acTemp((unsigned int)strtoul(sub.c_str(), NULL, 10));
            reply = "AC temp set to " + to_string(acTemp()) + "C";
        } else {
            reply = "AC temp: " + to_string(acTemp()) + "C";
        }
    } else if (cmd == "mode") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "ac" || sub == "cool") {
            acMode(AC_AC);
            reply = "AC mode set to cool";
        } else if (sub == "heat" || sub == "heater") {
            acMode(AC_HEATER);
            reply = "AC mode set to heat";
        } else if (sub == "dry" || sub == "dehumidifier" || sub == "dehumifier") {
            acMode(AC_DEHUMIDIFIER);
            reply = "AC mode set to dry";
        } else if (sub == "fan") {
            acMode(AC_FAN);
            reply = "AC mode set to fan";
        } else if (sub == "auto") {
            acMode(AC_AUTO);
            reply = "AC mode set to auto";
        } else {
            reply = "AC mode: " + acModeStr();
        }
    } else if (cmd == "fanspeed" || cmd == "fan") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "up") {
            acFanSpeed(acFanSpeed() + 1);
            reply = "AC fanspeed UP to " + acFanSpeedStr();
        } else if (sub == "down") {
            acFanSpeed(acFanSpeed() > 0 ? acFanSpeed() - 1 : 0);
            reply = "AC fanspeed DOWN to " + acFanSpeedStr();
        } else if (sub == "auto") {
            acFanSpeed(0);
            reply = "AC fanspeed set to auto";
        } else if (sub == "quiet" || sub == "min") {
            acFanSpeed(1);
            reply = "AC fanspeed set to 1 (quiet)";
        } else if (sub == "low") {
            acFanSpeed(2);
            reply = "AC fanspeed set to 2 (low)";
        } else if (sub == "med" || sub == "medium") {
            acFanSpeed(3);
            reply = "AC fanspeed set to 3 (med)";
        } else if (sub == "high") {
            acFanSpeed(4);
            reply = "AC fanspeed set to 4 (high)";
        } else if (sub == "max") {
            acFanSpeed(5);
            reply = "AC fanspeed set to 5 (max)";
        } else if (!sub.empty()) {
            acFanSpeed((unsigned int)strtoul(sub.c_str(), NULL, 10));
            reply = "AC fanspeed set to " + acFanSpeedStr();
        } else {
            reply = "AC fanspeed: " + acFanSpeedStr();
        }
    } else if (cmd == "fandir" || cmd == "vane" || cmd == "swing") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "up") {
            acFanDir(1);
            reply = "AC vane set to 1 (up)";
        } else if (sub == "up-mid" || sub == "upmid") {
            acFanDir(2);
            reply = "AC vane set to 2 (up-mid)";
        } else if (sub == "mid" || sub == "middle") {
            acFanDir(3);
            reply = "AC vane set to 3 (mid)";
        } else if (sub == "down-mid" || sub == "downmid") {
            acFanDir(4);
            reply = "AC vane set to 4 (down-mid)";
        } else if (sub == "down") {
            acFanDir(5);
            reply = "AC vane set to 5 (down)";
        } else if (sub == "auto") {
            acFanDir(0);
            reply = "AC vane set to auto";
        } else if (!sub.empty()) {
            acFanDir((unsigned int)strtoul(sub.c_str(), NULL, 10));
            reply = "AC vane set to " + acFanDirStr();
        } else {
            reply = "AC vane: " + acFanDirStr();
        }
    } else if (cmd == "powerful" || cmd == "turbo" || cmd == "boost") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "on") {
            acPowerful(true);
        } else if (sub == "off") {
            acPowerful(false);
        } else {
            acPowerful(!acPowerful());
        }
        reply = string("AC turbo ") + (acPowerful() ? "ON" : "OFF");
    } else if (cmd == "quiet" || cmd == "silent") {
        string sub;
        ss >> sub;
        toLowercase(sub);
        if (sub == "on") {
            acQuiet(true);
        } else if (sub == "off") {
            acQuiet(false);
        } else {
            acQuiet(!acQuiet());
        }
        reply = string("AC quiet ") + (acQuiet() ? "ON" : "OFF");
    } else if (cmd == "set") {
        string token;
        while (ss >> token) {
            toLowercase(token);
            if (token == "on") {
                acOnOff(true);
            } else if (token == "off") {
                acOnOff(false);
            } else if (token == "cool" || token == "ac") {
                acMode(AC_AC);
            } else if (token == "heat" || token == "heater") {
                acMode(AC_HEATER);
            } else if (token == "dry" || token == "dehumidifier") {
                acMode(AC_DEHUMIDIFIER);
            } else if (token == "fan") {
                acMode(AC_FAN);
            } else if (token == "auto") {
                acMode(AC_AUTO);
            } else {
                char *endptr = NULL;
                unsigned long val = strtoul(token.c_str(), &endptr, 10);
                if (*endptr == '\0') {
                    if (val >= 16 && val <= 30) {
                        acTemp((unsigned int)val);
                    } else if (val <= 5) {
                        acFanSpeed((unsigned int)val);
                    }
                }
            }
        }
        reply = string("AC: ") + (acOnOff() ? "ON" : "OFF") +
                ", Mode: " + acModeStr() +
                ", Temp: " + to_string(acTemp()) + "C" +
                ", Fan: " + acFanSpeedStr();
    } else {
        reply = string("AC: ") + (acOnOff() ? "ON" : "OFF") +
                ", Mode: " + acModeStr() +
                ", Temp: " + to_string(acTemp()) + "C" +
                ", Fan: " + acFanSpeedStr() +
                ", Vane: " + acFanDirStr() +
                ", Powerful: " + (acPowerful() ? "ON" : "OFF") +
                ", Quiet: " + (acQuiet() ? "ON" : "OFF") +
                ", IR: " + acIrProtocolStr();
    }

    return reply;
}

string MeshRoom::handleReset(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    return reply;
}

string MeshRoom::handleBuzz(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    buzz();

    return reply;
}

string MeshRoom::handleMorse(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    addMorseText(message);
    reply = "buzzing morse code: '" + message + "'";

    return reply;
}

int MeshRoom::vprintf(const char *format, va_list ap) const
{
    return consoles_vprintf(format, ap);
}

#define FLASH_TARGET_SIZE   (FLASH_SECTOR_SIZE * 2)
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_TARGET_SIZE)

bool MeshRoom::loadNvm(void)
{
    bool result = false;
    size_t size = 0;
    size_t max_payload = 0;
    const struct nvm_header *header = NULL;
    const struct nvm_main_body *main_body = NULL;
    const struct nvm_authchan_entry *authchans = NULL;
    const struct nvm_admin_entry *admins = NULL;
    const struct nvm_mate_entry *mates = NULL;
    const struct nvm_footer *footer = NULL;
    uint32_t n_authchans, n_admins, n_mates;
    uint32_t crc;
    unsigned int i;

    header = (const struct nvm_header *) (XIP_BASE + FLASH_TARGET_OFFSET);
    if (header->magic != NVM_HEADER_MAGIC) {
        consoles_printf("Wrong header magic!\n");
        result = false;
        goto done;
    }

    main_body = (const struct nvm_main_body *)
        (((const uint8_t *) header) + sizeof(*header));
    n_authchans = main_body->n_authchans;
    n_admins = main_body->n_admins;
    n_mates = main_body->n_mates;

    size = sizeof(struct nvm_header) + sizeof(struct nvm_main_body) +
        sizeof(struct nvm_footer);
    if (size > FLASH_TARGET_SIZE) {
        consoles_printf("Too big size=%zu!\n", size);
        result = false;
        goto done;
    }
    max_payload = FLASH_TARGET_SIZE - size;
    if (n_authchans > (max_payload / sizeof(struct nvm_authchan_entry))) {
        consoles_printf("Too many authchans=%lu!\n",
                        (unsigned long) n_authchans);
        result = false;
        goto done;
    }
    size += n_authchans * sizeof(struct nvm_authchan_entry);
    max_payload = FLASH_TARGET_SIZE - size;
    if (n_admins > (max_payload / sizeof(struct nvm_admin_entry))) {
        consoles_printf("Too many admins=%lu!\n", (unsigned long) n_admins);
        result = false;
        goto done;
    }
    size += n_admins * sizeof(struct nvm_admin_entry);
    max_payload = FLASH_TARGET_SIZE - size;
    if (n_mates > (max_payload / sizeof(struct nvm_mate_entry))) {
        consoles_printf("Too many mates=%lu!\n", (unsigned long) n_mates);
        result = false;
        goto done;
    }
    size += n_mates * sizeof(struct nvm_mate_entry);

    authchans = (const struct nvm_authchan_entry *)
        (((uint8_t *) main_body) + sizeof(*main_body));
    admins = (const struct nvm_admin_entry *)
        (((uint8_t *) authchans) +
         (sizeof(struct nvm_authchan_entry) * n_authchans));
    mates = (const struct nvm_mate_entry *)
        (((uint8_t *) admins) +
         (sizeof(struct nvm_admin_entry) * n_admins));
    footer = (const struct nvm_footer *)
        (((uint8_t *) mates) +
         (sizeof(struct nvm_mate_entry) * n_mates));
    if (footer->magic != NVM_FOOTER_MAGIC) {
        consoles_printf("Wrong footer magic!\n");
        result = false;
        goto done;
    }
    crc = crc32_le((const uint8_t *) header,
                   size - sizeof(footer->crc32));
    if ((footer->crc32 != 0) && (footer->crc32 != crc)) {
        consoles_printf("Bad nvm crc32!\n");
        result = false;
        goto done;
    }
    memcpy(&_main_body, main_body, sizeof(struct nvm_main_body));
    _nvm_authchans.clear();
    for (i = 0; i < n_authchans; i++) {
        _nvm_authchans.push_back(authchans[i]);
    }
    _nvm_admins.clear();
    for (i = 0; i < n_admins; i++) {
        _nvm_admins.push_back(admins[i]);
    }
    _nvm_mates.clear();
    for (i = 0; i < n_mates; i++) {
        _nvm_mates.push_back(mates[i]);
    }

    result = true;

done:

    return result;
}

struct nvm_write_params {
    const uint8_t *buf;
    size_t size;
};

static void write_to_nvm(void *args)
{
    struct nvm_write_params *params = (struct nvm_write_params *) args;

    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_TARGET_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, params->buf, params->size);
}

bool MeshRoom::saveNvm(void)
{
    bool result = false;
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t prog_size = 0;
    struct nvm_header *header = NULL;
    struct nvm_main_body *main_body = NULL;
    struct nvm_authchan_entry *authchans = NULL;
    struct nvm_admin_entry *admins = NULL;
    struct nvm_mate_entry *mates = NULL;
    struct nvm_footer *footer = NULL;
    unsigned int i;
    struct nvm_write_params params;
    int rc;

    _main_body.n_authchans = nvmAuthchans().size();
    _main_body.n_admins = nvmAdmins().size();
    _main_body.n_mates = nvmMates().size();

    size =
        sizeof(struct nvm_header) +
        sizeof(struct nvm_main_body) +
        (sizeof(struct nvm_authchan_entry) * _main_body.n_authchans) +
        (sizeof(struct nvm_admin_entry) * _main_body.n_admins) +
        (sizeof(struct nvm_mate_entry) * _main_body.n_mates) +
        sizeof(struct nvm_footer);

    if (size > FLASH_TARGET_SIZE) {
        result = false;
        goto done;
    }

    prog_size = (size + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);
    if (prog_size > FLASH_TARGET_SIZE) {
        result = false;
        goto done;
    }

    buf = (uint8_t *) pvPortMalloc(prog_size);
    if (buf == NULL) {
        result = false;
        goto done;
    }

    memset(buf, 0xff, prog_size);
    memset(buf, 0x0, size);

    header = (struct nvm_header *) buf;
    header->magic = NVM_HEADER_MAGIC;
    main_body = (struct nvm_main_body *)
        (((uint8_t *) header) + sizeof(struct nvm_header));
    memcpy(main_body, &_main_body, sizeof(struct nvm_main_body));
    authchans = (struct nvm_authchan_entry *)
        (((uint8_t *) main_body) + sizeof(struct nvm_main_body));
    for (i = 0; i < _main_body.n_authchans; i++) {
        memcpy(&authchans[i], &nvmAuthchans()[i],
               sizeof(struct nvm_authchan_entry));
    }
    admins = (struct nvm_admin_entry *)
        (((uint8_t *) authchans) +
         (sizeof(struct nvm_authchan_entry) * _main_body.n_authchans));
    for (i = 0; i < _main_body.n_admins; i++) {
        memcpy(&admins[i], &nvmAdmins()[i],
               sizeof(struct nvm_admin_entry));
    }
    mates = (struct nvm_mate_entry *)
        (((uint8_t *) admins) +
         (sizeof(struct nvm_admin_entry) * _main_body.n_admins));
    for (i = 0; i < _main_body.n_mates; i++) {
        memcpy(&mates[i], &nvmMates()[i],
               sizeof(struct nvm_mate_entry));
    }
    footer = (struct nvm_footer *)
        (((uint8_t *) mates) +
         (sizeof(struct nvm_mate_entry) * _main_body.n_mates));
    footer->magic = NVM_FOOTER_MAGIC;
    footer->crc32 = crc32_le(buf, size - sizeof(footer->crc32));

    params.buf = buf;
    params.size = prog_size;
    flash_safe_execute_core_init();
    rc = flash_safe_execute(write_to_nvm, &params, 1000);
    flash_safe_execute_core_deinit();
    if (rc != PICO_OK) {
        result = false;
        goto done;
    }

    result = true;

done:

    if (buf) {
        vPortFree(buf);
    }

    return result;
}

bool MeshRoom::applyNvmToHomeChat(void)
{
    bool result = true;


    clearAuthchansAdminsMates();

    for (vector<struct nvm_authchan_entry>::const_iterator it =
             nvmAuthchans().begin(); it != nvmAuthchans().end(); it++) {
        if (addAuthChannel(it->name, it->psk) == false) {
            result = false;
        }
    }

    for (vector<struct nvm_admin_entry>::const_iterator it =
             nvmAdmins().begin(); it != nvmAdmins().end(); it++) {
        if (addAdmin(it->node_num, it->pubkey) == false) {
            result = false;
        }
    }

    for (vector<struct nvm_mate_entry>::const_iterator it =
             nvmMates().begin(); it != nvmMates().end(); it++) {
        if (addMate(it->node_num, it->pubkey) == false) {
            result = false;
        }
    }

    return result;
}

void MeshRoom::sleepForMs(unsigned int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void MeshRoom::toggleBuzzer(bool onOff)
{
    if (onOff) {
        gpio_put(BUZZER_PIN, true);
    } else {
        gpio_put(BUZZER_PIN, false);
    }
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
