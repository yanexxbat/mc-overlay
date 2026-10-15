# Authentication dependencies

Unmodified pinned source dependencies, compiled statically into the native
client. Full copyright and license texts are retained here and embedded into
the production executables as RCDATA 104 and 105.

- nlohmann/json **3.12.0**, MIT license.
  Source: https://github.com/nlohmann/json/tree/v3.12.0
  Single header: `single_include/nlohmann/json.hpp`.
- Monocypher **4.0.2**, dual BSD-2-Clause / CC0; mcov uses BSD-2-Clause.
  Source: https://github.com/LoupVaillant/Monocypher/tree/4.0.2
  Core and optional Ed25519/SHA-512 implementation under `src/`.

The KeyAuth 1.3 wire protocol and public response verification key are taken
from its official API documentation and C++ reference. The KeyAuth SDK itself
is not vendored or linked; mcov implements HTTPS requests with WinHTTP.

- https://docs.keyauth.cc/api/getting-started/initialization
- https://docs.keyauth.cc/api/features/check-session
- https://github.com/KeyAuth/keyauth-cpp-library-1.3API
