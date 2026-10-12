# kiss_fft (optional)

This directory is a placeholder for a vendored [KissFFT](https://github.com/mborgerding/kissfft) tree (BSD-3-Clause).

Enable with:

    cmake .. -DKeeB_USE_KISSFFT=ON

The default build uses the in-house radix-2 RFFT in `src/fft.cpp` and does not require KissFFT.
