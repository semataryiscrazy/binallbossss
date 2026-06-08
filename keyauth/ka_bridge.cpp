#include <string>
#include <cstdio>
#include <cctype>
#include <chrono>
#include <thread>

#undef WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include "../source/Cfg/strenc.h"
#include "ka_bridge.h"
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

static std::string g_user, g_err, g_token;
static long long g_expiry = 0;

// ─── ALTERE AQUI O IP/PORTA DO SEU SERVIDOR ───
static const wchar_t* SERVER_HOST = L"satella-auth-bot.onrender.com";
static const int SERVER_PORT = 443;
static const bool SERVER_SSL = true;
// ───────────────────────────────────────────────

static std::string gw() {
    HW_PROFILE_INFOA i;
    return GetCurrentHwProfileA(&i) ? i.szHwProfileGuid : "";
}

static std::string json_str(const std::string& j, const std::string& k) {
    std::string s = "\"" + k + "\":\"";
    auto p = j.find(s);
    if (p == j.npos) { s = "\"" + k + "\": \""; p = j.find(s); }
    if (p == j.npos) return "";
    auto st = p + s.length();
    auto en = j.find('"', st);
    return (en == j.npos) ? "" : j.substr(st, en - st);
}

static bool json_bool(const std::string& j, const std::string& k) {
    return j.find("\"" + k + "\":true") != j.npos || j.find("\"" + k + "\": true") != j.npos;
}

static long long json_int(const std::string& j, const std::string& k) {
    std::string s = "\"" + k + "\":";
    auto p = j.find(s);
    if (p == j.npos) { s = "\"" + k + "\": "; p = j.find(s); }
    if (p == j.npos) return 0;
    auto st = p + s.length();
    while (st < j.size() && (j[st] == ' ' || j[st] == '\t')) st++;
    long long v = 0;
    int sign = 1;
    if (j[st] == '-') { sign = -1; st++; }
    while (st < j.size() && j[st] >= '0' && j[st] <= '9') {
        v = v * 10 + (j[st] - '0');
        st++;
    }
    return v * sign;
}

static std::string api_call(const std::string& endpoint, const std::string& json_body) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"Satella/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) { g_err = "WinHttpOpen failed"; return result; }

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_HOST, SERVER_PORT, 0);
    if (!hConnect) {
        g_err = "API server not running";
        WinHttpCloseHandle(hSession);
        return result;
    }

    std::wstring wpath = L"/api/";
    wpath += std::wstring(endpoint.begin(), endpoint.end());

    DWORD reqFlags = SERVER_SSL ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(), NULL, NULL, NULL, reqFlags);
    if (!hRequest) {
        g_err = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (SERVER_SSL) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!g_token.empty()) {
        std::wstring wauth = L"Authorization: Bearer ";
        wauth += std::wstring(g_token.begin(), g_token.end());
        wauth += L"\r\n";
        headers += wauth;
    }

    BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(),
        (LPVOID)json_body.c_str(), (DWORD)json_body.length(),
        (DWORD)json_body.length(), 0);

    if (!bResult) {
        g_err = "WinHttpSendRequest failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) {
        g_err = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        char* buf = new char[dwSize + 1];
        ZeroMemory(buf, dwSize + 1);

        if (WinHttpReadData(hRequest, (LPVOID)buf, dwSize, &dwDownloaded)) {
            result.append(buf, dwDownloaded);
        }

        delete[] buf;
    } while (dwDownloaded > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

int ka_init() {
    g_err.clear();
    return 1;
}

int ka_login(const char* u, const char* p) {
    g_err.clear();

    std::string body = "{\"username\":\"" + std::string(u) +
                       "\",\"password\":\"" + std::string(p) +
                       "\",\"hwid\":\"" + gw() + "\"}";

    std::string r = api_call("login", body);

    if (r.empty()) {
        if (g_err.empty()) g_err = "Connection failed";
        return 0;
    }

    if (json_bool(r, "success")) {
        g_user = json_str(r, "username");
        g_token = json_str(r, "token");
        g_expiry = json_int(r, "expires_at");
        if (g_expiry == 0) {
            long long days = json_int(r, "expires_in_days");
            if (days > 0) {
                auto now_s = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                g_expiry = now_s + days * 86400;
            }
        }
        return 1;
    }

    g_err = json_str(r, "error");
    if (g_err.empty()) g_err = "Login failed";
    return 0;
}

int ka_register(const char* u, const char* p, const char* k) {
    g_err.clear();

    std::string body = "{\"username\":\"" + std::string(u) +
                       "\",\"password\":\"" + std::string(p) +
                       "\",\"key\":\"" + std::string(k) +
                       "\",\"hwid\":\"" + gw() + "\"}";

    std::string r = api_call("register", body);

    if (r.empty()) {
        if (g_err.empty()) g_err = "Connection failed";
        return 0;
    }

    if (json_bool(r, "success")) {
        g_user = json_str(r, "username");
        g_token = json_str(r, "token");
        long long days = json_int(r, "expires_in_days");
        if (days > 0) {
            auto now_s = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            g_expiry = now_s + days * 86400;
        }
        return 1;
    }

    g_err = json_str(r, "error");
    if (g_err.empty()) g_err = "Register failed";
    return 0;
}

const char* ka_get_username() { return g_user.c_str(); }

const char* ka_get_error() { return g_err.empty() ? NULL : g_err.c_str(); }

int ka_get_days_remaining() {
    if (g_expiry <= 0) return 0;
    auto now_s = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long diff = g_expiry - now_s;
    return diff > 0 ? (int)(diff / 86400) : 0;
}

void ka_cleanup() {
    g_token.clear();
    g_user.clear();
    g_err.clear();
    g_expiry = 0;
}
