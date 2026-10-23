# Build preference

For user-facing builds, always build the portable CMake target `mcov` in the
existing `../build` directory. The deliverable is `../build/client/Keim.exe`
(`keim.exe` on the case-insensitive Windows filesystem). Do not deliver only
`build/bin/mcov_injector.exe`; that is a developer target.
