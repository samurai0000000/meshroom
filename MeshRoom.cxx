/*
 * MeshRoom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stddef.h>
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
    SimpleClient::gotTextMessage(packet, message);
    string reply;
    bool result = false;

    if (packet.to == whoami()) {
        serial_printf(console_chan, "%s: %s\n",
                      getDisplayName(packet.from).c_str(), message.c_str());

        reply = lookupShortName(packet.from) + ", you said '" + message + "'!";
        if (reply.size() > 200) {
            reply = "oopsie daisie!";
        }

        result = textMessage(packet.from, packet.channel, reply);
        if (result == false) {
            serial_printf(console_chan, "textMessage '%s' failed!\n",
                          reply.c_str());
        } else {
            serial_printf(console_chan, "my_reply to %s: %s\n",
                          getDisplayName(packet.from).c_str(),
                          reply.c_str());
        }
    } else {
        serial_printf(console_chan, "%s on #%s: %s\n",
                      getDisplayName(packet.from).c_str(),
                      getChannelName(packet.channel).c_str(),
                      message.c_str());
        if (getChannelName(packet.channel) == "serial") {
            string msg = message;
            transform(msg.begin(), msg.end(), msg.begin(),
                      [](unsigned char c) {
                          return tolower(c); });
            if (msg.find("hello") != string::npos) {
                reply = "greetings, " + lookupShortName(packet.from) + "!";
            } else if (msg.find(lookupShortName(whoami())) != string::npos) {
                reply = lookupShortName(packet.from) + ", you said '" +
                    message + "'!";
                if (reply.size() > 200) {
                    reply = "oopsie daisie!";
                }
            }

            if (!reply.empty()) {
                result = textMessage(0xffffffffU, packet.channel, reply);
                if (result == false) {
                    serial_printf(console_chan, "textMessage '%s' failed!\n",
                                  reply.c_str());
                } else {
                    serial_printf(console_chan, "my reply to %s: %s\n",
                                  getDisplayName(packet.from).c_str(),
                                  reply.c_str());
                }
            }
        }
    }
}

void MeshRoom::gotPosition(const meshtastic_MeshPacket &packet,
			   const meshtastic_Position &position)
{
    SimpleClient::gotPosition(packet, position);
    serial_printf(console_chan, "%s sent position\n",
                  getDisplayName(packet.from).c_str());
}

void MeshRoom::gotRouting(const meshtastic_MeshPacket &packet,
                         const meshtastic_Routing &routing)
{
    SimpleClient::gotRouting(packet, routing);
    if ((routing.which_variant == meshtastic_Routing_error_reason_tag) &&
        (routing.error_reason == meshtastic_Routing_Error_NONE) &&
        (packet.from != packet.to)) {
        serial_printf(console_chan,
                      "traceroute from %s -> %s[%.2fdB]\n",
                      getDisplayName(packet.from).c_str(),
                      packet.rx_snr);
    }
}

void MeshRoom::gotDeviceMetrics(const meshtastic_MeshPacket &packet,
				const meshtastic_DeviceMetrics &metrics)
{
    SimpleClient::gotDeviceMetrics(packet, metrics);
    serial_printf(console_chan, "%s sent device metrics\n",
                  getDisplayName(packet.from).c_str());
}

void MeshRoom::gotEnvironmentMetrics(const meshtastic_MeshPacket &packet,
				     const meshtastic_EnvironmentMetrics &metrics)
{
    SimpleClient::gotEnvironmentMetrics(packet, metrics);
    serial_printf(console_chan, "%s sent environment metrics\n",
                  getDisplayName(packet.from).c_str());
}

void MeshRoom::gotTraceRoute(const meshtastic_MeshPacket &packet,
                             const meshtastic_RouteDiscovery &routeDiscovery)
{
    SimpleClient::gotTraceRoute(packet, routeDiscovery);
    if ((routeDiscovery.route_count > 0) &&
        (routeDiscovery.route_back_count == 0)) {
        float rx_snr;
        serial_printf(console_chan, "traceroute from %s -> ",
                      getDisplayName(packet.from).c_str());
        for (unsigned int i = 0; i < routeDiscovery.route_count; i++) {
            if (i > 0) {
                serial_printf(console_chan, " -> ");
            }
            serial_printf(console_chan, "%s",
                          getDisplayName(routeDiscovery.route[i]).c_str());
            if (routeDiscovery.snr_towards[i] != INT8_MIN) {
                rx_snr = routeDiscovery.snr_towards[i];
                rx_snr /= 4.0;
                serial_printf(console_chan, "[%.2fdB]", rx_snr);
            } else {
                serial_printf(console_chan, "[???dB]");
            }
        }
        rx_snr = packet.rx_snr;
        serial_printf(console_chan, " -> %s[%.2fdB]\n",
                      getDisplayName(packet.to).c_str(), rx_snr);
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
