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
#include <BaseNVM.hxx>

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

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class MeshRoom : public SimpleClient, public HomeChat, public BaseNVM,
                 public enable_shared_from_this<MeshRoom> {

public:

    MeshRoom();
    ~MeshRoom();

protected:

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);
    virtual void gotTelemetry(const meshtastic_MeshPacket &packet,
                              const meshtastic_Telemetry &telemetry);
    virtual void gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing);
    virtual void gotTraceRoute(const meshtastic_MeshPacket &packet,
                               const meshtastic_RouteDiscovery &routeDiscovery);

protected:

    virtual string handleMeshAuth(uint32_t node_num, const string &message);
    virtual int vprintf(const char *format, va_list ap) const;

public:

    inline uint32_t ir_flags(void) const {
        return _main_body.ir_flags;
    }

    void set_ir_flags(uint32_t ir_flags) {
        _main_body.ir_flags = ir_flags;
    }

    virtual bool loadNvm(void);
    virtual bool saveNvm(void);
    bool applyNvmToHomeChat(void);

private:

    struct nvm_main_body _main_body;

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
