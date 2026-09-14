// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYAPNG_H
#define UTILITYAPNG_H

#include <vector>
#include <QByteArray>

// Assembles an animated PNG (APNG) from a list of still frames.
// Each input frame may be any format QImage can decode (PNG / GIF / JPEG).
// Frames are normalised to the first frame's size and re-encoded, so the IDAT
// streams can be spliced verbatim into fdAT chunks - no deflate step is needed.
class UtilityApng {
public:
    // delayMs is the on-screen time per frame. Returns "" if fewer than two
    // frames decode successfully.
    static QByteArray fromFrames(const std::vector<QByteArray>& frames, int delayMs);
};

#endif  // UTILITYAPNG_H
