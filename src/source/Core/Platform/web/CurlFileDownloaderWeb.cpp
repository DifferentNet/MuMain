// Web build only: there's no libcurl in the browser, and the cash shop it downloads for is switched
// off on our server. Every download fails the way a failed curl session does.

#include "GameShop/FileDownloader/CurlFileDownloader.h"
#include "GameShop/ShopListManager/interface/WZResult/ErrorCodeDefine.h"

WZResult CurlFileDownloader::DownloadFile(const std::wstring& /*url*/, const std::wstring& /*localPath*/,
                                          const std::wstring& /*username*/, const std::wstring& /*password*/,
                                          bool /*passiveFtp*/, const std::atomic_bool* /*pBreak*/)
{
    return WZResult::BuildResult(DL_CREATE_SESSION, 0, L"Downloads aren't available in the web client");
}
