// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYGIF_H
#define UTILITYGIF_H

#include <vector>
#include <QByteArray>
#include <QImage>

// Writes an animated GIF from a list of frames, with no external tool. One palette of 256 colors is chosen from all the frames (median cut, weighted by how often
// a color is used) so the colors do not flicker between frames; each frame after the first stores only the rectangle that changed. Transparent pixels are flattened onto white.
class UtilityGif {
public:
    // delayMs is the on-screen time per frame (GIF counts hundredths of a second, at least 2). Frames are made the size of the first. Empty with no frames.
    static QByteArray fromImages(const std::vector<QImage>& frames, int delayMs);
};

#endif  // UTILITYGIF_H
