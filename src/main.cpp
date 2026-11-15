#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <limits>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// ---------------- DATA MODELS (WEIGHTED GRAPH + DIJKSTRA) ----------------
struct SentMessage {
    int id;
    string fromCity;
    string toCity;
    string category;
    string text;
    string pathTaken;
    int totalTimeMs;
};

struct Connection {
    string targetCity;
    int timeMs;
    bool isBlocked = false;
};

// Normalized coordinates (0.0 to 1.0) so cities scale cleanly at any screen size
struct CityPos {
    double nx, ny;
};

struct PathStep {
    string city;
    int shortestTime;
    string cameFrom;
    bool partOfRoute;
};

class DeliveryNetwork {
public:
    unordered_map<string, vector<Connection>> adjList;
    unordered_map<string, CityPos> cityCoords;
    vector<string> cities;
    vector<string> categories = {"Order Update", "Payment Receipt", "Urgent Alert", "Parcel Tracking"};

    vector<SentMessage> history;
    vector<string> activeRoute;
    vector<PathStep> lastCalculationTable;

    int lastDeliveryTime = 0;
    int nextId = 1;

    DeliveryNetwork() {
        resetMap();
    }

    void resetMap() {
        adjList.clear();
        cityCoords.clear();
        cities.clear();
        activeRoute.clear();
        lastCalculationTable.clear();
        lastDeliveryTime = 0;

        // 9 Indian Cities laid out on a spacious, non-overlapping planar grid
        addCity("Chandigarh", 0.28, 0.08);
        addCity("Delhi",      0.47, 0.22);
        addCity("Lucknow",    0.75, 0.22);
        addCity("Jaipur",     0.19, 0.36);
        addCity("Ahmedabad",  0.14, 0.62);
        addCity("Kolkata",    0.86, 0.52);
        addCity("Mumbai",     0.23, 0.85);
        addCity("Hyderabad",  0.58, 0.66);
        addCity("Bengaluru",  0.48, 0.90);

        // 13 Non-crossing bidirectional connections with delivery times (ms)
        connectCities("Chandigarh", "Delhi",     10);
        connectCities("Chandigarh", "Jaipur",    24);
        connectCities("Delhi",      "Lucknow",   14);
        connectCities("Delhi",      "Jaipur",    15);
        connectCities("Delhi",      "Hyderabad", 32);
        connectCities("Lucknow",    "Kolkata",   22);
        connectCities("Lucknow",    "Hyderabad", 26);
        connectCities("Jaipur",     "Ahmedabad", 18);
        connectCities("Ahmedabad",  "Mumbai",    14);
        connectCities("Mumbai",     "Hyderabad", 22);
        connectCities("Mumbai",     "Bengaluru", 24);
        connectCities("Hyderabad",  "Bengaluru", 12);
        connectCities("Hyderabad",  "Kolkata",   28);
    }

    void addCity(const string& name, double nx, double ny) {
        if (cityCoords.find(name) == cityCoords.end()) {
            cityCoords[name] = {nx, ny};
            adjList[name] = {};
            cities.push_back(name);
        }
    }

    bool hasDirectLink(const string& a, const string& b) const {
        auto it = adjList.find(a);
        if (it == adjList.end()) return false;
        for (const auto& c : it->second) {
            if (c.targetCity == b) return true;
        }
        return false;
    }

    void connectCities(const string& a, const string& b, int timeMs) {
        if (a == b) return;
        for (auto& conn : adjList[a]) {
            if (conn.targetCity == b) {
                conn.timeMs = timeMs;
                conn.isBlocked = false;
                for (auto& conn2 : adjList[b]) {
                    if (conn2.targetCity == a) {
                        conn2.timeMs = timeMs;
                        conn2.isBlocked = false;
                    }
                }
                return;
            }
        }
        adjList[a].push_back({b, timeMs, false});
        adjList[b].push_back({a, timeMs, false});
    }

    int toggleBlock(const string& a, const string& b) {
        bool found = false, blockedState = false;
        for (auto& conn : adjList[a]) {
            if (conn.targetCity == b) {
                conn.isBlocked = !conn.isBlocked;
                blockedState = conn.isBlocked;
                found = true;
            }
        }
        for (auto& conn : adjList[b]) {
            if (conn.targetCity == a) conn.isBlocked = blockedState;
        }
        if (!found) return -1;
        return blockedState ? 1 : 0;
    }

    int countBlockedConnections() const {
        int count = 0;
        for (const auto& pair : adjList) {
            for (const auto& conn : pair.second) {
                if (pair.first < conn.targetCity && conn.isBlocked) count++;
            }
        }
        return count;
    }

