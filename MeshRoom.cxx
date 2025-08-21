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
#include <hardware/adc.h>
#include <hardware/sync.h>
#include <FreeRTOS.h>
#include <task.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <meshroom.h>
#include <MeshRoom.hxx>

extern shared_ptr<MeshRoom> meshroom;

MeshRoom::MeshRoom()
    : SimpleClient(), HomeChat(), BaseNvm()
{
    bzero(&_main_body, sizeof(_main_body));
    _main_body.ir_flags =
        MESHROOM_IR_SONY_BRAVIA |
        MESHROOM_IR_PANASONIC_AC;
    _tvOnOff = false;
    _tvVol = 10;
    _tvChan = 1;
    _acOnOff = false;
    _acMode = AC_AC;
    _acTemp = 24;
    _acFanSpeed = 0;
    _acFanDir = 0;
    _resetCount = 0;
    _lastReset = 1;

    gpio_init(PUSHBUTTON_PIN);
    gpio_set_dir(PUSHBUTTON_PIN, GPIO_IN);
    gpio_pull_up(PUSHBUTTON_PIN);
    gpio_set_irq_enabled_with_callback(PUSHBUTTON_PIN,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true,
                                       MeshRoom::gpio_callback);

    gpio_init(OUTRESET_PIN);
    gpio_set_dir(OUTRESET_PIN, GPIO_OUT);
    gpio_put(OUTRESET_PIN, true);

    gpio_init(IR_BLAST_PIN);
    gpio_set_dir(IR_BLAST_PIN, GPIO_OUT);
    gpio_put(IR_BLAST_PIN, false);

    gpio_init(ALERT_LED_PIN);
    gpio_set_dir(ALERT_LED_PIN, GPIO_OUT);
    setAlertLed(false);

    gpio_init(ONBOARD_LED_PIN);
    gpio_set_dir(ONBOARD_LED_PIN, GPIO_OUT);
    setOnboardLed(false);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
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

    meshroom->_buttonEvents.push_back(event);

done:

    return;
}

bool MeshRoom::getButtonEvent(struct button_event &event, bool clearOld)
{
    bool result = false;

    if (_buttonEvents.empty()) {
        goto done;
    }

    if (clearOld) {
        event = _buttonEvents.back();
        _buttonEvents.clear();
    } else {
        event = _buttonEvents.front();
    }

done:

    return result;
}

void MeshRoom::tvOnOff(bool onOff)
{
    _tvOnOff = onOff;
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

    _tvChan = chan;
}

unsigned int MeshRoom::tvChan(void) const
{
    return _tvChan;
}

void MeshRoom::acOnOff(bool onOff)
{
    _acOnOff = onOff;
}

bool MeshRoom::acOnOff(void) const
{
    return _acOnOff;
}

void MeshRoom::acMode(enum AcMode mode)
{
    if ((mode >= AC_AC) && (mode <= AC_AUTO)) {
        _acMode = mode;
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
        s = "ac";
        break;
    case AC_HEATER:
        s = "heater";
        break;
    case AC_DEHUMIDIFIER:
        s = "dehumidifier";
        break;
    case AC_AUTO:
        s = "auto";
        break;
    default:
        break;
    }

    return s;
}

void MeshRoom::acTemp(unsigned int temp)
{
    if ((temp >= 20) && (temp <= 30)) {
        _acTemp = temp;
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
    }
}

unsigned int MeshRoom::acFanSpeed(void) const
{
    return _acFanSpeed;
}

void MeshRoom::acFanDir(unsigned int dir)
{
    if (dir <= 6) {
        _acFanDir = dir;
    }
}

unsigned int MeshRoom::acFanDir(void) const
{
    return _acFanDir;
}

