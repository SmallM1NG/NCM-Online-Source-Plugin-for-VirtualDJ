#pragma execution_character_set("utf-8")
#define _CRT_SECURE_NO_WARNINGS 
#define _HAS_STD_BYTE 0
#include "NeteaseCloudMusic.h"
#include <map> 
#include <iomanip>


#include <thread>
#include <commdlg.h>
//#pragma comment(lib, "Comdlg32.lib") // 用于弹出保存对话框


#include <shlobj.h> // 用于获取系统文件夹


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

    if (!config.enableLogOutput) return;

    string logPath = GetPluginPath() + "plugin_log.txt";
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

// 获取 插件DLL 所在目录
string CNeteaseCloud::GetPluginPath() {
    char p[MAX_PATH];
    GetModuleFileNameA(hInstance, p, MAX_PATH);
    string s = p;
    return s.substr(0, s.find_last_of("\\/") + 1);
}


// 加载设置
void CNeteaseCloud::LoadSettings() {
    string setPath = GetPluginPath() + "settings.txt";
    string defaultDownloadPath;
    PWSTR pathTmp = NULL;

    if (SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &pathTmp) == S_OK) {
        char pathArray[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, pathTmp, -1, pathArray, MAX_PATH, NULL, NULL);
        defaultDownloadPath = string(pathArray) + "\\NeteaseCloudMusic DL";
        CoTaskMemFree(pathTmp);
    }
    else {
        defaultDownloadPath = GetPluginPath() + "Downloads";
    }

    // 默认值初始化
    config.loadDailyRecommend = false;
    config.loadFavoriteSongs = true;
    config.showCreatedPL = true;
    config.showSubscribedPL = true;
    config.showCreatedPodcastPL = false;
    config.showSubscribedPodcastPL = false;

    config.limitCreatedPL = 1000;
    config.limitSubscribedPL = 1000;
    config.limitCreatedPodcastPL = 1000;
    config.limitSubscribedPodcastPL = 1000;
    config.limitPLTrack = 1000;
    config.limitSearch = 30;

    config.playQuality = "exhigh";
    config.enableDownload = false;
    config.downloadQuality = "exhigh";
    config.downloadPath = defaultDownloadPath;
    config.enableAddTag = true;
    config.enableAddInfoTag = true;
    config.enablePodcastRename = true;

    config.enableLogOutput = true;
    config.apiPort = 3000;

    //没有setting.txt的情况下自动生成默认配置
    ifstream f(setPath);
    if (!f.is_open()) {
        ofstream out(setPath);
        out << "# --- 基础开关  ---" << endl;
        out << "# 仅可填入 true 或 false" << endl;
        out << "加载每日歌曲推荐=false" << endl;
        out << "加载我喜欢的音乐=true" << endl;
        out << "加载我创建的歌单=true" << endl;
        out << "加载我收藏的歌单=true" << endl;
        out << "加载我创建的播客=false" << endl;
        out << "加载我收藏的播客=false" << endl << endl;

        out << "# --- 数量上限 ---" << endl;
        out << "# 为防止卡顿请酌情设限" << endl;
        out << "# 仅可填入 1 - 1000 值 " << endl;
        out << "# xxx歌单/播客上限是指显示该类型歌单数量" << endl;
        out << "# 曲目展示上限为单个歌单内可显示的曲目数量上限" << endl;
        out << "# 由于API限制 歌单内曲目仅可返回至多1000首 播客为500首" << endl;
        out << "# 搜索返回曲目建议 1 - 100 首" << endl;
        out << "我创建的歌单上限=1000" << endl;
        out << "我收藏的歌单上限=1000" << endl;
        out << "我创建的播客上限=1000" << endl;
        out << "我收藏的播客上限=1000" << endl;
        out << "歌单内曲目展示上限=1000" << endl;
        out << "搜索返回曲目数量上限=30" << endl << endl;

        out << "# --- 音质与下载 ---" << endl;
        out << "# 音质类可选 flac/mp3 即为无损/极高 (mp3码率为320Kbps)" << endl;
        out << "# !如果曲目没有此处设定的音质等级 则默认返回降级音频流!" << endl;
        out << "# !且正常曲目可以下载flac 但播客内容仅可下载-mp3- 即使下载音质设为flac!" << endl;
        out << "# 启用写入Tag信息会在下载曲目时将对应的 曲名 作者 封面 写入至曲目Tag" << endl;
        out << "# 当 启用写入Tag信息 时 启用写入InfoTag信息会将 曲目ID 链接 等写入Tag的备注类" << endl;
        out << "# 播客标题自动切分是指 例如获取到的曲目名称格式为 AAA(作者) - BBB(曲目名称)时 自动解析并变更Tag内容" << endl;
        out << "播放音质=mp3" << endl;
        out << "启用下载功能=false" << endl;
        out << "下载音质=mp3" << endl;
        out << "保存路径=" << defaultDownloadPath << endl;
        out << "写入Tag信息=true"  << endl;
        out << "写入InfoTag信息=false" << endl;
        out << "播客标题自动切分=true" << endl << endl;

        out << "# --- Debug ---" << endl;
        out << "# 保持默认即可 如果3000端口被占用可手动指定端口 1-65535" << endl;
        out << "启用日志输出=true" << endl;
        out << "API端口设置=3000" << endl;

        out.close();
    }
    else {
        string line;
        // 严格限制 1-1000 范围
        auto parseNum = [](string s, int def, int minVal = 1, int maxVal = 1000) {
            try {
                int val = stoi(s);
                if (val < minVal || val > maxVal) return def; // 不在范围内应用默认值
                return val;
            }
            catch (...) { return def; } // 非数字应用默认值
            };

        while (getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t pos = line.find('=');
            if (pos == string::npos) continue;
            string key = line.substr(0, pos), val = line.substr(pos + 1);

            // 清理 val 末尾的空格、换行符、回车、制表符
            size_t last = val.find_last_not_of(" \n\r\t");
            if (last != string::npos) {
                val.erase(last + 1);
            }
            else {
                val.clear(); // 如果全是空格，直接清空
            }

            if (key == "加载每日歌曲推荐") config.loadDailyRecommend = (val == "true");
            else if (key == "加载我喜欢的音乐") config.loadFavoriteSongs = (val == "true");
            else if (key == "加载我创建的歌单") config.showCreatedPL = (val == "true");
            else if (key == "加载我收藏的歌单") config.showSubscribedPL = (val == "true");
            else if (key == "加载我创建的播客") config.showCreatedPodcastPL = (val == "true");
            else if (key == "加载我收藏的播客") config.showSubscribedPodcastPL = (val == "true");

            else if (key == "我创建的歌单上限") config.limitCreatedPL = parseNum(val, 1000);
            else if (key == "我收藏的歌单上限") config.limitSubscribedPL = parseNum(val, 1000);
            else if (key == "我创建的播客上限") config.limitCreatedPodcastPL = parseNum(val, 1000);
            else if (key == "我收藏的播客上限") config.limitSubscribedPodcastPL = parseNum(val, 1000);
            else if (key == "歌单内曲目展示上限") config.limitPLTrack = parseNum(val, 1000);
            else if (key == "搜索返回曲目数量上限") config.limitSearch = parseNum(val, 30, 1, 100); // 搜索限制 1-100

            else if (key == "播放音质") config.playQuality = (val == "flac") ? "lossless" : "exhigh";
            else if (key == "启用下载功能") config.enableDownload = (val == "true");
            else if (key == "下载音质") config.downloadQuality = (val == "flac") ? "lossless" : "exhigh";
            else if (key == "保存路径") config.downloadPath = val;
            else if (key == "写入Tag信息") config.enableAddTag = (val == "true");
            else if (key == "写入InfoTag信息") config.enableAddInfoTag = (val == "true");
            else if (key == "播客标题自动切分") config.enablePodcastRename = (val == "true");

            else if (key == "启用日志输出") config.enableLogOutput = (val == "true");
            else if (key == "API端口设置") config.apiPort = parseNum(val, 3000, 1, 65535); // API端口限制
        }
    }
    ApiBase = "http://localhost:" + to_string(config.apiPort);
    WriteLog("[SETTING] 配置加载完成。端口: " + to_string(config.apiPort));
}



