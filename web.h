// web.h - the built-in browser: a borderless window that is ONLY the web page (no bars, no buttons).
// The palette is the address bar: type  w  + space + a site or search words, Enter. Esc / Ctrl+W close it.
// The engine (Microsoft WebView2) starts in the background as soon as you type "w ", stays warm briefly after closing,
// then exits completely so it uses no memory.
#pragma once

static HWND g_ww = nullptr;
static bool g_webShown = false, g_webReady = false, g_webCreating = false;
static const UINT WM_WEB_BACK = WM_APP + 21, WM_WEB_FWD = WM_APP + 22, WM_WEB_RELOAD = WM_APP + 23, WM_WEB_FULL = WM_APP + 24,
                  WM_WEB_PALETTE = WM_APP + 25, WM_WEB_NAVURL = WM_APP + 26;

// ---- built-in ad / tracker blocking (no extension needed, almost no RAM)
static const wchar_t* kAdHosts[] = {
    L"doubleclick.net", L"googlesyndication.com", L"googleadservices.com", L"adservice.google.com", L"google-analytics.com",
    L"googletagservices.com", L"2mdn.net", L"adnxs.com", L"adsrvr.org", L"advertising.com", L"amazon-adsystem.com", L"criteo.com",
    L"criteo.net", L"taboola.com", L"taboolasyndication.com", L"outbrain.com", L"outbrainimg.com", L"scorecardresearch.com",
    L"quantserve.com", L"rubiconproject.com", L"pubmatic.com", L"openx.net", L"casalemedia.com", L"moatads.com", L"smartadserver.com",
    L"serving-sys.com", L"adform.net", L"bidswitch.net", L"contextweb.com", L"lijit.com", L"sharethrough.com", L"teads.tv",
    L"yieldmo.com", L"zedo.com", L"revcontent.com", L"mgid.com", L"popads.net", L"propellerads.com", L"exoclick.com", L"hotjar.com",
    L"mixpanel.com", L"fullstory.com", L"mouseflow.com", L"crazyegg.com", L"chartbeat.com", L"ads-twitter.com", L"analytics.twitter.com",
    L"px.ads.linkedin.com", L"analytics.tiktok.com", L"bat.bing.com", L"clarity.ms", L"adsymptotic.com", L"tapad.com", L"bluekai.com",
    L"demdex.net", L"everesttech.net", L"krxd.net", L"agkn.com", L"rlcdn.com", L"liadm.com", L"id5-sync.com", L"mathtag.com",
    L"yieldlab.net", L"smaato.net", L"inmobi.com", L"applovin.com", L"ads.yahoo.com", L"adtechus.com", L"media.net", L"gumgum.com",
    L"triplelift.com", L"3lift.com", L"indexww.com", L"adsafeprotected.com", L"doubleverify.com", L"imrworldwide.com", L"pixel.facebook.com",
    L"stats.g.doubleclick.net", L"ad.doubleclick.net", L"pagead2.googlesyndication.com", L"securepubads.g.doubleclick.net",
};
static std::unordered_set<wstring> g_blockSet;
static bool g_blockInit = false;
static wstring HostOf(const wchar_t* uri) {
    const wchar_t* p = wcsstr(uri, L"://");
    if (!p) return L"";
    p += 3;
    const wchar_t* e = p;
    while (*e && *e != L'/' && *e != L':' && *e != L'?' && *e != L'#') e++;
    wstring h(p, (size_t)(e - p));
    for (auto& c : h) c = (wchar_t)towlower(c);
    return h;
}
static bool HostBlocked(const wstring& host) {
    if (!g_blockInit) {
        g_blockInit = true;
        for (auto h : kAdHosts) g_blockSet.insert(h);
        for (auto& h : g_webBlock) g_blockSet.insert(lower(h));
    }
    wstring h = host;
    for (;;) {
        if (g_blockSet.count(h)) return true;
        size_t d = h.find(L'.');
        if (d == wstring::npos) return false;
        h = h.substr(d + 1);
    }
}
static bool IsLocalHost(const wstring& h) {
    return h == L"localhost" || h.rfind(L"127.", 0) == 0 || h == L"[::1]";
}
static bool IsPrivateHost(const wstring& h) {   // this PC or the home / office network
    if (h.empty()) return false;
    if (IsLocalHost(h) || h.rfind(L"10.", 0) == 0 || h.rfind(L"192.168.", 0) == 0 || h.rfind(L"169.254.", 0) == 0) return true;
    if (h.rfind(L"172.", 0) == 0) { int b = _wtoi(h.c_str() + 4); if (b >= 16 && b <= 31) return true; }
    return h.size() > 6 && h.compare(h.size() - 6, 6, L".local") == 0;
}