void MeshRoom::reset(void)
{
    _resetCount++;
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

void MeshRoom::buzz(unsigned int ms)
{
    (void)(ms);
}

void MeshRoom::buzzMorseCode(const string &text, bool clearPrevious)
{
    (void)(text);
    (void)(clearPrevious);
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

bool MeshRoom::isOnboardLedOn(void) const
{
    return _onboardLed;
}

void MeshRoom::setOnboardLed(bool onOff)
{
    _onboardLed = onOff;
    gpio_put(ONBOARD_LED_PIN, onOff);
}

void MeshRoom::flipOnboardLed(void)
{
    _onboardLed = !_onboardLed;
    gpio_put(ONBOARD_LED_PIN, _onboardLed);
}

float MeshRoom::getOnboardTempC(void) const
{
    static const float conversionFactor = 3.3f / (1 << 12);
    float adc;
    float temperature_c = 0.0;

    adc = (float) adc_read() * conversionFactor;
    temperature_c = 27.0f - (adc - 0.706f) / 0.001721f;

    return temperature_c;
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

string MeshRoom::handleUnknown(uint32_t node_num, string &message)
{
    string reply;
    string first_word;

    (void)(node_num);

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
    }

    return reply;
}

string MeshRoom::handleStatus(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

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
    (void)(message);

    return reply;
}

string MeshRoom::handleAc(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    return reply;
}

string MeshRoom::handleReset(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

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
    const struct nvm_header *header = NULL;
    const struct nvm_main_body *main_body = NULL;
    const struct nvm_authchan_entry *authchans = NULL;
    const struct nvm_admin_entry *admins = NULL;
    const struct nvm_mate_entry *mates = NULL;
    const struct nvm_footer *footer = NULL;
    unsigned int i;

    header = (const struct nvm_header *) (XIP_BASE + FLASH_TARGET_OFFSET);
    if (header->magic != NVM_HEADER_MAGIC) {
        consoles_printf("Wrong header magic!\n");
        result = false;
        goto done;
    }

    main_body = (const struct nvm_main_body *)
        (((const uint8_t *) header) + sizeof(*header));
    size =
        sizeof(struct nvm_header) +
        sizeof(struct nvm_main_body) +
        (main_body->n_authchans * sizeof(struct nvm_authchan_entry)) +
        (main_body->n_admins * sizeof(struct nvm_admin_entry)) +
        (main_body->n_mates * sizeof(struct nvm_mate_entry)) +
        sizeof(struct nvm_footer);
    if (size > FLASH_TARGET_SIZE) {
        consoles_printf("Too big size=%zu!\n", size);
        result = false;
        goto done;
    }
    authchans = (const struct nvm_authchan_entry *)
        (((uint8_t *) main_body) + sizeof(*main_body));
    admins = (const struct nvm_admin_entry *)
        (((uint8_t *) authchans) +
         (sizeof(struct nvm_authchan_entry) * main_body->n_authchans));
    mates = (const struct nvm_mate_entry *)
        (((uint8_t *) admins) +
         (sizeof(struct nvm_admin_entry) * main_body->n_admins));
    footer = (const struct nvm_footer *)
        (((uint8_t *) mates) +
         (sizeof(struct nvm_mate_entry) * main_body->n_mates));
    if (footer->magic != NVM_FOOTER_MAGIC) {
        consoles_printf("Wrong footer magic!\n");
        result = false;
        goto done;
    }
    memcpy(&_main_body, main_body, sizeof(struct nvm_main_body));
    _nvm_authchans.clear();
    for (i = 0; i < main_body->n_authchans; i++) {
        _nvm_authchans.push_back(authchans[i]);
    }
    _nvm_admins.clear();
    for (i = 0; i < main_body->n_admins; i++) {
        _nvm_admins.push_back(admins[i]);
    }
    _nvm_mates.clear();
    for (i = 0; i < main_body->n_mates; i++) {
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
    struct nvm_header *header = NULL;
    struct nvm_main_body *main_body = NULL;
    struct nvm_authchan_entry *authchans = NULL;
    struct nvm_admin_entry *admins = NULL;
    struct nvm_mate_entry *mates = NULL;
    struct nvm_footer *footer = NULL;
    unsigned int i;
    struct nvm_write_params params;

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

    buf = (uint8_t *) pvPortMalloc(size);
    if (buf == NULL) {
        result = false;
        goto done;
    }

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
    footer->crc32 = 0;

    params.buf = buf;
    params.size = size;
    flash_safe_execute_core_init();
    flash_safe_execute(write_to_nvm, &params, 1000);
    flash_safe_execute_core_deinit();

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

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