// 提取 UID
string CNeteaseCloud::GetUserIdFromData() {
    string dataPath = GetPluginPath() + "user_data.txt";
    ifstream f(dataPath);
    if (!f.is_open()) {
        WriteLog("[USERDATA] user_data.txt 缺失");
        return "";
    }
    stringstream ss; ss << f.rdbuf();
    Json::Value root;
    if (Json::Reader().parse(ss.str(), root)) {
        if (root.isMember("data") && root["data"].isMember("profile")) {
            string uid = root["data"]["profile"]["userId"].asString();
            WriteLog("[USERDATA] 成功获取 UID: " + uid);
            return uid;
        }
    }
    WriteLog("[USERDATA] user_data.txt 解析失败");
    return "";
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

    if (!res.empty()) {
        WriteLog("[COOKIE] 成功提取关键 Cookie 字段");
    }
    else if (!raw.empty()) {
        WriteLog("[COOKIE][ERROR] 文件不为空但未匹配到 MUSIC_U 等关键字段");
    }

    return res;
}


// 网络请求基准
string CNeteaseCloud::HttpGet(const string& url) {
    CURL* curl = curl_easy_init();
    string buf;
    if (curl) {
        string lean = "";
        string cookiePath = GetPluginPath() + "cookie.txt";
        ifstream f(cookiePath);


        if (!f.is_open()) {
            WriteLog("[COOKIE][ERROR] cookie.txt 缺失，将尝试匿名访问");
        }
        else {
            stringstream ss;
            ss << f.rdbuf();
            lean = ExtractLeanCookie(ss.str()); // 提取成功后赋值给 lean
            f.close(); // 读完关掉
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            WriteLog("HTTP FAIL: " + string(curl_easy_strerror(code)) + " URL: " + url);
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


    if (config.enableDownload && !config.downloadPath.empty()) {
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
                WriteLog("[DL][ERROR] 无法创建下载目录，错误代码: " + to_string(err));
            }
        }
    }

    WriteLog("-----插件加载完成-----");
    return S_OK;
}


