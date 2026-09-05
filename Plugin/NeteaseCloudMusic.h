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
#include <utility>
#include <shellapi.h>
#include <atomic>
#include <mutex>
#include <thread>

using namespace std;

struct PluginConfig {
    bool loadDailyRecommend;
    bool loadFavoriteSongs;
    bool showCreatedPlaylists;
    bool showSubscribedPlaylists;
    bool showCreatedPodcasts;
    bool showSubscribedPodcasts;
    bool showSubscribedAlbums;
    bool showSubscribedVideos;
    int maxVideoPlaybackQuality;
    int maxVideoDownloadQuality;
    int limitCreatedPlaylists;
    int limitSubscribedPlaylists;
    int limitCreatedPodcasts;
    int limitSubscribedPodcasts;
    int limitSubscribedAlbums;
    int limitItemsPerFolder;
    int searchResultLimit;
    int coverSize = 1000;            // 封面尺寸上限（音频与视频共用，?param=NxN）
    int searchType;                  // 搜索类型：1=单曲, 2000=声音, 1014=视频, 1004=MV
    string playQuality;
    bool enableTrackDownload;
    bool enableVideoDownload;
    string trackDownloadQuality;
    string downloadPath;
    bool writeTags;
    bool writeSourceUrlToComment;
    bool splitPodcastTitle;
    bool splitLargeFolders;
    bool enableLogging;
    int apiPort;
    bool showApiConsole;
};

class CNeteaseCloud : public IVdjPluginOnlineSource {
public:
    HRESULT VDJ_API OnLoad() override;
    ULONG VDJ_API Release() override;
    HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8* info) override;
    HRESULT VDJ_API OnGetUserInterface(TVdjPluginInterface8* pluginInterface) override;
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
    enum ControlId {
        IDC_LOGIN = 2001, IDC_LOGOUT, IDC_STATUS, IDC_AVATAR, IDC_NICKNAME,
        IDC_DAILY, IDC_FAVORITES, IDC_CREATED_PL, IDC_SUBSCRIBED_PL,
        IDC_CREATED_PODCAST, IDC_SUBSCRIBED_PODCAST, IDC_SUBSCRIBED_ALBUM, IDC_SUBSCRIBED_VIDEO,
        IDC_LIMIT_CREATED, IDC_LIMIT_SUBSCRIBED, IDC_LIMIT_CREATED_PODCAST,
        IDC_LIMIT_SUBSCRIBED_PODCAST, IDC_LIMIT_SUBSCRIBED_ALBUM, IDC_LIMIT_TRACK, IDC_LIMIT_SEARCH,
        IDC_PLAY_QUALITY, IDC_TRACK_DOWNLOAD, IDC_TRACK_DOWNLOAD_QUALITY, IDC_DOWNLOAD_PATH,
        IDC_VIDEO_PLAYBACK_QUALITY, IDC_VIDEO_DOWNLOAD, IDC_VIDEO_DOWNLOAD_QUALITY,
        IDC_ADD_TAG, IDC_ADD_INFO_TAG, IDC_SEARCH_SONG, IDC_SEARCH_VOICE, IDC_SEARCH_VIDEO, IDC_SEARCH_MV,
        IDC_PODCAST_RENAME, IDC_SPLIT_LARGE_PLAYLISTS, IDC_LOG_OUTPUT,
        IDC_API_PORT, IDC_SHOW_API_CONSOLE,
        IDC_API_STATUS, IDC_RESTART_API, IDC_DEFAULTS, IDC_BROWSE, IDC_AUTHOR_LINK,
        IDC_COVER,
        IDC_SECTION_ACCOUNT, IDC_SECTION_CONTENT, IDC_SECTION_QUANTITY, IDC_SECTION_SEARCH, IDC_SECTION_MEDIA, IDC_SECTION_DOWNLOAD, IDC_SECTION_OTHER, IDC_SECTION_SYSTEM,
        IDC_LIMIT_HINT
    };

    string ApiBase = "http://127.0.0.1:";
    PluginConfig config{};
    mutable mutex stateMutex;
    atomic<bool> shuttingDown{ false };
    atomic<bool> searchCancel{ false };
    atomic<bool> loginInProgress{ false };
    atomic<bool> apiOnline{ false };
    mutex apiProcessMutex;
    std::thread apiMonitorThread;
    HANDLE apiProcessHandle = nullptr;
    HANDLE apiJob = nullptr;   // 作业对象，使用「关闭作业时结束进程」
    int apiPortActive = 0;
    HWND configWindow = nullptr;
    HBRUSH darkBackgroundBrush = nullptr;
    HBRUSH darkControlBrush = nullptr;
    HFONT uiFont = nullptr;
    int uiFontHeight = -12;
    HFONT linkFont = nullptr;   // 底部署名/版本号带下划线字体
    HBITMAP avatarBitmap = nullptr;
    string loginCookie;
    string leanLoginCookie;
    string userId;
    string userNickname;
    string userAvatarUrl;

    void SetDefaultSettings();
    void LoadSettings();
    bool SaveSettings(bool log = true);
    void ApplySettings();
    string GetPluginPath();
    string GetUserIdFromData();
    string ExtractLeanCookie(const string& raw);
    string HttpGet(const string& url, bool logFailure = true, long timeoutS = 10);
    string HttpPostJson(const string& url, const string& body);
    void WriteLog(const string& text);
    string UrlEncode(const string& value);
    bool HandleSearchLink(const string& text, IVdjTracksList* tracksList, int wanted, int& added);
    void DownloadSong(const string& sid, const string& artist, const string& title, bool isPodcast, const string& picUrl);
    void DownloadVideo(const string& videoId, bool isMusicVideo);
    std::wstring Utf8ToWide(const std::string& str);
    string WideToUtf8(const std::wstring& str);
    void AddTrack(IVdjTracksList* list, const Json::Value& data, bool isDj);
    void AddVideoTrack(IVdjTracksList* list, const Json::Value& data);
    void AddTags(const string& filePath, const string& artist, const string& title, const string& sid, const string& picUrl, bool isPodcast);

    bool StartApiIfNeeded(bool force = false);
    void StartApiMonitor();
    void StopApiService();
    void RestartApiService();
    bool CreateApiJob();
    void CloseApiJob();
    bool IsApiRunning();
    void UpdateApiStatusControl();
    bool RefreshLoginState();
    void LoadLoginData();
    void SaveLoginData();
    void ClearLoginData();
    void BeginQrLogin();
    void LogoutAccount();
    bool CacheUserAvatar();
    HBITMAP LoadAvatarBitmap(const string& filePath, UINT width, UINT height);
    void RefreshAvatarControl();

    HWND CreateConfigWindow();
    void PopulateConfigWindow();
    void ReadConfigWindow();
    void UpdateLoginStatusControl();
    void InvalidateRadioControls();            // 重绘四个自绘搜索单选钮
    static LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK NumEditWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam); // 数字输入框子类化
    static LRESULT CALLBACK LabelClickProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam); // 静态文本子类化：点击标签时归还焦点
    LRESULT HandleConfigMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

#endif