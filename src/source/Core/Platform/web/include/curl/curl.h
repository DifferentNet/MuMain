#pragma once

// Web build only: the browser build has no libcurl. The cash shop's downloader (excluded from every
// build) is still declared in GameShop/ShopListManager/interface/, which needs these types.
typedef void CURL;
typedef long long curl_off_t;