// 创建歌单文件夹 (按顺序：日推 -> 普通 -> 播客)
HRESULT VDJ_API CNeteaseCloud::GetFolderList(IVdjSubfoldersList* subList) {
    string uid = GetUserIdFromData();
    if (uid.empty()) {
        WriteLog("[FOLDER] UID 为空，取消加载列表");
        return S_OK;
    }

    // 统计计数器
    int totalNormalPlaylists = 0;
    int totalPodcastPlaylists = 0;

    // --- 第一部分：日推 ---
    if (config.loadDailyRecommend) {
        subList->add("NCM_DAILY_RECOMMEND", "每日歌曲推荐");
        totalNormalPlaylists++;
    }

    // --- 第二部分：普通歌单 ---
    string res = HttpGet(ApiBase + "/user/playlist?uid=" + uid);
    Json::Value root;
    if (Json::Reader().parse(res, root)) {
        // 我喜欢的音乐
        if (config.loadFavoriteSongs && root["playlist"].size() > 0) {
            string favId = "NCM_NORMAL_PLAYLIST_" + root["playlist"][0]["id"].asString();
            subList->add(favId.c_str(), root["playlist"][0]["name"].asString().c_str());
            totalNormalPlaylists++;
        }

        // 我创建的歌单
        if (config.showCreatedPL) {
            subList->add("SEP_CREATED", "------ 我创建的歌单 ------");
            int cIdx = 0;
            for (int i = 1; i < (int)root["playlist"].size(); i++) {
                auto& pl = root["playlist"][i];
                if (!pl["subscribed"].asBool() && cIdx < config.limitCreatedPL) {
                    string id = "NCM_NORMAL_PLAYLIST_" + pl["id"].asString();
                    subList->add(id.c_str(), pl["name"].asString().c_str());
                    cIdx++;
                    totalNormalPlaylists++;
                }
            }
        }

        // 我收藏的歌单
        if (config.showSubscribedPL) {
            subList->add("SEP_SUB", "------ 我收藏的歌单 ------");
            int sIdx = 0;
            for (auto& pl : root["playlist"]) {
                if (pl["subscribed"].asBool() && sIdx < config.limitSubscribedPL) {
                    string id = "NCM_NORMAL_PLAYLIST_" + pl["id"].asString();
                    subList->add(id.c_str(), pl["name"].asString().c_str());
                    sIdx++;
                    totalNormalPlaylists++;
                }
            }
        }
    }

    // --- 第三部分：播客歌单 ---
    // 我创建的播客
    if (config.showCreatedPodcastPL) {
        string resDj = HttpGet(ApiBase + "/user/audio?uid=" + uid);
        Json::Value djRoot;
        if (Json::Reader().parse(resDj, djRoot) && djRoot["djRadios"].size() > 0) {
            subList->add("SEP_PC_CREATED", "------ 我创建的播客 ------");
            int count = 0;
            for (auto& r : djRoot["djRadios"]) {
                if (count >= config.limitCreatedPodcastPL) break;
                string id = "NCM_PODCAST_PLAYLIST_" + r["id"].asString();
                subList->add(id.c_str(), r["name"].asString().c_str());
                count++;
                totalPodcastPlaylists++;
            }
        }
    }

    // 我收藏的播客
    if (config.showSubscribedPodcastPL) {
        string resSub = HttpGet(ApiBase + "/dj/sublist");
        Json::Value subRoot;
        if (Json::Reader().parse(resSub, subRoot) && subRoot["djRadios"].size() > 0) {
            subList->add("SEP_PC_SUB", "------ 我收藏的播客 ------");
            int count = 0;
            for (auto& r : subRoot["djRadios"]) {
                if (count >= config.limitSubscribedPodcastPL) break;
                string id = "NCM_PODCAST_PLAYLIST_" + r["id"].asString();
                subList->add(id.c_str(), r["name"].asString().c_str());
                count++;
                totalPodcastPlaylists++;
            }
        }
    }

    // --- 函数末尾：统一输出总结 LOG ---
    string finalLog = "[FOLDER] 列表加载完毕: 成功识别 " +
        to_string(totalNormalPlaylists) + " 个普通歌单, " +
        to_string(totalPodcastPlaylists) + " 个播客歌单。";
    WriteLog(finalLog);

    return S_OK;
}



