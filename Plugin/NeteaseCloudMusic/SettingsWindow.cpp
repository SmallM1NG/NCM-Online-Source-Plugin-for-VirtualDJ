#pragma execution_character_set("utf-8")
#define _HAS_STD_BYTE 0
#include "NeteaseCloudMusic.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <shlobj.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <wincodec.h>

namespace {
constexpr int kConfigWidth = 420;
constexpr int kConfigHeight = 740;
// 说明注记控件的私有 ID（避开 ControlId 枚举，文字保持白色）
constexpr int IDC_NOTE_QUANT   = 9001;   // 数量控制－“上限均 999”
constexpr int IDC_NOTE_SEARCH  = 9002;   // 搜索类别说明
constexpr int IDC_NOTE_AVCFG   = 9003;   // 音/画质配置说明
constexpr int IDC_NOTE_DL      = 9004;   // 下载配置说明
constexpr int IDC_NOTE_API     = 9005;   // 服务与调试－重启提示
float g_uiScale = 1.0f;
int ScaleUi(int value) { return (int)(value * g_uiScale + 0.5f); }
int ClampInt(int v, int lo, int hi) { return (std::max)(lo, (std::min)(hi, v)); }
bool JsonBool(const Json::Value& v, const char* key, bool fallback) { return v.isMember(key) && v[key].isBool() ? v[key].asBool() : fallback; }
int JsonInt(const Json::Value& v, const char* key, int fallback, int lo, int hi) { return v.isMember(key) && v[key].isInt() ? ClampInt(v[key].asInt(), lo, hi) : fallback; }
string JsonString(const Json::Value& v, const char* key, const string& fallback) { return v.isMember(key) && v[key].isString() ? v[key].asString() : fallback; }

}

void CNeteaseCloud::SetDefaultSettings() {
    config.loadDailyRecommend = false;
    config.loadFavoriteSongs = true;
    config.showCreatedPlaylists = true;
    config.showSubscribedPlaylists = true;
    config.showCreatedPodcasts = false;
    config.showSubscribedPodcasts = false;
    config.showSubscribedAlbums = true;
    config.showSubscribedVideos = true;
    config.maxVideoPlaybackQuality = 1080;
    config.maxVideoDownloadQuality = 1080;
    config.limitCreatedPlaylists = 500;
    config.limitSubscribedPlaylists = 500;
    config.limitCreatedPodcasts = 500;
    config.limitSubscribedPodcasts = 500;
    config.limitSubscribedAlbums = 500;
    config.limitItemsPerFolder = 500;
    config.searchResultLimit = 20;
    config.coverSize = 1000;        // 封面尺寸上限（音频/视频共用）默认 1000
    config.searchType = 1;
    config.playQuality = "exhigh";
    config.enableTrackDownload = false;
    config.enableVideoDownload = false;
    config.trackDownloadQuality = "exhigh";
    PWSTR downloads = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloads) == S_OK) {
        config.downloadPath = WideToUtf8(downloads) + "\\NeteaseCloudMusic DL";
        CoTaskMemFree(downloads);
    } else config.downloadPath = GetPluginPath() + "Downloads";
    config.writeTags = true;
    config.writeSourceUrlToComment = false;
    config.splitPodcastTitle = true;
    config.splitLargeFolders = true;
    config.enableLogging = true;
    config.apiPort = 3000;
    config.showApiConsole = false;
}

void CNeteaseCloud::LoadSettings() {
    SetDefaultSettings();
    const string path = GetPluginPath() + "settings.json";
    ifstream in(path, ios::binary);
    if (in.is_open()) {
        Json::Value r;
        Json::CharReaderBuilder b;
        string errors;
        if (Json::parseFromStream(b, in, &r, &errors) && r.isObject()) {
            config.loadDailyRecommend = JsonBool(r, "loadDailyRecommend", config.loadDailyRecommend);
            config.loadFavoriteSongs = JsonBool(r, "loadFavoriteSongs", config.loadFavoriteSongs);
            config.showCreatedPlaylists = JsonBool(r, "showCreatedPlaylists", config.showCreatedPlaylists);
            config.showSubscribedPlaylists = JsonBool(r, "showSubscribedPlaylists", config.showSubscribedPlaylists);
            config.showCreatedPodcasts = JsonBool(r, "showCreatedPodcasts", config.showCreatedPodcasts);
            config.showSubscribedPodcasts = JsonBool(r, "showSubscribedPodcasts", config.showSubscribedPodcasts);
            config.showSubscribedAlbums = JsonBool(r, "showSubscribedAlbums", config.showSubscribedAlbums);
            config.showSubscribedVideos = JsonBool(r, "showSubscribedVideos", config.showSubscribedVideos);
            int playbackQuality = JsonInt(r, "maxVideoPlaybackQuality", config.maxVideoPlaybackQuality, 720, 1080);
            config.maxVideoPlaybackQuality = playbackQuality == 720 ? 720 : 1080;
            int downloadVideoQuality = JsonInt(r, "maxVideoDownloadQuality", config.maxVideoDownloadQuality, 720, 1080);
            config.maxVideoDownloadQuality = downloadVideoQuality == 720 ? 720 : 1080;
            config.limitCreatedPlaylists = JsonInt(r, "createdPlaylistLimit", config.limitCreatedPlaylists, 1, 999);
            config.limitSubscribedPlaylists = JsonInt(r, "subscribedPlaylistLimit", config.limitSubscribedPlaylists, 1, 999);
            config.limitCreatedPodcasts = JsonInt(r, "createdPodcastLimit", config.limitCreatedPodcasts, 1, 999);
            config.limitSubscribedPodcasts = JsonInt(r, "subscribedPodcastLimit", config.limitSubscribedPodcasts, 1, 999);
            config.limitSubscribedAlbums = JsonInt(r, "subscribedAlbumLimit", config.limitSubscribedAlbums, 1, 999);
            config.limitItemsPerFolder = JsonInt(r, "folderItemLimit", config.limitItemsPerFolder, 1, 999);
            config.searchResultLimit = JsonInt(r, "searchResultLimit", config.searchResultLimit, 1, 999);
            config.coverSize = JsonInt(r, "coverSize", config.coverSize, 1, 999999999);
            int rawSearchType = JsonInt(r, "searchType", config.searchType, 1, 2000);
            config.searchType = (rawSearchType == 1 || rawSearchType == 2000 || rawSearchType == 1004 || rawSearchType == 1014)
                ? rawSearchType : 1;
            config.playQuality = JsonString(r, "playQuality", config.playQuality) == "lossless" ? "lossless" : "exhigh";
            config.enableTrackDownload = JsonBool(r, "enableTrackDownload", config.enableTrackDownload);
            config.enableVideoDownload = JsonBool(r, "enableVideoDownload", config.enableVideoDownload);
            config.trackDownloadQuality = JsonString(r, "trackDownloadQuality", config.trackDownloadQuality) == "lossless" ? "lossless" : "exhigh";
            config.downloadPath = JsonString(r, "downloadPath", config.downloadPath);
            config.writeTags = JsonBool(r, "writeTags", config.writeTags);
            config.writeSourceUrlToComment = JsonBool(r, "writeSourceUrlToComment", config.writeSourceUrlToComment);
            config.splitPodcastTitle = JsonBool(r, "splitPodcastTitle", config.splitPodcastTitle);
            config.splitLargeFolders = JsonBool(r, "splitLargeFolders", config.splitLargeFolders);
            config.enableLogging = JsonBool(r, "enableLogging", config.enableLogging);
            config.apiPort = JsonInt(r, "apiPort", config.apiPort, 1, 65535);
            config.showApiConsole = JsonBool(r, "showApiConsole", config.showApiConsole);
        } else {
            WriteLog("[CFG] settings.json 解析失败，使用默认配置");
        }
    } else {
        WriteLog("[CFG] 未找到 settings.json，将写入默认配置");
    }
    SaveSettings(false); // 写出配置文件，并把非法/缺失值规范成默认值（启动时不重复打“已保存”）
    ApplySettings();
}