    // Dijkstra's Shortest Path Algorithm
    pair<int, vector<string>> findFastestRoute(const string& startCity, const string& endCity) {
        unordered_map<string, int> dist;
        unordered_map<string, string> parent;

        for (const auto& c : cities) {
            dist[c] = numeric_limits<int>::max();
            parent[c] = "-";
        }

        priority_queue<pair<int, string>, vector<pair<int, string>>, greater<>> pq;
        dist[startCity] = 0;
        parent[startCity] = "START";
        pq.push({0, startCity});

        while (!pq.empty()) {
            int currentDist = pq.top().first;
            string u = pq.top().second;
            pq.pop();

            if (currentDist > dist[u]) continue;

            for (const auto& conn : adjList[u]) {
                if (conn.isBlocked) continue;
                string v = conn.targetCity;
                if (dist[u] != numeric_limits<int>::max() && dist[u] + conn.timeMs < dist[v]) {
                    dist[v] = dist[u] + conn.timeMs;
                    parent[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }

        vector<string> path;
        if (dist[endCity] != numeric_limits<int>::max()) {
            for (string curr = endCity; curr != "START" && curr != "-"; curr = parent[curr]) {
                path.push_back(curr);
                if (curr == startCity) break;
            }
            reverse(path.begin(), path.end());
        }

        lastCalculationTable.clear();
        for (const auto& c : cities) {
            bool used = find(path.begin(), path.end(), c) != path.end();
            lastCalculationTable.push_back({c, dist[c], parent[c], used});
        }

        if (path.empty()) return {-1, {}};
        return {dist[endCity], path};
    }

    string sendMessage(const string& from, const string& to, const string& category, const string& text) {
        if (from == to) {
            activeRoute = {from};
            lastDeliveryTime = 0;
            int msgNum = nextId++;
            history.push_back({msgNum, from, to, category, text, from + " (Same City)", 0});
            return "[DELIVERED] Message #" + to_string(msgNum) + " arrived instantly inside " + from + " (0 ms).";
        }

        auto result = findFastestRoute(from, to);
        if (result.first == -1) {
            activeRoute.clear();
            return "[BLOCKED] Cannot reach " + to + " from " + from + " because all paths are blocked.\r\n"
                   "Tip: Unblock a line or click 'Reset Map Back to Default'.";
        }

        activeRoute = result.second;
        lastDeliveryTime = result.first;

        string pathStr = "";
        for (size_t i = 0; i < result.second.size(); ++i) {
            pathStr += result.second[i] + (i + 1 < result.second.size() ? " -> " : "");
        }

        int msgNum = nextId++;
        history.push_back({msgNum, from, to, category, text, pathStr, result.first});

        return "[DELIVERED] Message #" + to_string(msgNum) + " reached " + to + " in " + to_string(result.first) + " ms (" + to_string(result.second.size() - 1) + " hops).\r\n"
               "Fastest Path: " + pathStr;
    }
};

// ---------------- GUI HANDLES ----------------
DeliveryNetwork g_net;
HWND hTab, hMapCanvas, hResultBox;
HWND hCardLbl[4], hCardVal[4];

// Tab 0 Controls
vector<HWND> tab0Controls;
HWND gSend, lFrom, hFromCombo, lTo, hToCombo, lCat, hCatCombo, lMsg, hMsgEdit, hSendBtn, hDemoBtn;
HWND gTest, lPick, hCityACombo, hCityBCombo, hBlockBtn, lSpeed, hSpeedEdit, hSpeedBtn, hResetBtn;

// Tab 1 Controls
vector<HWND> tab1Controls;
HWND hHistoryBox, hRefreshBtn, hClearHistBtn;

HFONT hFontTitle, hFontSub, hFontCardVal, hFontCardLbl, hFontBold, hFontNormal, hFontMono;
HBRUSH hBrushBg, hBrushCard;

#define ID_TAB        100
#define ID_SEND_BTN   101
#define ID_DEMO_BTN   102
#define ID_BLOCK_BTN  103
#define ID_SPEED_BTN  104
#define ID_RESET_BTN  105
#define ID_REFRESH    106
#define ID_CLEAR_HIST 107

wstring toW(const string& s) {
    return wstring(s.begin(), s.end());
}

string fromW(const wstring& ws) {
    return string(ws.begin(), ws.end());
}

bool isPartOfActiveRoute(const string& a, const string& b) {
    const auto& r = g_net.activeRoute;
    for (size_t i = 0; i + 1 < r.size(); ++i) {
        if ((r[i] == a && r[i + 1] == b) || (r[i] == b && r[i + 1] == a)) return true;
    }
    return false;
}

void updateSummaryCards() {
    SetWindowText(hCardVal[0], (to_wstring(g_net.cities.size()) + L" Cities").c_str());
    SetWindowText(hCardVal[1], (to_wstring(g_net.history.size()) + L" Delivered").c_str());
    SetWindowText(hCardVal[2], (g_net.history.empty() ? L"-- ms" : (to_wstring(g_net.lastDeliveryTime) + L" ms")).c_str());
    SetWindowText(hCardVal[3], (to_wstring(g_net.countBlockedConnections()) + L" Blocked").c_str());
}

// ---------------- RESPONSIVE FULL-SCREEN LAYOUT ENGINE ----------------
void LayoutControls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int winW = max(1060, (int)(rc.right - rc.left));
    int winH = max(680, (int)(rc.bottom - rc.top));

    // 1. Top 4 Summary Cards
    int cardGap = 16;
    int marginX = 22;
    int cardW = (winW - 2 * marginX - 3 * cardGap) / 4;
    for (int i = 0; i < 4; ++i) {
        int cx = marginX + i * (cardW + cardGap);
        MoveWindow(hCardLbl[i], cx + 8, 86, cardW - 16, 16, TRUE);
        MoveWindow(hCardVal[i], cx + 8, 105, cardW - 16, 24, TRUE);
    }

    // 2. Main Tab Control
    int tabY = 148;
    int tabW = winW - 2 * marginX;
    int tabH = winH - tabY - 18;
    MoveWindow(hTab, marginX, tabY, tabW, tabH, TRUE);

    // 3. Tab 0 Layout (Left Map Canvas + Right Control Panels)
    int innerPad = 14;
    int contentTop = tabY + 36;
    int contentBottom = tabY + tabH - innerPad;
    int rightPanelW = 450;
    int rightX = marginX + tabW - innerPad - rightPanelW;

    int mapX = marginX + innerPad;
    int mapW = rightX - mapX - 18;
    int statusH = 44;
    int mapH = (contentBottom - contentTop) - statusH - 10;

    MoveWindow(hMapCanvas, mapX, contentTop, mapW, mapH, TRUE);
    MoveWindow(hResultBox, mapX, contentTop + mapH + 10, mapW, statusH, TRUE);

    // Right Panel Box 1: Send a Message
    int box1H = 248;
    int rx = rightX + 16;
    int rw = rightPanelW - 32;
    int labelW = 105;
    int inputX = rx + labelW + 8;
    int inputW = rw - labelW - 8;

    MoveWindow(gSend, rightX, contentTop - 2, rightPanelW, box1H, TRUE);
    MoveWindow(lFrom, rx, contentTop + 30, labelW, 20, TRUE);
    MoveWindow(hFromCombo, inputX, contentTop + 26, inputW, 260, TRUE);

    MoveWindow(lTo, rx, contentTop + 66, labelW, 20, TRUE);
    MoveWindow(hToCombo, inputX, contentTop + 62, inputW, 260, TRUE);

    MoveWindow(lCat, rx, contentTop + 102, labelW, 20, TRUE);
    MoveWindow(hCatCombo, inputX, contentTop + 98, inputW, 200, TRUE);

    MoveWindow(lMsg, rx, contentTop + 138, labelW, 20, TRUE);
    MoveWindow(hMsgEdit, inputX, contentTop + 134, inputW, 26, TRUE);

    MoveWindow(hSendBtn, rx, contentTop + 172, rw, 34, TRUE);
    MoveWindow(hDemoBtn, rx, contentTop + 212, rw, 28, TRUE);

    // Right Panel Box 2: Roadblocks & Connections
    int box2Y = contentTop + box1H + 10;
    int box2H = contentBottom - box2Y;
    MoveWindow(gTest, rightX, box2Y, rightPanelW, box2H, TRUE);

    int halfComboW = (inputW - 10) / 2;
    MoveWindow(lPick, rx, box2Y + 34, labelW, 20, TRUE);
    MoveWindow(hCityACombo, inputX, box2Y + 30, halfComboW, 260, TRUE);
    MoveWindow(hCityBCombo, inputX + halfComboW + 10, box2Y + 30, halfComboW, 260, TRUE);

    MoveWindow(hBlockBtn, rx, box2Y + 68, rw, 34, TRUE);

    MoveWindow(lSpeed, rx, box2Y + 116, labelW, 20, TRUE);
    MoveWindow(hSpeedEdit, inputX, box2Y + 112, 85, 26, TRUE);
    MoveWindow(hSpeedBtn, inputX + 95, box2Y + 111, inputW - 95, 28, TRUE);

    MoveWindow(hResetBtn, rx, box2Y + 154, rw, 34, TRUE);

    // 4. Tab 1 Layout (History & Calculation Table)
    int histW = tabW - 2 * innerPad;
    int histH = (contentBottom - contentTop) - 44;
    MoveWindow(hHistoryBox, mapX, contentTop, histW, histH, TRUE);
    MoveWindow(hRefreshBtn, mapX, contentTop + histH + 10, 165, 34, TRUE);
    MoveWindow(hClearHistBtn, mapX + 178, contentTop + histH + 10, 195, 34, TRUE);
}

// ---------------- DOUBLE-BUFFERED VISUAL CITY MAP ----------------
LRESULT CALLBACK MapCanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1; // Prevent flicker
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);
        int w = max(1, (int)(rect.right - rect.left));
        int h = max(1, (int)(rect.bottom - rect.top));

