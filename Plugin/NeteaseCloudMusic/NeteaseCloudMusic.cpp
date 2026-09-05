#pragma execution_character_set("utf-8")
#define _CRT_SECURE_NO_WARNINGS
#define _HAS_STD_BYTE 0
#include "NeteaseCloudMusic.h"
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <set>
#include <initializer_list>

#include <thread>

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tbytevector.h>

// 处理 MP3 (ID3v2) 封面所需
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>


// 处理 FLAC 封面所需
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>





// 输出日志
void CNeteaseCloud::WriteLog(const string& text) {
    // 确保日志写在 DLL 所在的目录下

    if (!config.enableLogging) return;

    string logPath = GetPluginPath() + "log.log";
    ofstream f(logPath, ios::app);
    if (f.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timestamp[64];
        // 输出格式：[2026-04-20 16:30:05.123] 日志内容
        sprintf(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        f << timestamp << text << endl;
        f.close();
    }
}

// 给封面 URL 追加最大尺寸参数 ?param=NxN（剥去已有查询串）
static string SizedCover(const string& url, int size) {
    if (url.empty() || size < 1) return url;
    size_t q = url.find('?');
    string u = (q == string::npos) ? url : url.substr(0, q);
    return u + "?param=" + std::to_string(size) + "y" + std::to_string(size);
}

// 声音/播客封面：优先节目自己的 coverUrl（与电脑版一致），
// 没有再退到电台头像 radio.picUrl，最后才用原曲专辑图 mainSong.album.picUrl。
static string FirstNonEmptyUrl(std::initializer_list<string> urls) {
    for (const string& u : urls) {
        if (!u.empty()) return u;
    }
    return {};
}
static string PodcastCoverFromSearch(const Json::Value& resource, const Json::Value& binfo) {
    return FirstNonEmptyUrl({
        binfo.get("coverUrl", "").asString(),
        resource["uiElement"]["image"].get("imageUrl", "").asString(),
        binfo["radio"].get("picUrl", "").asString(),
        resource["baseInfo"]["radio"].get("picUrl", "").asString(),
        binfo["mainSong"]["album"].get("picUrl", "").asString()
    });
}
static string PodcastCoverFromProgram(const Json::Value& program) {
    return FirstNonEmptyUrl({
        program.get("coverUrl", "").asString(),
        program["radio"].get("picUrl", "").asString(),
        program["mainSong"]["album"].get("picUrl", "").asString()
    });
}

// 把英文字母转成小写，便于比较 URL 路径/主机名
static string ToLowerCopy(string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    return s;
}

// 去掉首尾空白
static string TrimCopy(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// 是否整串都是数字（用来区分官方 MV 的纯数字 id 和云视频的 hash id）
static bool IsPureDigits(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

// 把 JSON 里的 id（字符串或数字）统一转成字符串
static string JsonId(const Json::Value& v) {
    if (v.isString()) return v.asString();
    if (v.isUInt()) return to_string(v.asUInt());
    if (v.isInt()) return to_string(v.asInt());
    if (v.isDouble()) return to_string((long long)v.asDouble());
    return {};
}

// 把 JSON 时长字段（毫秒，数字或字符串）转成 long long
static long long JsonMs(const Json::Value& v) {
    if (v.isUInt()) return (long long)v.asUInt();
    if (v.isInt()) return (long long)v.asInt();
    if (v.isDouble()) return (long long)v.asDouble();
    if (v.isString()) {
        try { return stoll(v.asString()); }
        catch (...) { return 0; }
    }
    return 0;
}

static string JsonId(const Json::Value& obj, const char* key) {
    return JsonId(obj[key]);
}

// jsoncpp 的 asInt/asString 在类型不符或值为 null 时会抛异常；下载线程里未捕获就会把 VDJ 一起干掉
static string JsonStr(const Json::Value& v) {
    if (v.isNull() || v.isObject() || v.isArray()) return {};
    try { return v.asString(); }
    catch (...) { return {}; }
}
static string JsonStr(const Json::Value& obj, const char* key) {
    return obj.isObject() ? JsonStr(obj[key]) : string{};
}
static int JsonIntLoose(const Json::Value& v, int fallback) {
    try {
        if (v.isInt()) return v.asInt();
        if (v.isUInt()) return (int)v.asUInt();
        if (v.isDouble()) return (int)v.asDouble();
        if (v.isString()) return stoi(v.asString());
    }
    catch (...) {}
    return fallback;
}

// 从 /mv/url 或 /video/url 的 JSON 里取出第一条可用直链和实际清晰度
static bool ExtractMvDownloadUrl(const Json::Value& root, string& url, int& quality) {
    if (!root["data"]["url"].isString()) return false;
    url = root["data"]["url"].asString();
    if (url.empty()) return false;
    quality = JsonIntLoose(root["data"]["r"], quality);
    return true;
}
static bool ExtractVideoDownloadUrl(const Json::Value& root, string& url, int& quality) {
    const Json::Value& urls = root["urls"];
    if (!urls.isArray()) return false;
    for (const auto& item : urls) {
        if (!item["url"].isString()) continue;
        string u = item["url"].asString();
        if (u.empty()) continue;
        url = u;
        quality = JsonIntLoose(item["r"], quality);
        return true;
    }
    return false;
}

// 把艺人/创作者数组拼成逗号分隔的名字
static string JoinNameList(const Json::Value& arr, const char* key1, const char* key2) {
    string out;
    if (arr.isArray()) {
        for (const auto& a : arr) {
            string n = a.get(key1, "").asString();
            if (n.empty()) n = a.get(key2, "").asString();
            if (n.empty()) continue;
            if (!out.empty()) out += ",";
            out += n;
        }
    }
    else if (arr.isObject()) {
        out = arr.get(key1, "").asString();
        if (out.empty()) out = arr.get(key2, "").asString();
    }
    return out;
}

// 搜索/链接解析时添加一条视频或 MV
static void AddSearchVideo(IVdjTracksList* list, int coverSize,
    const string& prefix, const string& vid, const string& title,
    const string& creator, string cover, long long durationMs) {
    if (!list || vid.empty() || title.empty()) return;
    if (cover.find("//") == 0) cover = "https:" + cover;
    cover = SizedCover(cover, coverSize);
    const float lengthSec = (float)(durationMs > 0 ? durationMs : 0) / 1000.0f;
    list->add((prefix + vid).c_str(), title.c_str(), creator.c_str(),
        nullptr, nullptr, nullptr, nullptr, cover.c_str(), nullptr,
        lengthSec, 0.0f, 0, 0, true);
}

// 去掉 id 后面可能粘上的查询串、路径或音频扩展名
static string CleanResourceId(string id) {
    if (id.empty()) return {};
    size_t cut = id.find_first_of("&?#/ ");
    if (cut != string::npos) id = id.substr(0, cut);
    size_t dot = id.rfind('.');
    if (dot != string::npos) {
        string ext = ToLowerCopy(id.substr(dot + 1));
        if (ext == "mp3" || ext == "flac" || ext == "wav" || ext == "m4a" ||
            ext == "aac" || ext == "ogg" || ext == "mp4" || ext == "ncm")
            id = id.substr(0, dot);
    }
    return id;
}

// 从 URL 查询串取指定参数（忽略 userid 等无关字段）
static string QueryParamRaw(const string& url, const string& key) {
    const string lower = ToLowerCopy(url);
    const string pat = ToLowerCopy(key) + "=";
    size_t p = 0;
    while ((p = lower.find(pat, p)) != string::npos) {
        if (p == 0 || lower[p - 1] == '?' || lower[p - 1] == '&' || lower[p - 1] == '#' || lower[p - 1] == ';') {
            size_t vs = p + pat.size();
            size_t ve = vs;
            while (ve < url.size() && url[ve] != '&' && url[ve] != '#' && url[ve] != '?' && url[ve] != '/') ++ve;
            return CleanResourceId(url.substr(vs, ve - vs));
        }
        ++p;
    }
    return {};
}

// 判断小写 URL 是否包含 /song、/playlist 这类路径段
static bool HasUrlPath(const string& lower, const char* name) {
    const string token = string("/") + name;
    size_t p = 0;
    while ((p = lower.find(token, p)) != string::npos) {
        const size_t after = p + token.size();
        const char c = (after < lower.size()) ? lower[after] : '\0';
        if (c == '\0' || c == '/' || c == '?' || c == '&' || c == '#' || c == '=' || c == '.') return true;
        ++p;
    }
    return false;
}

// 取 /song/123 这种路径里紧跟在类型名后面的 id
static string PathIdAfter(const string& orig, const string& lower, const char* name) {
    const string token = string("/") + name + "/";
    const size_t p = lower.find(token);
    if (p == string::npos) return {};
    size_t vs = p + token.size();
    size_t ve = vs;
    while (ve < orig.size()) {
        const unsigned char c = (unsigned char)orig[ve];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-'))
            break;
        ++ve;
    }
    if (ve == vs) return {};
    return CleanResourceId(orig.substr(vs, ve - vs));
}

// 规范化网易云链接：去空白、反斜杠改正斜杠、去掉 /#/
static string NormalizeNcmUrl(string url) {
    url = TrimCopy(url);
    for (char& c : url) if (c == '\\') c = '/';
    size_t h;
    while ((h = url.find("/#/")) != string::npos) url.replace(h, 3, "/");
    while ((h = url.find("/#")) != string::npos) url.replace(h, 2, "/");
    return url;
}

// 插件支持的网易云链接类型
enum class NcmLinkKind { None, Song, Program, Radio, Playlist, Album, Mv, Video };

struct NcmLink {
    NcmLinkKind kind = NcmLinkKind::None;
    string id;
};

// 是否网易云正式域名（不含短链）
static bool IsNeteaseHost(const string& lower) {
    return lower.find("music.163.com") != string::npos;
}

// 从搜索框文本里抽出第一条 music.163.com 链接
static bool ExtractFirstNeteaseUrl(const string& text, string& url) {
    const string t = TrimCopy(text);
    if (t.empty()) return false;
    const string lower = ToLowerCopy(t);
    auto take = [&](size_t start) -> string {
        size_t end = start;
        while (end < t.size()) {
            const unsigned char c = (unsigned char)t[end];
            if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' ||
                c == ')' || c == ']' || c == '}' || c == '|')
                break;
            ++end;
        }
        while (end > start) {
            const char c = t[end - 1];
            if (c == '.' || c == ',' || c == ';' || c == '!' || c == '?' || c == '，' || c == '。') --end;
            else break;
        }
        return t.substr(start, end - start);
    };

    size_t best = string::npos;
    auto consider = [&](const char* needle) {
        const size_t p = lower.find(needle);
        if (p != string::npos && (best == string::npos || p < best)) best = p;
    };
    consider("https://");
    consider("http://");
    consider("music.163.com");
    if (best == string::npos) return false;
    if (!IsNeteaseHost(lower)) return false;

    string u = take(best);
    const string ul = ToLowerCopy(u);
    if (!IsNeteaseHost(ul)) return false;
    if (ul.find("://") == string::npos) u = "https://" + u;
    url = u;
    return true;
}

// 按路径识别链接类型，只取资源 id，忽略 userid 等分享参数
static NcmLink ParseNcmLink(string url) {
    NcmLink out;
    url = NormalizeNcmUrl(url);
    const string lower = ToLowerCopy(url);

    auto idOf = [&](const char* key) { return QueryParamRaw(url, key); };
    auto pathId = [&](const char* name) { return PathIdAfter(url, lower, name); };
    auto setKind = [&](NcmLinkKind k, const string& id) -> bool {
        if (id.empty()) return false;
        out.kind = k;
        out.id = id;
        return true;
    };

    if (lower.find("outchain") != string::npos) {
        const string type = idOf("type");
        const string id = idOf("id");
        if (!id.empty()) {
            if (type == "0") out.kind = NcmLinkKind::Playlist;
            else if (type == "1") out.kind = NcmLinkKind::Album;
            else if (type == "4") out.kind = NcmLinkKind::Mv;
            else out.kind = NcmLinkKind::Song;
            out.id = id;
            return out;
        }
    }

    if (HasUrlPath(lower, "djradio") || HasUrlPath(lower, "radio")) {
        if (setKind(NcmLinkKind::Radio, idOf("id")) || setKind(NcmLinkKind::Radio, idOf("rid")) ||
            setKind(NcmLinkKind::Radio, pathId("djradio")) || setKind(NcmLinkKind::Radio, pathId("radio")))
            return out;
    }
    if (HasUrlPath(lower, "program") || HasUrlPath(lower, "dj")) {
        if (setKind(NcmLinkKind::Program, idOf("id")) ||
            setKind(NcmLinkKind::Program, pathId("program")) || setKind(NcmLinkKind::Program, pathId("dj")))
            return out;
    }
    if (HasUrlPath(lower, "playlist") || HasUrlPath(lower, "toplist") || HasUrlPath(lower, "my/m/playlist")) {
        if (setKind(NcmLinkKind::Playlist, idOf("id")) || setKind(NcmLinkKind::Playlist, idOf("pid")) ||
            setKind(NcmLinkKind::Playlist, pathId("playlist")) || setKind(NcmLinkKind::Playlist, pathId("toplist")))
            return out;
    }
    if (HasUrlPath(lower, "album")) {
        if (setKind(NcmLinkKind::Album, idOf("id")) || setKind(NcmLinkKind::Album, pathId("album")))
            return out;
    }
    if (HasUrlPath(lower, "mlog") || HasUrlPath(lower, "video")) {
        if (setKind(NcmLinkKind::Video, idOf("id")) || setKind(NcmLinkKind::Video, idOf("vid")) ||
            setKind(NcmLinkKind::Video, pathId("mlog")) || setKind(NcmLinkKind::Video, pathId("video")))
            return out;
    }
    if (HasUrlPath(lower, "mv")) {
        if (setKind(NcmLinkKind::Mv, idOf("id")) || setKind(NcmLinkKind::Mv, idOf("mvid")) ||
            setKind(NcmLinkKind::Mv, pathId("mv")))
            return out;
    }
    if (HasUrlPath(lower, "song") || lower.find("song/media/outer") != string::npos) {
        // 分享链接常带 userid=...，只取资源 id / songid，忽略用户字段
        if (setKind(NcmLinkKind::Song, idOf("id")) || setKind(NcmLinkKind::Song, idOf("songid")) ||
            setKind(NcmLinkKind::Song, pathId("song")))
            return out;
    }
    return out;
}

// 获取 插件DLL 所在目录
string CNeteaseCloud::GetPluginPath() {
    char p[MAX_PATH];
    GetModuleFileNameA(hInstance, p, MAX_PATH);
    string s = p;
    return s.substr(0, s.find_last_of("\\/") + 1);
}


// 读取登录状态 JSON 中保存的 UID。
string CNeteaseCloud::GetUserIdFromData() {
    lock_guard<mutex> lock(stateMutex);
    return userId;
}


// 解析Cookie
string CNeteaseCloud::ExtractLeanCookie(const string& raw) {
    string res;
    vector<string> keys = { "MUSIC_U", "NMTID", "__csrf" };
    stringstream ss(raw);
    string item;

    while (getline(ss, item, ';')) {
        // 去除前面的空格
        size_t s = item.find_first_not_of(" ");
        if (s != string::npos) item = item.substr(s);

        for (auto& k : keys) {
            if (item.compare(0, k.length(), k) == 0) {
                if (!res.empty()) res += "; ";
                res += item;
                break;
            }
        }
    }

    return res;
}


// 网络请求基准
string CNeteaseCloud::HttpGet(const string& url, bool logFailure, long timeoutS) {
    CURL* curl = curl_easy_init();
    string buf;
    if (curl) {
        string lean;
        {
            lock_guard<mutex> lock(stateMutex);
            lean = leanLoginCookie;
        }
        struct curl_slist* h = NULL;
        // 如果 lean 不为空，则添加 Cookie 请求头
        if (!lean.empty()) {
            string cookieHeader = "Cookie: " + lean;
            h = curl_slist_append(h, cookieHeader.c_str());
        }

        h = curl_slist_append(h, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* c, size_t s, size_t n, void* u) {
            ((string*)u)->append((char*)c, s * n); return s * n;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutS);

        CURLcode code = curl_easy_perform(curl);
        if (code != CURLE_OK && logFailure) {
            WriteLog("[NETWORK] 请求失败：" + string(curl_easy_strerror(code)) + "，地址：" + url);
        }

        if (h) curl_slist_free_all(h);
        curl_easy_cleanup(curl);
    }
    return buf;
}



// 加载插件
HRESULT VDJ_API CNeteaseCloud::OnLoad() {
    curl_global_init(CURL_GLOBAL_ALL);
    LoadSettings();
    LoadLoginData();
    // 创建「关闭作业时结束进程」的作业对象，使 VDJ 进程退出时无论 Release() 是否
    // 被调用，API 都会自动随之消亡。
    CreateApiJob();
    // 独立拉起、不等待 API 初始化完成，让 VirtualDJ 的启动与退出都和 API
    // 的就绪状态解耦。
    StartApiIfNeeded();
    StartApiMonitor();

    if ((config.enableTrackDownload || config.enableVideoDownload) && !config.downloadPath.empty()) {
        std::wstring wDownloadPath = Utf8ToWide(config.downloadPath);

        // CreateDirectoryW 如果文件夹已存在会返回 FALSE，但不会报错，所以可以直接调用
        if (CreateDirectoryW(wDownloadPath.c_str(), NULL)) {
            WriteLog("[DL] 下载功能已启用，成功创建下载目录: " + config.downloadPath);
        }
        else {
            DWORD err = GetLastError();
            if (err == ERROR_ALREADY_EXISTS) {
                WriteLog("[DL] 下载目录已存在，无需重复创建。");
            }
            else {
                WriteLog("[DL][ERROR] 无法创建下载目录，错误代码：" + to_string(err));
            }
        }
    }

    WriteLog("-----插件加载完成-----");
    return S_OK;
}


// 创建歌单文件夹 (按顺序：日推 -> 普通 -> 播客)
HRESULT VDJ_API CNeteaseCloud::GetFolderList(IVdjSubfoldersList* subList) {
    string uid = GetUserIdFromData();
    WriteLog("-----开始加载文件夹列表-----");
    if (uid.empty()) {
        WriteLog("[FOLDER] 用户 ID 为空，取消加载列表");
        return S_OK;
    }
    WriteLog("[FOLDER] 当前用户 ID：" + uid);

    // 统计计数器
    int totalNormalPlaylists = 0;
    int totalPodcastPlaylists = 0;
    int totalSubscribedAlbums = 0;
    int totalVideoFolders = 0;
    const int chunkSize = (std::max)(1, (std::min)(999, config.limitItemsPerFolder));
    auto addNormalPlaylist = [&](const Json::Value& pl) {
        string sourceId = pl["id"].asString();
        string name = pl["name"].asString();
        int trackCount = pl.get("trackCount", 0).asInt();
        int parts = config.splitLargeFolders && trackCount > chunkSize
            ? (trackCount + chunkSize - 1) / chunkSize : 1;
        for (int part = 1; part <= parts; ++part) {
            string id = "NCM_NORMAL_LIST_" + sourceId;
            string displayName = name;
            if (parts > 1) {
                id += "_PART_" + to_string(part);
                displayName += "-" + to_string(part);
            }
            subList->add(id.c_str(), displayName.c_str());
        }
        totalNormalPlaylists += parts;
    };
    auto addPodcastPlaylist = [&](const Json::Value& radio) {
        const string radioId = radio["id"].asString();
        if (radioId.empty()) return;
        const int programCount = (std::max)(1, radio.get("programCount", 1).asInt());
        const int parts = config.splitLargeFolders && programCount > chunkSize
            ? (programCount + chunkSize - 1) / chunkSize : 1;
        if (parts > 1) {
            WriteLog("[FOLDER] 播客自动拆分：电台 ID=" + radioId + "，节目 " +
                to_string(programCount) + " 项，注册 " + to_string(parts) + " 个列表");
        }
        for (int part = 1; part <= parts; ++part) {
            string id = "NCM_PODCAST_LIST_" + radioId;
            string displayName = radio["name"].asString();
            if (parts > 1) {
                id += "_PART_" + to_string(part);
                displayName += "-" + to_string(part);
            }
            subList->add(id.c_str(), displayName.c_str());
        }
        totalPodcastPlaylists += parts;
    };

    // --- 第一部分：日推 ---
    // 与其它列表一样，只有服务连通、能取到数据时才注册该行；
    // 否则（服务未启动等）不显示，避免静态出现而实际打不开的空列表。
    if (config.loadDailyRecommend) {
        string dres = HttpGet(ApiBase + "/recommend/songs");
        Json::Value droot;
        if (Json::Reader().parse(dres, droot) && droot["data"]["dailySongs"].size() > 0) {
            subList->add("NCM_DAILY_RECOMMEND_LIST", "每日歌曲推荐");
            totalNormalPlaylists++;
        } else {
            WriteLog("[FOLDER] 每日推荐请求失败或为空，跳过注册");
        }
    }

    // --- 第二部分：普通歌单 ---
    string res = HttpGet(ApiBase + "/user/playlist?uid=" + uid);
    Json::Value root;
    if (Json::Reader().parse(res, root)) {
        // 我喜欢的音乐
        if (config.loadFavoriteSongs && root["playlist"].size() > 0) {
            addNormalPlaylist(root["playlist"][0]);
        }

        // 我创建的歌单
        if (config.showCreatedPlaylists) {
            subList->add("SEP_CREATED_LIST", "------ 我创建的歌单 ------");
            int cIdx = 0;
            for (int i = 1; i < (int)root["playlist"].size(); i++) {
                auto& pl = root["playlist"][i];
                if (!pl["subscribed"].asBool() && cIdx < config.limitCreatedPlaylists) {
                    addNormalPlaylist(pl);
                    cIdx++;
                }
            }
        }

        // 我收藏的歌单
        if (config.showSubscribedPlaylists) {
            subList->add("SEP_SUBSCRIBED_LIST", "------ 我收藏的歌单 ------");
            int sIdx = 0;
            for (auto& pl : root["playlist"]) {
                if (pl["subscribed"].asBool() && sIdx < config.limitSubscribedPlaylists) {
                    addNormalPlaylist(pl);
                    sIdx++;
                }
            }
        }
    }

    // --- 第三部分：播客歌单 ---
    // 我创建的播客
    if (config.showCreatedPodcasts) {
        string resDj = HttpGet(ApiBase + "/user/audio?uid=" + uid);
        Json::Value djRoot;
        if (Json::Reader().parse(resDj, djRoot) && djRoot["djRadios"].size() > 0) {
            subList->add("SEP_CREATED_PODCAST_LIST", "------ 我创建的播客 ------");
            int count = 0;
            for (const auto& r : djRoot["djRadios"]) {
                if (count >= config.limitCreatedPodcasts) break;
                addPodcastPlaylist(r);
                ++count;
            }
        }
    }

    // 我收藏的播客
    if (config.showSubscribedPodcasts) {
        string resSub = HttpGet(ApiBase + "/dj/sublist");
        Json::Value subRoot;
        if (Json::Reader().parse(resSub, subRoot) && subRoot["djRadios"].size() > 0) {
            subList->add("SEP_SUBSCRIBED_PODCAST_LIST", "------ 我收藏的播客 ------");
            int count = 0;
            for (const auto& r : subRoot["djRadios"]) {
                if (count >= config.limitSubscribedPodcasts) break;
                addPodcastPlaylist(r);
                ++count;
            }
        }
    }

    // --- 第四部分：收藏专辑 ---
    if (config.showSubscribedAlbums) {
        string resAlbum = HttpGet(ApiBase + "/album/sublist?limit=" +
            to_string(config.limitSubscribedAlbums) + "&offset=0");
        Json::Value albumRoot;
        const bool albumResponseValid = Json::Reader().parse(resAlbum, albumRoot);
        if (albumResponseValid && albumRoot["data"].size() > 0) {
            subList->add("SEP_SUBSCRIBED_ALBUM_LIST", "------ 我收藏的专辑 ------");
            int count = 0;
            int albumFolders = 0;
            for (const auto& album : albumRoot["data"]) {
                if (count >= config.limitSubscribedAlbums) break;
                string albumId = album["id"].asString();
                if (albumId.empty()) continue;
                string artists;
                for (const auto& artist : album["artists"]) {
                    if (!artists.empty()) artists += ",";
                    artists += artist["name"].asString();
                }
                string displayName = album["name"].asString();
                if (!artists.empty()) displayName += " - " + artists;
                const int trackCount = (std::max)(1, album.get("size", 1).asInt());
                const int parts = config.splitLargeFolders && trackCount > chunkSize
                    ? (trackCount + chunkSize - 1) / chunkSize : 1;
                if (parts > 1) {
                    WriteLog("[FOLDER] 收藏专辑自动拆分：专辑 ID=" + albumId + "，曲目 " +
                        to_string(trackCount) + " 项，注册 " + to_string(parts) + " 个列表");
                }
                for (int part = 1; part <= parts; ++part) {
                    string id = "NCM_SUBSCRIBED_ALBUM_LIST_" + albumId;
                    string partName = displayName;
                    if (parts > 1) {
                        id += "_PART_" + to_string(part);
                        partName += "-" + to_string(part);
                    }
                    subList->add(id.c_str(), partName.c_str());
                }
                albumFolders += parts;
                ++count;
                ++totalSubscribedAlbums;
            }
            WriteLog("[FOLDER] 收藏专辑列表：读取 " + to_string(count) + " 张，注册 " +
                to_string(albumFolders) + " 个专辑列表");
        } else if (albumResponseValid) {
            WriteLog("[FOLDER] 收藏专辑列表为空");
        } else {
            WriteLog("[FOLDER][ERROR] 收藏专辑列表响应解析失败");
        }
    }

    // --- 第五部分：收藏视频 ---
    // /mv/sublist 同时返回普通视频与官网 MV。在此统计两者数量，让“全部视频”
    // 和“MV”都能沿用与歌单一致的 _PART_n 分段约定。
    if (config.showSubscribedVideos) {
        int allVideoCount = 0;
        int mvCount = 0;
        string resVideo = HttpGet(ApiBase + "/mv/sublist?limit=999&offset=0");
        Json::Value videoRoot;
        const bool videoResponseValid = Json::Reader().parse(resVideo, videoRoot);
        if (videoResponseValid) {
            for (const auto& video : videoRoot["data"]) {
                ++allVideoCount;
                if (video.get("type", -1).asInt() == 0) ++mvCount;
            }
            int videoFolders = 0;
            auto addVideoFolder = [&](const char* prefix, const char* title, int itemCount) {
                const int parts = config.splitLargeFolders && itemCount > chunkSize
                    ? (itemCount + chunkSize - 1) / chunkSize : 1;
                if (parts > 1) {
                    WriteLog("[FOLDER] 收藏视频自动拆分：" + string(title) + " " +
                        to_string(itemCount) + " 项，注册 " + to_string(parts) + " 个列表");
                }
                for (int part = 1; part <= parts; ++part) {
                    string id = prefix;
                    string displayName = title;
                    if (parts > 1) {
                        id += "_PART_" + to_string(part);
                        displayName += "-" + to_string(part);
                    }
                    subList->add(id.c_str(), displayName.c_str());
                }
                totalVideoFolders += parts;
                videoFolders += parts;
            };
            subList->add("SEP_SUBSCRIBED_VIDEO_LIST", "------ 我收藏的视频 ------");
            addVideoFolder("NCM_SUBSCRIBED_VIDEO_ALL_LIST", "全部视频", allVideoCount);
            addVideoFolder("NCM_SUBSCRIBED_VIDEO_MV_LIST", "MV", mvCount);
            WriteLog("[FOLDER] 收藏视频列表：全部 " + to_string(allVideoCount) + " 项，MV " +
                to_string(mvCount) + " 项，注册 " + to_string(videoFolders) + " 个视频列表");
        } else {
            WriteLog("[FOLDER][ERROR] 收藏视频列表响应解析失败，跳过注册");
        }
    }

    // --- 函数末尾：统一输出总结日志 ---
    string finalLog = "[FOLDER] 列表加载完毕：成功识别 " +
        to_string(totalNormalPlaylists) + " 个普通歌单, " +
        to_string(totalPodcastPlaylists) + " 个播客歌单, " +
        to_string(totalSubscribedAlbums) + " 张收藏专辑, " +
        to_string(totalVideoFolders) + " 个收藏视频列表。";
    WriteLog(finalLog);

    return S_OK;
}



HRESULT VDJ_API CNeteaseCloud::GetFolder(const char* folderId, IVdjTracksList* tracksList) {
    if (!folderId) return S_OK;
    string fid = folderId;
    WriteLog("[FOLDER] 开始加载列表内容，ID：" + fid);

    // 过滤掉分隔符
    if (fid.find("SEP_") == 0) {
        tracksList->finish();
        return S_OK;
    }

    const int chunkSize = (std::max)(1, (std::min)(999, config.limitItemsPerFolder));
    string listType = ""; // 用于日志区分类型
    int part = 1;
    size_t partMarker = fid.rfind("_PART_");
    if (partMarker != string::npos) {
        try { part = (std::max)(1, stoi(fid.substr(partMarker + 6))); }
        catch (...) { tracksList->finish(); return S_OK; }
        fid = fid.substr(0, partMarker);
    }
    const long long offset = (long long)(part - 1) * chunkSize;
    int count = 0;

    const bool isNormalList = fid.find("NCM_NORMAL_LIST_") == 0;
    const bool isPodcastList = fid.find("NCM_PODCAST_LIST_") == 0;
    const bool isAlbumList = fid.find("NCM_SUBSCRIBED_ALBUM_LIST_") == 0;
    const bool isVideoAll = fid == "NCM_SUBSCRIBED_VIDEO_ALL_LIST";
    const bool isVideoMv = fid == "NCM_SUBSCRIBED_VIDEO_MV_LIST";
    const bool isPaginated = isNormalList || isPodcastList || isVideoAll || isVideoMv;
    if (isAlbumList) listType = "收藏专辑";
    else if (isVideoAll) listType = "收藏视频（全部）";
    else if (isVideoMv) listType = "收藏 MV";
    else if (isNormalList) listType = "普通歌单";
    else if (isPodcastList) listType = "播客歌单";
    else if (fid == "NCM_DAILY_RECOMMEND_LIST") listType = "每日推荐";

    if (fid == "NCM_DAILY_RECOMMEND_LIST") {
        string dres = HttpGet(ApiBase + "/recommend/songs");
        Json::Value droot;
        if (Json::Reader().parse(dres, droot)) {
            for (auto& s : droot["data"]["dailySongs"]) {
                if (count >= chunkSize) break;
                AddTrack(tracksList, s, false);
                count++;
            }
        }
    }
    else if (isPaginated) {
        // 直接从 API 分页拉取，使本段的 [offset, offset+chunkSize) 窗口既精确又
        // 顺序稳定：反复取页，直到攒够足够多的接受项再切分。MV 列表只统计 type 为 MV 的项。
        Json::Value accepted(Json::arrayValue);
        int rawOffset = 0;
        const int pageSize = (std::max)(50, chunkSize);
        const long long windowEnd = offset + chunkSize;
        int safety = 0;
        while ((long long)accepted.size() < windowEnd && safety < 200) {
            string pageUrl;
            if (isNormalList)
                pageUrl = ApiBase + "/playlist/track/all?id=" + fid.substr(16) + "&limit=" + to_string(pageSize) + "&offset=" + to_string(rawOffset);
            else if (isPodcastList)
                pageUrl = ApiBase + "/dj/program?rid=" + fid.substr(17) + "&limit=" + to_string(pageSize) + "&offset=" + to_string(rawOffset);
            else
                pageUrl = ApiBase + "/mv/sublist?limit=" + to_string(pageSize) + "&offset=" + to_string(rawOffset);
            string pageRes = HttpGet(pageUrl);
            Json::Value pageRoot;
            int got = 0;
            if (Json::Reader().parse(pageRes, pageRoot)) {
                const Json::Value& rows = (isVideoAll || isVideoMv)
                    ? pageRoot["data"]
                    : (isPodcastList ? pageRoot["programs"] : pageRoot["songs"]);
                for (const auto& row : rows) {
                    if (isVideoAll || isVideoMv) {
                        if (isVideoMv && row.get("type", -1).asInt() != 0) continue;
                        if (row["vid"].asString().empty()) continue;
                    }
                    accepted.append(row);
                    ++got;
                }
            }
            if (got == 0) break;
            rawOffset += got;
            ++safety;
        }
        long long global = 0;
        for (const auto& row : accepted) {
            if (global++ < offset) continue;
            if (count >= chunkSize) break;
            if (isVideoAll || isVideoMv) AddVideoTrack(tracksList, row);
            else AddTrack(tracksList, row, isPodcastList);
            ++count;
        }
    }
    else if (isAlbumList) {
        string ares = HttpGet(ApiBase + "/album?id=" + fid.substr(26));
        Json::Value aroot;
        if (Json::Reader().parse(ares, aroot)) {
            long long idx = 0;
            for (const auto& s : aroot["songs"]) {
                if (idx++ < offset) continue;
                if (count >= chunkSize) break;
                AddTrack(tracksList, s, false);
                ++count;
            }
        }
    }
    else {
        // 未知列表 ID（其余分隔/未实现类型）直接返回空。
        tracksList->finish();
        return S_OK;
    }

    // --- 重点：在完成加载前写日志 ---
    string folderInfo = "[FOLDER] " + listType + "内容加载完毕（ID：" + fid + "），共加载 " + to_string(count) + " 项";
    WriteLog(folderInfo);

    tracksList->finish();
    return S_OK;
}



// 曲目添加基准
void CNeteaseCloud::AddTrack(IVdjTracksList* list, const Json::Value& data, bool isPC) {
    string id, name, ar, pic;
    if (isPC) {
        // 播客：使用 NCM_PODCAST_TRACK_ 前缀 + 节目ID
        id = "NCM_PODCAST_TRACK_" + data["id"].asString();

        // --- 增加播客解析逻辑 ---
        string rawName = data["name"].asString();
        string rawAr = data["dj"]["nickname"].asString();

        if (config.splitPodcastTitle) {               //也执行切分逻辑
            size_t pos = rawName.find(" - ");
            if (pos != string::npos) {
                ar = rawName.substr(0, pos);          
                name = rawName.substr(pos + 3);       
            }
            else {
                name = rawName;
                ar = rawAr;
            }
        }
        else {
            name = rawName;
            ar = rawAr;
        }
        // ------------------------

        pic = PodcastCoverFromProgram(data);
    }
    else {
        // 普通：使用 NCM_NORMAL_TRACK_ 前缀 + 歌曲ID
        id = "NCM_NORMAL_TRACK_" + data["id"].asString();
        name = data["name"].asString();
        for (auto& a : data["ar"]) {
            if (!ar.empty()) ar += ",";
            ar += a["name"].asString();
        }
        pic = data["al"]["picUrl"].asString();
    }

    // ------------------------------------------------------------------
    // 功能（暂未启用，仅注释占位）：
    //   把网易云 song 顶层 tns 字段（灰色附加标题，如 “普通话版”）并入 VDJ 显示的
    //   title，格式为  title (tns) 例如  Dehors (普通话版)。
    //   依据：网易云对某些曲目会在主标题之外给出灰色后缀，该后缀位于响应歌对象的
    //   顶层数组字段 tns（不是歌手 ar[].tns —— 那个是艺名翻译，等同 “威肯”；
    //   也不是专辑 al.tns）。这里只应读 data["tns"]。
    // 启用方式：把下面整段放开。
    // 注意：tns 缺失或为空数组成员时保持 name 原样；多 tns 可逐个 (xx) 追加。
    /*  待启用
    {
        const Json::Value& tnsArr = data["tns"];
        if (tnsArr.isArray()) {
            for (const auto& t : tnsArr) {
                std::string suffix = t.asString();
                if (suffix.empty()) continue;
                // 若主名里已包含该后缀则不重复
                if (name.find(suffix) == std::string::npos) {
                    name += " (" + suffix + ")";
                }
            }
        }
    }
    待启用结束 */

    if (pic.find("//") == 0) pic = "https:" + pic;
    pic = SizedCover(pic, config.coverSize);   // 曲目/播客封面应用 ?param=NxN

    list->add(
        id.c_str(),    // 1. 唯一 ID：该曲目在 VDJ 数据库中的标识
        name.c_str(),  // 2. 标题：对应 VDJ 界面标题列
        ar.c_str(),    // 3. 艺人：对应 VDJ 界面艺人列
        0,             // 4. 混音/版本名，空
        0,             // 5. 风格，空
        0,             // 6. 厂牌，空
        0,             // 7. 备注，空
        pic.c_str(),   // 8. 封面链接
        0,             // 9. 直链（留空，加载时再走 GetStreamUrl）
        0.0f           // 10. 时长（秒，0 表示让 VDJ 加载时自行计算）
    );
}

void CNeteaseCloud::AddVideoTrack(IVdjTracksList* list, const Json::Value& data) {
    const string videoId = data["vid"].asString();
    if (videoId.empty()) return;

    // 收藏接口把“本站 MV”标成 type 0（纯数字 id）；其余条目都是云视频
    // （hash id），需走 /video/url。
    const bool isMv = data.get("type", -1).asInt() == 0;
    const string id = string(isMv ? "NCM_MUSIC_VIDEO_" : "NCM_NORMAL_VIDEO_") + videoId;
    const string title = data.get("title", "").asString();
    string creator;
    for (const auto& item : data["creator"]) {
        if (!creator.empty()) creator += ",";
        creator += item.get("userName", "").asString();
    }
    if (creator.empty()) creator = "网易云音乐视频";

    string cover = data.get("coverUrl", "").asString();
    if (cover.find("//") == 0) cover = "https:" + cover;
    cover = SizedCover(cover, config.coverSize);
    const float durationSeconds = (float)data.get("durationms", 0).asInt() / 1000.0f;
    // IVdjTracksList::add 把时长当作第 10 个（下标 10）参数。把本条标成视频，
    // 好让 VirtualDJ 把返回的 MP4 流当作视频媒体播放。
    list->add(id.c_str(), title.c_str(), creator.c_str(),
        nullptr, nullptr, nullptr, nullptr, cover.c_str(), nullptr,
        durationSeconds, 0.0f, 0, 0, true);
}


// 使用搜索时重编码特殊字符
string CNeteaseCloud::UrlEncode(const string& value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (unsigned char c : value) {
        // 如果是标准的英文字母、数字或未保留字符，直接添加
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // 空格和其他所有特殊字符（包括中文）统一进行 %XX 转义
        // 十六进制用大写，输出如 %2F 而不是 %2f
        escaped << uppercase << '%' << setw(2) << int(c) << nouppercase;
    }

    return escaped.str();
}

// 链接类型中文名，用于日志
static const char* NcmLinkKindName(NcmLinkKind kind) {
    switch (kind) {
    case NcmLinkKind::Song: return "单曲";
    case NcmLinkKind::Program: return "声音/节目";
    case NcmLinkKind::Radio: return "播客/电台";
    case NcmLinkKind::Playlist: return "歌单";
    case NcmLinkKind::Album: return "专辑";
    case NcmLinkKind::Mv: return "MV";
    case NcmLinkKind::Video: return "视频";
    default: return "未知";
    }
}

// 搜索框粘贴网易云链接时：按链接类型拉对应条目，忽略 userid 等无关参数，
// 并按「搜索返回项数上限」截断。识别成功即不再走关键词搜索。
bool CNeteaseCloud::HandleSearchLink(const string& text, IVdjTracksList* tracksList, int wanted, int& added) {
    string url;
    if (!ExtractFirstNeteaseUrl(text, url)) return false;

    const NcmLink link = ParseNcmLink(url);
    if (link.kind == NcmLinkKind::None || link.id.empty()) {
        WriteLog("[SEARCH][LINK] 未能识别链接类型：" + url);
        return true;
    }

    WriteLog("[SEARCH][LINK] 识别为" + string(NcmLinkKindName(link.kind)) + "，id=" + link.id);
    added = 0;
    std::set<string> seen;

    auto take = [&](const string& id) -> bool {
        if (id.empty() || seen.count(id) || added >= wanted || searchCancel.load()) return false;
        seen.insert(id);
        return true;
    };
    auto addSong = [&](const Json::Value& s) {
        if (!take(JsonId(s, "id"))) return;
        AddTrack(tracksList, s, false);
        ++added;
    };
    auto addProgram = [&](const Json::Value& p) {
        if (!take(JsonId(p, "id"))) return;
        AddTrack(tracksList, p, true);
        ++added;
    };
    auto pageFetch = [&](const string& path, const char* arrayKey, bool isProgram) {
        int offset = 0;
        int safety = 0;
        while (added < wanted && safety < 80 && !searchCancel.load()) {
            const int pageSize = (std::min)(100, wanted - added);
            const string pageUrl = ApiBase + path + "&limit=" + to_string(pageSize) + "&offset=" + to_string(offset);
            string res = HttpGet(pageUrl);
            Json::Value root;
            if (!Json::Reader().parse(res, root)) {
                WriteLog("[SEARCH][LINK] 分页 JSON 解析失败，offset=" + to_string(offset));
                break;
            }
            const Json::Value& rows = root[arrayKey];
            if (!rows.isArray() || rows.empty()) break;
            const int got = (int)rows.size();
            int newCount = 0;
            for (const auto& row : rows) {
                if (added >= wanted || searchCancel.load()) break;
                const int before = added;
                if (isProgram) addProgram(row);
                else addSong(row);
                if (added > before) ++newCount;
            }
            if (got <= 0 || newCount <= 0) break;
            offset += got;
            ++safety;
        }
    };

    if (link.kind == NcmLinkKind::Song) {
        string res = HttpGet(ApiBase + "/song/detail?ids=" + link.id);
        Json::Value root;
        if (Json::Reader().parse(res, root)) {
            for (const auto& s : root["songs"]) addSong(s);
        } else {
            WriteLog("[SEARCH][LINK][ERROR] 单曲详情解析失败，id=" + link.id);
        }
    }
    else if (link.kind == NcmLinkKind::Program) {
        string res = HttpGet(ApiBase + "/dj/program/detail?id=" + link.id);
        Json::Value root;
        if (Json::Reader().parse(res, root) && root["program"].isObject())
            addProgram(root["program"]);
        else
            WriteLog("[SEARCH][LINK][ERROR] 节目详情解析失败，id=" + link.id);
    }
    else if (link.kind == NcmLinkKind::Radio) {
        pageFetch("/dj/program?rid=" + link.id, "programs", true);
    }
    else if (link.kind == NcmLinkKind::Playlist) {
        pageFetch("/playlist/track/all?id=" + link.id, "songs", false);
    }
    else if (link.kind == NcmLinkKind::Album) {
        string res = HttpGet(ApiBase + "/album?id=" + link.id);
        Json::Value root;
        if (Json::Reader().parse(res, root)) {
            for (const auto& s : root["songs"]) {
                if (added >= wanted || searchCancel.load()) break;
                addSong(s);
            }
        }
    }
    else if (link.kind == NcmLinkKind::Mv) {
        string res = HttpGet(ApiBase + "/mv/detail?mvid=" + link.id);
        Json::Value root;
        if (Json::Reader().parse(res, root) && root["data"].isObject()) {
            const Json::Value& d = root["data"];
            const string vid = FirstNonEmptyUrl({ JsonId(d, "id"), link.id });
            if (take(vid)) {
                string cover = FirstNonEmptyUrl({
                    d.get("cover", "").asString(),
                    d.get("imgurl16v9", "").asString(),
                    d.get("imgurl", "").asString()
                });
                AddSearchVideo(tracksList, config.coverSize, "NCM_MUSIC_VIDEO_", vid,
                    d.get("name", "").asString(), JoinNameList(d["artists"], "name", "name"),
                    cover, JsonMs(d["duration"]));
                ++added;
            }
        }
    }
    else if (link.kind == NcmLinkKind::Video) {
        string res = HttpGet(ApiBase + "/video/detail?id=" + link.id);
        Json::Value root;
        bool ok = Json::Reader().parse(res, root) && root["data"].isObject()
            && (!root["data"].get("title", "").asString().empty()
                || !root["data"].get("vid", "").asString().empty());
        if (ok) {
            const Json::Value& d = root["data"];
            const string vid = FirstNonEmptyUrl({ d.get("vid", "").asString(), JsonId(d, "vid"), link.id });
            if (take(vid)) {
                string creator = JoinNameList(d["creator"], "userName", "nickname");
                if (creator.empty()) creator = d["creator"].get("nickname", "").asString();
                AddSearchVideo(tracksList, config.coverSize, "NCM_NORMAL_VIDEO_", vid,
                    d.get("title", "").asString(), creator,
                    d.get("coverUrl", "").asString(), JsonMs(d["durationms"]));
                ++added;
            }
        }
        else if (IsPureDigits(link.id)) {
            string mvRes = HttpGet(ApiBase + "/mv/detail?mvid=" + link.id);
            Json::Value mvRoot;
            if (Json::Reader().parse(mvRes, mvRoot) && mvRoot["data"].isObject()) {
                const Json::Value& d = mvRoot["data"];
                const string vid = FirstNonEmptyUrl({ JsonId(d, "id"), link.id });
                if (take(vid)) {
                    string cover = FirstNonEmptyUrl({
                        d.get("cover", "").asString(),
                        d.get("imgurl16v9", "").asString(),
                        d.get("imgurl", "").asString()
                    });
                    AddSearchVideo(tracksList, config.coverSize, "NCM_MUSIC_VIDEO_", vid,
                        d.get("name", "").asString(), JoinNameList(d["artists"], "name", "name"),
                        cover, JsonMs(d["duration"]));
                    ++added;
                }
            }
        }
    }

    if (searchCancel.load())
        WriteLog("[SEARCH][LINK] 解析已取消，已返回 " + to_string(added) + " 条");
    else
        WriteLog("[SEARCH][LINK] 解析完成，返回 " + to_string(added) + " 条（上限 " + to_string(wanted) + "）");
    return true;
}

// 搜索功能
// 上游单页硬顶：声音 /search type=2000 永远最多 20 且不 400；
// 单曲/视频 cloudsearch、MV /search 单页 limit>100 直接 400。
// 这里按配置的「搜索返回项数上限」用 offset 自动翻页，把各页合并成一份结果返回，不拆列表。
// 若输入是网易云链接（含分享附带的 userid 等无关参数），则按链接类型解析条目。
HRESULT VDJ_API CNeteaseCloud::OnSearch(const char* search, IVdjTracksList* tracksList) {
    if (!search || strlen(search) < 1) return S_OK;

    string rawText = search;
    WriteLog("-----开始搜索-----");

    searchCancel = false;

    const int wanted = (std::max)(1, (std::min)(999, config.searchResultLimit));
    int linkAdded = 0;
    if (HandleSearchLink(rawText, tracksList, wanted, linkAdded)) {
        tracksList->finish();
        return S_OK;
    }

    string encodedSearch = UrlEncode(search);
    WriteLog("[SEARCH] 原始内容：" + rawText + "，编码后：" + encodedSearch);

    // 配置里选择的类型：1 单曲 / 2000 声音 / 1014 视频(云视频) / 1004 MV(官方MV)
    const int type = config.searchType;
    const int pageCap = (type == 2000) ? 20 : 100;

    auto isPureDigits = [](const string& s) {
        if (s.empty()) return false;
        for (char c : s) if (c < '0' || c > '9') return false;
        return true;
    };
    auto addVideoEntry = [&](const string& prefix, const string& vid, const string& title,
                             const string& creator, string cover, long long durationMs) {
        if (vid.empty() || title.empty()) return;
        if (cover.find("//") == 0) cover = "https:" + cover;
        cover = SizedCover(cover, config.coverSize);   // 搜索 MV/视频封面
        const float lengthSec = (float)(durationMs > 0 ? durationMs : 0) / 1000.0f;
        tracksList->add((prefix + vid).c_str(), title.c_str(), creator.c_str(),
            nullptr, nullptr, nullptr, nullptr, cover.c_str(), nullptr,
            lengthSec, 0.0f, 0, 0, true);
    };
    auto joinArtists = [](const Json::Value& arr) {
        string out;
        for (const auto& a : arr) {
            if (!out.empty()) out += ",";
            // 单曲/声音的 artists/audios 里艺人字段是 name；
            // 视频(1014)的 creator 里是 userName，两者都兼容
            string n = a.get("name", "").asString();
            if (n.empty()) n = a.get("userName", "").asString();
            out += n;
        }
        return out;
    };

    int offset = 0;
    int added = 0;
    int safety = 0;
    std::set<string> seen;

    while (added < wanted && safety < 80 && !searchCancel.load()) {
        const int pageSize = (std::min)(pageCap, wanted - added);
        string url;
        if (type == 1)
            url = ApiBase + "/cloudsearch?keywords=" + encodedSearch + "&type=1&limit=" + to_string(pageSize) + "&offset=" + to_string(offset);
        else if (type == 2000)
            url = ApiBase + "/search?type=2000&keywords=" + encodedSearch + "&limit=" + to_string(pageSize) + "&offset=" + to_string(offset);
        else if (type == 1014)
            url = ApiBase + "/cloudsearch?type=1014&keywords=" + encodedSearch + "&limit=" + to_string(pageSize) + "&offset=" + to_string(offset);
        else if (type == 1004)
            url = ApiBase + "/search?type=1004&keywords=" + encodedSearch + "&limit=" + to_string(pageSize) + "&offset=" + to_string(offset);
        else
            break;

        string res = HttpGet(url);
        Json::Value root;
        if (!Json::Reader().parse(res, root)) {
            WriteLog("[SEARCH][ERROR] 第" + to_string(safety + 1) + "页 JSON 解析失败");
            break;
        }
        if (root.get("code", 0).asInt() == 400) {
            WriteLog("[SEARCH][ERROR] 第" + to_string(safety + 1) + "页返回 code=400");
            break;
        }

        int gotThisPage = 0;
        int newThisPage = 0;
        auto takeId = [&](const string& id) -> bool {
            if (id.empty() || seen.count(id)) return false;
            seen.insert(id);
            ++newThisPage;
            return true;
        };

        if (type == 1) {
            // --- 单曲 ---
            const Json::Value& songs = root["result"]["songs"];
            gotThisPage = (int)songs.size();
            for (const auto& s : songs) {
                if (added >= wanted || searchCancel.load()) break;
                if (!takeId(s["id"].asString())) continue;
                AddTrack(tracksList, s, false);
                ++added;
            }
        }
        else if (type == 2000) {
            // --- 声音 ---
            // 网易“声音”语音条本质上就是一条电台节目(program)，可播载体才是它嵌的
            // mainSong。为让唯一 ID 与“播客/节目”语义一致，这里取该节目自己的 id
            // 来作 NCM_PODCAST_TRACK_ 后缀。
            const Json::Value& resources = root["data"]["resources"];
            gotThisPage = (int)resources.size();
            for (const auto& resource : resources) {
                if (added >= wanted || searchCancel.load()) break;
                if (resource.get("resourceType", "").asString() != "voice") continue;
                const Json::Value& binfo = resource["baseInfo"];
                // 取该条“电台节目”的 id。实测每条节目均有 id 且与顶层 resourceId 相等，
                // 因此这里只在节目候选间取，绝不退到 mainSong 当节目用(那会造成前缀/通道错配)。
                string programId = binfo.get("id", "").asString();
                if (programId.empty()) programId = binfo["program"].get("id", "").asString();
                if (programId.empty()) programId = resource.get("resourceId", "").asString();
                if (programId.empty()) continue;   // 正常不会发生；确无则宁可不列，也不误标成歌
                if (!takeId(programId)) continue;

                string fullName = binfo.get("name", "").asString();
                if (fullName.empty()) fullName = binfo["mainSong"].get("name", "").asString();
                string hostName = resource["baseInfo"]["dj"].get("nickname", "").asString();
                const string mainId = binfo["mainSong"].get("id", "").asString();
                string cover = PodcastCoverFromSearch(resource, binfo);
                if (cover.find("//") == 0) cover = "https:" + cover;

                Json::Value prog;              // 存成“播客条目”同构对象，@走 AddTrack(...,isPC)
                prog["id"] = programId;
                prog["name"] = fullName;
                prog["dj"]["nickname"] = hostName;
                prog["coverUrl"] = cover;
                // mainSong 也带上，帮助后续流解析(可播的歌 id)
                if (!mainId.empty()) prog["_mainSongId"] = mainId;
                AddTrack(tracksList, prog, true);   // isPC=true → 前缀 NCM_PODCAST_TRACK_，并应用 拆分声音标题
                ++added;
            }
        }
        else if (type == 1014) {
            // --- 视频（网易云视频分类）---
            // 网易该分类含大量其实是官方 MV 的纯数字 id；纯数字要走 /mv/url，
            // 只有含字母的 hash vid 才走 /video/url。否则会落到空地址导致播不了。
            const Json::Value& videos = root["result"]["videos"];
            gotThisPage = (int)videos.size();
            for (const auto& v : videos) {
                if (added >= wanted || searchCancel.load()) break;
                const string vid = v.get("vid", "").asString();
                if (!takeId(vid)) continue;
                addVideoEntry(isPureDigits(vid) ? "NCM_MUSIC_VIDEO_" : "NCM_NORMAL_VIDEO_",
                    vid, v.get("title", "").asString(), joinArtists(v["creator"]),
                    v.get("coverUrl", "").asString(), v.get("durationms", 0).asInt());
                ++added;
            }
        }
        else if (type == 1004) {
            // --- MV（官方，mvid 数字，走 /mv/url）---
            const Json::Value& mvs = root["result"]["mvs"];
            gotThisPage = (int)mvs.size();
            for (const auto& mv : mvs) {
                if (added >= wanted || searchCancel.load()) break;
                const string vid = mv["id"].asString();
                if (!takeId(vid)) continue;
                addVideoEntry("NCM_MUSIC_VIDEO_", vid,
                    mv.get("name", "").asString(), mv.get("artistName", "").asString(),
                    mv.get("cover", "").asString(), mv.get("duration", 0).asInt());
                ++added;
            }
        }

        WriteLog("[SEARCH] 第" + to_string(safety + 1) + "页 offset=" + to_string(offset)
            + " limit=" + to_string(pageSize) + " 本页" + to_string(gotThisPage)
            + "条 新增" + to_string(newThisPage) + " 累计" + to_string(added) + "/" + to_string(wanted));

        if (gotThisPage <= 0 || newThisPage <= 0) break;
        offset += gotThisPage;
        ++safety;
    }

    if (searchCancel.load())
        WriteLog("[SEARCH] 搜索已取消，已返回 " + to_string(added) + " 条");
    else
        WriteLog("[SEARCH] 搜索完成，共返回 " + to_string(added) + " 条（上限 " + to_string(wanted) + "）");

    tracksList->finish();
    return S_OK;
}



// 获取流基类
HRESULT VDJ_API CNeteaseCloud::GetStreamUrl(const char* id, IVdjString& url, IVdjString& err) {
    string fullId = id;
    string realTrackId = "";

    // 识别曲目身份
    if (fullId.find("NCM_NORMAL_TRACK_") == 0) {
        realTrackId = fullId.substr(17); // 截取 NCM_NORMAL_TRACK_ 后的 ID
        WriteLog("[STREAM] 获取普通歌曲音频流，ID：" + realTrackId);
    }
    else if (fullId.find("NCM_PODCAST_TRACK_") == 0) {
        string programId = fullId.substr(18); // 截取 NCM_PODCAST_TRACK_ 后的 ID
        WriteLog("[STREAM] 获取播客节目音频流，节目 ID：" + programId);

        // 播客需要通过接口换取真正的音频流 ID
        string res = HttpGet(ApiBase + "/dj/program/detail?id=" + programId);
        Json::Value pRoot;
        if (Json::Reader().parse(res, pRoot)) {
            realTrackId = pRoot["program"]["mainSong"]["id"].asString();
            WriteLog("[STREAM] 播客节目 ID 转换成功，实际音频 ID：" + realTrackId);
        }
    }
    else if (fullId.find("NCM_MUSIC_VIDEO_") == 0) {
        const string mvId = fullId.substr(16);
        WriteLog("[STREAM] 获取收藏 MV 视频流，ID：" + mvId + "，最高请求 " +
            to_string(config.maxVideoPlaybackQuality) + "P");
        string res = HttpGet(ApiBase + "/mv/url?id=" + mvId + "&r=" + to_string(config.maxVideoPlaybackQuality));
        Json::Value root;
        if (Json::Reader().parse(res, root) && root["data"]["url"].isString()) {
            const int actualQuality = root["data"].get("r", config.maxVideoPlaybackQuality).asInt();
            WriteLog("[STREAM] MV 视频流已获取，实际清晰度 " + to_string(actualQuality) + "P");
            url = root["data"]["url"].asString().c_str();
            return S_OK;
        }
        WriteLog("[STREAM][ERROR] 无法获取 MV 视频流，ID：" + mvId);
        return S_FALSE;
    }
    else if (fullId.find("NCM_NORMAL_VIDEO_") == 0) {
        const string videoId = fullId.substr(17);
        WriteLog("[STREAM] 获取收藏视频流，ID：" + videoId + "，最高请求 " +
            to_string(config.maxVideoPlaybackQuality) + "P");
        string res = HttpGet(ApiBase + "/video/url?id=" + videoId + "&res=" + to_string(config.maxVideoPlaybackQuality));
        Json::Value root;
        if (Json::Reader().parse(res, root) && root["urls"].isArray() &&
            !root["urls"].empty() && root["urls"][0]["url"].isString()) {
            const int actualQuality = root["urls"][0].get("r", config.maxVideoPlaybackQuality).asInt();
            WriteLog("[STREAM] 视频流已获取，实际清晰度 " + to_string(actualQuality) + "P");
            url = root["urls"][0]["url"].asString().c_str();
            return S_OK;
        }
        WriteLog("[STREAM][ERROR] 无法获取视频流，ID：" + videoId);
        return S_FALSE;
    }

    if (realTrackId.empty()) {
        WriteLog("[STREAM][ERROR] 无法识别曲目 ID 类型或解析失败：" + fullId);
        return S_FALSE;
    }

    // 获取播放直链
    string res = HttpGet(ApiBase + "/song/url/v1?id=" + realTrackId + "&level=" + config.playQuality);
    Json::Value root;
    if (Json::Reader().parse(res, root) && !root["data"][0]["url"].isNull()) {
        string streamUrl = root["data"][0]["url"].asString();


        WriteLog("[STREAM] 音频直链地址：" + streamUrl);

        url = streamUrl.c_str();
        return S_OK;
    }

    WriteLog("[STREAM][ERROR] 无法获取音频直链，音频 ID：" + realTrackId);
    return S_FALSE;
}



//取消搜索
HRESULT VDJ_API CNeteaseCloud::OnSearchCancel() {
    searchCancel = true;
    return S_OK;
}




//当获取插件info
HRESULT VDJ_API CNeteaseCloud::OnGetPluginInfo(TVdjPluginInfo8* info)
{

    info->PluginName = "NeteaseCloudMusic";
    info->Author = "小小小小铭";
    info->Version = "260905 v0.3.0";
    info->Description = "网易云音乐在线源支持";
    info->Flags = 0x00;

    return S_OK;
}




// 旧版 Release() 已移至 SettingsWindow.cpp，此处不再使用。




// 右键点击文件夹 只做展示
HRESULT VDJ_API CNeteaseCloud::GetFolderContextMenu(const char* folderUniqueId, IVdjContextMenu* contextMenu)
{
    // 右键点击的是插件的根目录
    if (folderUniqueId == NULL)
    {
        // 0 作者 ／ 1 版本 —— 都触发打开主页/空间链接
        contextMenu->add("By 小小小小铭");
        contextMenu->add("260905 v0.3.0");
        // 2+ 各入口
        contextMenu->add("打开插件设置");
        contextMenu->add("打开配置文件");
        contextMenu->add("打开日志文件");
        contextMenu->add("打开插件目录");
        contextMenu->add("打开下载目录");
    }

    return S_OK;
}

// 右键点击文件夹 功能实现
HRESULT VDJ_API CNeteaseCloud::OnFolderContextMenu(const char* folderUniqueId, size_t menuIndex)
{
    if (folderUniqueId == NULL)
    {
        // 作者名(0) 与 版本(1) 点击都打开 主页+空间 两个链接
        if (menuIndex == 0 || menuIndex == 1)
        {
            ShellExecuteW(NULL, L"open", L"https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ", NULL, NULL, SW_SHOWNORMAL);
            ShellExecuteW(NULL, L"open", L"https://space.bilibili.com/475951038", NULL, NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 2)
        {
            HWND wnd = CreateConfigWindow();
            if (wnd) { ShowWindow(wnd, SW_SHOW); SetForegroundWindow(wnd); }
        }
        else if (menuIndex == 3)
        {
            string path = GetPluginPath() + "settings.json";
            ShellExecuteW(NULL, L"open", L"notepad.exe", Utf8ToWide(path).c_str(), NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 4)
        {
            string path = GetPluginPath() + "log.log";
            ShellExecuteW(NULL, L"open", L"notepad.exe", Utf8ToWide(path).c_str(), NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 5)
        {
            string path = GetPluginPath();
            ShellExecuteW(NULL, L"open", Utf8ToWide(path).c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 6)
        {
            string path = config.downloadPath;
            ShellExecuteW(NULL, L"open", Utf8ToWide(path).c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    }

    return S_OK;
}


//右键点击曲目 只做展示
HRESULT VDJ_API CNeteaseCloud::GetContextMenu(const char* id, IVdjContextMenu* contextMenu)
{
    if (!id) return S_OK;
    const string trackId = id;
    const bool isVideo = trackId.find("NCM_NORMAL_VIDEO_") == 0 ||
        trackId.find("NCM_MUSIC_VIDEO_") == 0;
    if (isVideo ? !config.enableVideoDownload : !config.enableTrackDownload) return S_OK;
    contextMenu->add(isVideo ? "下载此视频" : "下载此曲目");
    return S_OK;
}




//右键点击曲目 功能实现
HRESULT VDJ_API CNeteaseCloud::OnContextMenu(const char* id, size_t i) {
    if (!id || i != 0) return S_OK;
    string fullId = id;
    const bool isVideo = fullId.find("NCM_NORMAL_VIDEO_") == 0 ||
        fullId.find("NCM_MUSIC_VIDEO_") == 0;
    if (isVideo ? !config.enableVideoDownload : !config.enableTrackDownload) return S_OK;
    // 右键只负责分流；详情请求和文件下载都放到后台线程，避免卡住 VirtualDJ
    if (fullId.find("NCM_NORMAL_TRACK_") == 0) {
        DownloadSong(fullId.substr(17), false);
        return S_OK;
    }
    if (fullId.find("NCM_PODCAST_TRACK_") == 0) {
        DownloadSong(fullId.substr(18), true);
        return S_OK;
    }
    if (fullId.find("NCM_NORMAL_VIDEO_") == 0) {
        DownloadVideo(fullId.substr(17), false);
        return S_OK;
    }
    if (fullId.find("NCM_MUSIC_VIDEO_") == 0) {
        DownloadVideo(fullId.substr(16), true);
        return S_OK;
    }

    WriteLog("[DL][ERROR] 无法识别曲目类型，ID: " + fullId);
    return S_OK;
}




void CNeteaseCloud::DownloadVideo(const string& videoId, bool isMusicVideo) {
    // 进线程前拷走端口、清晰度和保存路径，避免下载过程中配置被改、或 API 被重启后读到空基址
    const string apiBase = ApiBase;
    const int wantQuality = config.maxVideoDownloadQuality;
    const string saveDir = config.downloadPath;
    std::thread([this, videoId, isMusicVideo, apiBase, wantQuality, saveDir]() {
        string logTag = isMusicVideo ? "[DL][MV] " : "[DL][VIDEO] ";
        try {
            if (shuttingDown.load()) return;
            bool treatAsMv = isMusicVideo;
            string title, creator;

            auto readDetail = [&](bool mv, const string& body) -> bool {
                Json::Value root;
                if (!Json::Reader().parse(body, root) || !root["data"].isObject()) return false;
                const Json::Value& d = root["data"];
                if (mv) {
                    title = JsonStr(d, "name");
                    creator = JoinNameList(d["artists"], "name", "name");
                } else {
                    title = JsonStr(d, "title");
                    creator = JoinNameList(d["creator"], "userName", "nickname");
                    if (creator.empty()) creator = JsonStr(d["creator"], "nickname");
                }
                return !title.empty() || !creator.empty();
            };

            // 普通视频详情的 creator 是单个对象，不是数组；按数组 for 会触发 jsoncpp 断言，VDJ 直接崩。
            // 搜索/链接有时还会把纯数字 MV 标成 NCM_NORMAL_VIDEO_，详情失败时改走另一套接口。
            string detailBody = HttpGet(treatAsMv
                ? apiBase + "/mv/detail?mvid=" + videoId
                : apiBase + "/video/detail?id=" + videoId);
            if (!readDetail(treatAsMv, detailBody)) {
                const bool otherIsMv = !treatAsMv;
                string otherBody = HttpGet(otherIsMv
                    ? apiBase + "/mv/detail?mvid=" + videoId
                    : apiBase + "/video/detail?id=" + videoId);
                if (readDetail(otherIsMv, otherBody)) {
                    treatAsMv = otherIsMv;
                    logTag = treatAsMv ? "[DL][MV] " : "[DL][VIDEO] ";
                    WriteLog(logTag + "已按" + string(treatAsMv ? "MV" : "视频") + "接口重新识别，ID：" + videoId);
                } else {
                    WriteLog(logTag + "视频详情读取失败，将使用 ID 作为文件名：" + videoId);
                }
            }
            if (title.empty()) title = videoId;

            auto fetchStream = [&](bool mv, string& url, int& quality) -> bool {
                const string streamApi = mv
                    ? apiBase + "/mv/url?id=" + videoId + "&r=" + to_string(wantQuality)
                    : apiBase + "/video/url?id=" + videoId + "&res=" + to_string(wantQuality);
                string body = HttpGet(streamApi);
                Json::Value root;
                if (!Json::Reader().parse(body, root)) return false;
                return mv ? ExtractMvDownloadUrl(root, url, quality)
                          : ExtractVideoDownloadUrl(root, url, quality);
            };

            string downloadUrl;
            int actualQuality = wantQuality;
            if (!fetchStream(treatAsMv, downloadUrl, actualQuality)) {
                const bool otherIsMv = !treatAsMv;
                if (fetchStream(otherIsMv, downloadUrl, actualQuality)) {
                    treatAsMv = otherIsMv;
                    logTag = treatAsMv ? "[DL][MV] " : "[DL][VIDEO] ";
                    WriteLog(logTag + "直链改走" + string(treatAsMv ? "MV" : "视频") + "接口，ID：" + videoId);
                }
            }
            if (downloadUrl.empty()) {
                WriteLog(logTag + "[ERROR] 无法获取视频下载链接，ID：" + videoId);
                return;
            }

            string fileName = creator.empty() ? title + ".mp4" : creator + " - " + title + ".mp4";
            const string illegal = "\\/:*?\"<>|";
            for (char& c : fileName) if (illegal.find(c) != string::npos) c = '_';
            CreateDirectoryW(Utf8ToWide(saveDir).c_str(), nullptr);
            const string fullPath = saveDir + "\\" + fileName;
            const std::wstring wPath = Utf8ToWide(fullPath);

            CURL* curl = curl_easy_init();
            if (!curl) {
                WriteLog(logTag + "[ERROR] 无法创建下载任务");
                return;
            }
            FILE* fp = _wfopen(wPath.c_str(), L"wb");
            if (!fp) {
                WriteLog(logTag + "[ERROR] 无法创建文件：" + fileName);
                curl_easy_cleanup(curl);
                return;
            }
            WriteLog(logTag + "开始下载：" + fileName);
            curl_easy_setopt(curl, CURLOPT_URL, downloadUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);          // 后台线程里避免 curl 超时信号把进程打死
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L); // 持续低于 1KB/s
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);    // 超过 60 秒视为卡死，结束任务
            CURLcode code = curl_easy_perform(curl);
            fclose(fp);
            curl_easy_cleanup(curl);
            if (code == CURLE_OK) {
                WriteLog(logTag + "下载成功：" + fileName);
            } else {
                WriteLog(logTag + "[ERROR] 下载失败：" + string(curl_easy_strerror(code)) + "，文件：" + fileName);
                DeleteFileW(wPath.c_str());
            }
        }
        catch (const std::exception& e) {
            WriteLog(logTag + "[ERROR] 下载异常：" + string(e.what()) + "，ID：" + videoId);
        }
        catch (...) {
            WriteLog(logTag + "[ERROR] 下载发生未知异常，ID：" + videoId);
        }
    }).detach();
}

// 下载功能基准
void CNeteaseCloud::DownloadSong(const string& sid, bool isPodcast) {
    const string apiBase = ApiBase;
    const string saveDir = config.downloadPath;
    const string quality = config.trackDownloadQuality;
    const bool splitTitle = config.splitPodcastTitle;
    const bool doWriteTags = config.writeTags;
    std::thread([this, sid, isPodcast, apiBase, saveDir, quality, splitTitle, doWriteTags]() {
        const string logTag = isPodcast ? "[DL][PODCAST] " : "[DL][NORMAL] ";
        try {
            if (shuttingDown.load()) return;
            string title, artist, picUrl, realAudioId = sid;

            if (isPodcast) {
                // 播客详情和音频 ID 都在后台取，避免右键时卡住界面
                string res = HttpGet(apiBase + "/dj/program/detail?id=" + sid);
                Json::Value pRoot;
                if (!Json::Reader().parse(res, pRoot) || !pRoot["program"].isObject()) {
                    WriteLog(logTag + "[ERROR] 播客节目解析失败，ID：" + sid);
                    return;
                }
                const Json::Value& program = pRoot["program"];
                string rawTitle = JsonStr(program, "name");
                string rawArtist = JsonStr(program["dj"], "nickname");
                if (splitTitle) {
                    size_t pos = rawTitle.find(" - ");
                    if (pos != string::npos) {
                        artist = rawTitle.substr(0, pos);
                        title = rawTitle.substr(pos + 3);
                    } else {
                        title = rawTitle;
                        artist = rawArtist;
                    }
                } else {
                    title = rawTitle;
                    artist = rawArtist;
                }
                picUrl = PodcastCoverFromProgram(program);
                realAudioId = JsonId(program["mainSong"], "id");
                if (realAudioId.empty()) realAudioId = JsonId(program["mainSong"]["id"]);
                WriteLog(logTag + "播客节目解析成功: " + artist + " - " + title);
            } else {
                string res = HttpGet(apiBase + "/song/detail?ids=" + sid);
                Json::Value root;
                if (!Json::Reader().parse(res, root) || !root["songs"].isArray() || root["songs"].empty()) {
                    WriteLog(logTag + "[ERROR] 普通歌曲解析失败，ID：" + sid);
                    return;
                }
                const Json::Value& song = root["songs"][0];
                title = JsonStr(song, "name");
                picUrl = JsonStr(song["al"], "picUrl");
                artist = JoinNameList(song["ar"], "name", "name");
                WriteLog(logTag + "普通歌曲解析成功: " + artist + " - " + title);
            }

            if (title.empty()) {
                WriteLog(logTag + "[ERROR] 解析失败，无法开始任务。ID: " + sid);
                return;
            }
            if (realAudioId.empty()) {
                WriteLog(logTag + "[ERROR] 无法解析音频 ID，ID：" + sid);
                return;
            }

            string res = HttpGet(apiBase + "/song/url/v1?id=" + realAudioId + "&level=" + quality);
            Json::Value root;
            if (!Json::Reader().parse(res, root) || !root["data"].isArray() || root["data"].empty()
                || !root["data"][0]["url"].isString()) {
                WriteLog(logTag + "[ERROR] 无法获取直链，音频 ID：" + realAudioId);
                return;
            }
            string dUrl = root["data"][0]["url"].asString();
            if (dUrl.empty()) {
                WriteLog(logTag + "[ERROR] 无法获取直链，音频 ID：" + realAudioId);
                return;
            }

            string ext = ".mp3";
            if (!isPodcast && quality == "lossless") ext = ".flac";
            string fileName = (!artist.empty() && !title.empty())
                ? artist + " - " + title + ext
                : sid + ext;
            const string illegal = "\\/:*?\"<>|";
            for (char& c : fileName) if (illegal.find(c) != string::npos) c = '_';

            CreateDirectoryW(Utf8ToWide(saveDir).c_str(), nullptr);
            string fullPath = saveDir + "\\" + fileName;
            std::wstring wPath = Utf8ToWide(fullPath);

            CURL* curl = curl_easy_init();
            if (!curl) {
                WriteLog(logTag + "[ERROR] 无法创建下载任务");
                return;
            }
            FILE* fp = _wfopen(wPath.c_str(), L"wb");
            if (!fp) {
                WriteLog(logTag + "[ERROR] 无法打开文件：" + fileName);
                curl_easy_cleanup(curl);
                return;
            }
            WriteLog(logTag + "开始下载：" + fileName);
            curl_easy_setopt(curl, CURLOPT_URL, dUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
            CURLcode code = curl_easy_perform(curl);
            fclose(fp);
            curl_easy_cleanup(curl);

            if (code == CURLE_OK) {
                WriteLog(logTag + "下载成功: " + fileName);
                if (doWriteTags) {
                    WriteLog(logTag + "开始写入音频标签及详情页链接...");
                    AddTags(fullPath, artist, title, sid, picUrl, isPodcast);
                }
            } else {
                WriteLog(logTag + "[ERROR] 下载失败：" + string(curl_easy_strerror(code)));
                DeleteFileW(wPath.c_str());
            }
        }
        catch (const std::exception& e) {
            WriteLog(logTag + "[ERROR] 下载异常：" + string(e.what()) + "，ID：" + sid);
        }
        catch (...) {
            WriteLog(logTag + "[ERROR] 下载发生未知异常，ID：" + sid);
        }
    }).detach();
}




void CNeteaseCloud::AddTags(const string& filePath, const string& artist, const string& title, const string& sid, const string& picUrl, bool isPodcast) {
    std::wstring wPath = Utf8ToWide(filePath);
    auto fetchCover = [this](const string& url) -> string {
        if (url.empty()) return {};
        // 封面可能比普通 API 大，给 30 秒；失败只影响封面，不回滚已下好的音频
        return HttpGet(url, false, 30);
    };

    // --- 情况 A: 处理 MP3 ---
    if (filePath.find(".mp3") != string::npos) {
        TagLib::MPEG::File mp3File(wPath.c_str());

        // true：若不存在 ID3v2 标签则新建一个
        TagLib::ID3v2::Tag* tag = mp3File.ID3v2Tag(true);
        if (tag) {
            // 1. 基础信息写入
            tag->setArtist(TagLib::String(artist, TagLib::String::UTF8));
            tag->setTitle(TagLib::String(title, TagLib::String::UTF8));

            if (config.writeSourceUrlToComment) {
                string webUrl = isPodcast ? "https://music.163.com/#/program?id=" + sid : "https://music.163.com/#/song?id=" + sid;
                tag->setComment(TagLib::String(webUrl, TagLib::String::UTF8));
                WriteLog("[TAG] MP3 详情链接已写入 Comment");
            }

            // 2. 写入封面
            if (!picUrl.empty()) {
                string imgData = fetchCover(picUrl);
                if (!imgData.empty()) {
                    TagLib::ByteVector bv(imgData.data(), (unsigned int)imgData.size());

                    // 先清理旧封面帧，防止多次下载导致文件无限增大
                    tag->removeFrames("APIC");

                    TagLib::ID3v2::AttachedPictureFrame* frame = new TagLib::ID3v2::AttachedPictureFrame;
                    frame->setMimeType("image/jpeg");
                    frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                    frame->setPicture(bv);
                    tag->addFrame(frame);
                    WriteLog("[TAG] 封面数据已压入 APIC 帧");
                }
            }

            
            mp3File.save();
            WriteLog("[TAG] MP3 标签已保存为 ID3v2.4 (含 v1 副本)");
        }
    }
    // --- 情况 B: 处理 FLAC ---
    else if (filePath.find(".flac") != string::npos) {
        TagLib::FLAC::File flacFile(wPath.c_str());

        if (flacFile.tag()) {
            // 1. 基础信息写入
            flacFile.tag()->setArtist(TagLib::String(artist, TagLib::String::UTF8));
            flacFile.tag()->setTitle(TagLib::String(title, TagLib::String::UTF8));

            if (config.writeSourceUrlToComment) {
                string webUrl = isPodcast ? "https://music.163.com/#/program?id=" + sid : "https://music.163.com/#/song?id=" + sid;
                flacFile.tag()->setComment(TagLib::String(webUrl, TagLib::String::UTF8));
                WriteLog("[TAG] FLAC 详情链接已写入 Comment");
            }
        }

        // 2. 写入封面
        if (!picUrl.empty()) {
            string imgData = fetchCover(picUrl);
            if (!imgData.empty()) {
                TagLib::ByteVector bv(imgData.data(), (unsigned int)imgData.size());
                flacFile.removePictures(); // 清理旧图片

                TagLib::FLAC::Picture* picture = new TagLib::FLAC::Picture();
                picture->setMimeType("image/jpeg");
                picture->setType(TagLib::FLAC::Picture::FrontCover);
                picture->setData(bv);
                flacFile.addPicture(picture);
                WriteLog("[TAG] FLAC 封面数据已压入");
            }
        }

        flacFile.save();
        WriteLog("[TAG] FLAC 标签及元数据已保存");
    }
}


// UTF-8 转 UTF-16
std::wstring CNeteaseCloud::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}