bool CNeteaseCloud::SaveSettings(bool log) {
    Json::Value r(Json::objectValue);
    r["loadDailyRecommend"] = config.loadDailyRecommend;
    r["loadFavoriteSongs"] = config.loadFavoriteSongs;
    r["showCreatedPlaylists"] = config.showCreatedPlaylists;
    r["showSubscribedPlaylists"] = config.showSubscribedPlaylists;
    r["showCreatedPodcasts"] = config.showCreatedPodcasts;
    r["showSubscribedPodcasts"] = config.showSubscribedPodcasts;
    r["showSubscribedAlbums"] = config.showSubscribedAlbums;
    r["showSubscribedVideos"] = config.showSubscribedVideos;
    r["maxVideoPlaybackQuality"] = config.maxVideoPlaybackQuality;
    r["maxVideoDownloadQuality"] = config.maxVideoDownloadQuality;
    r["createdPlaylistLimit"] = config.limitCreatedPlaylists;
    r["subscribedPlaylistLimit"] = config.limitSubscribedPlaylists;
    r["createdPodcastLimit"] = config.limitCreatedPodcasts;
    r["subscribedPodcastLimit"] = config.limitSubscribedPodcasts;
    r["subscribedAlbumLimit"] = config.limitSubscribedAlbums;
    r["folderItemLimit"] = config.limitItemsPerFolder;
    r["searchResultLimit"] = config.searchResultLimit;
    r["coverSize"]          = config.coverSize;
    r["searchType"] = config.searchType;
    r["playQuality"] = config.playQuality;
    r["enableTrackDownload"] = config.enableTrackDownload;
    r["enableVideoDownload"] = config.enableVideoDownload;
    r["trackDownloadQuality"] = config.trackDownloadQuality;
    r["downloadPath"] = config.downloadPath;
    r["writeTags"] = config.writeTags;
    r["writeSourceUrlToComment"] = config.writeSourceUrlToComment;
    r["splitPodcastTitle"] = config.splitPodcastTitle;
    r["splitLargeFolders"] = config.splitLargeFolders;
    r["enableLogging"] = config.enableLogging;
    r["apiPort"] = config.apiPort;
    r["showApiConsole"] = config.showApiConsole;
    ofstream out(GetPluginPath() + "settings.json", ios::binary | ios::trunc);
    if (!out.is_open()) return false;
    Json::StreamWriterBuilder b; b["indentation"] = "  ";
    out << Json::writeString(b, r) << '\n';
    const bool ok = out.good();
    if (log)
        WriteLog(ok ? "[CFG] 配置已保存" : "[CFG][ERROR] 配置保存失败");
    return ok;
}

void CNeteaseCloud::ApplySettings() {
    // ApiBase 记录的是“正在生效的” API 端口，而不是 config.apiPort。生效端口
    // 只在 StartApiIfNeeded 真正拉起 API 进程时更新、在 StopApiService 时清空，
    // 从而保证所有请求始终指向当前存活的 API 实例。
    if ((config.enableTrackDownload || config.enableVideoDownload) && !config.downloadPath.empty())
        CreateDirectoryW(Utf8ToWide(config.downloadPath).c_str(), nullptr);
}

// UTF-16 转 UTF-8
string CNeteaseCloud::WideToUtf8(const std::wstring& str) {
    if (str.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0, nullptr, nullptr);
    string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), out.data(), n, nullptr, nullptr);
    return out;
}

string CNeteaseCloud::HttpPostJson(const string& target, const string& body) {
    CURL* curl = curl_easy_init(); string result;
    if (!curl) return result;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, target.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* c, size_t s, size_t n, void* u) { ((string*)u)->append((char*)c, s*n); return s*n; });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_perform(curl);
    curl_slist_free_all(headers); curl_easy_cleanup(curl);
    return result;
}

bool CNeteaseCloud::IsApiRunning() {
    // “在线”仅表示本地 API 进程仍存活 —— 不做任何 HTTP 探活，
    // 因此绝不会命中 /login/status（旧版那种每次就绪轮询都会让服务端日志
    // 刷屏 [weapi] /login/status 请求）。
    apiOnline = (apiProcessHandle != nullptr &&
                 WaitForSingleObject(apiProcessHandle, 0) != WAIT_OBJECT_0);
    return apiOnline;
}

void CNeteaseCloud::UpdateApiStatusControl() {
    if (!configWindow || !IsWindow(configWindow)) return;
    SetDlgItemTextW(configWindow, IDC_API_STATUS, apiOnline ? L"Online" : L"Offline");
}

void CNeteaseCloud::StartApiMonitor() {
    if (apiMonitorThread.joinable()) apiMonitorThread.join();
    apiMonitorThread = std::thread([this]() {
        bool lastOnline = false;
        while (!shuttingDown) {
            // “在线”= API 进程存活。此处绝无网络请求，因此服务端日志不会再
            // 被旧版 HTTP 探活刷出的 /login/status 请求污染。
            bool online = IsApiRunning();
            if (online != lastOnline) {
                lastOnline = online;
                UpdateApiStatusControl();
            }
            if (shuttingDown) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    });
}

bool CNeteaseCloud::CreateApiJob() {
    if (apiJob) return true;
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return false;
    }
    apiJob = job;
    return true;
}

void CNeteaseCloud::CloseApiJob() {
    if (apiJob) {
        CloseHandle(apiJob);
        apiJob = nullptr;
    }
}

bool CNeteaseCloud::StartApiIfNeeded(bool force) {
    // 启动即“发后不理”，绝不让 VirtualDJ 阻塞等待。后台监控线程会异步
    // 刷新界面上的状态文字。
    unique_lock<mutex> processLock(apiProcessMutex);
    if (shuttingDown) { UpdateApiStatusControl(); return false; }

    // 已按配置端口运行则无事可做（不做 HTTP 探活），除非是显式“重启”(force=true)，
    // 后者总是重新拉起 —— “显示 CMD 窗口”勾选就是在点“重启”后借此生效。
    if (!force && apiProcessHandle && apiPortActive == config.apiPort &&
        WaitForSingleObject(apiProcessHandle, 0) != WAIT_OBJECT_0) {
        UpdateApiStatusControl();
        return true;
    }

    // 旧句柄（端口已改、进程已退出或上一个实例）：重新拉起前先丢弃它。
    if (apiProcessHandle) { CloseHandle(apiProcessHandle); apiProcessHandle = nullptr; }

    std::wstring exe = Utf8ToWide(GetPluginPath() + "ncm_api_server.exe");
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        WriteLog("[API] 未找到 API 服务程序：" + WideToUtf8(exe));
        UpdateApiStatusControl();
        return false;
    }

    std::wstring workDir = Utf8ToWide(GetPluginPath());
    std::wstring cmdLine = L"\"" + exe + L"\"";
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    // 启动前把 PORT 注入进程环境，随后还原先前值（若有）。
    // 这样不必手工拼一整块环境块，也能让子进程读到端口。
    std::wstring portName = L"PORT";
    wchar_t oldPort[64] = {};
    DWORD oldPortLen = GetEnvironmentVariableW(portName.c_str(), oldPort, 64);
    BOOL envSet = SetEnvironmentVariableW(portName.c_str(),
        std::to_wstring(config.apiPort).c_str());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    DWORD flags = config.showApiConsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;
    BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, flags,
        nullptr, workDir.c_str(), &si, &pi);

    // 无论是否拉起成功，都还原此前的 PORT。
    if (envSet && oldPortLen && oldPortLen < 64)
        SetEnvironmentVariableW(portName.c_str(), oldPort);
    else if (envSet)
        SetEnvironmentVariableW(portName.c_str(), nullptr);

    if (!ok) {
        WriteLog("[API] 启动服务失败，系统错误代码：" + to_string(GetLastError()));
        UpdateApiStatusControl();
        return false;
    }
    apiProcessHandle = pi.hProcess;
    CloseHandle(pi.hThread);

    // 把 API 进程绑进「关闭作业时结束进程」的作业对象，这样 VirtualDJ 进程退出时
    // 它会自动被终止（无需依赖 Release() 被调用）。
    if (apiJob && !AssignProcessToJobObject(apiJob, apiProcessHandle)) {
        WriteLog("[API] 将服务加入作业对象失败，错误代码：" + to_string(GetLastError()));
    }

    apiPortActive = config.apiPort;
    ApiBase = "http://127.0.0.1:" + to_string(apiPortActive);
    apiOnline = false;
    UpdateApiStatusControl();
    return true;
}

void CNeteaseCloud::StopApiService() {
    HANDLE process = nullptr;
    {
        lock_guard<mutex> lock(apiProcessMutex);
        process = apiProcessHandle;
        apiProcessHandle = nullptr;
    }
    if (process) {
        WriteLog("[API] 正在终止 API 进程...");
        TerminateProcess(process, 0);
        CloseHandle(process);
        WriteLog("[API] API 进程已终止");
    } else {
        WriteLog("[API] 退出清理：无进程句柄可终止");
    }
    apiPortActive = 0;
    apiOnline = false;
    if (configWindow && IsWindow(configWindow))
        SetDlgItemTextW(configWindow, IDC_API_STATUS, L"Offline");
}

