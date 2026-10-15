#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

namespace mcov::auth {
inline constexpr char kAppName[]="mcov",kOwnerId[]="Uhn5Z5WPCa",kVersion[]="1.0";
inline constexpr std::size_t kMaxResponse=65536,kMaxSession=256;
using PublicKey=std::array<std::uint8_t,32>;
void Wipe(std::string& value) noexcept;
struct Session {
    std::string id;
    std::int64_t expires=0;
    Session()=default;
    Session(std::string value,std::int64_t expiry):id(std::move(value)),expires(expiry) {}
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;
    ~Session();
    bool Valid(std::int64_t now) const;
};
struct Status { bool success=false; std::string message; };
struct LoginResult : Status { Session session; };
struct HttpResponse {
    unsigned status=0;
    std::string body,signature,timestamp;
    ~HttpResponse() { Wipe(body); }
};
using Transport=std::function<HttpResponse(std::string_view,std::stop_token)>;
std::int64_t UnixTime();
std::string EncodeForm(std::string_view value);
bool VerifyResponse(const HttpResponse& response,const PublicKey& key,std::int64_t now);
HttpResponse Request(std::string_view form,std::stop_token stop={});
class Client {
public:
    Client();
    // Dependency injection for isolated, signed protocol fixtures; production
    // uses the constructor above and the fixed KeyAuth HTTPS endpoint/key.
    Client(Transport transport,PublicKey verificationKey);
    LoginResult Login(std::string license,std::stop_token stop={});
    Status Check(const Session& session,std::stop_token stop={});
    Status Probe(std::stop_token stop={}); // Initializes only; never authenticates.
private:
    Transport transport_;
    PublicKey key_{};
};
}
