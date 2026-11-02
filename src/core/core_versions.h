// Each game's core version, x.y.z (afcore_api.core_version; what the
// numbers mean is in include/afcore.h). Bump a game's line in the same
// commit as the change to its core.
#pragma once

#define AFCORE_CORE_VERSION_daytona93 AFCORE_MAKE_VERSION(1, 2, 1)
#define AFCORE_CORE_VERSION_daytona AFCORE_MAKE_VERSION(1, 2, 2)
#define AFCORE_CORE_VERSION_srallyc AFCORE_MAKE_VERSION(1, 2, 0)
#define AFCORE_CORE_VERSION_manxttc AFCORE_MAKE_VERSION(1, 2, 0)
#define AFCORE_CORE_VERSION_vcop2 AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_vcop AFCORE_MAKE_VERSION(1, 2, 1)
#define AFCORE_CORE_VERSION_vf2 AFCORE_MAKE_VERSION(1, 1, 2)
#define AFCORE_CORE_VERSION_indy500 AFCORE_MAKE_VERSION(1, 2, 0)
#define AFCORE_CORE_VERSION_fvipers AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_sgt24h AFCORE_MAKE_VERSION(1, 2, 0)
#define AFCORE_CORE_VERSION_overrevb AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_stcc AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_von AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_hotdo AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_gunblade AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_vstriker AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_zerogun AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_pltkids AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_motoraid AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_dynamcop AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_skytargt AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_desert AFCORE_MAKE_VERSION(1, 1, 2)
#define AFCORE_CORE_VERSION_lastbrnx AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_schamp AFCORE_MAKE_VERSION(1, 1, 3)
#define AFCORE_CORE_VERSION_dynabb97 AFCORE_MAKE_VERSION(1, 0, 3)
#define AFCORE_CORE_VERSION_waverunr AFCORE_MAKE_VERSION(1, 0, 3)
#define AFCORE_CORE_VERSION_segawski AFCORE_MAKE_VERSION(1, 0, 4)
#define AFCORE_CORE_VERSION_skisuprg AFCORE_MAKE_VERSION(1, 0, 3)
#define AFCORE_CORE_VERSION_bel AFCORE_MAKE_VERSION(1, 0, 3)
#define AFCORE_CORE_VERSION_topskatr AFCORE_MAKE_VERSION(1, 0, 3)
#define AFCORE_CORE_VERSION_rchase2 AFCORE_MAKE_VERSION(1, 1, 0)
#define AFCORE_CORE_VERSION_doab AFCORE_MAKE_VERSION(1, 0, 3)
#define AFCORE_CORE_VERSION_vr AFCORE_MAKE_VERSION(1, 1, 1)
#define AFCORE_CORE_VERSION_swa AFCORE_MAKE_VERSION(1, 1, 0)
#define AFCORE_CORE_VERSION_vf AFCORE_MAKE_VERSION(1, 1, 0)
#define AFCORE_CORE_VERSION_wingwar AFCORE_MAKE_VERSION(1, 1, 0)

#define AFCORE_CORE_VERSION_(set) AFCORE_CORE_VERSION_##set
#define AFCORE_CORE_VERSION(set) AFCORE_CORE_VERSION_(set)

#include <cstring>

// The front end's input as this core knows it: fields an older front end
// does not have (past its size) read as zero.
inline afcore_input afcore_read_input(const afcore_input *in) {
    afcore_input out{};
    if (in && in->size > sizeof(uint32_t)) std::memcpy(&out, in, in->size < sizeof out ? in->size : sizeof out);
    out.size = sizeof out;
    return out;
}

// The same for render_eyes' stereo settings.
inline afcore_stereo afcore_read_stereo(const afcore_stereo *in) {
    afcore_stereo out{};
    if (in && in->size > sizeof(uint32_t)) std::memcpy(&out, in, in->size < sizeof out ? in->size : sizeof out);
    out.size = sizeof out;
    return out;
}