        HDC hdc = CreateCompatibleDC(hdcScreen);
        HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdc, hbm);

        // Clean white background
        HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        // Subtle dot matrix grid
        for (int gx = 28; gx < w - 12; gx += 32) {
            for (int gy = 42; gy < h - 38; gy += 32) {
                SetPixel(hdc, gx, gy, RGB(226, 232, 240));
            }
        }

        // Card border
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(203, 213, 225));
        SelectObject(hdc, borderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, w, h);
        DeleteObject(borderPen);

        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, hFontBold);
        SetTextColor(hdc, RGB(30, 41, 59));
        wstring mapHeading = L"LIVE CITY NETWORK MAP  (Numbers show delivery time in ms)";
        TextOut(hdc, 16, 12, mapHeading.c_str(), (int)mapHeading.length());

        // Safe inner plotting area so cities never touch borders or legend
        int padX = 72;
        int topY = 48;
        int bottomY = h - 52;
        int plotW = max(100, w - 2 * padX);
        int plotH = max(100, bottomY - topY);

        auto getPixelPos = [&](const string& city) -> POINT {
            CityPos cp = g_net.cityCoords[city];
            return {padX + (LONG)(cp.nx * plotW), topY + (LONG)(cp.ny * plotH)};
        };

        // 1. Draw Connections (Lines)
        for (const auto& pair : g_net.adjList) {
            string a = pair.first;
            POINT p1 = getPixelPos(a);

            for (const auto& conn : pair.second) {
                string b = conn.targetCity;
                if (a > b) continue;
                POINT p2 = getPixelPos(b);

                bool highlighted = !conn.isBlocked && isPartOfActiveRoute(a, b);
                COLORREF lineCol = conn.isBlocked ? RGB(225, 29, 72) :
                                   (highlighted ? RGB(16, 185, 129) : RGB(148, 163, 184));
                int style = conn.isBlocked ? PS_DASH : PS_SOLID;
                int width = highlighted ? 4 : 2;

                if (highlighted) {
                    HPEN glowPen = CreatePen(PS_SOLID, 9, RGB(209, 250, 229));
                    SelectObject(hdc, glowPen);
                    MoveToEx(hdc, p1.x, p1.y, NULL);
                    LineTo(hdc, p2.x, p2.y);
                    DeleteObject(glowPen);
                }

                HPEN pen = CreatePen(style, width, lineCol);
                SelectObject(hdc, pen);
                MoveToEx(hdc, p1.x, p1.y, NULL);
                LineTo(hdc, p2.x, p2.y);
                DeleteObject(pen);

                // Time badge pill at midpoint
                int midX = (p1.x + p2.x) / 2;
                int midY = (p1.y + p2.y) / 2;
                wstring badgeText = conn.isBlocked ? L"BLOCKED" : (to_wstring(conn.timeMs) + L" ms");

                RECT badge = {midX - 28, midY - 10, midX + 28, midY + 10};
                HBRUSH badgeBg = CreateSolidBrush(conn.isBlocked ? RGB(255, 228, 230) :
                                                  (highlighted ? RGB(209, 250, 229) : RGB(248, 250, 252)));
                HPEN badgePen = CreatePen(PS_SOLID, 1, conn.isBlocked ? RGB(253, 164, 175) :
                                                       (highlighted ? RGB(52, 211, 153) : RGB(203, 213, 225)));
                SelectObject(hdc, badgeBg);
                SelectObject(hdc, badgePen);
                RoundRect(hdc, badge.left, badge.top, badge.right, badge.bottom, 8, 8);
                DeleteObject(badgeBg);
                DeleteObject(badgePen);

                SetTextColor(hdc, conn.isBlocked ? RGB(190, 18, 60) :
                                  (highlighted ? RGB(4, 120, 87) : RGB(51, 65, 85)));
                SelectObject(hdc, (highlighted || conn.isBlocked) ? hFontBold : hFontNormal);
                DrawText(hdc, badgeText.c_str(), -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // 2. Draw Cities (Rounded Pills with Drop Shadow)
        for (const auto& city : g_net.cities) {
            POINT pt = getPixelPos(city);
            bool inRoute = find(g_net.activeRoute.begin(), g_net.activeRoute.end(), city) != g_net.activeRoute.end();
            bool isStart = (!g_net.activeRoute.empty() && g_net.activeRoute.front() == city);
            bool isEnd   = (!g_net.activeRoute.empty() && g_net.activeRoute.back() == city);

            HBRUSH shadowBrush = CreateSolidBrush(RGB(226, 232, 240));
            SelectObject(hdc, shadowBrush);
            SelectObject(hdc, GetStockObject(NULL_PEN));
            RoundRect(hdc, pt.x - 48, pt.y - 16, pt.x + 52, pt.y + 22, 14, 14);
            DeleteObject(shadowBrush);

            COLORREF fill = (isStart || isEnd) ? RGB(16, 185, 129) :
                            (inRoute ? RGB(209, 250, 229) : RGB(239, 246, 255));
            COLORREF stroke = inRoute ? RGB(5, 150, 105) : RGB(59, 130, 246);
            COLORREF textCol = (isStart || isEnd) ? RGB(255, 255, 255) : RGB(15, 23, 42);

            HBRUSH nodeBrush = CreateSolidBrush(fill);
            HPEN nodePen = CreatePen(PS_SOLID, 2, stroke);
            SelectObject(hdc, nodeBrush);
            SelectObject(hdc, nodePen);
            RoundRect(hdc, pt.x - 50, pt.y - 19, pt.x + 50, pt.y + 19, 14, 14);
            DeleteObject(nodeBrush);
            DeleteObject(nodePen);

            SelectObject(hdc, hFontBold);
            SetTextColor(hdc, textCol);
            wstring wCity = toW(city);
            RECT txtRect = {pt.x - 48, pt.y - 18, pt.x + 48, pt.y + 18};
            DrawText(hdc, wCity.c_str(), -1, &txtRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // 3. Bottom Legend Strip
        RECT legRect = {1, h - 32, w - 1, h - 1};
        HBRUSH legBg = CreateSolidBrush(RGB(248, 250, 252));
        FillRect(hdc, &legRect, legBg);
        DeleteObject(legBg);

        HPEN legLine = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        SelectObject(hdc, legLine);
        MoveToEx(hdc, 1, h - 32, NULL);
        LineTo(hdc, w - 1, h - 32);
        DeleteObject(legLine);

        SelectObject(hdc, hFontNormal);
        SetTextColor(hdc, RGB(71, 85, 105));
        wstring legend = L"Guide:   [Solid Green] Start / End City    [Light Green] Fastest Route    [Blue] City Server    [Red Dashed] Blocked Line";
        TextOut(hdc, 16, h - 24, legend.c_str(), (int)legend.length());

        BitBlt(hdcScreen, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, hOldBm);
        DeleteObject(hbm);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void refreshHistoryTab() {
    wstringstream wss;
    wss << L"PART 1: ALL DELIVERED MESSAGES (" << g_net.history.size() << L" Total)\r\n";
    wss << left << setw(6) << L"NO." << setw(25) << L"FROM -> TO" << setw(18) << L"TYPE"
        << setw(10) << L"TIME" << setw(44) << L"FASTEST ROUTE TAKEN" << L"MESSAGE\r\n";
    wss << wstring(122, L'-') << L"\r\n";

    if (g_net.history.empty()) {
        wss << L"(No messages sent yet. Go to Tab 1 and click 'Send Message' or 'Quick Demo: Send 3 Sample Messages'.)\r\n";
    } else {
        for (const auto& m : g_net.history) {
            wss << left << setw(6) << (L"#" + to_wstring(m.id))
                << setw(25) << toW(m.fromCity + " -> " + m.toCity)
                << setw(18) << toW(m.category)
                << setw(10) << (to_wstring(m.totalTimeMs) + L" ms")
                << setw(44) << toW(m.pathTaken)
                << toW(m.text) << L"\r\n";
        }
    }

    wss << L"\r\n\r\nPART 2: HOW THE SHORTEST PATH WAS CALCULATED (DIJKSTRA'S ALGORITHM TABLE)\r\n";
    wss << left << setw(16) << L"CITY" << setw(22) << L"SHORTEST TIME"
        << setw(18) << L"CAME FROM" << L"PART OF FINAL ROUTE?\r\n";
    wss << wstring(80, L'-') << L"\r\n";

    if (g_net.lastCalculationTable.empty()) {
        wss << L"(Send a message in Tab 1 to see the shortest time calculated to every city.)\r\n";
    } else {
        for (const auto& row : g_net.lastCalculationTable) {
            wstring tStr = (row.shortestTime == numeric_limits<int>::max()) ? L"Unreachable" : (to_wstring(row.shortestTime) + L" ms");
            wss << left << setw(16) << toW(row.city)
                << setw(22) << tStr
                << setw(18) << toW(row.cameFrom)
                << (row.partOfRoute ? L"YES (Selected)" : L"No") << L"\r\n";
        }
    }
    SetWindowText(hHistoryBox, wss.str().c_str());
}

void switchTab(int idx) {
    for (HWND h : tab0Controls) ShowWindow(h, idx == 0 ? SW_SHOW : SW_HIDE);
    for (HWND h : tab1Controls) ShowWindow(h, idx == 1 ? SW_SHOW : SW_HIDE);

    if (idx == 0) {
        for (HWND h : tab0Controls) SetWindowPos(h, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        InvalidateRect(hMapCanvas, NULL, TRUE);
    } else {
        refreshHistoryTab();
        for (HWND h : tab1Controls) SetWindowPos(h, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void fillCities(HWND hCombo, int selectIdx) {
    for (const auto& c : g_net.cities) {
        wstring wc = toW(c);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)wc.c_str());
    }
    SendMessage(hCombo, CB_SETCURSEL, selectIdx, 0);
}

// ---------------- MAIN WINDOW PROCEDURE ----------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hBrushBg   = CreateSolidBrush(RGB(241, 245, 249));
            hBrushCard = CreateSolidBrush(RGB(255, 255, 255));

            hFontTitle   = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hFontSub     = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hFontCardVal = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hFontCardLbl = CreateFont(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hFontBold    = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hFontNormal  = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            hFontMono    = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

            WNDCLASS wcMap = {0};
            wcMap.lpfnWndProc = MapCanvasProc;
            wcMap.hInstance = GetModuleHandle(NULL);
            wcMap.lpszClassName = L"SwiftRouteMapV3";
            RegisterClass(&wcMap);

            // 4 Summary Cards
            const wchar_t* cardNames[4] = {L"ACTIVE CITIES", L"MESSAGES DELIVERED", L"LAST DELIVERY TIME", L"BLOCKED CONNECTIONS"};
            for (int i = 0; i < 4; ++i) {
                hCardLbl[i] = CreateWindow(L"STATIC", cardNames[i], WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 100, 16, hwnd, NULL, NULL, NULL);
                hCardVal[i] = CreateWindow(L"STATIC", L"-", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 100, 24, hwnd, NULL, NULL, NULL);
                SendMessage(hCardLbl[i], WM_SETFONT, (WPARAM)hFontCardLbl, TRUE);
                SendMessage(hCardVal[i], WM_SETFONT, (WPARAM)hFontCardVal, TRUE);
            }

            // Tab Control
            hTab = CreateWindow(WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, 0, 100, 100, hwnd, (HMENU)ID_TAB, NULL, NULL);
            SendMessage(hTab, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            TCITEM tie;
            tie.mask = TCIF_TEXT;
            tie.pszText = (LPWSTR)L"  1. Send Message & Live City Map  ";
            TabCtrl_InsertItem(hTab, 0, &tie);
            tie.pszText = (LPWSTR)L"  2. Delivery History & Fastest Path Table  ";
            TabCtrl_InsertItem(hTab, 1, &tie);

            // Tab 0 Controls
            hMapCanvas = CreateWindow(L"SwiftRouteMapV3", L"", WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hwnd, NULL, NULL, NULL);
            hResultBox = CreateWindow(L"EDIT", L"WELCOME! Pick a 'From City' and 'To City' on the right, then click 'Send Message' to watch the fastest route light up in green.", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY, 0, 0, 100, 40, hwnd, NULL, NULL, NULL);
            tab0Controls.insert(tab0Controls.end(), {hMapCanvas, hResultBox});

            gSend      = CreateWindow(L"BUTTON", L" STEP 1: Send a Message ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 100, 100, hwnd, NULL, NULL, NULL);
            lFrom      = CreateWindow(L"STATIC", L"From City:", WS_CHILD | WS_VISIBLE, 0, 0, 95, 20, hwnd, NULL, NULL, NULL);
            hFromCombo = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE, 0, 0, 100, 240, hwnd, NULL, NULL, NULL);
            lTo        = CreateWindow(L"STATIC", L"To City:", WS_CHILD | WS_VISIBLE, 0, 0, 95, 20, hwnd, NULL, NULL, NULL);
            hToCombo   = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE, 0, 0, 100, 240, hwnd, NULL, NULL, NULL);
            lCat       = CreateWindow(L"STATIC", L"Message Type:", WS_CHILD | WS_VISIBLE, 0, 0, 95, 20, hwnd, NULL, NULL, NULL);
            hCatCombo  = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE, 0, 0, 100, 180, hwnd, NULL, NULL, NULL);
            lMsg       = CreateWindow(L"STATIC", L"Your Message:", WS_CHILD | WS_VISIBLE, 0, 0, 95, 20, hwnd, NULL, NULL, NULL);
            hMsgEdit   = CreateWindow(L"EDIT", L"Order #101 Dispatched", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 0, 100, 24, hwnd, NULL, NULL, NULL);
            hSendBtn   = CreateWindow(L"BUTTON", L"Send Message (Find Fastest Route)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 34, hwnd, (HMENU)ID_SEND_BTN, NULL, NULL);
            hDemoBtn   = CreateWindow(L"BUTTON", L"Quick Demo: Send 3 Sample Messages", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 28, hwnd, (HMENU)ID_DEMO_BTN, NULL, NULL);

            tab0Controls.insert(tab0Controls.end(), {gSend, lFrom, hFromCombo, lTo, hToCombo, lCat, hCatCombo, lMsg, hMsgEdit, hSendBtn, hDemoBtn});

            gTest       = CreateWindow(L"BUTTON", L" STEP 2: Test Roadblocks or Add/Edit Connections ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 100, 100, hwnd, NULL, NULL, NULL);
            lPick       = CreateWindow(L"STATIC", L"Pick Two Cities:", WS_CHILD | WS_VISIBLE, 0, 0, 105, 20, hwnd, NULL, NULL, NULL);
            hCityACombo = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE, 0, 0, 100, 240, hwnd, NULL, NULL, NULL);
            hCityBCombo = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE, 0, 0, 100, 240, hwnd, NULL, NULL, NULL);
            hBlockBtn   = CreateWindow(L"BUTTON", L"Block / Unblock Line Between These Cities", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32, hwnd, (HMENU)ID_BLOCK_BTN, NULL, NULL);
            lSpeed      = CreateWindow(L"STATIC", L"Speed (ms):", WS_CHILD | WS_VISIBLE, 0, 0, 85, 20, hwnd, NULL, NULL, NULL);
            hSpeedEdit  = CreateWindow(L"EDIT", L"15", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 0, 0, 80, 24, hwnd, NULL, NULL, NULL);
            hSpeedBtn   = CreateWindow(L"BUTTON", L"Set Speed / Connect Cities", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 26, hwnd, (HMENU)ID_SPEED_BTN, NULL, NULL);
            hResetBtn   = CreateWindow(L"BUTTON", L"Reset Map Back to Default", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32, hwnd, (HMENU)ID_RESET_BTN, NULL, NULL);

            tab0Controls.insert(tab0Controls.end(), {gTest, lPick, hCityACombo, hCityBCombo, hBlockBtn, lSpeed, hSpeedEdit, hSpeedBtn, hResetBtn});

            // Tab 1 Controls
            hHistoryBox   = CreateWindow(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 0, 0, 100, 100, hwnd, NULL, NULL, NULL);
            hRefreshBtn   = CreateWindow(L"BUTTON", L"Refresh List", WS_CHILD | BS_PUSHBUTTON, 0, 0, 160, 32, hwnd, (HMENU)ID_REFRESH, NULL, NULL);
            hClearHistBtn = CreateWindow(L"BUTTON", L"Clear Delivery History", WS_CHILD | BS_PUSHBUTTON, 0, 0, 190, 32, hwnd, (HMENU)ID_CLEAR_HIST, NULL, NULL);
            tab1Controls.insert(tab1Controls.end(), {hHistoryBox, hRefreshBtn, hClearHistBtn});

            // Populate Dropdowns (0=Chandigarh, 8=Bengaluru, 7=Hyderabad)
            fillCities(hFromCombo, 0);
            fillCities(hToCombo, 8);
            fillCities(hCityACombo, 7);
            fillCities(hCityBCombo, 8);

            for (const auto& cat : g_net.categories) {
                wstring wCat = toW(cat);
                SendMessage(hCatCombo, CB_ADDSTRING, 0, (LPARAM)wCat.c_str());
            }
            SendMessage(hCatCombo, CB_SETCURSEL, 0, 0);

            for (HWND h : tab0Controls) SendMessage(h, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            for (HWND h : tab1Controls) SendMessage(h, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

            SendMessage(gSend, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            SendMessage(gTest, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            SendMessage(hSendBtn, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            SendMessage(hResultBox, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            SendMessage(hHistoryBox, WM_SETFONT, (WPARAM)hFontMono, TRUE);

            updateSummaryCards();
            LayoutControls(hwnd);
            switchTab(0);
            break;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 1080;
            mmi->ptMinTrackSize.y = 690;
            return 0;
        }

        case WM_SIZE: {
            LayoutControls(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            InvalidateRect(hMapCanvas, NULL, TRUE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT winRect;
            GetClientRect(hwnd, &winRect);
            int winW = winRect.right - winRect.left;
            FillRect(hdc, &winRect, hBrushBg);

            // Top Dark Navy Header Banner
            RECT banner = {0, 0, winW, 68};
            HBRUSH navBrush = CreateSolidBrush(RGB(15, 23, 42));
            FillRect(hdc, &banner, navBrush);
            DeleteObject(navBrush);

            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, hFontTitle);
            SetTextColor(hdc, RGB(255, 255, 255));
            wstring title = L"SwiftRoute   |   Smart City-to-City Message Delivery System";
            TextOut(hdc, 22, 11, title.c_str(), (int)title.length());

            SelectObject(hdc, hFontSub);
            SetTextColor(hdc, RGB(148, 163, 184));
            wstring sub = L"Finds the fastest route (lowest time in ms) between cities. If a path is blocked, it automatically picks the best backup route.";
            TextOut(hdc, 22, 39, sub.c_str(), (int)sub.length());

            // 4 Responsive Summary Cards
            COLORREF accents[4] = {RGB(59, 130, 246), RGB(16, 185, 129), RGB(245, 158, 11), RGB(225, 29, 72)};
            int cardGap = 16;
            int marginX = 22;
            int cardW = (winW - 2 * marginX - 3 * cardGap) / 4;

            for (int i = 0; i < 4; ++i) {
                int x = marginX + i * (cardW + cardGap);
                RECT card = {x, 78, x + cardW, 136};
                HPEN cardPen = CreatePen(PS_SOLID, 1, RGB(203, 213, 225));
                SelectObject(hdc, hBrushCard);
                SelectObject(hdc, cardPen);
                RoundRect(hdc, card.left, card.top, card.right, card.bottom, 8, 8);
                DeleteObject(cardPen);

                RECT topStrip = {x + 1, 79, x + cardW - 1, 83};
                HBRUSH accBrush = CreateSolidBrush(accents[i]);
                FillRect(hdc, &topStrip, accBrush);
                DeleteObject(accBrush);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            for (int i = 0; i < 4; ++i) {
                if (hCtrl == hCardVal[i] || hCtrl == hCardLbl[i]) {
                    SetBkColor(hdcStatic, RGB(255, 255, 255));
                    SetTextColor(hdcStatic, hCtrl == hCardVal[i] ? RGB(15, 23, 42) : RGB(100, 116, 139));
                    return (INT_PTR)hBrushCard;
                }
            }
            SetBkMode(hdcStatic, TRANSPARENT);
            SetTextColor(hdcStatic, RGB(51, 65, 85));
            return (INT_PTR)hBrushBg;
        }

        case WM_NOTIFY: {
            LPNMHDR lpnmhdr = (LPNMHDR)lParam;
            if (lpnmhdr->idFrom == ID_TAB && lpnmhdr->code == TCN_SELCHANGE) {
                switchTab(TabCtrl_GetCurSel(hTab));
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            wchar_t b1[64], b2[64], cBuf[64], mBuf[256], sBuf[32];

            if (id == ID_SEND_BTN) {
                GetWindowText(hFromCombo, b1, 64);
                GetWindowText(hToCombo, b2, 64);
                GetWindowText(hCatCombo, cBuf, 64);
                GetWindowText(hMsgEdit, mBuf, 256);

                string res = g_net.sendMessage(fromW(b1), fromW(b2), fromW(cBuf), fromW(mBuf));
                SetWindowText(hResultBox, toW(res).c_str());
                updateSummaryCards();
                InvalidateRect(hMapCanvas, NULL, TRUE);

            } else if (id == ID_DEMO_BTN) {
                g_net.sendMessage("Chandigarh", "Bengaluru", "Order Update", "Order #101: Laptop Shipped");
                g_net.sendMessage("Jaipur", "Kolkata", "Payment Receipt", "Payment #552: Rs 4,200 Paid");
                string last = g_net.sendMessage("Ahmedabad", "Lucknow", "Parcel Tracking", "Parcel #889: Out for Delivery");
                SetWindowText(hResultBox, toW("[DEMO] Sent 3 sample messages across India! Latest -> " + last).c_str());
                updateSummaryCards();
                InvalidateRect(hMapCanvas, NULL, TRUE);

            } else if (id == ID_BLOCK_BTN) {
                GetWindowText(hCityACombo, b1, 64);
                GetWindowText(hCityBCombo, b2, 64);
                string a = fromW(b1), b = fromW(b2);
                int status = g_net.toggleBlock(a, b);
                if (status == -1) {
                    SetWindowText(hResultBox, toW("[NOTE] " + a + " and " + b + " are not directly connected. Click 'Set Speed / Connect Cities' to link them first!").c_str());
                } else if (status == 1) {
                    SetWindowText(hResultBox, toW("[BLOCKED] Line between " + a + " and " + b + " is now broken! Click 'Send Message' to see the backup path.").c_str());
                } else {
                    SetWindowText(hResultBox, toW("[OPENED] Line between " + a + " and " + b + " is working again.").c_str());
                }
                updateSummaryCards();
                InvalidateRect(hMapCanvas, NULL, TRUE);

            } else if (id == ID_SPEED_BTN) {
                GetWindowText(hCityACombo, b1, 64);
                GetWindowText(hCityBCombo, b2, 64);
                GetWindowText(hSpeedEdit, sBuf, 32);
                string a = fromW(b1), b = fromW(b2);
                int ms = max(1, _wtoi(sBuf));
                if (a != b) {
                    bool existed = g_net.hasDirectLink(a, b);
                    g_net.connectCities(a, b, ms);
                    string msg = existed ? ("[UPDATED] Speed between " + a + " and " + b + " set to " + to_string(ms) + " ms.")
                                         : ("[NEW LINK] Connected " + a + " and " + b + " directly (" + to_string(ms) + " ms).");
                    SetWindowText(hResultBox, toW(msg).c_str());
                    updateSummaryCards();
                    InvalidateRect(hMapCanvas, NULL, TRUE);
                }

            } else if (id == ID_RESET_BTN) {
                g_net.resetMap();
                SetWindowText(hResultBox, L"[RESET] Map restored to default 9 cities and 13 open connections.");
                updateSummaryCards();
                InvalidateRect(hMapCanvas, NULL, TRUE);

            } else if (id == ID_REFRESH) {
                refreshHistoryTab();

            } else if (id == ID_CLEAR_HIST) {
                g_net.history.clear();
                g_net.nextId = 1;
                g_net.lastDeliveryTime = 0;
                updateSummaryCards();
                refreshHistoryTab();
            }
            break;
        }

        case WM_DESTROY:
            DeleteObject(hBrushBg);
            DeleteObject(hBrushCard);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES};
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"SwiftRouteV3Window";
    RegisterClass(&wc);

    // WS_OVERLAPPEDWINDOW enables Maximize, Minimize, and smooth resizing
    HWND hwnd = CreateWindow(L"SwiftRouteV3Window", L"SwiftRoute — Smart City-to-City Message Delivery System (ADS Project)",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1280, 760, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