HRESULT VDJ_API CNeteaseCloud::GetFolder(const char* folderId, IVdjTracksList* tracksList) {
    if (!folderId) return S_OK;
    string fid = folderId;

    // 过滤掉分隔符
    if (fid.find("SEP_") == 0) {
        tracksList->finish();
        return S_OK;
    }

    string url;
    string limitStr = to_string(config.limitPLTrack);
    string listType = ""; // 用于 Log 区分类型

    // --- URL 构造逻辑 ---
    if (fid == "NCM_DAILY_RECOMMEND") {
        url = ApiBase + "/recommend/songs";
        listType = "每日推荐";
    }
    else if (fid.find("NCM_NORMAL_PLAYLIST_") == 0) {
        url = ApiBase + "/playlist/track/all?id=" + fid.substr(20) + "&limit=" + limitStr;
        listType = "普通歌单";
    }
    else if (fid.find("NCM_PODCAST_PLAYLIST_") == 0) {
        url = ApiBase + "/dj/program?rid=" + fid.substr(21) + "&limit=" + limitStr;
        listType = "播客歌单";
    }
    else {
        tracksList->finish();
        return S_OK;
    }

    string res = HttpGet(url);
    Json::Value root;
    int count = 0; // 计数器

    if (Json::Reader().parse(res, root)) {
        // --- 数据解析循环 ---
        if (fid == "NCM_DAILY_RECOMMEND") {
            for (auto& s : root["data"]["dailySongs"]) {
                if (count >= config.limitPLTrack) break;
                AddTrack(tracksList, s, false);
                count++;
            }
        }
        else if (fid.find("NCM_NORMAL_PLAYLIST_") == 0) {
            for (auto& s : root["songs"]) {
                if (count >= config.limitPLTrack) break;
                AddTrack(tracksList, s, false);
                count++;
            }
        }
        else if (fid.find("NCM_PODCAST_PLAYLIST_") == 0) {
            for (auto& p : root["programs"]) {
                if (count >= config.limitPLTrack) break;
                AddTrack(tracksList, p, true);
                count++;
            }
        }
    }

    // --- 重点：在完成加载前写 Log ---
    string folderInfo = "[FOLDER] " + listType + " 内容加载完毕 (ID: " + fid + ")，共推送到 VDJ: " + to_string(count) + " 首曲目";
    WriteLog(folderInfo);

    tracksList->finish();
    return S_OK;
}