#ifdef QP_WEBVIEW
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
static ComPtr<ICoreWebView2Environment> g_wvEnv;
static ComPtr<ICoreWebView2Controller> g_wvCtl;
static ComPtr<ICoreWebView2> g_wv;
static wstring g_webPending;
static int g_extPending = 0;

static bool UriAllowed(const wchar_t* uri) {   // only web pages
    return uri && (_wcsnicmp(uri, L"https://", 8) == 0 || _wcsnicmp(uri, L"http://", 7) == 0 || _wcsnicmp(uri, L"about:blank", 11) == 0);
}
static void WebResize() {
    if (!g_wvCtl || !g_ww) return;
    RECT rc; GetClientRect(g_ww, &rc);
    g_wvCtl->put_Bounds(rc);
}
static void WebFocus() { if (g_wvCtl && g_webShown) g_wvCtl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC); }
static void WebNavigate(const wstring& text) {
    wstring u = WebNormalize(text);
    if (u.empty()) return;
    if (!g_wv || !g_webReady) { g_webPending = text; return; }   // the engine is still starting: go there as soon as it is ready
    g_wv->Navigate(u.c_str());
    WebFocus();
}
static void WebBack() { if (g_wv) g_wv->GoBack(); }
static void WebFwd() { if (g_wv) g_wv->GoForward(); }
static void WebReload() { if (g_wv) g_wv->Reload(); }

static void WebFinishSetup() {
    g_webReady = true;
    g_webCreating = false;
    if (!g_webPending.empty()) { wstring p = g_webPending; g_webPending.clear(); WebNavigate(p); }
}

