/*
 * MeshRoom.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOM_HXX
#define MESHROOM_HXX

#include <SimpleClient.hxx>
#include <HomeChat.hxx>

using namespace std;

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class MeshRoom : public SimpleClient, public HomeChat,
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

    virtual int vprintf(const char *format, va_list ap) const;

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