// 曲目添加基准
// 曲目添加基准
void CNeteaseCloud::AddTrack(IVdjTracksList* list, const Json::Value& data, bool isPC) {
    string id, name, ar, pic;
    if (isPC) {
        // 播客：使用 NCM_PODCAST_SONG_ 前缀 + 节目ID
        id = "NCM_PODCAST_SONG_" + data["id"].asString();

        // --- 增加播客解析逻辑 ---
        string rawName = data["name"].asString();
        string rawAr = data["dj"]["nickname"].asString();

        if (config.enablePodcastRename) {               //也执行切分逻辑
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

        pic = data["coverUrl"].asString();
    }
    else {
        // 普通：使用 NCM_NORMAL_SONG_ 前缀 + 歌曲ID
        id = "NCM_NORMAL_SONG_" + data["id"].asString();
        name = data["name"].asString();
        for (auto& a : data["ar"]) {
            if (!ar.empty()) ar += ",";
            ar += a["name"].asString();
        }
        pic = data["al"]["picUrl"].asString();
    }
    if (pic.find("//") == 0) pic = "https:" + pic;

    list->add(
        id.c_str(),    // 1. [Unique ID] 必填：该曲目在 VDJ 数据库中的唯一标识
        name.c_str(),  // 2. [Title]     必填：歌曲标题 (对应 VDJ 界面中的 Title 列)
        ar.c_str(),    // 3. [Artist]    必填：艺人名字 (对应 VDJ 界面中的 Artist 列)
        0,             // 4. [Duration]  可选：歌曲时长 (单位：秒，填 0 表示让 VDJ 加载时自动计算)
        0,             // 5. [BPM]       可选：每分钟节拍数 (填 0 表示待计算)
        0,             // 6. [Year]      可选：发行年份
        0,             // 7. [Genre]     可选：风格流派
        pic.c_str(),   // 8. [Cover URL] 可选：封面图链接 (VDJ 侧边栏和浏览器会显示此图)
        0,             // 9. [Key]       可选：调号 (如 1A, 2B 等)
        0.0f           // 10. [Quality]  可选：音质评分 (0.0 到 1.0)
    );
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
        // uppercase 确保输出如 %2F 而不是 %2f
        escaped << uppercase << '%' << setw(2) << int(c) << nouppercase;
    }

    return escaped.str();
}



// 搜索功能
HRESULT VDJ_API CNeteaseCloud::OnSearch(const char* search, IVdjTracksList* tracksList) {
    if (!search || strlen(search) < 1) return S_OK;

    // 实际使用搜索API时输入重编码过数据
    string encodedSearch = UrlEncode(search);

    WriteLog("-----搜索触发-----");
    WriteLog("[SEARCH] 原始内容: " + string(search));
    WriteLog("[SEARCH] 重编码: " + encodedSearch);

    // 使用转码后的数据拼接 URL
    string url = ApiBase + "/cloudsearch?keywords=" + encodedSearch + "&limit=" + to_string(config.limitSearch);
    string res = HttpGet(url);
    Json::Value root;
    if (Json::Reader().parse(res, root)) {
        for (auto& s : root["result"]["songs"]) {
            AddTrack(tracksList, s, false);
        }
    }
    tracksList->finish();
    return S_OK;
}



