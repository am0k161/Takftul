#ifndef KEEB_ONSET_H
#define KEEB_ONSET_H

#include <cstdint>
#include <vector>

namespace keeb {

std::vector<float> compute_onset_envelope(const std::vector<std::vector<float> >& mag_frames,
                                          bool hfc_weight);

} // namespace keeb

#endif