static void WebSetup() {
    if (!g_wvCtl) return;
    g_wvCtl->get_CoreWebView2(&g_wv);
    if (!g_wv) { g_webCreating = false; return; }
    WebResize();
    g_wvCtl->put_IsVisible(g_webShown ? TRUE : FALSE);
    {   // match the theme so there is no white flash
        ComPtr<ICoreWebView2Controller2> c2;
        if (SUCCEEDED(g_wvCtl.As(&c2))) {
            COREWEBVIEW2_COLOR col = { 255, GetRValue(g_th->bg), GetGValue(g_th->bg), GetBValue(g_th->bg) };
            c2->put_DefaultBackgroundColor(col);
        }
    }

    ComPtr<ICoreWebView2Settings> st;
    g_wv->get_Settings(&st);
    if (st) {
        st->put_AreDevToolsEnabled(FALSE);
        st->put_IsStatusBarEnabled(TRUE);
        st->put_IsZoomControlEnabled(TRUE);
        st->put_IsWebMessageEnabled(FALSE);
        st->put_AreHostObjectsAllowed(FALSE);
        ComPtr<ICoreWebView2Settings4> s4;
        if (SUCCEEDED(st.As(&s4))) { s4->put_IsPasswordAutosaveEnabled(FALSE); s4->put_IsGeneralAutofillEnabled(FALSE); }
        ComPtr<ICoreWebView2Settings8> s8;
        if (SUCCEEDED(st.As(&s8))) s8->put_IsReputationCheckingRequired(TRUE);   // SmartScreen on
    }
    ComPtr<ICoreWebView2_19> w19;
    if (SUCCEEDED(g_wv.As(&w19))) w19->put_MemoryUsageTargetLevel(COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW);

    ComPtr<ICoreWebView2_13> w13; ComPtr<ICoreWebView2Profile> prof; ComPtr<ICoreWebView2Profile7> p7;
    if (SUCCEEDED(g_wv.As(&w13)) && SUCCEEDED(w13->get_Profile(&prof))) {
        ComPtr<ICoreWebView2Profile3> p3;
        if (SUCCEEDED(prof.As(&p3))) p3->put_PreferredTrackingPreventionLevel(COREWEBVIEW2_TRACKING_PREVENTION_LEVEL_STRICT);
        prof.As(&p7);
    }

    EventRegistrationToken tk;
    g_wv->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* a) -> HRESULT {
            PWSTR uri = nullptr;
            a->get_Uri(&uri);
            wstring u = uri ? uri : L"";
            if (uri) CoTaskMemFree(uri);
            if (!UriAllowed(u.c_str())) { a->put_Cancel(TRUE); return S_OK; }   // only http / https
            if (g_webHttpsOnly && _wcsnicmp(u.c_str(), L"http://", 7) == 0 && !IsPrivateHost(HostOf(u.c_str()))) {
                a->put_Cancel(TRUE);   // upgrade to https instead of loading plain http
                if (g_ww) PostMessageW(g_ww, WM_WEB_NAVURL, 0, (LPARAM)new wstring(L"https://" + u.substr(7)));
            }
            return S_OK;
        }).Get(), &tk);
    g_wv->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(   // no pop-up windows: open in this view
        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* a) -> HRESULT {
            a->put_Handled(TRUE);
            PWSTR uri = nullptr;
            a->get_Uri(&uri);
            if (UriAllowed(uri) && g_wv) g_wv->Navigate(uri);
            if (uri) CoTaskMemFree(uri);
            return S_OK;
        }).Get(), &tk);
    g_wv->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(   // camera, mic, location, notifications...: always no
        [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* a) -> HRESULT {
            a->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); return S_OK;
        }).Get(), &tk);
    ComPtr<ICoreWebView2_4> w4;
    if (SUCCEEDED(g_wv.As(&w4)))
        w4->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
            [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* a) -> HRESULT {
                if (g_webBlockDl) { a->put_Cancel(TRUE); Balloon(L"Quick Palette", L"Downloads are blocked in the palette browser. Use your normal browser for downloads.", NIIF_INFO); }
                return S_OK;
            }).Get(), &tk);
    ComPtr<ICoreWebView2_14> w14;   // nobody can click through a certificate error
    if (SUCCEEDED(g_wv.As(&w14)))
        w14->add_ServerCertificateErrorDetected(Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* a) -> HRESULT {
                a->put_Action(COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_CANCEL); return S_OK;
            }).Get(), &tk);
    ComPtr<ICoreWebView2_18> w18;   // pages may not launch other programs (mailto:, ms-msdt:, ...)
    if (SUCCEEDED(g_wv.As(&w18)))
        w18->add_LaunchingExternalUriScheme(Callback<ICoreWebView2LaunchingExternalUriSchemeEventHandler>(
            [](ICoreWebView2*, ICoreWebView2LaunchingExternalUriSchemeEventArgs* a) -> HRESULT {
                a->put_Cancel(TRUE); return S_OK;
            }).Get(), &tk);
    g_wv->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    g_wv->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(   // ad / tracker blocking + private-network protection
        [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* a) -> HRESULT {
            ComPtr<ICoreWebView2WebResourceRequest> req;
            a->get_Request(&req);
            if (!req) return S_OK;
            PWSTR uri = nullptr;
            req->get_Uri(&uri);
            bool blk = false;
            if (uri) {
                wstring host = HostOf(uri);
                blk = HostBlocked(host);
                if (!blk && IsPrivateHost(host) && g_wv) {   // a public site may not poke at your PC or home network
                    PWSTR src = nullptr;
                    if (SUCCEEDED(g_wv->get_Source(&src)) && src) {
                        wstring ph = HostOf(src);
                        CoTaskMemFree(src);
                        if (!ph.empty() && !IsPrivateHost(ph)) blk = true;
                    }
                }
                CoTaskMemFree(uri);
            }
            if (blk && g_wvEnv) {
                ComPtr<ICoreWebView2WebResourceResponse> resp;
                if (SUCCEEDED(g_wvEnv->CreateWebResourceResponse(nullptr, 403, L"Blocked", L"", &resp))) a->put_Response(resp.Get());
            }
            return S_OK;
        }).Get(), &tk);
    g_wvCtl->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
        [](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* a) -> HRESULT {
            COREWEBVIEW2_KEY_EVENT_KIND kind;
            UINT vk = 0;
            a->get_KeyEventKind(&kind);
            a->get_VirtualKey(&vk);
            if ((kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) && g_ww) {
                bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
                UINT cmd = 0;
                if (vk == VK_ESCAPE || (ctrl && vk == 'W')) cmd = WM_CLOSE;
                else if (ctrl && vk == 'L') cmd = WM_WEB_PALETTE;   // new address: opens the palette with "w "
                else if (alt && vk == VK_LEFT) cmd = WM_WEB_BACK;
                else if (alt && vk == VK_RIGHT) cmd = WM_WEB_FWD;
                else if (vk == VK_F5 || (ctrl && vk == 'R')) cmd = WM_WEB_RELOAD;
                else if (vk == VK_F11) cmd = WM_WEB_FULL;
                if (cmd) { PostMessageW(g_ww, cmd, 0, 0); a->put_Handled(TRUE); }
            }
            return S_OK;
        }).Get(), &tk);

    // optional extensions (e.g. uBlock Origin Lite), loaded before the first page
    if (!g_webExt.empty() && p7) {
        g_extPending = (int)g_webExt.size();
        for (auto& e : g_webExt) {
            wstring path = ExpandEnv(e);
            HRESULT hr = p7->AddBrowserExtension(path.c_str(), Callback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
                [](HRESULT, ICoreWebView2BrowserExtension*) -> HRESULT {
                    if (--g_extPending <= 0) WebFinishSetup();
                    return S_OK;
                }).Get());
            if (FAILED(hr) && --g_extPending <= 0) WebFinishSetup();
        }
        return;
    }
    WebFinishSetup();
}

