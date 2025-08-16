/*
 * MeshRoomShell.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOMSHELL_HXX
#define MESHROOMSHELL_HXX

#include <SimpleShell.hxx>

using namespace std;

class MeshRoomShell : public SimpleShell {

public:

    MeshRoomShell(shared_ptr<SimpleClient> client = NULL);

    ~MeshRoomShell();

protected:

    virtual int printf(const char *format, ...);
    virtual int rx_ready(void) const;
    virtual int rx_read(uint8_t *buf, size_t size);

    virtual int system(int argc, char **argv);
    virtual int reboot(int argc, char **argv);
    virtual int nvm(int argc, char **argv);
    virtual int ir(int argc, char **argv);
    virtual int bootsel(int argc, char **argv);
    virtual int wcfg(int argc, char **argv);
    virtual int disc(int argc, char **argv);
    virtual int hb(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

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