// 获取流基类
HRESULT VDJ_API CNeteaseCloud::GetStreamUrl(const char* id, IVdjString& url, IVdjString& err) {
    string fullId = id;
    string realTrackId = "";

    // 识别曲目身份
    if (fullId.find("NCM_NORMAL_SONG_") == 0) {
        realTrackId = fullId.substr(16); // 截取 NCM_NORMAL_SONG_ 后的 ID
        WriteLog("[STREAM] 获取普通歌曲流, ID: " + realTrackId);
    }
    else if (fullId.find("NCM_PODCAST_SONG_") == 0) {
        string programId = fullId.substr(17); // 截取 NCM_PODCAST_SONG_ 后的 ID
        WriteLog("[STREAM] 获取播客节目流, ProgramID: " + programId);

        // 播客需要通过接口换取真正的音频流 ID
        string res = HttpGet(ApiBase + "/dj/program/detail?id=" + programId);
        Json::Value pRoot;
        if (Json::Reader().parse(res, pRoot)) {
            realTrackId = pRoot["program"]["mainSong"]["id"].asString();
            WriteLog("[STREAM] 播客 ID 转换成功 -> 实际流ID: " + realTrackId);
        }
    }

    if (realTrackId.empty()) {
        WriteLog("[STREAM][ERROR] 无法识别 ID 类型或解析失败: " + fullId);
        return S_FALSE;
    }

    // 获取播放直链
    string res = HttpGet(ApiBase + "/song/url/v1?id=" + realTrackId + "&level=" + config.playQuality);
    Json::Value root;
    if (Json::Reader().parse(res, root) && !root["data"][0]["url"].isNull()) {
        string streamUrl = root["data"][0]["url"].asString();


        WriteLog("[STREAM] 流直链地址: " + streamUrl);

        url = streamUrl.c_str();
        return S_OK;
    }

    WriteLog("[STREAM][ERROR] 无法获取音频直链, ID: " + realTrackId);
    return S_FALSE;
}



//取消搜索
HRESULT VDJ_API CNeteaseCloud::OnSearchCancel() {
    WriteLog("-----用户取消搜索-----");
    return S_OK;
}




//当获取插件info
HRESULT VDJ_API CNeteaseCloud::OnGetPluginInfo(TVdjPluginInfo8* info)
{

    info->PluginName = "NeteaseCloudMusic";
    info->Author = "小小小小铭";
    info->Version = "Build Date:260331 v0.1";
    info->Description = "网易云音乐在线源支持";
    info->Flags = 0x00;

    return S_OK;
}




//ULONG VDJ_API CNeteaseCloud::Release() {
//    WriteLog("--- PLUGIN RELEASED ---");
//    curl_global_cleanup();
//    delete this;
//    return 0;
//}




// 右键点击文件夹 只做展示
HRESULT VDJ_API CNeteaseCloud::GetFolderContextMenu(const char* folderUniqueId, IVdjContextMenu* contextMenu)
{
    // 右键点击的是插件的根目录
    if (folderUniqueId == NULL)
    {
        
        contextMenu->add("By 小小小小铭 Version: 260420 v0.2");
        contextMenu->add("打开配置文件");
        contextMenu->add("打开日志文件");
        contextMenu->add("打开插件安装目录");
        contextMenu->add("打开曲目下载目录");


    }

    return S_OK;
}