static void WebCreateController() {
    if (!g_wvEnv || !g_ww) { g_webCreating = false; return; }
    auto handler = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [](HRESULT r, ICoreWebView2Controller* ctl) -> HRESULT {
            if (!g_ww) { g_webCreating = false; if (ctl) ctl->Close(); return S_OK; }   // window closed meanwhile
            if (FAILED(r) || !ctl) { g_webCreating = false; Balloon(L"Quick Palette", L"The browser could not start.", NIIF_WARNING); return S_OK; }
            g_wvCtl = ctl;
            WebSetup();
            return S_OK;
        });
    ComPtr<ICoreWebView2Environment10> e10;   // private (in-memory) profile: nothing is ever written to disk
    if (g_webPrivate && g_webExt.empty() && SUCCEEDED(g_wvEnv.As(&e10))) {
        ComPtr<ICoreWebView2ControllerOptions> co;
        if (SUCCEEDED(e10->CreateCoreWebView2ControllerOptions(&co)) && co) {
            co->put_IsInPrivateModeEnabled(TRUE);
            if (SUCCEEDED(e10->CreateCoreWebView2ControllerWithOptions(g_ww, co.Get(), handler.Get()))) return;
        }
    }
    g_wvEnv->CreateCoreWebView2Controller(g_ww, handler.Get());
}
static void WebCreate() {
    if (g_wvCtl && g_webReady) return;
    if (g_webCreating) return;   // engine or controller already on its way
    g_webCreating = true;
    if (g_wvEnv) { WebCreateController(); return; }
    wstring udf = g_dir + L"\\web";
    CreateDirectoryW(udf.c_str(), nullptr);
    auto opts = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    wstring args = L"--disable-background-networking --disable-sync --disable-breakpad --disable-component-update --no-first-run "
                   L"--disable-default-apps --disable-domain-reliability --disable-features=Translate,OptimizationHints,MediaRouter --disk-cache-size=33554432";
    if (g_webLowRam) args += L" --renderer-process-limit=2 --enable-features=NetworkServiceInProcess --js-flags=--max-old-space-size=512";
    opts->put_AdditionalBrowserArguments(args.c_str());
    if (!g_webExt.empty()) opts->put_AreBrowserExtensionsEnabled(TRUE);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, udf.c_str(), opts.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT r, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(r) || !env) { g_webCreating = false; return S_OK; }
                g_wvEnv = env;
                WebCreateController();
                return S_OK;
            }).Get());
    if (FAILED(hr)) {
        g_webCreating = false;
        if (g_webShown) Balloon(L"Quick Palette", L"The Microsoft Edge WebView2 Runtime was not found. Install it to use the built-in browser.", NIIF_WARNING);
    }
}
static void WebTeardown() {
    g_webReady = false; g_webCreating = false;
    if (g_wvCtl) { g_wvCtl->Close(); g_wvCtl.Reset(); }
    g_wv.Reset(); g_wvEnv.Reset();
    g_webPending.clear();
}
static void WebSetVisible(bool v) { if (g_wvCtl) g_wvCtl->put_IsVisible(v ? TRUE : FALSE); }
static void WebBlank() { if (g_wv && g_webReady) g_wv->Navigate(L"about:blank"); g_webPending.clear(); }
#else
static void WebResize() {}
static void WebFocus() {}
static void WebNavigate(const wstring&) {}
static void WebBack() {}
static void WebFwd() {}
static void WebReload() {}
static void WebCreate() { Balloon(L"Quick Palette", L"This build has no browser (the build step could not get the WebView2 SDK).", NIIF_WARNING); }
static void WebTeardown() {}
static void WebSetVisible(bool) {}
static void WebBlank() {}
#endif

