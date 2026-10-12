#ifndef KEEB_CHROMA_H
#define KEEB_CHROMA_H

#include <keeb/api.h>

namespace keeb {

std::vector<std::vector<float> > compute_chroma(const std::vector<std::vector<float> >& mag_frames,
                                                uint32_t fft_size,
                                                uint32_t sample_rate,
                                                const Params& params);

std::vector<float> aggregate_pcp(const std::vector<std::vector<float> >& chroma);

} // namespace keeb

#endif