void CNeteaseCloud::RestartApiService() {
    WriteLog("[API] 正在重启 API 服务...");
    StopApiService();
    if (shuttingDown) return;
    // force=true：不受“同端口已在跑就跳过”的守护影响，确保按当前配置重新拉起进程，
    // 从而 “显示 CMD 窗口” 勾选在点“重启 API 服务”后真正以新控制台生效。
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 让旧进程退出、端口释放
    StartApiIfNeeded(true);
}

void CNeteaseCloud::LoadLoginData() {
    ifstream in(GetPluginPath() + "ncm_user_data.json", ios::binary);
    Json::Value j;
    string loadedUid;
    if (in.is_open() && Json::Reader().parse(in, j)) {
        {
            lock_guard<mutex> lock(stateMutex);
            loginCookie = j.get("cookie", "").asString();
            leanLoginCookie = ExtractLeanCookie(loginCookie);
            userId = j.get("userId", "").asString();
            userNickname = j.get("nickname", "").asString();
            userAvatarUrl = j.get("avatarUrl", "").asString();
            loadedUid = userId;
        }
        WriteLog(loadedUid.empty() ? "[LOGIN] 本地登录数据已读取，但用户 ID 为空"
                                   : "[LOGIN] 已读取本地登录数据，用户 ID：" + loadedUid);
    }
}

void CNeteaseCloud::SaveLoginData() {
    Json::Value j;
    {
        lock_guard<mutex> lock(stateMutex);
        j["cookie"] = loginCookie;
        j["userId"] = userId;
        j["nickname"] = userNickname;
        j["avatarUrl"] = userAvatarUrl;
    }
    ofstream out(GetPluginPath() + "ncm_user_data.json", ios::binary | ios::trunc);
    Json::StreamWriterBuilder b; b["indentation"] = "  "; out << Json::writeString(b, j) << '\n';
}

void CNeteaseCloud::ClearLoginData() {
    {
        lock_guard<mutex> lock(stateMutex);
        loginCookie.clear(); leanLoginCookie.clear(); userId.clear(); userNickname.clear(); userAvatarUrl.clear();
    }
    DeleteFileW(Utf8ToWide(GetPluginPath() + "ncm_user_data.json").c_str());
    DeleteFileW(Utf8ToWide(GetPluginPath() + "ncm_user_avatar.jpg").c_str());
    RefreshAvatarControl();
    UpdateLoginStatusControl();
    WriteLog("[LOGIN] 已退出登录并清除本地登录数据");
}

bool CNeteaseCloud::RefreshLoginState() {
    string cookie;
    { lock_guard<mutex> lock(stateMutex); cookie = loginCookie; }
    if (cookie.empty()) return false;
    Json::Value body; body["cookie"] = cookie;
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    string response = HttpPostJson(ApiBase + "/login/status?timestamp=" + to_string(GetTickCount64()) + "&ua=pc", Json::writeString(wb, body));
    Json::Value root;
    if (!Json::Reader().parse(response, root)) return false;
    const Json::Value* profile = nullptr;
    if (root["data"]["profile"].isObject()) profile = &root["data"]["profile"];
    else if (root["data"]["data"]["profile"].isObject()) profile = &root["data"]["data"]["profile"];
    if (!profile || (*profile)["userId"].isNull()) return false;
    {
        lock_guard<mutex> lock(stateMutex);
        userId = (*profile)["userId"].asString();
        userNickname = profile->get("nickname", "").asString();
        userAvatarUrl = profile->get("avatarUrl", "").asString();
    }
    SaveLoginData();
    CacheUserAvatar();
    UpdateLoginStatusControl();
    RefreshAvatarControl();
    return true;
}

bool CNeteaseCloud::CacheUserAvatar() {
    string avatarUrl;
    { lock_guard<mutex> lock(stateMutex); avatarUrl = userAvatarUrl; }
    if (avatarUrl.empty()) return false;
    string image = HttpGet(avatarUrl);
    if (image.empty()) return false;
    ofstream out(GetPluginPath() + "ncm_user_avatar.jpg", ios::binary | ios::trunc);
    if (!out.is_open()) return false;
    out.write(image.data(), (streamsize)image.size());
    return out.good();
}

HBITMAP CNeteaseCloud::LoadAvatarBitmap(const string& filePath, UINT width, UINT height) {
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninitialize = init == S_OK || init == S_FALSE;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    HBITMAP bitmap = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(Utf8ToWide(filePath).c_str(), nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(frame, width, height, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    void* pixels = nullptr;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = (LONG)width;
    info.bmiHeader.biHeight = -(LONG)height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    if (SUCCEEDED(hr)) bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap && FAILED(converter->CopyPixels(nullptr, width * 4, width * height * 4, (BYTE*)pixels))) {
        DeleteObject(bitmap); bitmap = nullptr;
    }

    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (uninitialize) CoUninitialize();
    return bitmap;
}

void CNeteaseCloud::RefreshAvatarControl() {
    if (!configWindow || !IsWindow(configWindow)) return;
    HWND avatar = GetDlgItem(configWindow, IDC_AVATAR);
    if (!avatar) return;
    RECT rect{}; GetClientRect(avatar, &rect);
    UINT size = (UINT)(std::max)(1L, rect.right - rect.left);
    HBITMAP replacement = LoadAvatarBitmap(GetPluginPath() + "ncm_user_avatar.jpg", size, size);
    SendMessageW(avatar, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)replacement);
    if (avatarBitmap) DeleteObject(avatarBitmap);
    avatarBitmap = replacement;
    InvalidateRect(avatar, nullptr, TRUE);
}

