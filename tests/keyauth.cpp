#include "common/keyauth.hpp"
#include "common/auth_handoff.hpp"
#include "common/win_handle.hpp"
#include <optional/monocypher-ed25519.h>
#include <nlohmann/json.hpp>
#include <array>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace mcov;
using Json=nlohmann::json;
namespace {
void Require(bool ok,const char* reason) { if(!ok) throw std::runtime_error(reason); }
std::string Hex(const std::uint8_t* bytes,std::size_t size) {
    constexpr char digits[]="0123456789abcdef"; std::string text;
    for(std::size_t i=0;i<size;++i) { text+=digits[bytes[i]>>4]; text+=digits[bytes[i]&15]; } return text;
}
struct Server {
    auth::PublicKey key{}; std::array<std::uint8_t,64> secret{};
    Server() { std::array<std::uint8_t,32> seed{}; for(int i=0;i<32;++i) seed[i]=std::uint8_t(i+1); crypto_ed25519_key_pair(secret.data(),key.data(),seed.data()); }
    auth::HttpResponse Raw(std::string body,std::int64_t time=auth::UnixTime()) {
        auth::HttpResponse r; r.status=200; r.body=std::move(body); r.timestamp=std::to_string(time);
        const auto message=r.timestamp+r.body; std::array<std::uint8_t,64> signature{};
        crypto_ed25519_sign(signature.data(),secret.data(),reinterpret_cast<const std::uint8_t*>(message.data()),message.size());
        r.signature=Hex(signature.data(),signature.size()); return r;
    }
    auth::HttpResponse Reply(Json value,std::int64_t time=auth::UnixTime()) { return Raw(value.dump(),time); }
    Json Ok() { return {{"success",true},{"code",68},{"ownerid",auth::kOwnerId},{"message","OK"}}; }
    Json Init() { auto j=Ok(); j["sessionid"]="fixture-session"; return j; }
    Json License(std::int64_t expiry=auth::UnixTime()+3600) {
        auto j=Ok(); j["info"]={{"subscriptions",Json::array({{{"subscription","default"},{"expiry",std::to_string(expiry)}}})}}; return j;
    }
};
void Signatures() {
    Server server; auto r=server.Reply(server.Ok());
    Require(auth::VerifyResponse(r,server.key,auth::UnixTime()),"Valid Ed25519 response rejected");
    r.body+=' '; Require(!auth::VerifyResponse(r,server.key,auth::UnixTime()),"Modified response was trusted");
    r=server.Reply(server.Ok(),auth::UnixTime()-121); Require(!auth::VerifyResponse(r,server.key,auth::UnixTime()),"Stale response was trusted");
    r=server.Reply(server.Ok(),auth::UnixTime()+121); Require(!auth::VerifyResponse(r,server.key,auth::UnixTime()),"Future response was trusted");
    r=server.Reply(server.Ok()); r.signature[0]='z'; Require(!auth::VerifyResponse(r,server.key,auth::UnixTime()),"Non-hex signature was trusted");
    r=server.Reply(server.Ok()); r.signature.clear(); Require(!auth::VerifyResponse(r,server.key,auth::UnixTime()),"Unsigned response was trusted");
    r=server.Reply(server.Ok()); auto wrong=server.key; wrong[0]^=1;
    Require(!auth::VerifyResponse(r,wrong,auth::UnixTime()),"Wrong signing key was accepted");
    r=server.Raw(std::string(auth::kMaxResponse+1,'a')); Require(!auth::VerifyResponse(r,server.key,auth::UnixTime()),"Oversized response accepted");
}
void Protocol() {
    Server server; int calls=0;
    const std::string license="TEST&KEY=+ ?";
    const auto hardwareId=auth::HardwareId();
    Require(hardwareId.starts_with("S-1-")&&hardwareId==auth::HardwareId(),"Launcher HWID is unavailable or unstable");
    auth::Client client([&](std::string_view form,std::stop_token) {
        ++calls;
        Require(form.find("&name=mcov&ownerid=Uhn5Z5WPCa")!=form.npos,"Wrong application identity");
        if(form.starts_with("type=init")) { Require(form.find("&ver=1.0&hash=")!=form.npos,"Version/hash missing"); return server.Reply(server.Init()); }
        if(form.starts_with("type=license")) {
            Require(form.find("&key=TEST%26KEY%3D%2B%20%3F")!=form.npos&&form.ends_with("&hwid="+auth::EncodeForm(hardwareId)),"License encoding or displayed/authenticated HWID mismatch");
            return server.Reply(server.License());
        }
        Require(form.starts_with("type=check")&&form.find("&sessionid=fixture-session")!=form.npos,"Check omitted session");
        return server.Reply(server.Ok());
    },server.key);
    auto login=client.Login(license); Require(login.success&&login.session.Valid(auth::UnixTime())&&calls==2,"Valid license did not unlock a session");
    Require(client.Check(login.session).success&&calls==3,"Authenticated session check failed");
    auth::Session moved=std::move(login.session); Require(!login.session.Valid(auth::UnixTime())&&moved.Valid(auth::UnixTime()),"Moved session retained authorization");
    auth::Session expired("fixture-session",auth::UnixTime()-1);
    Require(!client.Check(expired).success&&calls==3,"Expired session made a request or was accepted");
    Require(!client.Login("").success&&calls==3,"Empty license reached the server");
    std::stop_source cancel; cancel.request_stop(); Require(!client.Login(license,cancel.get_token()).success&&calls==3,"Canceled login contacted the server");
    Require(auth::EncodeForm("&= +/\r\n")=="%26%3D%20%2B%2F%0D%0A","Request fields can inject parameters");
    auto reject=[&](auto response,const char* message) {
        auth::Client bad([&](std::string_view,std::stop_token){return response();},server.key);
        auto result=bad.Login(license); Require(!result.success&&!result.session.Valid(auth::UnixTime()),message);
    };
    reject([&]{ auto j=server.Init(); j["ownerid"]="other-owner"; return server.Reply(j); },"Wrong owner authorized a session");
    reject([&]{ auto j=server.Init(); j["code"]=67; return server.Reply(j); },"Wrong success code authorized a session");
    reject([&]{ return server.Raw("{broken json"); },"Malformed signed JSON authorized a session");
    reject([&]{ auto r=server.Reply(server.Init()); r.status=503; return r; },"HTTP failure authorized a session");
    reject([&]{ auto r=server.Reply(server.Init()); r.signature.clear(); return r; },"Unsigned initialization authorized a session");
    reject([&]{ auto j=server.Init(); j["sessionid"]=std::string(auth::kMaxSession+1,'x'); return server.Reply(j); },"Oversized session token accepted");
    auth::Client expiredLicense([&](std::string_view form,std::stop_token) {
        return server.Reply(form.starts_with("type=init")?server.Init():server.License(auth::UnixTime()-1));
    },server.key);
    Require(!expiredLicense.Login(license).success,"Expired subscription authorized a session");
    auth::Client noSubscriptions([&](std::string_view form,std::stop_token) {
        return server.Reply(form.starts_with("type=init")?server.Init():server.Ok());
    },server.key);
    Require(!noSubscriptions.Login(license).success,"Missing subscription authorized a session");
    auth::Client denied([&](std::string_view form,std::stop_token) {
        if(form.starts_with("type=init")) return server.Reply(server.Init());
        auto j=server.Ok(); j["success"]=false; j["message"]="Rejected "+license; return server.Reply(j);
    },server.key);
    auto deniedResult=denied.Login(license); Require(!deniedResult.success&&deniedResult.message.find(license)==deniedResult.message.npos,"Server rejection exposed the license");
    auth::Client failure([](std::string_view,std::stop_token)->auth::HttpResponse { throw std::runtime_error("network details"); },server.key);
    Require(!failure.Login(license).success,"Network failure authorized a session");
}
void Handoff() {
    Require(!auth::ReadHandoff(nullptr).Valid(auth::UnixTime()),"Missing handoff authorized a session");
    wchar_t path[32768]{}; Require(GetModuleFileNameW(nullptr,path,32768)!=0,"Missing test executable");
    const auto executable=std::filesystem::path(path); std::wstring command=L"\""+executable.wstring()+L"\" --handoff-child";
    auth::Session session("fixture-session",auth::UnixTime()+3600); PROCESS_INFORMATION process{}; std::string error;
    Require(auth::LaunchOverlay(executable,executable.parent_path(),command,session,process,error),"Could not launch isolated handoff child");
    WinHandle child(process.hProcess),thread(process.hThread); Require(WaitForSingleObject(child.get(),5000)==WAIT_OBJECT_0,"Handoff child did not exit");
    DWORD code=1; GetExitCodeProcess(child.get(),&code); Require(code==0,"Inherited session was missing or invalid");
    auth::Session expired("fixture-session",auth::UnixTime()-1);
    Require(!auth::LaunchOverlay(executable,executable.parent_path(),command,expired,process,error),"Expired handoff launched a child");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string_view(argv[1])=="--live-init") {
            auto result=auth::Client{}.Probe(); std::cout<<result.message<<'\n'; return result.success?0:1;
        }
        if(argc==4&&std::string_view(argv[1])=="--handoff-child"&&std::string_view(argv[2])=="--auth-pipe") {
            std::uintptr_t handle=0; const std::string_view value(argv[3]); std::from_chars(value.data(),value.data()+value.size(),handle);
            auto session=auth::ReadHandoff(reinterpret_cast<HANDLE>(handle));
            return session.Valid(auth::UnixTime())&&session.id=="fixture-session"?0:2;
        }
        Signatures(); Protocol(); Handoff();
        std::cout<<"PASS KeyAuth: signed responses, application identity, license/session expiry, encoding, rejection, cancellation, inherited handoff\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL KeyAuth: "<<e.what()<<'\n'; return 1; }
}
