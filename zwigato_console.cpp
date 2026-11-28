/**
 * ZWIGATO - Hyperlocal Food Delivery Web Application (Pantnagar, India)
 * Backend & DSA Engine in C++17 + Embedded Editorial Frontend (HTML/CSS/JS)
 *
 * DSA Components Implemented From Scratch:
 *  1. Hash Table (Separate Chaining) -> O(1) Auth & Session Lookup
 *  2. Trie (Prefix Tree)              -> O(L) Dish & Restaurant Search
 *  3. Singly Linked List              -> Dynamic Shopping Cart
 *  4. Stack (LIFO)                    -> Recent Searches & Browsing History
 *  5. Queue (FIFO)                    -> Live Order Dispatch Pipeline
 *  6. Min-Heap (Priority Queue)       -> Nearest Rider Assignment
 *  7. Weighted Graph + Dijkstra       -> Pantnagar Shortest Delivery Route
 *  8. Merge Sort                      -> O(N log N) Restaurant Ranking
 *
 * Compile & Run:
 *   Linux/macOS: g++ -std=c++17 -O2 zwigato_server.cpp -o zwigato && ./zwigato
 *   Windows:     g++ -std=c++17 -O2 zwigato_server.cpp -lws2_32 -o zwigato.exe && zwigato.exe
 *   Open Browser: http://localhost:8080
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  typedef int socket_t;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define closesocket close
#endif

using namespace std;

// ============================================================================
// 1. DOMAIN MODELS (Arrays / Structs)
// ============================================================================
struct Dish {
    int id;
    int restaurantId;
    string name;
    string category;
    int price;
    bool isVeg;
    bool isHealthy;
    int calories;
    string nutritionTag;
    string img;
};

struct Restaurant {
    int id;
    string name;
    string cuisine;
    string location;
    double rating;
    int etaMins;
    bool noPackagingFee;
    string offerBadge;
    string coverImg;
    int graphNodeId; // Node in Pantnagar Graph
};

struct Rider {
    int id;
    string name;
    double rating;
    int deliveries;
    string vehicle;
    string plate;
    double distanceKm; // Priority key for Min-Heap
};

// ============================================================================
// 2. DSA #1: HASH TABLE WITH SEPARATE CHAINING (O(1) User Auth)
// ============================================================================
struct UserNode {
    string email;
    string password;
    string role; // "customer" or "partner"
    string avatar;
    UserNode* next;
    UserNode(string e, string p, string r, string a)
        : email(e), password(p), role(r), avatar(a), next(nullptr) {}
};

class UserHashTable {
private:
    static const int BUCKETS = 31;
    UserNode* table[BUCKETS];

    int hashFn(const string& key) const {
        unsigned long h = 5381;
        for (char c : key) h = ((h << 5) + h) + static_cast<unsigned char>(c);
        return h % BUCKETS;
    }

public:
    UserHashTable() {
        for (int i = 0; i < BUCKETS; ++i) table[i] = nullptr;
    }

    void insert(const string& email, const string& pass, const string& role, const string& avatar = "🍛") {
        int idx = hashFn(email);
        UserNode* curr = table[idx];
        while (curr) {
            if (curr->email == email) {
                curr->password = pass;
                curr->role = role;
                curr->avatar = avatar;
                return;
            }
            curr = curr->next;
        }
        UserNode* node = new UserNode(email, pass, role, avatar);
        node->next = table[idx];
        table[idx] = node;
    }

    UserNode* find(const string& email) const {
        int idx = hashFn(email);
        UserNode* curr = table[idx];
        while (curr) {
            if (curr->email == email) return curr;
            curr = curr->next;
        }
        return nullptr;
    }
};

// ============================================================================
// 3. DSA #2: TRIE / PREFIX TREE (O(L) Dish & Restaurant Search)
// ============================================================================
struct TrieNode {
    TrieNode* children[36]; // a-z (0..25), 0-9 (26..35)
    vector<int> dishIds;
    TrieNode() {
        for (int i = 0; i < 36; ++i) children[i] = nullptr;
    }
};

class SearchTrie {
private:
    TrieNode* root;

    int charIndex(char c) const {
        if (c >= 'a' && c <= 'z') return c - 'a';
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= '0' && c <= '9') return 26 + (c - '0');
        return -1;
    }

    void collectAll(TrieNode* node, vector<int>& out) const {
        if (!node) return;
        for (int id : node->dishIds) {
            if (find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
        }
        for (int i = 0; i < 36; ++i) {
            if (node->children[i]) collectAll(node->children[i], out);
        }
    }

public:
    SearchTrie() { root = new TrieNode(); }

    void insertWord(const string& word, int dishId) {
        TrieNode* curr = root;
        for (char c : word) {
            int idx = charIndex(c);
            if (idx == -1) continue;
            if (!curr->children[idx]) curr->children[idx] = new TrieNode();
            curr = curr->children[idx];
            if (find(curr->dishIds.begin(), curr->dishIds.end(), dishId) == curr->dishIds.end())
                curr->dishIds.push_back(dishId);
        }
    }

    void indexPhrase(const string& phrase, int dishId) {
        stringstream ss(phrase);
        string token;
        while (ss >> token) insertWord(token, dishId);
    }

    vector<int> searchPrefix(const string& prefix) const {
        TrieNode* curr = root;
        for (char c : prefix) {
            int idx = charIndex(c);
            if (idx == -1) continue;
            if (!curr->children[idx]) return {};
            curr = curr->children[idx];
        }
        vector<int> results;
        collectAll(curr, results);
        return results;
    }
};

// ============================================================================
// 4. DSA #3: SINGLY LINKED LIST (Shopping Cart)
// ============================================================================
struct CartNode {
    Dish dish;
    int quantity;
    CartNode* next;
    CartNode(const Dish& d, int q) : dish(d), quantity(q), next(nullptr) {}
};

class CartLinkedList {
private:
    CartNode* head;

public:
    CartLinkedList() : head(nullptr) {}

    void addOrUpdate(const Dish& dish, int delta) {
        CartNode* curr = head;
        CartNode* prev = nullptr;
        while (curr) {
            if (curr->dish.id == dish.id) {
                curr->quantity += delta;
                if (curr->quantity <= 0) {
                    if (prev) prev->next = curr->next;
                    else head = curr->next;
                    delete curr;
                }
                return;
            }
            prev = curr;
            curr = curr->next;
        }
        if (delta > 0) {
            CartNode* node = new CartNode(dish, delta);
            node->next = head;
            head = node;
        }
    }

    void clear() {
        while (head) {
            CartNode* tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    int getSubtotal() const {
        int sum = 0;
        for (CartNode* c = head; c; c = c->next) sum += c->dish.price * c->quantity;
        return sum;
    }

    int getTotalCount() const {
        int cnt = 0;
        for (CartNode* c = head; c; c = c->next) cnt += c->quantity;
        return cnt;
    }

    string toJson() const {
        stringstream ss;
        ss << "[";
        CartNode* c = head;
        bool first = true;
        while (c) {
            if (!first) ss << ",";
            ss << "{\"id\":" << c->dish.id
               << ",\"name\":\"" << c->dish.name << "\""
               << ",\"price\":" << c->dish.price
               << ",\"qty\":" << c->quantity
               << ",\"isVeg\":" << (c->dish.isVeg ? "true" : "false") << "}";
            first = false;
            c = c->next;
        }
        ss << "]";
        return ss.str();
    }
};

// ============================================================================
// 5. DSA #4: STACK (LIFO - Recent Searches & Browsing History)
// ============================================================================
class NavigationStack {
private:
    static const int MAX_CAP = 8;
    string items[MAX_CAP];
    int topIdx;

public:
    NavigationStack() : topIdx(-1) {}

    void push(const string& query) {
        if (query.empty()) return;
        // Remove duplicate if already present
        for (int i = 0; i <= topIdx; ++i) {
            if (items[i] == query) {
                for (int j = i; j < topIdx; ++j) items[j] = items[j + 1];
                topIdx--;
                break;
            }
        }
        if (topIdx == MAX_CAP - 1) {
            for (int i = 0; i < MAX_CAP - 1; ++i) items[i] = items[i + 1];
            topIdx--;
        }
        items[++topIdx] = query;
    }

    string pop() {
        if (topIdx < 0) return "";
        return items[topIdx--];
    }

    string toJson() const {
        stringstream ss;
        ss << "[";
        for (int i = topIdx; i >= 0; --i) {
            if (i != topIdx) ss << ",";
            ss << "\"" << items[i] << "\"";
        }
        ss << "]";
        return ss.str();
    }
};

// ============================================================================
// 6. DSA #5: FIFO QUEUE (Live Kitchen Order Dispatch Pipeline)
// ============================================================================
struct OrderRecord {
    string orderId;
    string customerEmail;
    int totalAmount;
    string status;
    Rider assignedRider;
    vector<int> shortestPathNodes;
    int shortestDistanceKm;
};

struct OrderQueueNode {
    OrderRecord data;
    OrderQueueNode* next;
    OrderQueueNode(const OrderRecord& o) : data(o), next(nullptr) {}
};

class OrderQueue {
private:
    OrderQueueNode* frontPtr;
    OrderQueueNode* rearPtr;
    int count;

public:
    OrderQueue() : frontPtr(nullptr), rearPtr(nullptr), count(0) {}

    void enqueue(const OrderRecord& order) {
        OrderQueueNode* node = new OrderQueueNode(order);
        if (!rearPtr) frontPtr = rearPtr = node;
        else {
            rearPtr->next = node;
            rearPtr = node;
        }
        count++;
    }

    bool dequeue(OrderRecord& out) {
        if (!frontPtr) return false;
        OrderQueueNode* tmp = frontPtr;
        out = tmp->data;
        frontPtr = frontPtr->next;
        if (!frontPtr) rearPtr = nullptr;
        delete tmp;
        count--;
        return true;
    }

    OrderRecord* latest() const {
        return rearPtr ? &rearPtr->data : nullptr;
    }

    int size() const { return count; }
};

// ============================================================================
// 7. DSA #6: MIN-HEAP / PRIORITY QUEUE (Nearest Rider Assignment)
// ============================================================================
class RiderMinHeap {
private:
    vector<Rider> heap;

    void siftUp(int idx) {
        while (idx > 0) {
            int parent = (idx - 1) / 2;
            if (heap[idx].distanceKm < heap[parent].distanceKm) {
                swap(heap[idx], heap[parent]);
                idx = parent;
            } else break;
        }
    }

    void siftDown(int idx) {
        int n = static_cast<int>(heap.size());
        while (true) {
            int smallest = idx;
            int left = 2 * idx + 1;
            int right = 2 * idx + 2;
            if (left < n && heap[left].distanceKm < heap[smallest].distanceKm) smallest = left;
            if (right < n && heap[right].distanceKm < heap[smallest].distanceKm) smallest = right;
            if (smallest != idx) {
                swap(heap[idx], heap[smallest]);
                idx = smallest;
            } else break;
        }
    }

public:
    void push(const Rider& r) {
        heap.push_back(r);
        siftUp(static_cast<int>(heap.size()) - 1);
    }

    Rider extractMin() {
        if (heap.empty()) return {1, "Rohan Bisht", 4.9, 1420, "Hero Splendor+", "UK-06-AW-4821", 0.8};
        Rider minRider = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) siftDown(0);
        // Re-insert with updated simulated distance so pool stays available
        Rider recycled = minRider;
        recycled.distanceKm += 1.4;
        push(recycled);
        return minRider;
    }
};

// ============================================================================
// 8. DSA #7: WEIGHTED GRAPH + DIJKSTRA'S ALGORITHM (Pantnagar Map Routing)
// ============================================================================
struct Edge {
    int to;
    int weightKm;
};

class PantnagarGraph {
private:
    vector<string> nodeNames;
    vector<vector<Edge>> adj;

public:
    PantnagarGraph() {
        nodeNames = {
            "GBPUAT Main Gate",      // 0
            "Phoolbagh Market",      // 1
            "Nagla Chowk",           // 2
            "Saraswati Vihar",       // 3
            "Haldi Campus",          // 4
            "Rudrapur Highway",      // 5
            "Tagore Bhawan Hostel"   // 6 (Customer Drop-off)
        };
        adj.resize(nodeNames.size());
        addRoad(0, 1, 2);
        addRoad(0, 2, 3);
        addRoad(1, 3, 2);
        addRoad(2, 3, 2);
        addRoad(2, 4, 4);
        addRoad(1, 5, 5);
        addRoad(3, 6, 2);
        addRoad(4, 6, 3);
        addRoad(5, 6, 4);
    }

    void addRoad(int u, int v, int w) {
        adj[u].push_back({v, w});
        adj[v].push_back({u, w});
    }

    pair<int, vector<int>> dijkstra(int src, int dest) const {
        int n = static_cast<int>(nodeNames.size());
        vector<int> dist(n, INT_MAX);
        vector<int> parent(n, -1);
        vector<bool> visited(n, false);
        dist[src] = 0;

        for (int i = 0; i < n; ++i) {
            int u = -1;
            for (int v = 0; v < n; ++v) {
                if (!visited[v] && (u == -1 || dist[v] < dist[u])) u = v;
            }
            if (u == -1 || dist[u] == INT_MAX) break;
            visited[u] = true;

            for (const auto& e : adj[u]) {
                if (dist[u] + e.weightKm < dist[e.to]) {
                    dist[e.to] = dist[u] + e.weightKm;
                    parent[e.to] = u;
                }
            }
        }

        vector<int> path;
        for (int at = dest; at != -1; at = parent[at]) path.push_back(at);
        reverse(path.begin(), path.end());
        return {dist[dest], path};
    }

    string getNodeName(int id) const {
        return (id >= 0 && id < (int)nodeNames.size()) ? nodeNames[id] : "Pantnagar";
    }
};

// ============================================================================
// 9. DSA #8: MERGE SORT (O(N log N) Restaurant Ranking)
// ============================================================================
void mergeRestaurants(vector<Restaurant>& arr, int l, int m, int r, const string& sortBy) {
    int n1 = m - l + 1;
    int n2 = r - m;
    vector<Restaurant> L(n1), R(n2);
    for (int i = 0; i < n1; ++i) L[i] = arr[l + i];
    for (int j = 0; j < n2; ++j) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        bool takeLeft = (sortBy == "eta") ? (L[i].etaMins <= R[j].etaMins)
                                          : (L[i].rating >= R[j].rating);
        if (takeLeft) arr[k++] = L[i++];
        else arr[k++] = R[j++];
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

void mergeSortRestaurants(vector<Restaurant>& arr, int l, int r, const string& sortBy) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    mergeSortRestaurants(arr, l, m, sortBy);
    mergeSortRestaurants(arr, m + 1, r, sortBy);
    mergeRestaurants(arr, l, m, r, sortBy);
}

// ============================================================================
// 10. GLOBAL APPLICATION STATE (Seeded Database)
// ============================================================================
UserHashTable g_users;
SearchTrie g_searchTrie;
CartLinkedList g_cart;
NavigationStack g_recentSearches;
OrderQueue g_orderQueue;
RiderMinHeap g_riderHeap;
PantnagarGraph g_cityGraph;

vector<Restaurant> g_restaurants;
vector<Dish> g_dishes;
int g_orderCounter = 1042;

void seedDatabase() {
    // Seed Users in Hash Table
    g_users.insert("ishaanthusoo80@gmail.com", "Ishaan123", "customer", "🍛");
    g_users.insert("partner@zwigato.in", "Ishaan123", "partner", "👨‍🍳");

    // Seed Delivery Riders in Min-Heap
    g_riderHeap.push({101, "Rohan Bisht", 4.9, 1420, "Hero Splendor+", "UK-06-AW-4821", 0.7});
    g_riderHeap.push({102, "Deepak Joshi", 4.8, 980, "Honda Activa 6G", "UK-06-BK-1934", 1.2});
    g_riderHeap.push({103, "Amit Negi", 4.95, 2110, "TVS Raider 125", "UK-06-CD-7702", 1.8});

    // Seed Pantnagar Restaurants (Array)
    g_restaurants = {
        {1, "Kumaon Spice Kitchen", "North Indian, Pahadi, Thali", "Phoolbagh, Pantnagar", 4.8, 22, true, "50% OFF up to ₹100",
         "https://images.unsplash.com/photo-1585937421612-70a008356fbe?auto=format&fit=crop&w=800&q=80", 1},
        {2, "GreenBowl NutriCafe", "Healthy, Salads, Smoothie Bowls", "GBPUAT Gate, Pantnagar", 4.9, 18, true, "No Packaging Fee",
         "https://images.unsplash.com/photo-1512621776951-a57141f2eefd?auto=format&fit=crop&w=800&q=80", 0},
        {3, "Royal Dum Biryani House", "Hyderabadi Biryani, Kebabs", "Nagla Chowk, Pantnagar", 4.7, 26, false, "Flat ₹125 OFF",
         "https://images.unsplash.com/photo-1563379091339-03b21ab4a4f8?auto=format&fit=crop&w=800&q=80", 2},
        {4, "Artisan Woodfire Pizzeria", "Italian, Sourdough Pizza, Pasta", "Saraswati Vihar, Pantnagar", 4.6, 28, false, "Buy 1 Get 1",
         "https://images.unsplash.com/photo-1513104890138-7c749659a591?auto=format&fit=crop&w=800&q=80", 3},
        {5, "Chai & Bun Maska Co.", "Cafe, Street Food, Tandoori Chai", "Haldi Road, Pantnagar", 4.8, 15, true, "Meals Under ₹199",
         "https://images.unsplash.com/photo-1561336313-0bd5e0b27ec8?auto=format&fit=crop&w=800&q=80", 4},
        {6, "Wok & Dragon Pan-Asian", "Momos, Ramen, Dimsum, Thai", "Rudrapur Hwy, Pantnagar", 4.5, 30, false, "20% OFF",
         "https://images.unsplash.com/photo-1569718212165-3a8278d5f624?auto=format&fit=crop&w=800&q=80", 5}
    };

    // Seed Dishes & Index into SearchTrie
    g_dishes = {
        {101, 1, "Paneer Tikka Lababdar Bowl", "North Indian", 229, true, false, 480, "Rich Protein",
         "https://images.unsplash.com/photo-1631452180519-c014fe946bc7?auto=format&fit=crop&w=400&q=80"},
        {102, 1, "Pahadi Bhatt Ki Churkani Thali", "Thali", 199, true, true, 390, "High Fiber",
         "https://images.unsplash.com/photo-1546833999-b9f581a1996d?auto=format&fit=crop&w=400&q=80"},
        {103, 1, "Butter Chicken & Garlic Naan", "North Indian", 279, false, false, 620, "Chef Special",
         "https://images.unsplash.com/photo-1603894584373-5ac82b2ae398?auto=format&fit=crop&w=400&q=80"},
        {201, 2, "Quinoa Avocado Superfood Salad", "Healthy", 219, true, true, 260, "260 kcal · Low Cal",
         "https://images.unsplash.com/photo-1512621776951-a57141f2eefd?auto=format&fit=crop&w=400&q=80"},
        {202, 2, "Grilled Tofu & Millet Buddha Bowl", "Healthy", 239, true, true, 310, "310 kcal · High Protein",
         "https://images.unsplash.com/photo-1540420773420-3366772f4999?auto=format&fit=crop&w=400&q=80"},
        {203, 2, "Cold-Pressed Berry Chia Smoothie", "Beverages", 149, true, true, 165, "165 kcal · Antioxidant",
         "https://images.unsplash.com/photo-1553530666-ba11a7da3888?auto=format&fit=crop&w=400&q=80"},
        {301, 3, "Hyderabadi Subz Dum Biryani", "Biryani", 219, true, false, 510, "Aromatic",
         "https://images.unsplash.com/photo-1563379091339-03b21ab4a4f8?auto=format&fit=crop&w=400&q=80"},
        {302, 3, "Royal Chicken Tikka Biryani", "Biryani", 269, false, false, 590, "Bestseller",
         "https://images.unsplash.com/photo-1589302168068-964664d93dc0?auto=format&fit=crop&w=400&q=80"},
        {401, 4, "Margherita Burrata Sourdough Pizza", "Pizza", 249, true, false, 470, "Woodfired",
         "https://images.unsplash.com/photo-1574071318508-1cdbab80d002?auto=format&fit=crop&w=400&q=80"},
        {402, 4, "Pesto Zucchini Thin-Crust Pizza", "Pizza", 239, true, true, 340, "340 kcal · Light Crust",
         "https://images.unsplash.com/photo-1513104890138-7c749659a591?auto=format&fit=crop&w=400&q=80"},
        {501, 5, "Kulhad Adrak Chai & Bun Maska", "Cafe", 99, true, false, 240, "Campus Favorite",
         "https://images.unsplash.com/photo-1561336313-0bd5e0b27ec8?auto=format&fit=crop&w=400&q=80"},
        {502, 5, "Multigrain Paneer Grilled Panini", "Healthy", 159, true, true, 290, "290 kcal · Balanced",
         "https://images.unsplash.com/photo-1528735602780-2552fd46c7af?auto=format&fit=crop&w=400&q=80"},
        {601, 6, "Steamed Spinach & Corn Dimsum", "Momos", 169, true, true, 210, "210 kcal · Steamed",
         "https://images.unsplash.com/photo-1534422298391-e4f8c172dddb?auto=format&fit=crop&w=400&q=80"},
        {602, 6, "Spicy Miso Tofu Ramen Bowl", "Ramen", 249, true, true, 350, "350 kcal · Gut Healthy",
         "https://images.unsplash.com/photo-1569718212165-3a8278d5f624?auto=format&fit=crop&w=400&q=80"}
    };

    for (const auto& d : g_dishes) {
        g_searchTrie.indexPhrase(d.name + " " + d.category, d.id);
    }

    g_recentSearches.push("Biryani");
    g_recentSearches.push("Healthy Bowl");
    g_recentSearches.push("Paneer");
}

// ============================================================================
// 11. EMBEDDED EDITORIAL ZWIGATO FRONTEND (Logo Option 1 + Full UI)
// ============================================================================
string renderHtmlPage() {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Zwigato — Pantnagar Food Delivery (C++ DSA Engine)</title>
<link href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,700;9..144,800&family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
  :root {
    --bg: #F8F6F2; --card: #FFFFFF; --ink: #141416; --muted: #686873;
    --brand: #E04E2A; --gold: #F59E0B; --green: #16A34A;
    --border: rgba(20,20,22,0.08);
    --sh: 0 4px 20px rgba(20,20,22,0.06);
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Plus Jakarta Sans', sans-serif; background: var(--bg); color: var(--ink); padding-bottom: 90px; }

  /* Netflix-style Cinematic Intro */
  #splash {
    position: fixed; inset: 0; background: #000; z-index: 9999;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    transition: opacity 0.6s ease;
  }
  #splash.hide { opacity: 0; pointer-events: none; }
  .nf-logo { animation: nfZoom 1.8s cubic-bezier(.22,1,.36,1) forwards; }
  @keyframes nfZoom {
    0% { transform: scale(0.7); opacity: 0; }
    50% { transform: scale(1.06); opacity: 1; }
    100% { transform: scale(1); opacity: 1; }
  }
  .skip-btn {
    position: absolute; top: 24px; right: 24px; background: rgba(255,255,255,0.14);
    color: #fff; border: 1px solid rgba(255,255,255,0.25); padding: 8px 16px;
    border-radius: 999px; cursor: pointer; font-weight: 600;
  }

  /* Frosted Sticky Header */
  header {
    position: sticky; top: 0; z-index: 100;
    background: rgba(248,246,242,0.88); backdrop-filter: blur(18px);
    border-bottom: 1px solid var(--border); padding: 12px 20px;
  }
  .nav-wrap { max-width: 1200px; margin: 0 auto; display: flex; align-items: center; gap: 14px; flex-wrap: wrap; }
  .brand { display: flex; align-items: center; gap: 10px; text-decoration: none; color: var(--ink); }
  .brand-title { font-family: 'Fraunces', serif; font-size: 1.5rem; font-weight: 800; letter-spacing: -0.02em; }
  .loc-pill { font-size: 0.8rem; font-weight: 600; color: var(--muted); background: var(--card); padding: 6px 12px; border-radius: 999px; border: 1px solid var(--border); }

  .search-bar {
    flex: 1; min-width: 240px; display: flex; align-items: center; gap: 8px;
    background: var(--card); border: 1px solid var(--border); border-radius: 14px;
    padding: 8px 14px; box-shadow: var(--sh);
  }
  .search-bar input { border: none; outline: none; width: 100%; font-size: 0.92rem; background: transparent; }

  .toggle-pill {
    display: flex; align-items: center; gap: 6px; padding: 7px 12px; border-radius: 999px;
    border: 1px solid var(--border); background: var(--card); font-size: 0.78rem;
    font-weight: 700; cursor: pointer; transition: 0.2s;
  }
  .toggle-pill.active-veg { background: #DCFCE7; color: #15803D; border-color: #86EFAC; }
  .toggle-pill.active-healthy { background: #D1FAE5; color: #065F46; border-color: #6EE7B7; }

  .container { max-width: 1200px; margin: 0 auto; padding: 20px; }

  /* DSA Telemetry Banner */
  .dsa-strip {
    background: #141416; color: #FAF7F2; border-radius: 16px; padding: 14px 18px;
    margin-bottom: 20px; display: flex; flex-wrap: wrap; gap: 12px; align-items: center;
    justify-content: space-between; font-size: 0.82rem;
  }
  .dsa-badge { background: rgba(224,78,42,0.2); border: 1px solid rgba(224,78,42,0.5); color: #FF9E7D; padding: 4px 10px; border-radius: 999px; font-weight: 600; }

  /* Promo Banner */
  .hero-banner {
    background: linear-gradient(135deg, #E04E2A 0%, #F59E0B 100%);
    color: #fff; border-radius: 22px; padding: 26px; margin-bottom: 22px;
    display: flex; justify-content: space-between; align-items: center; box-shadow: var(--sh);
  }
  .hero-banner h1 { font-family: 'Fraunces', serif; font-size: 1.8rem; margin-bottom: 6px; }

  /* Filter & Sort Pills */
  .controls { display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 20px; align-items: center; }
  .chip {
    padding: 8px 14px; border-radius: 999px; border: 1px solid var(--border);
    background: var(--card); font-size: 0.82rem; font-weight: 600; cursor: pointer;
  }
  .chip.active { background: var(--ink); color: #fff; }

  /* Restaurant Grid */
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(310px, 1fr)); gap: 20px; }
  .card {
    background: var(--card); border-radius: 20px; overflow: hidden;
    border: 1px solid var(--border); box-shadow: var(--sh); transition: transform 0.25s ease;
  }
  .card:hover { transform: translateY(-4px); }
  .card-cover { position: relative; height: 175px; overflow: hidden; }
  .card-cover img { width: 100%; height: 100%; object-fit: cover; }
  .card-scrim {
    position: absolute; inset: 0;
    background: linear-gradient(to top, rgba(0,0,0,0.68), transparent 55%);
    display: flex; align-items: flex-end; justify-content: space-between; padding: 12px; color: #fff;
  }
  .card-body { padding: 16px; }
  .rating-badge { background: var(--green); color: #fff; font-weight: 700; font-size: 0.78rem; padding: 3px 8px; border-radius: 8px; }

  /* Dish List inside Card */
  .dish-item {
    display: flex; justify-content: space-between; align-items: center; gap: 10px;
    padding: 12px 0; border-top: 1px solid var(--border);
  }
  .veg-dot {
    display: inline-block; width: 12px; height: 12px; border: 1.5px solid var(--green);
    border-radius: 3px; position: relative; margin-right: 6px;
  }
  .veg-dot::after {
    content: ''; position: absolute; inset: 2px; border-radius: 50%; background: var(--green);
  }
  .nonveg-dot { border-color: #DC2626; }
  .nonveg-dot::after { background: #DC2626; }
  .nutri-tag { font-size: 0.72rem; background: #ECFDF5; color: #047857; padding: 2px 7px; border-radius: 6px; font-weight: 600; }
  .add-btn {
    background: #FFF5F2; color: var(--brand); border: 1px solid rgba(224,78,42,0.3);
    font-weight: 800; padding: 6px 14px; border-radius: 10px; cursor: pointer;
  }
  .add-btn:hover { background: var(--brand); color: #fff; }

  /* Floating Cart Bar */
  .cart-bar {
    position: fixed; bottom: 18px; left: 50%; transform: translateX(-50%);
    width: min(640px, 92vw); background: var(--ink); color: #fff;
    padding: 14px 20px; border-radius: 18px; display: none;
    justify-content: space-between; align-items: center; z-index: 200;
    box-shadow: 0 12px 32px rgba(0,0,0,0.28);
  }
  .checkout-btn {
    background: var(--brand); color: #fff; border: none; padding: 10px 18px;
    border-radius: 12px; font-weight: 700; cursor: pointer;
  }

  /* Modal */
  .modal-backdrop {
    position: fixed; inset: 0; background: rgba(0,0,0,0.55); z-index: 300;
    display: none; align-items: center; justify-content: center; padding: 16px;
  }
  .modal-card {
    background: var(--card); width: min(500px, 96vw); border-radius: 22px;
    padding: 22px; max-height: 88vh; overflow-y: auto;
  }
  .rider-box {
    background: #F8F6F2; border: 1px solid var(--border); border-radius: 14px;
    padding: 14px; margin-top: 12px;
  }
</style>
</head>
<body>

<!-- SVG Symbol: Logo Option 1 ("The Folded Ribbon Z") -->
<svg style="display:none">
  <symbol id="z-logo" viewBox="0 0 64 64">
    <rect width="64" height="64" rx="16" fill="#141416"/>
    <defs>
      <linearGradient id="ribTop" x1="0" y1="0" x2="1" y2="0">
        <stop offset="0%" stop-color="#E04E2A"/><stop offset="100%" stop-color="#F59E0B"/>
      </linearGradient>
      <linearGradient id="ribDiag" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0%" stop-color="#F59E0B"/><stop offset="100%" stop-color="#C83E1B"/>
      </linearGradient>
    </defs>
    <polygon points="15,14 49,14 43,23 15,23" fill="url(#ribTop)"/>
    <polygon points="49,14 23,50 15,43 41,14" fill="url(#ribDiag)"/>
    <polygon points="15,42 49,42 49,50 15,50" fill="url(#ribTop)"/>
  </symbol>
</svg>

<!-- Netflix-Style Intro Splash -->
<div id="splash">
  <button class="skip-btn" onclick="dismissSplash()">Skip Intro ›</button>
  <svg class="nf-logo" width="110" height="110"><use href="#z-logo"/></svg>
  <div style="color:#fff;font-family:'Fraunces',serif;font-size:2rem;font-weight:800;margin-top:14px;letter-spacing:2px;">ZWIGATO</div>
  <div style="color:#9CA3AF;font-size:0.82rem;margin-top:6px;">Powered by C++17 DSA Engine • Pantnagar</div>
</div>

<header>
  <div class="nav-wrap">
    <a class="brand" href="#">
      <svg width="38" height="38"><use href="#z-logo"/></svg>
      <span class="brand-title">Zwigato</span>
    </a>
    <span class="loc-pill">📍 Pantnagar, Uttarakhand</span>

    <div class="search-bar">
      <span>🔍</span>
      <input id="searchInput" type="text" placeholder="Search Trie index: Biryani, Quinoa, Paneer, Chai..." oninput="runSearch()">
      <button onclick="startVoiceSearch()" title="Voice Search" style="border:none;background:none;cursor:pointer;">🎙️</button>
    </div>

    <button id="vegBtn" class="toggle-pill" onclick="toggleVeg()">🟢 VEG</button>
    <button id="healthyBtn" class="toggle-pill" onclick="toggleHealthy()">🥗 HEALTHY</button>
    <button class="toggle-pill" onclick="openAuthModal()" id="authPill">👤 ishaanthusoo80@gmail.com</button>
  </div>
</header>

<div class="container">
  <!-- Live C++ DSA Architecture Bar -->
  <div class="dsa-strip">
    <div><strong>Active C++ DSA Structures:</strong></div>
    <span class="dsa-badge">Hash Table (Auth)</span>
    <span class="dsa-badge">Trie (Prefix Search)</span>
    <span class="dsa-badge">Linked List (Cart)</span>
    <span class="dsa-badge">Stack (Recent Searches)</span>
    <span class="dsa-badge">FIFO Queue (Orders)</span>
    <span class="dsa-badge">Min-Heap (Riders)</span>
    <span class="dsa-badge">Dijkstra Graph (Pantnagar)</span>
    <span class="dsa-badge">Merge Sort (Ranking)</span>
  </div>

  <div class="hero-banner">
    <div>
      <h1>Craving Something Delicious in Pantnagar?</h1>
      <p>Use code <strong>ZWIGATO50</strong> for 50% OFF • Shortest path delivery via Dijkstra Routing.</p>
    </div>
    <div style="font-size:2.4rem;">🍲🛵</div>
  </div>

  <!-- Recent Searches (C++ Stack) & Merge Sort Controls -->
  <div class="controls">
    <span style="font-size:0.82rem;font-weight:700;color:var(--muted);">Sort (Merge Sort):</span>
    <button class="chip active" id="sortRating" onclick="setSort('rating')">⭐ Top Rated</button>
    <button class="chip" id="sortEta" onclick="setSort('eta')">⚡ Fastest Delivery</button>
    <span style="font-size:0.82rem;font-weight:700;color:var(--muted);margin-left:10px;">Recent (Stack):</span>
    <div id="recentStackChips" style="display:flex;gap:6px;flex-wrap:wrap;"></div>
  </div>

  <div id="restaurantGrid" class="grid"></div>
</div>

<!-- Sticky Cart Bar -->
<div id="cartBar" class="cart-bar">
  <div>
    <div id="cartSummary" style="font-weight:800;">0 Items • ₹0</div>
    <div style="font-size:0.76rem;color:#D1D5DB;">Stored in C++ Singly Linked List</div>
  </div>
  <button class="checkout-btn" onclick="openCheckout()">View Cart & Track →</button>
</div>

<!-- Checkout & Live Dijkstra Rider Tracker Modal -->
<div id="checkoutModal" class="modal-backdrop">
  <div class="modal-card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;">
      <h2 style="font-family:'Fraunces',serif;">Your Cart & Live Dispatch</h2>
      <button onclick="closeCheckout()" style="border:none;background:none;font-size:1.3rem;cursor:pointer;">✕</button>
    </div>
    <div id="cartItemsList"></div>
    <div style="margin-top:12px;display:flex;gap:8px;">
      <input id="promoInput" placeholder="Promo code (ZWIGATO50)" style="flex:1;padding:9px;border-radius:10px;border:1px solid var(--border);">
      <button class="checkout-btn" onclick="placeOrder()">Place Order (FIFO Queue)</button>
    </div>
    <div id="orderTrackerArea"></div>
  </div>
</div>

<!-- Login Modal (Hash Table Auth) -->
<div id="authModal" class="modal-backdrop">
  <div class="modal-card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;">
      <h2 style="font-family:'Fraunces',serif;">Zwigato Hash-Table Auth</h2>
      <button onclick="document.getElementById('authModal').style.display='none'" style="border:none;background:none;font-size:1.3rem;cursor:pointer;">✕</button>
    </div>
    <p style="font-size:0.84rem;color:var(--muted);margin-bottom:10px;">Login with any @gmail.com ID using master password <strong>Ishaan123</strong>.</p>
    <input id="loginEmail" value="ishaanthusoo80@gmail.com" style="width:100%;padding:10px;margin-bottom:8px;border-radius:10px;border:1px solid var(--border);">
    <input id="loginPass" type="password" value="Ishaan123" style="width:100%;padding:10px;margin-bottom:12px;border-radius:10px;border:1px solid var(--border);">
    <button class="checkout-btn" style="width:100%;" onclick="submitLogin()">Sign In via C++ Hash Table</button>
  </div>
</div>

<script>
  let state = { veg: false, healthy: false, sort: 'rating', query: '', email: 'ishaanthusoo80@gmail.com' };

  function dismissSplash() {
    const s = document.getElementById('splash');
    if (s) { s.classList.add('hide'); setTimeout(() => s.style.display = 'none', 600); }
  }
  setTimeout(dismissSplash, 2000);

  async function loadCatalog() {
    const params = new URLSearchParams({
      q: state.query,
      veg: state.veg ? '1' : '0',
      healthy: state.healthy ? '1' : '0',
      sort: state.sort
    });
    const res = await fetch('/api/catalog?' + params.toString());
    const data = await res.json();
    renderRestaurants(data.restaurants);
    renderRecentStack(data.recentStack);
  }

  function renderRestaurants(list) {
    const grid = document.getElementById('restaurantGrid');
    grid.innerHTML = list.map(r => `
      <div class="card">
        <div class="card-cover">
          <img src="${r.coverImg}" alt="${r.name}">
          <div class="card-scrim">
            <span style="font-weight:700;font-size:0.82rem;">⚡ ${r.etaMins} mins</span>
            <span style="background:var(--brand);padding:3px 8px;border-radius:8px;font-size:0.75rem;font-weight:700;">${r.offerBadge}</span>
          </div>
        </div>
        <div class="card-body">
          <div style="display:flex;justify-content:space-between;align-items:center;">
            <h3 style="font-size:1.08rem;">${r.name}</h3>
            <span class="rating-badge">★ ${r.rating.toFixed(1)}</span>
          </div>
          <div style="font-size:0.8rem;color:var(--muted);margin:4px 0 10px;">${r.cuisine} • ${r.location}</div>
          ${r.dishes.map(d => `
            <div class="dish-item">
              <div>
                <div style="font-weight:700;font-size:0.9rem;">
                  <span class="veg-dot ${d.isVeg ? '' : 'nonveg-dot'}"></span>${d.name}
                </div>
                <div style="font-size:0.82rem;margin-top:3px;">
                  <strong>₹${d.price}</strong>
                  ${d.isHealthy ? `<span class="nutri-tag">🥗 ${d.nutritionTag}</span>` : ''}
                </div>
              </div>
              <button class="add-btn" onclick="updateCart(${d.id}, 1)">+ ADD</button>
            </div>
          `).join('')}
        </div>
      </div>
    `).join('');
  }

  function renderRecentStack(items) {
    document.getElementById('recentStackChips').innerHTML = items.map(q =>
      `<button class="chip" onclick="applyStackQuery('${q}')">${q}</button>`
    ).join('');
  }

  function applyStackQuery(q) {
    document.getElementById('searchInput').value = q;
    state.query = q;
    loadCatalog();
  }

  function runSearch() {
    state.query = document.getElementById('searchInput').value.trim();
    loadCatalog();
  }

  function startVoiceSearch() {
    const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SR) return alert('Voice search requires Chrome/Edge.');
    const rec = new SR();
    rec.onresult = e => {
      document.getElementById('searchInput').value = e.results[0][0].transcript;
      runSearch();
    };
    rec.start();
  }

  function toggleVeg() {
    state.veg = !state.veg;
    document.getElementById('vegBtn').classList.toggle('active-veg', state.veg);
    loadCatalog();
  }

  function toggleHealthy() {
    state.healthy = !state.healthy;
    document.getElementById('healthyBtn').classList.toggle('active-healthy', state.healthy);
    loadCatalog();
  }

  function setSort(s) {
    state.sort = s;
    document.getElementById('sortRating').classList.toggle('active', s === 'rating');
    document.getElementById('sortEta').classList.toggle('active', s === 'eta');
    loadCatalog();
  }

  async function updateCart(dishId, delta) {
    const res = await fetch('/api/cart', {
      method: 'POST',
      body: JSON.stringify({ dishId, delta })
    });
    const cart = await res.json();
    const bar = document.getElementById('cartBar');
    if (cart.count > 0) {
      bar.style.display = 'flex';
      document.getElementById('cartSummary').textContent = `${cart.count} Item(s) • ₹${cart.subtotal}`;
    } else {
      bar.style.display = 'none';
    }
    renderCartModal(cart);
  }

  function renderCartModal(cart) {
    const el = document.getElementById('cartItemsList');
    if (!cart.items.length) {
      el.innerHTML = '<p style="color:var(--muted);">Your linked-list cart is empty.</p>';
      return;
    }
    el.innerHTML = cart.items.map(i => `
      <div class="dish-item">
        <div><strong>${i.name}</strong> × ${i.qty}</div>
        <div>
          <button class="add-btn" onclick="updateCart(${i.id}, -1)">−</button>
          <span style="margin:0 8px;font-weight:700;">₹${i.price * i.qty}</span>
          <button class="add-btn" onclick="updateCart(${i.id}, 1)">+</button>
        </div>
      </div>
    `).join('') + `<div style="margin-top:10px;font-weight:800;">Subtotal: ₹${cart.subtotal}</div>`;
  }

  async function openCheckout() {
    const res = await fetch('/api/cart', { method: 'POST', body: JSON.stringify({ dishId: 0, delta: 0 }) });
    const cart = await res.json();
    renderCartModal(cart);
    document.getElementById('checkoutModal').style.display = 'flex';
  }

  function closeCheckout() {
    document.getElementById('checkoutModal').style.display = 'none';
  }

  async function placeOrder() {
    const promo = document.getElementById('promoInput').value.trim();
    const res = await fetch('/api/order', {
      method: 'POST',
      body: JSON.stringify({ email: state.email, promo })
    });
    const order = await res.json();
    if (order.error) return alert(order.error);

    document.getElementById('cartBar').style.display = 'none';
    document.getElementById('orderTrackerArea').innerHTML = `
      <div class="rider-box">
        <div style="color:var(--green);font-weight:800;">✅ Order #${order.orderId} Dispatched (Queue Position #${order.queueSize})</div>
        <div style="font-size:0.86rem;margin-top:6px;"><strong>Total Paid:</strong> ₹${order.totalAmount}</div>
        <div style="font-size:0.86rem;margin-top:6px;">
          <strong>Min-Heap Assigned Rider:</strong> 🛵 ${order.rider.name} (★ ${order.rider.rating} • ${order.rider.deliveries} deliveries)<br>
          <strong>Vehicle:</strong> ${order.rider.vehicle} (${order.rider.plate})
        </div>
        <div style="font-size:0.84rem;margin-top:8px;padding-top:8px;border-top:1px solid var(--border);">
          <strong>Dijkstra Shortest Path (${order.distanceKm} km):</strong><br>
          ${order.routeNodes.join(' → ')}
        </div>
      </div>
    `;
  }

  function openAuthModal() {
    document.getElementById('authModal').style.display = 'flex';
  }

  async function submitLogin() {
    const email = document.getElementById('loginEmail').value.trim();
    const password = document.getElementById('loginPass').value.trim();
    const res = await fetch('/api/login', {
      method: 'POST',
      body: JSON.stringify({ email, password })
    });
    const data = await res.json();
    if (data.ok) {
      state.email = data.email;
      document.getElementById('authPill').textContent = `${data.avatar} ${data.email}`;
      document.getElementById('authModal').style.display = 'none';
    } else {
      alert(data.error);
    }
  }

  loadCatalog();
</script>
</body>
</html>)HTML";
}

// ============================================================================
// 12. HTTP ROUTER & JSON API HELPERS
// ============================================================================
string getQueryParam(const string& query, const string& key) {
    string search = key + "=";
    size_t pos = query.find(search);
    if (pos == string::npos) return "";
    pos += search.size();
    size_t end = query.find('&', pos);
    string val = (end == string::npos) ? query.substr(pos) : query.substr(pos, end - pos);
    // Basic URL decode for spaces
    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '+') val[i] = ' ';
        else if (val[i] == '%' && i + 2 < val.size() && val.substr(i, 3) == "%20") {
            val.replace(i, 3, " ");
        }
    }
    return val;
}

string extractJsonString(const string& body, const string& key) {
    string pattern = "\"" + key + "\":\"";
    size_t pos = body.find(pattern);
    if (pos == string::npos) return "";
    pos += pattern.size();
    size_t end = body.find('"', pos);
    return (end == string::npos) ? "" : body.substr(pos, end - pos);
}

int extractJsonInt(const string& body, const string& key) {
    string pattern = "\"" + key + "\":";
    size_t pos = body.find(pattern);
    if (pos == string::npos) return 0;
    pos += pattern.size();
    return atoi(body.c_str() + pos);
}

string handleCatalogApi(const string& query) {
    string q = getQueryParam(query, "q");
    bool vegOnly = (getQueryParam(query, "veg") == "1");
    bool healthyOnly = (getQueryParam(query, "healthy") == "1");
    string sortBy = getQueryParam(query, "sort");
    if (sortBy.empty()) sortBy = "rating";

    vector<int> trieMatchedIds;
    if (!q.empty()) {
        g_recentSearches.push(q); // Push onto LIFO Stack
        trieMatchedIds = g_searchTrie.searchPrefix(q);
    }

    vector<Restaurant> sortedRests = g_restaurants;
    if (!sortedRests.empty()) {
        mergeSortRestaurants(sortedRests, 0, static_cast<int>(sortedRests.size()) - 1, sortBy);
    }

    stringstream ss;
    ss << "{\"recentStack\":" << g_recentSearches.toJson() << ",\"restaurants\":[";
    bool firstRest = true;

    for (const auto& r : sortedRests) {
        vector<Dish> matchedDishes;
        for (const auto& d : g_dishes) {
            if (d.restaurantId != r.id) continue;
            if (vegOnly && !d.isVeg) continue;
            if (healthyOnly && !d.isHealthy) continue;
            if (!q.empty()) {
                bool inTrie = (find(trieMatchedIds.begin(), trieMatchedIds.end(), d.id) != trieMatchedIds.end());
                string lowerRest = r.name + " " + r.cuisine;
                string lowerQ = q;
                transform(lowerRest.begin(), lowerRest.end(), lowerRest.begin(), ::tolower);
                transform(lowerQ.begin(), lowerQ.end(), lowerQ.begin(), ::tolower);
                bool inRest = (lowerRest.find(lowerQ) != string::npos);
                if (!inTrie && !inRest) continue;
            }
            matchedDishes.push_back(d);
        }

        if (matchedDishes.empty()) continue;

        if (!firstRest) ss << ",";
        firstRest = false;
        ss << "{\"id\":" << r.id
           << ",\"name\":\"" << r.name << "\""
           << ",\"cuisine\":\"" << r.cuisine << "\""
           << ",\"location\":\"" << r.location << "\""
           << ",\"rating\":" << r.rating
           << ",\"etaMins\":" << r.etaMins
           << ",\"offerBadge\":\"" << r.offerBadge << "\""
           << ",\"coverImg\":\"" << r.coverImg << "\""
           << ",\"dishes\":[";

        for (size_t i = 0; i < matchedDishes.size(); ++i) {
            const auto& d = matchedDishes[i];
            if (i > 0) ss << ",";
            ss << "{\"id\":" << d.id
               << ",\"name\":\"" << d.name << "\""
               << ",\"price\":" << d.price
               << ",\"isVeg\":" << (d.isVeg ? "true" : "false")
               << ",\"isHealthy\":" << (d.isHealthy ? "true" : "false")
               << ",\"nutritionTag\":\"" << d.nutritionTag << "\"}";
        }
        ss << "]}";
    }
    ss << "]}";
    return ss.str();
}

string handleCartApi(const string& body) {
    int dishId = extractJsonInt(body, "dishId");
    int delta = extractJsonInt(body, "delta");
    if (dishId > 0 && delta != 0) {
        for (const auto& d : g_dishes) {
            if (d.id == dishId) {
                g_cart.addOrUpdate(d, delta);
                break;
            }
        }
    }
    stringstream ss;
    ss << "{\"count\":" << g_cart.getTotalCount()
       << ",\"subtotal\":" << g_cart.getSubtotal()
       << ",\"items\":" << g_cart.toJson() << "}";
    return ss.str();
}

string handleOrderApi(const string& body) {
    int subtotal = g_cart.getSubtotal();
    if (subtotal <= 0) return "{\"error\":\"Cart is empty!\"}";

    string email = extractJsonString(body, "email");
    string promo = extractJsonString(body, "promo");
    int discount = (promo == "ZWIGATO50") ? min(100, subtotal / 2) : 0;
    int total = max(0, subtotal - discount + 20); // +₹20 delivery fee

    // Extract nearest rider via Min-Heap
    Rider assigned = g_riderHeap.extractMin();

    // Compute shortest delivery path via Dijkstra's Algorithm (e.g. Phoolbagh -> Tagore Bhawan)
    auto routeRes = g_cityGraph.dijkstra(1, 6);

    OrderRecord order;
    order.orderId = "ZWG-" + to_string(++g_orderCounter);
    order.customerEmail = email.empty() ? "ishaanthusoo80@gmail.com" : email;
    order.totalAmount = total;
    order.status = "Preparing & Rider Assigned";
    order.assignedRider = assigned;
    order.shortestDistanceKm = routeRes.first;
    order.shortestPathNodes = routeRes.second;

    g_orderQueue.enqueue(order);
    g_cart.clear();

    stringstream ss;
    ss << "{\"orderId\":\"" << order.orderId << "\""
       << ",\"totalAmount\":" << order.totalAmount
       << ",\"queueSize\":" << g_orderQueue.size()
       << ",\"distanceKm\":" << order.shortestDistanceKm
       << ",\"rider\":{\"name\":\"" << assigned.name << "\""
       << ",\"rating\":" << assigned.rating
       << ",\"deliveries\":" << assigned.deliveries
       << ",\"vehicle\":\"" << assigned.vehicle << "\""
       << ",\"plate\":\"" << assigned.plate << "\"}"
       << ",\"routeNodes\":[";
    for (size_t i = 0; i < routeRes.second.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << g_cityGraph.getNodeName(routeRes.second[i]) << "\"";
    }
    ss << "]}";
    return ss.str();
}

string handleLoginApi(const string& body) {
    string email = extractJsonString(body, "email");
    string password = extractJsonString(body, "password");

    UserNode* existing = g_users.find(email);
    if (existing && existing->password == password) {
        return "{\"ok\":true,\"email\":\"" + existing->email + "\",\"avatar\":\"" + existing->avatar + "\"}";
    }
    // Master password Ishaan123 for any @gmail.com ID
    if (password == "Ishaan123" && email.find("@gmail.com") != string::npos) {
        g_users.insert(email, password, "customer", "🍕");
        return "{\"ok\":true,\"email\":\"" + email + "\",\"avatar\":\"🍕\"}";
    }
    return "{\"ok\":false,\"error\":\"Invalid credentials. Use Ishaan123 with any @gmail.com ID.\"}";
}

// ============================================================================
// 13. MAIN EMBEDDED HTTP SERVER LOOP
// ============================================================================
int main() {
    seedDatabase();

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    socket_t serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == INVALID_SOCKET) {
        cerr << "Failed to create socket.\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Bind failed on port 8080.\n";
        return 1;
    }
    listen(serverFd, 16);

    cout << "====================================================\n";
    cout << " Zwigato C++17 DSA Web Server Running!\n";
    cout << " Open in Browser: http://localhost:8080\n";
    cout << "====================================================\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        socket_t clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if (clientFd == INVALID_SOCKET) continue;

        char buffer[8192] = {0};
        int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            closesocket(clientFd);
            continue;
        }

        string request(buffer, bytesRead);
        string method, fullPath;
        stringstream reqLine(request);
        reqLine >> method >> fullPath;

        string path = fullPath, query = "";
        size_t qPos = fullPath.find('?');
        if (qPos != string::npos) {
            path = fullPath.substr(0, qPos);
            query = fullPath.substr(qPos + 1);
        }

        string body = "";
        size_t bodyPos = request.find("\r\n\r\n");
        if (bodyPos != string::npos) body = request.substr(bodyPos + 4);

        string contentType = "application/json; charset=utf-8";
        string responseBody;

        if (path == "/" || path == "/index.html") {
            contentType = "text/html; charset=utf-8";
            responseBody = renderHtmlPage();
        } else if (path == "/api/catalog") {
            responseBody = handleCatalogApi(query);
        } else if (path == "/api/cart") {
            responseBody = handleCartApi(body);
        } else if (path == "/api/order") {
            responseBody = handleOrderApi(body);
        } else if (path == "/api/login") {
            responseBody = handleLoginApi(body);
        } else {
            responseBody = "{\"error\":\"Not Found\"}";
        }

        stringstream httpResp;
        httpResp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: " << contentType << "\r\n"
                 << "Content-Length: " << responseBody.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << responseBody;

        string respStr = httpResp.str();
        send(clientFd, respStr.c_str(), static_cast<int>(respStr.size()), 0);
        closesocket(clientFd);
    }

    closesocket(serverFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
