#ifndef NETEASECLOUD_H
#define NETEASECLOUD_H

#define CURL_STATICLIB
#include "vdjOnlineSource.h"
#include <curl/curl.h>
#include <json/json.h>
#include <string>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <vector>
#include <shellapi.h>

using namespace std;

// 配置项结构体
struct PluginConfig {
    bool loadDailyRecommend;           // 是否加载每日推荐
    bool loadFavoriteSongs;             // 是否加载喜欢的音乐
    bool showCreatedPL;         // 是否显示创建的歌单
    bool showSubscribedPL;      // 是否显示收藏的歌单
    bool showCreatedPodcastPL;       // 是否显示创建的播客
    bool showSubscribedPodcastPL;    // 是否显示收藏的播客

    int limitCreatedPL;         // 创建歌单上限
    int limitSubscribedPL;      // 收藏歌单上限
    int limitCreatedPodcastPL;       // 创建播客上限
    int limitSubscribedPodcastPL;    // 收藏播客上限

    int limitPLTrack;     // 全部歌单曲目展示上限
    int limitSearch;          // 搜索返回上限

    string playQuality;           // 播放音质
    bool enableDownload;      // 启用下载功能
    string downloadQuality;   // 下载音质
    string downloadPath;      // 下载路径
    bool enableAddTag;        // 启用添加曲目Tag
    bool enableAddInfoTag;    // 启用添加曲目LINK至Tag备注区
    bool enablePodcastRename; // 启用播客标题自动切分

    bool enableLogOutput;     // 启用日志输出

    int apiPort;    // API端口
};


class CNeteaseCloud : public IVdjPluginOnlineSource {
public:
    HRESULT VDJ_API OnLoad() override;
    HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8* info) override;
    //ULONG   VDJ_API Release() override;
    HRESULT VDJ_API GetFolderList(IVdjSubfoldersList* subList) override;
    HRESULT VDJ_API GetFolder(const char* folderId, IVdjTracksList* tracksList) override;
    HRESULT VDJ_API GetStreamUrl(const char* id, IVdjString& url, IVdjString& err) override;
    HRESULT VDJ_API OnSearch(const char* search, IVdjTracksList* tracksList) override;
    HRESULT VDJ_API OnSearchCancel() override;


    HRESULT VDJ_API GetContextMenu(const char* id, IVdjContextMenu* m) override;
    HRESULT VDJ_API OnContextMenu(const char* id, size_t i) override;
    HRESULT VDJ_API GetFolderContextMenu(const char* id, IVdjContextMenu* m) override;
    HRESULT VDJ_API OnFolderContextMenu(const char* id, size_t i) override;

    

private:
    string ApiBase = "http://localhost:";
    PluginConfig config;

    void LoadSettings();
    string GetPluginPath();
    string GetUserIdFromData();
    string ExtractLeanCookie(const string& raw);
    string HttpGet(const string& url);
    void WriteLog(const string& text);
    string UrlEncode(const string& value);
    void DownloadSong(const string& sid, const string& artist, const string& title, bool isPodcast, const string& picUrl);
    std::wstring Utf8ToWide(const std::string& str);

    void AddTrack(IVdjTracksList* list, const Json::Value& data, bool isDj);
    void AddTags(const string& filePath, const string& artist, const string& title, const string& sid, const string& picUrl, bool isPodcast);
};

#endif