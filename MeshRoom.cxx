/*
 * MeshRoom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
#include <stdarg.h>
#include <hardware/uart.h>
#include <algorithm>
#include <meshroom.h>
#include <MeshRoom.hxx>

MeshRoom::MeshRoom()
    : SimpleClient()
{

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

int MeshRoom::vprintf(const char *format, va_list ap) const
{
    return console_vprintf(format, ap);
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
