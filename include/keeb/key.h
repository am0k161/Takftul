#ifndef KEEB_KEY_H
#define KEEB_KEY_H

#include <keeb/api.h>

namespace keeb {

KeyResult estimate_key(const std::vector<float>& pcp, const Params& params);

} // namespace keeb

#endif
