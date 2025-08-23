/*
 * MeshRoom.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOM_HXX
#define MESHROOM_HXX

#include <vector>
#include <SimpleClient.hxx>
#include <HomeChat.hxx>
#include <BaseNvm.hxx>
#include <MorseBuzzer.hxx>

#define PUSHBUTTON_PIN   13
#define OUTRESET_PIN     14
#define BUZZER_PIN       22
#define IR_BLAST_PIN     17
#define ALERT_LED_PIN    21
#define ONBOARD_LED_PIN  25

#define PUSHBUTTON_DURATION_THRESHOLD_US 1500000

using namespace std;

struct nvm_header {
    uint32_t magic;
#define NVM_HEADER_MAGIC 0x6a87f421
} __attribute__((packed));

struct nvm_main_body {
    uint32_t ir_flags;
#define MESHROOM_IR_SONY_BRAVIA   0x1
#define MESHROOM_IR_SAMSUNG_TV    0x2
#define MESHROOM_IR_PANASONIC_AC  0x4

    uint32_t n_authchans;
    uint32_t n_admins;
    uint32_t n_mates;
} __attribute__((packed));

struct nvm_footer {
    uint32_t magic;
#define NVM_FOOTER_MAGIC 0xe8148afd
    uint32_t crc32;
} __attribute__((packed));


struct button_event {
    uint64_t ts;
    uint64_t tdur;
};

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class MeshRoom : public SimpleClient, public HomeChat, public BaseNvm,
                 public MorseBuzzer,
                 public enable_shared_from_this<MeshRoom> {

public:

    MeshRoom();
    ~MeshRoom();

    void tvOnOff(bool onOff);
    bool tvOnOff(void) const;
    void tvVol(unsigned int volume);
    unsigned int tvVol(void) const;
    void tvChan(unsigned int chan);
    unsigned int tvChan(void) const;

    enum AcMode {
        AC_AC,
        AC_HEATER,
        AC_DEHUMIDIFIER,
        AC_AUTO,
    };

    bool getButtonEvent(struct button_event &event, bool clearOld = false);

    void acOnOff(bool onOff);
    bool acOnOff(void) const;
    void acMode(enum AcMode mode);
    enum AcMode acMode(void) const;
    string acModeStr(void) const;
    void acTemp(unsigned int temp);
    unsigned int acTemp(void) const;
    void acFanSpeed(unsigned int speed);
    unsigned int acFanSpeed(void) const;
    void acFanDir(unsigned int dir);
    unsigned int acFanDir(void) const;

    void reset(void);
    unsigned int getResetCount(void) const;
    time_t getLastReset(void) const;
    unsigned int getLastResetSecsAgo(void) const;

    void buzz(unsigned int ms = 500);
    void buzzMorseCode(const string &text, bool clearPrevious = false);

    bool isAlertLedOn(void) const;
    void setAlertLed(bool onOff);
    void flipAlertLed(void);

    bool isOnboardLedOn(void) const;
    void setOnboardLed(bool onOff);
    void flipOnboardLed(void);

    float getOnboardTempC(void) const;

protected:

    // Extend SimpleClient

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);
    virtual void gotTelemetry(const meshtastic_MeshPacket &packet,
                              const meshtastic_Telemetry &telemetry);
    virtual void gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing);
    virtual void gotTraceRoute(const meshtastic_MeshPacket &packet,
                               const meshtastic_RouteDiscovery &routeDiscovery);

protected:

    // Extend HomeChat

    virtual string handleUnknown(uint32_t node_num, string &message);
    virtual string handleEnv(uint32_t node_num, string &message);
    virtual string handleStatus(uint32_t node_num, string &message);
    virtual string handleTv(uint32_t node_num, string &message);
    virtual string handleAc(uint32_t node_num, string &message);
    virtual string handleReset(uint32_t node_num, string &message);
    virtual string handleBuzz(uint32_t node_num, string &message);
    virtual string handleMorse(uint32_t node_num, string &message);
    virtual int vprintf(const char *format, va_list ap) const;

public:

    // Extend BaseNVM

    inline uint32_t ir_flags(void) const {
        return _main_body.ir_flags;
    }

    void set_ir_flags(uint32_t ir_flags) {
        _main_body.ir_flags = ir_flags;
    }

    virtual bool loadNvm(void);
    virtual bool saveNvm(void);
    bool applyNvmToHomeChat(void);

protected:

    // Extend MorseBuzzer

    virtual void sleepForMs(unsigned int ms);
    virtual void toggleBuzzer(bool onOff);

private:

    static void gpio_callback(uint gpio, uint32_t events);

    struct nvm_main_body _main_body;

    vector<struct button_event> _buttonEvents;
    bool _tvOnOff;
    unsigned int _tvVol;
    unsigned int _tvChan;
    bool _acOnOff;
    AcMode _acMode;
    unsigned int _acTemp;
    unsigned int _acFanSpeed;
    unsigned int _acFanDir;
    unsigned int _resetCount;
    time_t _lastReset;
    vector<string> _morseText;
    bool _alertLed;
    bool _onboardLed;

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
