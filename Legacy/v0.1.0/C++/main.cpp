#include "NeteaseCloudMusic.h"

HRESULT VDJ_API DllGetClassObject(const GUID& rclsid, const GUID& riid, void** ppObject)
{
    if (memcmp(&rclsid, &CLSID_VdjPlugin8, sizeof(GUID)) == 0 &&
        memcmp(&riid, &IID_IVdjPluginOnlineSource, sizeof(GUID)) == 0)
    {
        *ppObject = new CNeteaseCloud();
        return S_OK;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}