// ------------------------------------------------------------------ the window
static void WebEnsure() {   // called as soon as you type "w ": window stays hidden, the engine starts in the background
    if (!g_ww && !MakeToolWindow(&g_ww, L"Browser", g_webW, g_webH)) return;
    WebCreate();
    SetTimer(g_hwnd, T_WEBFREE, 20000, nullptr);   // never used? let it go again
}
static void WebOpen(const wstring& text) {   // Enter: show the page (same window if it is already open: no second browser, no reload)
    WebEnsure();
    if (!g_ww) return;
    KillTimer(g_hwnd, T_WEBFREE); KillTimer(g_hwnd, T_WIPE);
    if (!g_webShown) {
        POINT pt; GetCursorPos(&pt);
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(mon, &mi);
        int ww = (int)(mi.rcWork.right - mi.rcWork.left), wh = (int)(mi.rcWork.bottom - mi.rcWork.top);
        int w = std::min<int>(px(g_webW), ww - px(40)), h = std::min<int>(px(g_webH), wh - px(40));
        if (!IsZoomed(g_ww)) SetWindowPos(g_ww, nullptr, (int)mi.rcWork.left + (ww - w) / 2, (int)mi.rcWork.top + (wh - h) / 2, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        WebSetVisible(true);
        ShowWindow(g_ww, IsIconic(g_ww) ? SW_RESTORE : SW_SHOW);
        g_webShown = true;
    } else if (IsIconic(g_ww)) ShowWindow(g_ww, SW_RESTORE);
    SetForegroundWindow(g_ww);
    WebNavigate(text);
}
static void WebClose() {   // Esc / Ctrl+W
    if (!g_ww) return;
    WebBlank();                 // frees the page's memory right away
    WebSetVisible(false);
    ShowWindow(g_ww, SW_HIDE);
    g_webShown = false;
    SetTimer(g_hwnd, T_WEBFREE, (UINT)std::max(1, g_webKeepAlive) * 1000, nullptr);   // stay warm a little (instant reopen), then exit completely
}
static void WebFreeIfIdle() {
    if (g_webShown) return;
    if (g_ww) DestroyWindow(g_ww);
    WebTeardown();
    if (g_webPrivate) SetTimer(g_hwnd, T_WIPE, 4000, nullptr);
}

static LRESULT CALLBACK WebProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT out;
    if (ToolFrameMsg(h, msg, wp, lp, out)) return out;
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            FillRect(dc, &rc, Brush(g_th->bg));
            EndPaint(h, &ps);
            return 0;
        }
        case WM_NCHITTEST: return HTCLIENT;
        case WM_SIZE: if (wp != SIZE_MINIMIZED) WebResize(); return 0;
        case WM_DPICHANGED: {
            const RECT* r = (const RECT*)lp;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            SetDpi((int)LOWORD(wp), true);
            WebResize();
            return 0;
        }
        case WM_ACTIVATE: if (LOWORD(wp) != WA_INACTIVE) WebFocus(); return 0;
        case WM_WEB_BACK: WebBack(); return 0;
        case WM_WEB_FWD: WebFwd(); return 0;
        case WM_WEB_RELOAD: WebReload(); return 0;
        case WM_WEB_FULL: ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE); return 0;
        case WM_WEB_PALETTE: PostMessageW(g_hwnd, WM_PALETTE_PREFILL, 0, 0); return 0;
        case WM_WEB_NAVURL: { auto* u = (wstring*)lp; if (u) { WebNavigate(*u); delete u; } return 0; }
        case WM_CLOSE: WebClose(); return 0;
        case WM_DESTROY:
            WebTeardown();
            g_ww = nullptr; g_webShown = false;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