// 右键点击文件夹 功能实现
HRESULT VDJ_API CNeteaseCloud::OnFolderContextMenu(const char* folderUniqueId, size_t menuIndex)
{
    if (folderUniqueId == NULL)
    {   
        if (menuIndex == 0)
        {   
            ShellExecuteW(NULL, L"open", L"https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ", NULL, NULL, SW_SHOWNORMAL);
            ShellExecuteW(NULL, L"open",L"https://space.bilibili.com/475951038", NULL, NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 1)
        {
            string path = GetPluginPath() + "settings.txt";
            
            ShellExecuteW(NULL, L"open", L"notepad.exe", Utf8ToWide(path).c_str(), NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 2)
        {
            string path = GetPluginPath() + "plugin_log.txt";
            ShellExecuteW(NULL, L"open", L"notepad.exe", Utf8ToWide(path).c_str(), NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 3)
        {
            string path = GetPluginPath();

            ShellExecuteW(NULL, L"open", Utf8ToWide(path).c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        else if (menuIndex == 4)
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
    
        if (config.enableDownload)
        {
            contextMenu->add("下载此歌曲");
        }

    return S_OK;
}




//右键点击曲目 功能实现
HRESULT VDJ_API CNeteaseCloud::OnContextMenu(const char* id, size_t i) {
    if (!id || !config.enableDownload || i != 0) return S_OK;


    string fullId = id;
    string title, artist, downloadId, picUrl; // 增加 picUrl 存储
    bool isPodcast = false;

    // --- 分支 A: 普通歌曲 ---
    if (fullId.find("NCM_NORMAL_SONG_") == 0) {
        downloadId = fullId.substr(16);
        string res = HttpGet(ApiBase + "/song/detail?ids=" + downloadId);
        Json::Value root;
        if (Json::Reader().parse(res, root) && root["songs"].size() > 0) {
            title = root["songs"][0]["name"].asString();
            picUrl = root["songs"][0]["al"]["picUrl"].asString(); // 封面 URL
            for (auto& a : root["songs"][0]["ar"]) {
                if (!artist.empty()) artist += ",";
                artist += a["name"].asString();
            }
            WriteLog("[DL][NORMAL] 普通歌曲解析成功: " + artist + " - " + title);
        }
    }
    // --- 分支 B: 播客节目 ---
    else if (fullId.find("NCM_PODCAST_SONG_") == 0) {
        isPodcast = true;
        string programId = fullId.substr(17); // 这里的 programId 用于生成详情页链接
        string res = HttpGet(ApiBase + "/dj/program/detail?id=" + programId);
        Json::Value root;
        if (Json::Reader().parse(res, root)) {
            string rawTitle = root["program"]["name"].asString();
            string rawArtist = root["program"]["dj"]["nickname"].asString();

            // 检测切分开关
            if (config.enablePodcastRename) {
                size_t pos = rawTitle.find(" - ");
                if (pos != string::npos) {
                    artist = rawTitle.substr(0, pos);          // AAA 部分作为作者
                    title = rawTitle.substr(pos + 3);          // BBB 部分作为标题
                    WriteLog("[DL][PODCAST] 检测到切分格式，自动重命名为: " + artist + " - " + title);
                }
                else {
                    title = rawTitle;
                    artist = rawArtist;
                }
            }
            else {
                title = rawTitle;
                artist = rawArtist;
            }

            picUrl = root["program"]["coverUrl"].asString(); // 封面 URL
            downloadId = programId; // 传出 programId
            WriteLog("[DL][PODCAST] 播客节目解析成功: " + artist + " - " + title);
        }
    }

    // 提交下载
    if (!downloadId.empty() && !title.empty()) {
        DownloadSong(downloadId, artist, title, isPodcast, picUrl);
    }
    else {
        WriteLog("[DL][ERROR] 解析失败，无法开始任务。ID: " + fullId);
    }
    return S_OK;
}




// 下载功能基准
void CNeteaseCloud::DownloadSong(const string& sid, const string& artist, const string& title, bool isPodcast, const string& picUrl) {
    // 这里的 sid 现在对于播客来说已经是 programId 了
    std::thread([this, sid, artist, title, isPodcast, picUrl]() {
        string logTag = isPodcast ? "[DL][PODCAST] " : "[DL][NORMAL] ";
        string realAudioId = sid;

        // 如果是播客，需要先通过 programId 换取真正的音频流 ID
        if (isPodcast) {
            string res = HttpGet(ApiBase + "/dj/program/detail?id=" + sid);
            Json::Value pRoot;
            if (Json::Reader().parse(res, pRoot)) {
                realAudioId = pRoot["program"]["mainSong"]["id"].asString();
            }
        }

        // 获取直链
        string res = HttpGet(ApiBase + "/song/url/v1?id=" + realAudioId + "&level=" + config.downloadQuality);
        Json::Value root;
        if (!Json::Reader().parse(res, root) || root["data"].empty() || root["data"][0]["url"].isNull()) {
            WriteLog(logTag + "ERROR: 无法获取直链，TrackID: " + realAudioId);
            return;
        }
        string dUrl = root["data"][0]["url"].asString();

        // 确定文件名和后缀
        string ext = ".mp3";
        if (!isPodcast && config.downloadQuality == "lossless") {
            ext = ".flac";
        }

        string fileName;
        if (!artist.empty() && !title.empty()) {
            // 这里统一采用 "作者 - 标题" 命名，如果是播客且触发了切分，此时 artist title 已是解析后的
            fileName = artist + " - " + title + ext;
        }
        else {
            fileName = sid + ext; // 兜底方案
        }

        // 过滤非法字符
        const string illegal = "\\/:*?\"<>|";
        for (char& c : fileName) if (illegal.find(c) != string::npos) c = '_';

        // 执行下载
        std::wstring wDownloadPath = Utf8ToWide(config.downloadPath);
        CreateDirectoryW(wDownloadPath.c_str(), NULL);

        string fullPath = config.downloadPath + "\\" + fileName;
        std::wstring wPath = Utf8ToWide(fullPath);

        CURL* curl = curl_easy_init();
        if (curl) {
            FILE* fp = _wfopen(wPath.c_str(), L"wb");
            if (fp) {
                WriteLog(logTag + "开始下载: " + fileName + " (URL: " + dUrl.substr(0, 30) + "...)");

                curl_easy_setopt(curl, CURLOPT_URL, dUrl.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

                CURLcode code = curl_easy_perform(curl);
                fclose(fp); // 必须先关闭文件，TagLib 才能接管写入

                if (code == CURLE_OK) {
                    WriteLog(logTag + "下载成功: " + fileName);

                    // --- 写入 Tag 逻辑 ---
                    if (config.enableAddTag) {
                        WriteLog(logTag + "开始写入音频标签及详情页链接...");
                        AddTags(fullPath, artist, title, sid, picUrl, isPodcast);
                    }
                }
                else {
                    WriteLog(logTag + "[ERROR] CURL 失败，代码: " + std::to_string(code));
                }
            }
            else {
                WriteLog(logTag + "[ERROR] 无法打开文件句柄: " + fileName);
            }
            curl_easy_cleanup(curl);
        }
        }).detach();
}




void CNeteaseCloud::AddTags(const string& filePath, const string& artist, const string& title, const string& sid, const string& picUrl, bool isPodcast) {
    std::wstring wPath = Utf8ToWide(filePath);

    // --- 情况 A: 处理 MP3 ---
    if (filePath.find(".mp3") != string::npos) {
        TagLib::MPEG::File mp3File(wPath.c_str());

        // true 表示如果不存在 ID3v2 标签则创建一个
        TagLib::ID3v2::Tag* tag = mp3File.ID3v2Tag(true);
        if (tag) {
            // 1. 基础信息写入
            tag->setArtist(TagLib::String(artist, TagLib::String::UTF8));
            tag->setTitle(TagLib::String(title, TagLib::String::UTF8));

            if (config.enableAddInfoTag) {
                string webUrl = isPodcast ? "https://music.163.com/#/program?id=" + sid : "https://music.163.com/#/song?id=" + sid;
                tag->setComment(TagLib::String(webUrl, TagLib::String::UTF8));
                WriteLog("[TAG] MP3 详情链接已写入 Comment");
            }

            // 2. 写入封面
            if (!picUrl.empty()) {
                string imgData = HttpGet(picUrl);
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

            if (config.enableAddInfoTag) {
                string webUrl = isPodcast ? "https://music.163.com/#/program?id=" + sid : "https://music.163.com/#/song?id=" + sid;
                flacFile.tag()->setComment(TagLib::String(webUrl, TagLib::String::UTF8));
                WriteLog("[TAG] FLAC 详情链接已写入 Comment");
            }
        }

        // 2. 写入封面
        if (!picUrl.empty()) {
            string imgData = HttpGet(picUrl);
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


// UTF8 TO UTF16 转码
std::wstring CNeteaseCloud::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}