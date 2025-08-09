/*
 * MeshRoom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
#include <stdarg.h>
#include <hardware/sync.h>
#include <hardware/flash.h>
#include <algorithm>
#include <meshroom.h>
#include <MeshRoom.hxx>

MeshRoom::MeshRoom()
    : SimpleClient(), HomeChat(), BaseNVM()
{
    bzero(&_main_body, sizeof(_main_body));
    _main_body.ir_flags =
        MESHROOM_IR_SONY_BRAVIA |
        MESHROOM_IR_PANASONIC_AC;
}

MeshRoom::~MeshRoom()
{

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
        console_printf("traceroute from %s -> %s[%.2fdB]\n",
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
        console_printf("traceroute from %s -> ",
                       getDisplayName(packet.from).c_str());
        for (unsigned int i = 0; i < routeDiscovery.route_count; i++) {
            if (i > 0) {
                console_printf(" -> ");
            }
            console_printf("%s",
                           getDisplayName(routeDiscovery.route[i]).c_str());
            if (routeDiscovery.snr_towards[i] != INT8_MIN) {
                rx_snr = routeDiscovery.snr_towards[i];
                rx_snr /= 4.0;
                console_printf("[%.2fdB]", rx_snr);
            } else {
                console_printf("[???dB]");
            }
        }
        rx_snr = packet.rx_snr;
        console_printf(" -> %s[%.2fdB]\n",
                       getDisplayName(packet.to).c_str(), rx_snr);
    }
}

string MeshRoom::handleMeshAuth(uint32_t node_num, const string &message)
{
    string reply = HomeChat::handleMeshAuth(node_num, message);

    if (reply.empty()) {
        // Update NVM
    }

    return reply;
}

int MeshRoom::vprintf(const char *format, va_list ap) const
{
    return console_vprintf(format, ap);
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
        console_printf("Wrong header magic!\n");
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
        console_printf("Too big size=%zu!\n", size);
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
        console_printf("Wrong footer magic!\n");
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
    uint32_t state;

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

    buf = (uint8_t *) malloc(size);
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

    state = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_TARGET_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buf, size);
    restore_interrupts(state);

    result = true;

done:

    if (buf) {
        free(buf);
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