void CNeteaseCloud::BeginQrLogin() {
    if (loginInProgress.exchange(true)) return;
    WriteLog("[LOGIN] 开始二维码登录");
    thread([this] {
        if (!StartApiIfNeeded()) {
            WriteLog("[LOGIN][ERROR] API 服务未就绪，取消登录");
            loginInProgress = false;
            return;
        }
        string session = to_string(GetCurrentProcessId()) + "-" + to_string(GetTickCount64()) + "-" + to_string((unsigned long long)this);
        string page = ApiBase + "/virtualdj_plugin_qrlogin.html?session=" + UrlEncode(session);
        ShellExecuteW(nullptr, L"open", Utf8ToWide(page).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        for (int i=0; i<150 && !shuttingDown; ++i) {
            this_thread::sleep_for(chrono::seconds(2));
            string response = HttpGet(ApiBase + "/virtualdj_plugin/login/result?session=" + UrlEncode(session) + "&timestamp=" + to_string(GetTickCount64()));
            Json::Value j;
            if (!Json::Reader().parse(response, j)) continue;
            string cookie = j.get("cookie", "").asString();
            if (j.get("code", 0).asInt() == 200 && !cookie.empty()) {
                { lock_guard<mutex> lock(stateMutex); loginCookie = cookie; leanLoginCookie = ExtractLeanCookie(loginCookie); userId.clear(); userNickname.clear(); userAvatarUrl.clear(); }
                SaveLoginData();
                if (RefreshLoginState()) WriteLog("[LOGIN] 登录成功");
                else WriteLog("[LOGIN] 登录凭据已保存，但刷新用户信息失败");
                break;
            }
        }
        if (!shuttingDown && loginInProgress.load())
            WriteLog("[LOGIN] 二维码登录超时或已取消");
        loginInProgress = false; UpdateLoginStatusControl();
    }).detach();
}

void CNeteaseCloud::LogoutAccount() {
    string cookie; { lock_guard<mutex> lock(stateMutex); cookie = loginCookie; }
    if (!cookie.empty()) { Json::Value b; b["cookie"] = cookie; Json::StreamWriterBuilder w; w["indentation"]=""; HttpPostJson(ApiBase + "/logout", Json::writeString(w,b)); }
    ClearLoginData();
}

ULONG VDJ_API CNeteaseCloud::Release() {
    shuttingDown = true;
    WriteLog("[API] 插件 Release() 被调用，开始退出清理");
    // 杀掉我们托管的 API 后立即退出。TerminateProcess 立即返回，不会阻塞
    // VirtualDJ 的退出流程。
    StopApiService();
    // 关闭作业句柄，借此触发「关闭作业时结束进程」，作为仍留在
    // 作业内任何子进程的兜底清理。
    CloseApiJob();
    // 置位 shuttingDown 后监控循环会立刻退出；在这里 join 可避免悬空指针
    // 访问问题（其循环只是轻量的进程存活检查、不发网络请求），因此退出时
    // 这点等待可以忽略不计。
    if (apiMonitorThread.joinable()) apiMonitorThread.join();
    if (configWindow) DestroyWindow(configWindow);
    if (darkBackgroundBrush) DeleteObject(darkBackgroundBrush);
    if (darkControlBrush) DeleteObject(darkControlBrush);
    if (uiFont) DeleteObject(uiFont);
    if (linkFont) { DeleteObject(linkFont); linkFont = nullptr; }
    if (avatarBitmap) DeleteObject(avatarBitmap);
    curl_global_cleanup(); delete this; return 0;
}

static HWND AddCtl(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
        ScaleUi(x), ScaleUi(y), ScaleUi(w), ScaleUi(h),
        parent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
}

HWND CNeteaseCloud::CreateConfigWindow() {
    if (configWindow && IsWindow(configWindow)) return configWindow;

    // 以 1080p 高度为基准，按当前系统主屏高度整体缩放窗口与控件
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    g_uiScale = screenHeight > 0 ? (float)screenHeight / 1080.0f : 1.0f;
    if (g_uiScale < 0.6f) g_uiScale = 0.6f;
    if (g_uiScale > 2.0f) g_uiScale = 2.0f;

    if (!darkBackgroundBrush) darkBackgroundBrush = CreateSolidBrush(RGB(25, 25, 25));
    if (!darkControlBrush) darkControlBrush = CreateSolidBrush(RGB(45, 45, 45));
    if (!uiFont) uiFont = CreateFontW((int)(uiFontHeight * g_uiScale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    if (!linkFont) linkFont = CreateFontW((int)(uiFontHeight * g_uiScale), 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    WNDCLASSW wc{};
    wc.lpfnWndProc = ConfigWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NCMVDJConfigWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = darkBackgroundBrush;
    RegisterClassW(&wc);

    configWindow = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"插件设置",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, ScaleUi(kConfigWidth), ScaleUi(kConfigHeight), nullptr, nullptr, hInstance, this);
    if (!configWindow) return nullptr;

    // 设置窗口固定尺寸，禁止从右键菜单打开后拖动边框调整大小或最大化
    LONG style = GetWindowLongW(configWindow, GWL_STYLE);
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX | WS_SIZEBOX);
    SetWindowLongW(configWindow, GWL_STYLE, style);
    SetWindowPos(configWindow, nullptr, 0, 0, ScaleUi(kConfigWidth), ScaleUi(kConfigHeight),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    BOOL dark = TRUE;
    DwmSetWindowAttribute(configWindow, 20, &dark, sizeof(dark));

    auto label = [&](const wchar_t* text, int x, int y, int width = 180, int id = 0) {
        return AddCtl(configWindow, L"STATIC", text, SS_LEFT, x, y, width, 20, id);
    };
    auto section = [&](const wchar_t* text, int x, int y, int id) {
        // 统一加上分隔头“——”并前导空格，蓝字
        return AddCtl(configWindow, L"STATIC", (std::wstring(L"—— ") + text + L" ——").c_str(), SS_LEFT, x, y, 372, 20, id);
    };
    // 复选框自绘（不用系统主题，方块固定大小，不吃系统缩放比例放大）
    auto check = [&](const wchar_t* text, int x, int y, int id, int width = 320) {
        return AddCtl(configWindow, L"BUTTON", text, BS_OWNERDRAW, x, y, width, 20, id);
    };
    auto edit = [&](int x, int y, int width, int id) {
        return AddCtl(configWindow, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, x, y, width, 20, id);
    };
    auto button = [&](const wchar_t* text, int x, int y, int width, int id, DWORD style = 0) {
        return AddCtl(configWindow, L"BUTTON", text, style, x, y, width, 20, id);
    };

    section(L"账户信息", 16, 12, IDC_SECTION_ACCOUNT);
    AddCtl(configWindow, L"STATIC", L"", SS_BITMAP | SS_CENTERIMAGE, 26, 34, 40, 40, IDC_AVATAR);
    AddCtl(configWindow, L"STATIC", L"用户名：未登录", SS_LEFT, 94, 34, 180, 20, IDC_NICKNAME);
    AddCtl(configWindow, L"STATIC", L"用户 ID：未登录", SS_LEFT, 94, 56, 180, 20, IDC_STATUS);
    button(L"登录", 278, 34, 110, IDC_LOGIN);
    button(L"退出登录", 278, 54, 110, IDC_LOGOUT);

    // ================= 内容/数量控制（2/5、3/5 两列） =================
    // 左列约占 2/5：内容开关；右列约占 3/5：数量上限配置
    const int QX = 180;                  // 内容/数量区域右列起点，避开左侧开关
    const int CX = 200;                  // 其它区域中心分列右栏起点
    const int QLabelW = 128;             // 数量区域说明列宽（输入框保持右侧留白不越界）
    const int stepY = 24;       // 同一区域内控件行距
    const int sectionGap = 24;  // 区域之间的统一间距
    const int titleGap = 24;    // 区域标题到第一行控件的间距
    section(L"内容配置", 16, 88, IDC_SECTION_CONTENT);
    AddCtl(configWindow, L"STATIC", L"(上限均为999/除封面尺寸外，下限1)", SS_CENTER, 130, 88, 258, 20, IDC_NOTE_QUANT);

    int rowY = 112;
    // -- 内容列开关 + 同行对应数量列描述 --
    check(L"每日歌曲推荐", 16, rowY, IDC_DAILY, 160);
    label(L"单个列表项数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_TRACK);
    rowY += stepY;

    check(L"我喜欢的音乐", 16, rowY, IDC_FAVORITES, 160);
    label(L"搜索返回项数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_SEARCH);
    rowY += stepY;

    check(L"我创建的歌单", 16, rowY, IDC_CREATED_PL, 160);
    label(L"我创建的歌单个数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_CREATED);
    rowY += stepY;

    check(L"我收藏的歌单", 16, rowY, IDC_SUBSCRIBED_PL, 160);
    label(L"我收藏的歌单个数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_SUBSCRIBED);
    rowY += stepY;

    check(L"我创建的播客", 16, rowY, IDC_CREATED_PODCAST, 160);
    label(L"我创建的播客个数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_CREATED_PODCAST);
    rowY += stepY;

    check(L"我收藏的播客", 16, rowY, IDC_SUBSCRIBED_PODCAST, 160);
    label(L"我收藏的播客个数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_SUBSCRIBED_PODCAST);
    rowY += stepY;

    check(L"我收藏的专辑", 16, rowY, IDC_SUBSCRIBED_ALBUM, 160);
    label(L"我收藏的专辑个数上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_LIMIT_SUBSCRIBED_ALBUM);
    rowY += stepY;

    check(L"我收藏的视频", 16, rowY, IDC_SUBSCRIBED_VIDEO, 160);
    // 数量列比左侧少一行，这里右侧补“封面尺寸上限”，正好上下对齐
    label(L"封面尺寸上限", QX, rowY, 152); edit(322, rowY - 2, 60, IDC_COVER);
    rowY += sectionGap;

    // ================= 搜索类别 =================
    section(L"搜索类别", 16, rowY, IDC_SECTION_SEARCH);
    AddCtl(configWindow, L"STATIC", L"(仅支持同时选择一种)", SS_CENTER, 130, rowY, 258, 20, IDC_NOTE_SEARCH);
    rowY += titleGap;

    // 四个单选（自绘：文字恒白；未选灰圈、选中蓝圈蓝点）
    AddCtl(configWindow, L"BUTTON", L"单曲", BS_OWNERDRAW | WS_GROUP | WS_TABSTOP, 16, rowY, 92, 20, IDC_SEARCH_SONG);
    AddCtl(configWindow, L"BUTTON", L"声音", BS_OWNERDRAW, 116, rowY, 92, 20, IDC_SEARCH_VOICE);
    AddCtl(configWindow, L"BUTTON", L"视频", BS_OWNERDRAW, 216, rowY, 92, 20, IDC_SEARCH_VIDEO);
    AddCtl(configWindow, L"BUTTON", L"MV", BS_OWNERDRAW, 316, rowY, 70, 20, IDC_SEARCH_MV);
    rowY += sectionGap;

    // ================= 音/画质配置 =================
    section(L"音/画质配置", 16, rowY, IDC_SECTION_MEDIA);
    AddCtl(configWindow, L"STATIC", L"(均为最高获取规格，自动降级返回)", SS_CENTER, 130, rowY, 258, 20, IDC_NOTE_AVCFG);
    rowY += titleGap;
    label(L"曲目播放音质", 16, rowY, 88);
    HWND pq = AddCtl(configWindow, L"COMBOBOX", L"", CBS_DROPDOWNLIST, 108, rowY - 2, 72, 100, IDC_PLAY_QUALITY);
    SendMessageW(pq, CB_ADDSTRING, 0, (LPARAM)L"MP3");
    SendMessageW(pq, CB_ADDSTRING, 0, (LPARAM)L"FLAC");
    label(L"视频播放分辨率", CX, rowY, 104);
    HWND pvq = AddCtl(configWindow, L"COMBOBOX", L"", CBS_DROPDOWNLIST, CX + 108, rowY - 2, 72, 100, IDC_VIDEO_PLAYBACK_QUALITY);
    SendMessageW(pvq, CB_ADDSTRING, 0, (LPARAM)L"1080P");
    SendMessageW(pvq, CB_ADDSTRING, 0, (LPARAM)L"720P");
    rowY += sectionGap;

    // ================= 下载配置 =================
    section(L"下载配置", 16, rowY, IDC_SECTION_DOWNLOAD);
    AddCtl(configWindow, L"STATIC", L"(均为最高获取规格，自动降级返回)", SS_CENTER, 130, rowY, 258, 20, IDC_NOTE_DL);
    rowY += titleGap;
    check(L"启用曲目下载", 16, rowY, IDC_TRACK_DOWNLOAD, 150);
    check(L"启用视频下载", CX, rowY, IDC_VIDEO_DOWNLOAD, 180);
    rowY += stepY;
    label(L"曲目下载音质", 16, rowY, 88);
    HWND tq = AddCtl(configWindow, L"COMBOBOX", L"", CBS_DROPDOWNLIST, 108, rowY - 2, 72, 100, IDC_TRACK_DOWNLOAD_QUALITY);
    SendMessageW(tq, CB_ADDSTRING, 0, (LPARAM)L"MP3");
    SendMessageW(tq, CB_ADDSTRING, 0, (LPARAM)L"FLAC");
    label(L"视频下载分辨率", CX, rowY, 104);
    HWND dvq = AddCtl(configWindow, L"COMBOBOX", L"", CBS_DROPDOWNLIST, CX + 108, rowY - 2, 72, 100, IDC_VIDEO_DOWNLOAD_QUALITY);
    SendMessageW(dvq, CB_ADDSTRING, 0, (LPARAM)L"1080P");
    SendMessageW(dvq, CB_ADDSTRING, 0, (LPARAM)L"720P");
    rowY += stepY;
    check(L"写入曲目信息至Tag", 16, rowY, IDC_ADD_TAG, 170);
    check(L"写入曲目链接至Tag", CX, rowY, IDC_ADD_INFO_TAG, 180);
    rowY += stepY;
    label(L"保存路径", 16, rowY, 72); edit(92, rowY - 2, 204, IDC_DOWNLOAD_PATH); button(L"浏览…", 300, rowY - 4, 80, IDC_BROWSE);
    rowY += sectionGap;

    // ================= 其他配置 =================
    section(L"其他配置", 16, rowY, IDC_SECTION_OTHER);
    rowY += titleGap;
    check(L"自动拆分声音标题", 16, rowY, IDC_PODCAST_RENAME, 160);
    check(L"自动切分超大列表", CX, rowY, IDC_SPLIT_LARGE_PLAYLISTS, 180);
    rowY += sectionGap;

    // ================= 服务与调试 =================
    section(L"服务与调试", 16, rowY, IDC_SECTION_SYSTEM);
    AddCtl(configWindow, L"STATIC", L"（更改 API 相关配置需重启生效）", SS_CENTER, 130, rowY, 258, 20, IDC_NOTE_API);
    rowY += titleGap;
    check(L"启用日志输出", 16, rowY, IDC_LOG_OUTPUT, 160);
    rowY += stepY;
    check(L"显示CMD窗口", 16, rowY, IDC_SHOW_API_CONSOLE, 164);
    AddCtl(configWindow, L"STATIC", L"API 服务端口", SS_LEFT, CX, rowY, 100, 20, 0);
    edit(322, rowY - 2, 60, IDC_API_PORT);   // 与内容配置数字框同一右缘，保持对称留白
    rowY += stepY;
    label(L"API 服务状态：", 16, rowY, 120); label(L"Offline", CX, rowY, 70, IDC_API_STATUS);
    button(L"重启 API 服务", 278, rowY - 4, 110, IDC_RESTART_API);
    rowY += sectionGap;

    // ================= 底部 作者/版本 + 恢复 =================
    // 作者和版本同一行、同一字号，左侧对齐；作者区保留链接点击行为
    const int footerY = rowY + 8;
    // 作者与版本分开（中间空隙不画线），但都带下划线；仅按钮设置窗口内文字带线
    AddCtl(configWindow, L"STATIC", L"By 小小小小铭", SS_LEFT | SS_NOTIFY, 16, footerY, 118, 20, IDC_AUTHOR_LINK);
    HWND hVersion = AddCtl(configWindow, L"STATIC", L"260905 v0.3.0", SS_LEFT | SS_NOTIFY, 16 + 90, footerY, 180, 20, 0);
    button(L"恢复默认配置", 278, footerY - 4, 110, IDC_DEFAULTS, BS_OWNERDRAW);

    // 对数量/端口数字输入框做子类化：无论点哪丢焦都触发 1..上限 回退
    {
        const int numIds[] = { IDC_LIMIT_TRACK, IDC_LIMIT_SEARCH, IDC_LIMIT_CREATED,
                               IDC_LIMIT_SUBSCRIBED, IDC_LIMIT_CREATED_PODCAST,
                               IDC_LIMIT_SUBSCRIBED_PODCAST, IDC_LIMIT_SUBSCRIBED_ALBUM,
                               IDC_COVER, IDC_API_PORT };
        for (int id : numIds) {
            HWND he = GetDlgItem(configWindow, id);
            if (!he) continue;
            // 把原窗口过程存到该控件的 GWLP_USERDATA（这些 edit 未用此槽）
            SetWindowLongPtrW(he, GWLP_USERDATA,
                (LONG_PTR)GetWindowLongPtrW(he, GWLP_WNDPROC));
            SetWindowLongPtrW(he, GWLP_WNDPROC, (LONG_PTR)&CNeteaseCloud::NumEditWndProc);
        }
    }

    // 对所有 STATIC（标签/章节标题/蓝注/头像/状态）做子类化：STATIC 不持焦，
    // 点击它们时键盘焦点仍留在输入框，失焦逻辑永远不触发。点击时把焦点
    // 交还主窗口，使输入框真正失焦（触发越界回退+保存），SS_NOTIFY 点击
    // 行为（如作者名开链接）仍由原过程保留。
    EnumChildWindows(configWindow, [](HWND child, LPARAM) -> BOOL {
        wchar_t cls[32]{};
        GetClassNameW(child, cls, 32);
        if (wcscmp(cls, L"STATIC") == 0) {
            SetWindowLongPtrW(child, GWLP_USERDATA,
                (LONG_PTR)GetWindowLongPtrW(child, GWLP_WNDPROC));
            SetWindowLongPtrW(child, GWLP_WNDPROC,
                (LONG_PTR)&CNeteaseCloud::LabelClickProc);
        }
        return TRUE;
    }, 0);

    EnumChildWindows(configWindow, [](HWND child, LPARAM font) -> BOOL {
        SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
        SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
        return TRUE;
    }, (LPARAM)uiFont);

    // 作者名与版本号都设下划线字体（空隙无线）
    if (linkFont && configWindow) {
        HWND alt = GetDlgItem(configWindow, IDC_AUTHOR_LINK);
        if (alt) SendMessageW(alt, WM_SETFONT, (WPARAM)linkFont, TRUE);   // 作者名
        if (hVersion && IsWindow(hVersion)) SendMessageW(hVersion, WM_SETFONT, (WPARAM)linkFont, TRUE); // 版本
    }

    PopulateConfigWindow();
    RefreshAvatarControl();
    SetTimer(configWindow, 1, 500, nullptr);
    return configWindow;
}

void CNeteaseCloud::PopulateConfigWindow() {
    if (!configWindow) return;
    auto ck=[&](int id,bool v){
        HWND c=GetDlgItem(configWindow,id);
        if (c) SetWindowLongPtrW(c,GWLP_USERDATA, v?1:0);   // 自绘复选框用自存状态
    };
    auto num=[&](int id,int v){SetDlgItemInt(configWindow,id,v,FALSE);};
    ck(IDC_DAILY,config.loadDailyRecommend); ck(IDC_FAVORITES,config.loadFavoriteSongs); ck(IDC_CREATED_PL,config.showCreatedPlaylists); ck(IDC_SUBSCRIBED_PL,config.showSubscribedPlaylists); ck(IDC_CREATED_PODCAST,config.showCreatedPodcasts); ck(IDC_SUBSCRIBED_PODCAST,config.showSubscribedPodcasts); ck(IDC_SUBSCRIBED_ALBUM,config.showSubscribedAlbums); ck(IDC_SUBSCRIBED_VIDEO,config.showSubscribedVideos);
    SendDlgItemMessageW(configWindow, IDC_VIDEO_PLAYBACK_QUALITY, CB_SETCURSEL,
        config.maxVideoPlaybackQuality == 720 ? 1 : 0, 0);
    num(IDC_LIMIT_CREATED,config.limitCreatedPlaylists); num(IDC_LIMIT_SUBSCRIBED,config.limitSubscribedPlaylists); num(IDC_LIMIT_CREATED_PODCAST,config.limitCreatedPodcasts); num(IDC_LIMIT_SUBSCRIBED_PODCAST,config.limitSubscribedPodcasts); num(IDC_LIMIT_SUBSCRIBED_ALBUM,config.limitSubscribedAlbums); num(IDC_LIMIT_TRACK,config.limitItemsPerFolder); num(IDC_LIMIT_SEARCH,config.searchResultLimit); num(IDC_COVER,config.coverSize);
    SendDlgItemMessageW(configWindow,IDC_PLAY_QUALITY,CB_SETCURSEL,config.playQuality=="lossless"?1:0,0);
    ck(IDC_TRACK_DOWNLOAD,config.enableTrackDownload); ck(IDC_VIDEO_DOWNLOAD,config.enableVideoDownload);
    SendDlgItemMessageW(configWindow,IDC_TRACK_DOWNLOAD_QUALITY,CB_SETCURSEL,config.trackDownloadQuality=="lossless"?1:0,0);
    SendDlgItemMessageW(configWindow,IDC_VIDEO_DOWNLOAD_QUALITY,CB_SETCURSEL,config.maxVideoDownloadQuality==720?1:0,0);
    SetDlgItemTextW(configWindow,IDC_DOWNLOAD_PATH,Utf8ToWide(config.downloadPath).c_str()); ck(IDC_ADD_TAG,config.writeTags); ck(IDC_ADD_INFO_TAG,config.writeSourceUrlToComment); ck(IDC_PODCAST_RENAME,config.splitPodcastTitle); ck(IDC_SPLIT_LARGE_PLAYLISTS,config.splitLargeFolders); ck(IDC_LOG_OUTPUT,config.enableLogging); ck(IDC_SHOW_API_CONSOLE,config.showApiConsole); num(IDC_API_PORT,config.apiPort);
    // 搜索单选：自绘按钮无 BM_SETCHECK 状态，直接按 config 重绘四钮
    InvalidateRadioControls();
    UpdateLoginStatusControl(); UpdateApiStatusControl();
}

void CNeteaseCloud::ReadConfigWindow() {
    if (!configWindow) return;
    auto ck=[&](int id){
        HWND h=GetDlgItem(configWindow,id);
        return h && GetWindowLongPtrW(h,GWLP_USERDATA)!=0;   // 读自存勾选状态
    };
    auto num=[&](int id,int old,int hi){
        BOOL ok=FALSE; UINT raw=GetDlgItemInt(configWindow,id,&ok,FALSE);
        int value=ok?ClampInt((int)raw,1,hi):old;
        SetDlgItemInt(configWindow,id,value,FALSE);
        return value;
    };
    auto text = [&](int id) {
        int n = GetWindowTextLengthW(GetDlgItem(configWindow, id));
        wstring value((size_t)n + 1, L'\0');
        GetDlgItemTextW(configWindow, id, value.data(), n + 1);
        value.resize(n);
        return WideToUtf8(value);
    };
    config.loadDailyRecommend=ck(IDC_DAILY); config.loadFavoriteSongs=ck(IDC_FAVORITES); config.showCreatedPlaylists=ck(IDC_CREATED_PL); config.showSubscribedPlaylists=ck(IDC_SUBSCRIBED_PL); config.showCreatedPodcasts=ck(IDC_CREATED_PODCAST); config.showSubscribedPodcasts=ck(IDC_SUBSCRIBED_PODCAST); config.showSubscribedAlbums=ck(IDC_SUBSCRIBED_ALBUM); config.showSubscribedVideos=ck(IDC_SUBSCRIBED_VIDEO);
    int playbackIdx=(int)SendDlgItemMessageW(configWindow,IDC_VIDEO_PLAYBACK_QUALITY,CB_GETCURSEL,0,0); config.maxVideoPlaybackQuality=playbackIdx==1?720:1080;
    config.limitCreatedPlaylists=num(IDC_LIMIT_CREATED,config.limitCreatedPlaylists,999); config.limitSubscribedPlaylists=num(IDC_LIMIT_SUBSCRIBED,config.limitSubscribedPlaylists,999); config.limitCreatedPodcasts=num(IDC_LIMIT_CREATED_PODCAST,config.limitCreatedPodcasts,999); config.limitSubscribedPodcasts=num(IDC_LIMIT_SUBSCRIBED_PODCAST,config.limitSubscribedPodcasts,999); config.limitSubscribedAlbums=num(IDC_LIMIT_SUBSCRIBED_ALBUM,config.limitSubscribedAlbums,999); config.limitItemsPerFolder=num(IDC_LIMIT_TRACK,config.limitItemsPerFolder,999); config.searchResultLimit=num(IDC_LIMIT_SEARCH,config.searchResultLimit,999); config.coverSize=num(IDC_COVER,config.coverSize,999999999);
    config.playQuality=SendDlgItemMessageW(configWindow,IDC_PLAY_QUALITY,CB_GETCURSEL,0,0)==1?"lossless":"exhigh";
    config.enableTrackDownload=ck(IDC_TRACK_DOWNLOAD); config.enableVideoDownload=ck(IDC_VIDEO_DOWNLOAD);
    config.trackDownloadQuality=SendDlgItemMessageW(configWindow,IDC_TRACK_DOWNLOAD_QUALITY,CB_GETCURSEL,0,0)==1?"lossless":"exhigh";
    int vidlIdx=(int)SendDlgItemMessageW(configWindow,IDC_VIDEO_DOWNLOAD_QUALITY,CB_GETCURSEL,0,0); config.maxVideoDownloadQuality=vidlIdx==1?720:1080;
    config.downloadPath=text(IDC_DOWNLOAD_PATH); config.writeTags=ck(IDC_ADD_TAG); config.writeSourceUrlToComment=ck(IDC_ADD_INFO_TAG); config.splitPodcastTitle=ck(IDC_PODCAST_RENAME); config.splitLargeFolders=ck(IDC_SPLIT_LARGE_PLAYLISTS); config.enableLogging=ck(IDC_LOG_OUTPUT); config.showApiConsole=ck(IDC_SHOW_API_CONSOLE); config.apiPort=num(IDC_API_PORT,config.apiPort,65535);
    // 搜索类型单选（自绘）：选中状态即 config.searchType 本身，点击时已更新，
    // 这里不再用 BM_GETCHECK 反读（自绘按钮读不到，会把选择错误冲回 1）。
    // 此处仅持久化端口改动；它不会自动重启 API，也不会立刻改 ApiBase ——
    // 要等用户按下“重启 API 服务”才会生效。
    SaveSettings(false); ApplySettings();
}

void CNeteaseCloud::UpdateLoginStatusControl() {
    if (!configWindow || !IsWindow(configWindow)) return;
    string uid, nickname;
    {
        lock_guard<mutex> lock(stateMutex);
        uid = userId;
        nickname = userNickname;
    }
    SetDlgItemTextW(configWindow, IDC_NICKNAME,
        uid.empty() ? L"用户名：未登录" : (L"用户名：" + (nickname.empty() ? wstring(L"网易云音乐用户") : Utf8ToWide(nickname))).c_str());
    SetDlgItemTextW(configWindow, IDC_STATUS,
        uid.empty() ? L"用户 ID：未登录" : (L"用户 ID：" + Utf8ToWide(uid)).c_str());
}

void CNeteaseCloud::InvalidateRadioControls() {
    // 自绘单选无勾选状态可设，直接整体失效重绘；WM_DRAWITEM 按 config.searchType
    // 决定谁画蓝圈蓝点、谁画灰圈白字。
    if (!configWindow || !IsWindow(configWindow)) return;
    static const int radioIds[] = { IDC_SEARCH_SONG, IDC_SEARCH_VOICE, IDC_SEARCH_VIDEO, IDC_SEARCH_MV };
    for (int id : radioIds) {
        HWND ctl = GetDlgItem(configWindow, id);
        if (ctl) InvalidateRect(ctl, nullptr, TRUE);
    }
}

// 数字输入框子类化：任意方式失焦时都对本框做“1..上限”回退，避免点击
// 标签/空白(STATIC 不持焦)不触发 EN_KILLFOCUS、光标不丢也不回退的问题。
LRESULT CALLBACK CNeteaseCloud::NumEditWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KILLFOCUS) {
        const int id = GetDlgCtrlID(hWnd);
        // 端口/封面尺寸框用更大上限；其余是 1..999 的列表数量
        // 封面上限放开（无上限，仅保下限 1）；端口 65535；其余数量 999
        const bool noUpper = (id == IDC_COVER);
        const int hi = (id == IDC_API_PORT) ? 65535 : (noUpper ? 999999999 : 999);
        wchar_t buf[24];
        const int len = GetWindowTextLengthW(hWnd);
        const int n = (len + 1) < (int)_countof(buf) ? len : ((int)_countof(buf) - 1);
        if (n > 0) {
            GetWindowTextW(hWnd, buf, n + 1);
            // 只要有内容就解析并强制回退到合法区间（含输入 0 的情况）
            int v = _wtoi(buf);
            int clamped = v;
            if (clamped < 1) clamped = 1;
            if (clamped > hi) clamped = hi;
            if (clamped != v) {
                wchar_t tmp[16];
                _itow_s(clamped, tmp, _countof(tmp), 10);
                SetWindowTextW(hWnd, tmp);
            }
        }
    }
    // 交还原窗口过程
    WNDPROC orig = (WNDPROC)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    return orig ? CallWindowProcW(orig, hWnd, msg, wParam, lParam)
                : DefWindowProcW(hWnd, msg, wParam, lParam);
}

// STATIC 子类化：STATIC 不持焦，点击它时键盘焦点仍留在输入框上，
// WM_KILLFOCUS/EN_KILLFOCUS 永远不触发。这里在点击时把焦点交给主窗口，
// 使正在编辑的输入框真正失焦（从而触发越界回退与保存），随后仍走原
// 过程保留 SS_NOTIFY 的 STN_CLICKED 等行为。
LRESULT CALLBACK CNeteaseCloud::LabelClickProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_LBUTTONDOWN) {
        HWND parent = GetParent(hWnd);
        if (parent) SetFocus(parent);
    }
    WNDPROC orig = (WNDPROC)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    return orig ? CallWindowProcW(orig, hWnd, msg, wParam, lParam)
                : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK CNeteaseCloud::ConfigWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    CNeteaseCloud* self=(CNeteaseCloud*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_NCCREATE){self=(CNeteaseCloud*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)self);}
    return self?self->HandleConfigMessage(hwnd,msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CNeteaseCloud::HandleConfigMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // 禁止标题栏右键弹出系统菜单
    if (msg == WM_NCRBUTTONDOWN || msg == WM_NCRBUTTONUP) return 0;
    if (msg == WM_COMMAND) {
        int id = LOWORD(wp);
        if (id == IDC_LOGIN) { BeginQrLogin(); return 0; }
        if (id == IDC_LOGOUT) { LogoutAccount(); return 0; }
        if (id == IDC_DEFAULTS) {
            SetDefaultSettings();
            SaveSettings(); // 恢复默认时打一行「配置已保存」
            // 不自动重启：影响 API 的值（端口、是否显示控制台等）只有在用户
            // 按下“重启 API 服务”之后才会真正生效。
            ApplySettings();
            PopulateConfigWindow();
            // 强制自绘控件（复选框等）按新默认值重绘
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (id == IDC_BROWSE) {
            BROWSEINFOW bi{};
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"请选择歌曲下载目录";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH]{};
                if (SHGetPathFromIDListW(pidl, path)) { SetDlgItemTextW(hwnd, IDC_DOWNLOAD_PATH, path); ReadConfigWindow(); }
                CoTaskMemFree(pidl);
            }
            return 0;
        }
        if (id == IDC_RESTART_API) { RestartApiService(); return 0; }
        // 搜索单选（自绘）：点击即写入 config.searchType，随后统一读回+保存并重绘四钮
        if (id == IDC_SEARCH_SONG || id == IDC_SEARCH_VOICE || id == IDC_SEARCH_VIDEO || id == IDC_SEARCH_MV) {
            if (HIWORD(wp) == BN_CLICKED) {
                config.searchType = id == IDC_SEARCH_SONG ? 1 : id == IDC_SEARCH_VOICE ? 2000
                                  : id == IDC_SEARCH_VIDEO ? 1014 : 1004;
                ReadConfigWindow();          // 持久化（searchType 不在 Read 中反读）
                InvalidateRadioControls();   // 按新选中态重绘
            }
            return 0;
        }
        if (id == IDC_AUTHOR_LINK && HIWORD(wp) == STN_CLICKED) {
            ShellExecuteW(nullptr,L"open",L"https://space.bilibili.com/475951038",nullptr,nullptr,SW_SHOWNORMAL);
            ShellExecuteW(nullptr,L"open",L"https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ",nullptr,nullptr,SW_SHOWNORMAL);
            return 0;
        }
        // 自绘复选框点击：我们自己翻转 BM 状态并重绘（无 AUTOBOX 系统自管）
        {
            const int cb = (int)LOWORD(wp);
            bool isCheck = cb==IDC_DAILY||cb==IDC_FAVORITES||cb==IDC_CREATED_PL||cb==IDC_SUBSCRIBED_PL||
                cb==IDC_CREATED_PODCAST||cb==IDC_SUBSCRIBED_PODCAST||cb==IDC_SUBSCRIBED_ALBUM||
                cb==IDC_SUBSCRIBED_VIDEO||cb==IDC_TRACK_DOWNLOAD||cb==IDC_VIDEO_DOWNLOAD||
                cb==IDC_ADD_TAG||cb==IDC_ADD_INFO_TAG||cb==IDC_PODCAST_RENAME||
                cb==IDC_SPLIT_LARGE_PLAYLISTS||cb==IDC_LOG_OUTPUT||cb==IDC_SHOW_API_CONSOLE;
            if (isCheck && HIWORD(wp) == BN_CLICKED) {
                HWND ctl = GetDlgItem(hwnd, cb);
                if (ctl) {
                    bool now = GetWindowLongPtrW(ctl, GWLP_USERDATA) != 0;
                    SetWindowLongPtrW(ctl, GWLP_USERDATA, now ? 0 : 1);
                    InvalidateRect(ctl, nullptr, TRUE);
                }
            }
        }
        if (HIWORD(wp) == BN_CLICKED || HIWORD(wp) == CBN_SELCHANGE || HIWORD(wp) == EN_KILLFOCUS) {
            ReadConfigWindow();
            return 0;
        }
    }
    // 点击窗口真正的空白区（无子控件覆盖、消息直接到父窗）：主动把焦点
    // 交给主窗口，让正在编辑的输入框失焦，触发越界回退与保存。
    if (msg == WM_LBUTTONDOWN) { SetFocus(hwnd); return 0; }
    if (msg == WM_CTLCOLORSTATIC) {
        HDC dc = (HDC)wp;
        HWND control = (HWND)lp;
        int id = GetDlgCtrlID(control);
        SetBkMode(dc, TRANSPARENT);
        if (id == IDC_API_STATUS) {
            wchar_t status[16]{}; GetWindowTextW(control,status,16);
            SetTextColor(dc, wcscmp(status,L"Online")==0 ? RGB(60,210,110) : RGB(255,90,90));
        } else if (id >= IDC_SECTION_ACCOUNT && id <= IDC_SECTION_SYSTEM)
            SetTextColor(dc, RGB(0,153,255));
        else SetTextColor(dc, RGB(238,238,238));
        return (LRESULT)darkBackgroundBrush;
    }
    if (msg == WM_DRAWITEM && wp == IDC_DEFAULTS) {
        auto* draw=(DRAWITEMSTRUCT*)lp;
        HBRUSH brush=CreateSolidBrush((draw->itemState & ODS_SELECTED)?RGB(150,35,35):RGB(195,45,45));
        HPEN pen=CreatePen(PS_SOLID, 1, (draw->itemState & ODS_SELECTED)?RGB(120,25,25):RGB(170,35,35));
        HGDIOBJ oldBrush=SelectObject(draw->hDC, brush);
        HGDIOBJ oldPen=SelectObject(draw->hDC, pen);
        RoundRect(draw->hDC, draw->rcItem.left, draw->rcItem.top,
            draw->rcItem.right, draw->rcItem.bottom, 8, 8);
        SelectObject(draw->hDC, oldPen); SelectObject(draw->hDC, oldBrush);
        DeleteObject(pen); DeleteObject(brush);
        SetBkMode(draw->hDC,TRANSPARENT); SetTextColor(draw->hDC,RGB(255,255,255));
        HFONT oldFont = uiFont ? (HFONT)SelectObject(draw->hDC, uiFont) : nullptr;
        DrawTextW(draw->hDC,L"恢复默认配置",-1,&draw->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if (oldFont) SelectObject(draw->hDC, oldFont);
        return TRUE;
    }
    // 搜索单选（自绘）：深色底 + 灰圈/蓝圈，选中画蓝点，文字白色；圆圈尺寸跟随 g_uiScale
    if (msg == WM_DRAWITEM && (wp == IDC_SEARCH_SONG || wp == IDC_SEARCH_VOICE ||
                               wp == IDC_SEARCH_VIDEO || wp == IDC_SEARCH_MV)) {
        auto* draw = (DRAWITEMSTRUCT*)lp;
        const int id = (int)wp;
        const int type = id == IDC_SEARCH_SONG ? 1 : id == IDC_SEARCH_VOICE ? 2000
                       : id == IDC_SEARCH_VIDEO ? 1014 : 1004;
        const wchar_t* text = id == IDC_SEARCH_SONG ? L"单曲" : id == IDC_SEARCH_VOICE ? L"声音"
                            : id == IDC_SEARCH_VIDEO ? L"视频" : L"MV";
        const bool selected = config.searchType == type;
        const COLORREF accent = RGB(0, 153, 255);
        FillRect(draw->hDC, &draw->rcItem, darkBackgroundBrush);
        const int cy = (draw->rcItem.top + draw->rcItem.bottom) / 2;
        const int r = ScaleUi(7);                     // 外圈半径
        const int gx = draw->rcItem.left + ScaleUi(4); // 圆圈左缘
        HPEN pen = CreatePen(PS_SOLID, 1, selected ? accent : RGB(150, 150, 150));
        HGDIOBJ oldPen = SelectObject(draw->hDC, pen);
        HGDIOBJ oldBrush = SelectObject(draw->hDC, GetStockObject(NULL_BRUSH));
        Ellipse(draw->hDC, gx, cy - r, gx + r * 2, cy + r);   // 外圈
        if (selected) {
            HBRUSH dot = CreateSolidBrush(accent);
            HGDIOBJ oldDot = SelectObject(draw->hDC, dot);
            Ellipse(draw->hDC, gx + ScaleUi(4), cy - ScaleUi(3),
                                gx + ScaleUi(10), cy + ScaleUi(3)); // 内点
            SelectObject(draw->hDC, oldDot);
            DeleteObject(dot);
        }
        SelectObject(draw->hDC, oldPen); DeleteObject(pen);
        SetBkMode(draw->hDC, TRANSPARENT);
        // 文字恒定白色：是否选中只通过 圈的颜色(灰/蓝) 与 蓝点 区分
        SetTextColor(draw->hDC, RGB(238, 238, 238));
        if (uiFont) SelectObject(draw->hDC, uiFont);
        RECT trc = draw->rcItem; trc.left = gx + r * 2 + ScaleUi(5);
        DrawTextW(draw->hDC, text, -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }
    // 复选框自绘：方块大小跟随 g_uiScale，避免高 DPI 下过小
    if (msg == WM_DRAWITEM &&
        (wp==IDC_DAILY||wp==IDC_FAVORITES||wp==IDC_CREATED_PL||wp==IDC_SUBSCRIBED_PL||
         wp==IDC_CREATED_PODCAST||wp==IDC_SUBSCRIBED_PODCAST||wp==IDC_SUBSCRIBED_ALBUM||
         wp==IDC_SUBSCRIBED_VIDEO||wp==IDC_TRACK_DOWNLOAD||wp==IDC_VIDEO_DOWNLOAD||
         wp==IDC_ADD_TAG||wp==IDC_ADD_INFO_TAG||wp==IDC_PODCAST_RENAME||
         wp==IDC_SPLIT_LARGE_PLAYLISTS||wp==IDC_LOG_OUTPUT||wp==IDC_SHOW_API_CONSOLE)) {
        auto* draw = (DRAWITEMSTRUCT*)lp;
        const int cid = (int)wp;
        const bool on = GetWindowLongPtrW(GetDlgItem(hwnd,cid), GWLP_USERDATA) != 0;
        FillRect(draw->hDC, &draw->rcItem, darkBackgroundBrush);
        const int box = ScaleUi(16);
        const int half = box / 2;
        int cy = draw->rcItem.top + (draw->rcItem.bottom - draw->rcItem.top) / 2;
        int lft = draw->rcItem.left + ScaleUi(3);
        int top = cy - half, bot = cy + half;
        // 内部填充：选中=亮蓝，未选=深灰
        HBRUSH fill = CreateSolidBrush(on ? RGB(0,120,220) : RGB(52,52,52));
        RECT boxR = { lft, top, lft + box, bot };
        FillRect(draw->hDC, &boxR, fill); DeleteObject(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, on ? RGB(120,190,255) : RGB(140,140,140));
        HGDIOBJ op = SelectObject(draw->hDC, pen);
        HGDIOBJ ob = SelectObject(draw->hDC, GetStockObject(NULL_BRUSH));
        Rectangle(draw->hDC, lft, top, lft + box, bot);
        SelectObject(draw->hDC, ob); SelectObject(draw->hDC, op); DeleteObject(pen);
        if (on) {
            // 粗白勾，更清晰（线宽与位置也跟随缩放）
            HPEN ck = CreatePen(PS_SOLID, ScaleUi(2), RGB(255,255,255));
            HGDIOBJ oc = SelectObject(draw->hDC, ck);
            MoveToEx(draw->hDC, lft + ScaleUi(4), top + ScaleUi(8), nullptr);
            LineTo(draw->hDC, lft + ScaleUi(7), top + ScaleUi(11));
            LineTo(draw->hDC, lft + ScaleUi(12), top + ScaleUi(4));
            SelectObject(draw->hDC, oc); DeleteObject(ck);
        }
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, RGB(238,238,238));
        if (uiFont) SelectObject(draw->hDC, uiFont);
        wchar_t txt[128]; GetWindowTextW(GetDlgItem(hwnd,cid), txt, 128);
        RECT tr = draw->rcItem; tr.left = lft + box + ScaleUi(4);
        DrawTextW(draw->hDC, txt, -1, &tr, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        return TRUE;
    }
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) {
        HDC dc = (HDC)wp;
        SetTextColor(dc, RGB(245, 245, 245));
        SetBkColor(dc, RGB(45, 45, 45));
        return (LRESULT)darkControlBrush;
    }
    if (msg == WM_CTLCOLORBTN) {
        HDC dc = (HDC)wp;
        SetTextColor(dc, RGB(238, 238, 238));
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)darkBackgroundBrush;
    }
    if (msg == WM_TIMER) {
        if (wp == 1) { UpdateApiStatusControl(); return 0; }
    }
    if (msg == WM_CLOSE) { ShowWindow(hwnd, SW_HIDE); return 0; }
    if (msg == WM_DESTROY) { KillTimer(hwnd, 1); configWindow = nullptr; return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
HRESULT VDJ_API CNeteaseCloud::OnGetUserInterface(TVdjPluginInterface8* ui) { HWND w=CreateConfigWindow(); if(!w)return E_FAIL; ui->Type=VDJINTERFACE_DIALOG; ui->hWnd=w; return S_OK; }
