#include "keyauth.hpp"
#include "win_handle.hpp"
#include <windows.h>
#include <winhttp.h>
#include <sddl.h>
#include <bcrypt.h>
#include <nlohmann/json.hpp>
#include <optional/monocypher-ed25519.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mcov::auth {
namespace {
using Json=nlohmann::json;
struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };
struct WipeOnExit { std::string& value; ~WipeOnExit() { Wipe(value); } };
struct Internet {
    HINTERNET value{};
    explicit Internet(HINTERNET handle):value(handle) {}
    ~Internet() { if(value) WinHttpCloseHandle(value); }
    Internet(const Internet&)=delete;
};
void Need(bool condition,const char* message) { if(!condition) throw Failure(message); }
void Cancel(std::stop_token stop) { Need(!stop.stop_requested(),"License verification canceled."); }
template<std::size_t N> bool Hex(std::string_view text,std::array<std::uint8_t,N>& out) {
    if(text.size()!=N*2) return false;
    auto digit=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; };
    for(std::size_t i=0;i<N;++i) { const int a=digit(text[i*2]),b=digit(text[i*2+1]); if(a<0||b<0) return false; out[i]=std::uint8_t(a*16+b); }
    return true;
}
PublicKey ProductionKey() {
    // Public response-signing key published in KeyAuth's official 1.3 SDK.
    PublicKey key{}; Hex("5586b4bc69c7a4b487e4563a4cd96afd39140f919bd31cea7d1c6a1e8439422b",key); return key;
}
std::string Header(HINTERNET request,const wchar_t* name) {
    wchar_t buffer[256]{}; DWORD bytes=sizeof(buffer);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_CUSTOM,name,buffer,&bytes,WINHTTP_NO_HEADER_INDEX)) return {};
    std::string text;
    for(const wchar_t* p=buffer;*p;++p) { if(*p>127) return {}; text+=char(*p); }
    return text;
}
std::string HardwareId() {
    WinHandle token; HANDLE raw=nullptr;
    Need(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw)!=FALSE,"Could not read the Windows account for license verification.");
    token=WinHandle(raw); DWORD bytes=0; GetTokenInformation(token.get(),TokenUser,nullptr,0,&bytes);
    std::vector<std::uint8_t> data(bytes);
    Need(bytes&&GetTokenInformation(token.get(),TokenUser,data.data(),bytes,&bytes),"Could not read the Windows account for license verification.");
    LPSTR text=nullptr;
    Need(ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&text)!=FALSE,"Could not read the Windows account for license verification.");
    std::string result(text); LocalFree(text); return result;
}
std::string BinaryHash() {
    // KeyAuth's hash field uses an MD5 build identifier; response authentication
    // is independently provided by TLS and Ed25519, never by this identifier.
    wchar_t path[32768]{}; const auto length=GetModuleFileNameW(nullptr,path,32768);
    Need(length&&length<32768,"Could not identify this client build.");
    std::ifstream file(std::filesystem::path(path),std::ios::binary); Need(bool(file),"Could not read this client build.");
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    struct Cleanup { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h; ~Cleanup() { if(h) BCryptDestroyHash(h); if(a) BCryptCloseAlgorithmProvider(a,0); } } cleanup{alg,hash};
    Need(BCryptOpenAlgorithmProvider(&alg,BCRYPT_MD5_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0,"Could not identify this client build.");
    std::array<unsigned char,65536> bytes{};
    while(file) { file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()); Need(BCryptHashData(hash,bytes.data(),ULONG(file.gcount()),0)>=0,"Could not identify this client build."); }
    std::array<unsigned char,16> digest{}; Need(file.eof()&&BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0,"Could not identify this client build.");
    constexpr char hex[]="0123456789abcdef"; std::string result;
    for(auto b:digest) { result+=hex[b>>4]; result+=hex[b&15]; } return result;
}
std::string Form(std::string_view type) {
    return "type="+EncodeForm(type)+"&name="+EncodeForm(kAppName)+"&ownerid="+EncodeForm(kOwnerId);
}
Json Decode(const HttpResponse& response,const PublicKey& key) {
    if(response.body=="KeyAuth_Invalid") throw Failure("KeyAuth could not find this application. Check the app name and owner ID.");
    Need(response.status==200,"KeyAuth is unavailable. Please try again later.");
    Need(VerifyResponse(response,key,UnixTime()),"Could not verify KeyAuth's response. Check your system clock and try again.");
    auto data=Json::parse(response.body,nullptr,false);
    Need(data.is_object()&&data.contains("success")&&data["success"].is_boolean()&&data.value("ownerid",std::string{})==kOwnerId,"KeyAuth returned an invalid response.");
    if(!data["success"].get<bool>()) {
        std::string message=data.value("message",std::string{"License verification was rejected."});
        if(message.size()>256||message.find_first_of("\r\n")!=std::string::npos) message="License verification was rejected. Check your key and subscription.";
        throw Failure(message);
    }
    Need(data.contains("code")&&data["code"].is_number_integer()&&data["code"].get<int>()==68,"KeyAuth returned an invalid authentication result.");
    return data;
}
std::string Init(const Transport& transport,const PublicKey& key,std::stop_token stop) {
    Cancel(stop); auto form=Form("init")+"&ver="+EncodeForm(kVersion)+"&hash="+BinaryHash(); WipeOnExit wipe{form};
    auto data=Decode(transport(form,stop),key);
    const auto id=data.value("sessionid",std::string{});
    Need(!id.empty()&&id.size()<=kMaxSession&&std::all_of(id.begin(),id.end(),[](unsigned char c){return c>=33&&c<=126;}),"KeyAuth did not return a valid session.");
    Cancel(stop); return id;
}
std::int64_t Expiry(const Json& data) {
    if(!data.contains("info")||!data["info"].is_object()||!data["info"].contains("subscriptions")||!data["info"]["subscriptions"].is_array()) return 0;
    std::int64_t expires=0;
    for(const auto& sub:data["info"]["subscriptions"]) {
        if(!sub.is_object()||!sub.contains("expiry")||sub.value("subscription",std::string{}).empty()) continue;
        std::int64_t value=0;
        if(sub["expiry"].is_string()) {
            const auto text=sub["expiry"].get<std::string>(); const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
            if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()) continue;
        } else if(sub["expiry"].is_number_integer()) value=sub["expiry"].get<std::int64_t>();
        expires=std::max(expires,value);
    }
    return expires;
}
}
void Wipe(std::string& value) noexcept { if(!value.empty()) SecureZeroMemory(value.data(),value.size()); value.clear(); }
Session::Session(Session&& other) noexcept { id=std::move(other.id); expires=std::exchange(other.expires,0); Wipe(other.id); }
Session& Session::operator=(Session&& other) noexcept {
    if(this!=&other) { Wipe(id); id=std::move(other.id); expires=std::exchange(other.expires,0); Wipe(other.id); } return *this;
}
Session::~Session() { Wipe(id); }
bool Session::Valid(std::int64_t now) const {
    return !id.empty()&&id.size()<=kMaxSession&&expires>now&&std::all_of(id.begin(),id.end(),[](unsigned char c){return c>=33&&c<=126;});
}
std::int64_t UnixTime() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
std::string EncodeForm(std::string_view value) {
    constexpr char hex[]="0123456789ABCDEF"; std::string result;
    for(unsigned char c:value) {
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') result+=char(c);
        else { result+='%'; result+=hex[c>>4]; result+=hex[c&15]; }
    }
    return result;
}
bool VerifyResponse(const HttpResponse& r,const PublicKey& key,std::int64_t now) {
    if(r.status!=200||r.body.empty()||r.body.size()>kMaxResponse||r.timestamp.size()<10||r.timestamp.size()>13) return false;
    std::int64_t timestamp=0; const auto p=std::from_chars(r.timestamp.data(),r.timestamp.data()+r.timestamp.size(),timestamp);
    if(p.ec!=std::errc{}||p.ptr!=r.timestamp.data()+r.timestamp.size()||timestamp<0||timestamp>now+120||timestamp<now-120) return false;
    std::array<std::uint8_t,64> sig{}; if(!Hex(r.signature,sig)) return false;
    auto message=r.timestamp+r.body; WipeOnExit wipe{message};
    return crypto_ed25519_check(sig.data(),key.data(),reinterpret_cast<const std::uint8_t*>(message.data()),message.size())==0;
}
HttpResponse Request(std::string_view form,std::stop_token stop) {
    Cancel(stop); Need(form.size()<8192,"License request is too large.");
    constexpr auto networkError="Could not reach KeyAuth. Check your internet connection and try again.";
    Internet session(WinHttpOpen(L"mcov/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    Need(session.value!=nullptr,networkError);
    WinHttpSetTimeouts(session.value,3000,3000,5000,5000);
    Internet connection(WinHttpConnect(session.value,L"keyauth.win",INTERNET_DEFAULT_HTTPS_PORT,0)); Need(connection.value!=nullptr,networkError);
    Internet request(WinHttpOpenRequest(connection.value,L"POST",L"/api/1.3/",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)); Need(request.value!=nullptr,networkError);
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    Need(WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))!=FALSE,networkError);
    constexpr wchar_t headers[]=L"Content-Type: application/x-www-form-urlencoded\r\nAccept: application/json\r\n";
    Need(WinHttpSendRequest(request.value,headers,DWORD(-1),const_cast<char*>(form.data()),DWORD(form.size()),DWORD(form.size()),0)!=FALSE,networkError);
    Cancel(stop); Need(WinHttpReceiveResponse(request.value,nullptr)!=FALSE,networkError); Cancel(stop);
    HttpResponse result; DWORD status=0,bytes=sizeof(status);
    Need(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&bytes,WINHTTP_NO_HEADER_INDEX)!=FALSE,networkError);
    result.status=status; result.signature=Header(request.value,L"x-signature-ed25519"); result.timestamp=Header(request.value,L"x-signature-timestamp");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    std::array<char,4096> buffer{};
    for(;;) {
        Cancel(stop); Need(std::chrono::steady_clock::now()<deadline,networkError); DWORD count=0;
        Need(WinHttpReadData(request.value,buffer.data(),DWORD(buffer.size()),&count)!=FALSE,networkError);
        if(!count) break; Need(result.body.size()+count<=kMaxResponse,"KeyAuth returned an oversized response."); result.body.append(buffer.data(),count);
    }
    SecureZeroMemory(buffer.data(),buffer.size()); return result;
}
Client::Client():transport_(Request),key_(ProductionKey()) {}
Client::Client(Transport transport,PublicKey key):transport_(std::move(transport)),key_(key) {}
LoginResult Client::Login(std::string license,std::stop_token stop) {
    WipeOnExit wipeLicense{license}; LoginResult result;
    try {
        Need(!license.empty()&&license.size()<=256&&license.find_first_of("\r\n\0",0,3)==std::string::npos,"Enter a valid license key.");
        auto id=Init(transport_,key_,stop); WipeOnExit wipeId{id};
        auto form=Form("license")+"&sessionid="+EncodeForm(id)+"&key="+EncodeForm(license)+"&hwid="+EncodeForm(HardwareId()); WipeOnExit wipeForm{form};
        auto data=Decode(transport_(form,stop),key_); Cancel(stop);
        const auto expiry=Expiry(data); Need(expiry>UnixTime(),"This license has no active subscription. Renew it and try again.");
        result.session=Session(std::move(id),expiry); result.success=true; result.message="License verified. Select Minecraft to start.";
    } catch(const Failure& e) { result.message=e.what(); }
    catch(...) { result.message="License verification failed. Please try again."; }
    // A server message must never copy the submitted license into UI logs.
    if(!license.empty()) for(auto at=result.message.find(license);at!=std::string::npos;at=result.message.find(license,at+10)) result.message.replace(at,license.size(),"[redacted]");
    return result;
}
Status Client::Check(const Session& session,std::stop_token stop) {
    try {
        Need(session.Valid(UnixTime()),"Your license has expired. Verify an active key to continue."); Cancel(stop);
        auto form=Form("check")+"&sessionid="+EncodeForm(session.id); WipeOnExit wipe{form}; Decode(transport_(form,stop),key_); Cancel(stop);
        return {true,"License session verified."};
    } catch(const Failure& e) { return {false,e.what()}; }
    catch(...) { return {false,"Could not verify your license session. Sign in again."}; }
}
Status Client::Probe(std::stop_token stop) {
    try { auto id=Init(transport_,key_,stop); Wipe(id); return {true,"KeyAuth application initialized. A license is required to sign in."}; }
    catch(const Failure& e) { return {false,e.what()}; }
    catch(...) { return {false,"KeyAuth application initialization failed."}; }
}
}
