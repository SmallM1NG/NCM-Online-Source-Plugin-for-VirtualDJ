# API Server

本目录不是完整的网易云 API 服务，而是对上游项目的补丁层：

[neteasecloudmusicapienhanced/api-enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced)

完整服务请先按上游仓库自行获取，再把这里的文件按相同相对路径覆盖进去。

```
API Server/
└─ api-enhanced/
   ├─ server.js
   └─ public/
      └─ virtualdj_plugin_qrlogin.html
```

对应上游路径：

| 本仓库 | 上游 api-enhanced |
| --- | --- |
| `api-enhanced/server.js` | `server.js` |
| `api-enhanced/public/virtualdj_plugin_qrlogin.html` | `public/virtualdj_plugin_qrlogin.html` |

## 相对上游的修改

1. `server.js`  
   增加仅限 localhost 的一次性登录交接：
   - `POST /virtualdj_plugin/login/complete`
   - `GET /virtualdj_plugin/login/result`  
   供 VirtualDJ 插件扫码页把 Cookie 交回 DLL。
2. `public/virtualdj_plugin_qrlogin.html`  
   新增插件专用二维码登录页。插件设置里点「登录」会打开：  
   `http://127.0.0.1:{port}/virtualdj_plugin_qrlogin.html?session=...`

其余 API 行为仍以上游为准。
