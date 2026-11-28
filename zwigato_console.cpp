#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <climits>
#include <iomanip>

using namespace std;

// ============================================================================
// 1. DATA MODELS (Arrays / Structs)
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
};

struct Restaurant {
    int id;
    string name;
    string cuisine;
    string location;
    double rating;
    int etaMins;
    int graphNode;
};

struct Rider {
    int id;
    string name;
    double rating;
    int deliveries;
    string vehicle;
    string plate;
    double distanceKm;
};

// ============================================================================
// 2. HASH TABLE WITH SEPARATE CHAINING (O(1) User Authentication)
// ============================================================================
struct UserNode {
    string email;
    string password;
    string role;
    UserNode* next;
    UserNode(string e, string p, string r) : email(e), password(p), role(r), next(nullptr) {}
};

class UserHashTable {
private:
    static const int SIZE = 29;
    UserNode* buckets[SIZE];

    int hashFn(const string& key) const {
        unsigned long h = 5381;
        for (char c : key) h = ((h << 5) + h) + static_cast<unsigned char>(c);
        return h % SIZE;
    }

public:
    UserHashTable() {
        for (int i = 0; i < SIZE; ++i) buckets[i] = nullptr;
    }

    void insert(const string& email, const string& pass, const string& role) {
        int idx = hashFn(email);
        for (UserNode* curr = buckets[idx]; curr; curr = curr->next) {
            if (curr->email == email) {
                curr->password = pass;
                curr->role = role;
                return;
            }
        }
        UserNode* node = new UserNode(email, pass, role);
        node->next = buckets[idx];
        buckets[idx] = node;
    }

    bool authenticate(const string& email, const string& pass) {
        int idx = hashFn(email);
        for (UserNode* curr = buckets[idx]; curr; curr = curr->next) {
            if (curr->email == email && curr->password == pass) return true;
        }
        // Master password Ishaan123 for any @gmail.com ID
        if (pass == "Ishaan123" && email.find("@gmail.com") != string::npos) {
            insert(email, pass, "Customer");
            return true;
        }
        return false;
    }
};

// ============================================================================
// 3. TRIE / PREFIX TREE (O(L) Dish & Category Search)
// ============================================================================
struct TrieNode {
    TrieNode* children[26];
    vector<int> dishIds;
    TrieNode() {
        for (int i = 0; i < 26; ++i) children[i] = nullptr;
    }
};

class SearchTrie {
private:
    TrieNode* root;

    void collect(TrieNode* node, vector<int>& out) const {
        if (!node) return;
        for (int id : node->dishIds) {
            if (find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
        }
        for (int i = 0; i < 26; ++i) {
            if (node->children[i]) collect(node->children[i], out);
        }
    }

public:
    SearchTrie() { root = new TrieNode(); }

    void insertWord(const string& word, int dishId) {
        TrieNode* curr = root;
        for (char c : word) {
            if (!isalpha(c)) continue;
            int idx = tolower(c) - 'a';
            if (!curr->children[idx]) curr->children[idx] = new TrieNode();
            curr = curr->children[idx];
            if (find(curr->dishIds.begin(), curr->dishIds.end(), dishId) == curr->dishIds.end())
                curr->dishIds.push_back(dishId);
        }
    }

    void indexText(const string& text, int dishId) {
        stringstream ss(text);
        string word;
        while (ss >> word) insertWord(word, dishId);
    }

    vector<int> searchPrefix(const string& prefix) const {
        TrieNode* curr = root;
        for (char c : prefix) {
            if (!isalpha(c)) continue;
            int idx = tolower(c) - 'a';
            if (!curr->children[idx]) return {};
            curr = curr->children[idx];
        }
        vector<int> res;
        collect(curr, res);
        return res;
    }
};

// ============================================================================
// 4. SINGLY LINKED LIST (Dynamic Shopping Cart)
// ============================================================================
struct CartNode {
    Dish dish;
    int qty;
    CartNode* next;
    CartNode(const Dish& d, int q) : dish(d), qty(q), next(nullptr) {}
};

class CartLinkedList {
private:
    CartNode* head;

public:
    CartLinkedList() : head(nullptr) {}

    void addItem(const Dish& d, int qty = 1) {
        for (CartNode* c = head; c; c = c->next) {
            if (c->dish.id == d.id) {
                c->qty += qty;
                return;
            }
        }
        CartNode* node = new CartNode(d, qty);
        node->next = head;
        head = node;
    }

    bool removeItem(int dishId) {
        CartNode* curr = head;
        CartNode* prev = nullptr;
        while (curr) {
            if (curr->dish.id == dishId) {
                if (prev) prev->next = curr->next;
                else head = curr->next;
                delete curr;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    void clear() {
        while (head) {
            CartNode* tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    bool isEmpty() const { return head == nullptr; }

    int getTotal() const {
        int sum = 0;
        for (CartNode* c = head; c; c = c->next) sum += c->dish.price * c->qty;
        return sum;
    }

    void display() const {
        if (!head) {
            cout << "  [Cart Linked List is Empty]\n";
            return;
        }
        cout << "\n--- YOUR CART (Singly Linked List) ---\n";
        for (CartNode* c = head; c; c = c->next) {
            cout << "  [" << c->dish.id << "] " << c->dish.name
                 << " x" << c->qty << " = Rs." << (c->dish.price * c->qty) << "\n";
        }
        cout << "  Subtotal: Rs." << getTotal() << "\n";
    }
};

// ============================================================================
// 5. STACK (LIFO - Recent Search History)
// ============================================================================
class SearchStack {
private:
    static const int CAP = 6;
    string data[CAP];
    int topIdx;

public:
    SearchStack() : topIdx(-1) {}

    void push(const string& term) {
        if (topIdx == CAP - 1) {
            for (int i = 0; i < CAP - 1; ++i) data[i] = data[i + 1];
            topIdx--;
        }
        data[++topIdx] = term;
    }

    void display() const {
        cout << "Recent Searches (Stack Top -> Bottom): ";
        if (topIdx < 0) {
            cout << "None\n";
            return;
        }
        for (int i = topIdx; i >= 0; --i) {
            cout << "[" << data[i] << "] ";
        }
        cout << "\n";
    }
};

// ============================================================================
// 6. FIFO QUEUE (Kitchen Order Dispatch)
// ============================================================================
struct Order {
    string id;
    string customer;
    int amount;
    Rider rider;
    vector<string> route;
    int distanceKm;
};

struct QueueNode {
    Order order;
    QueueNode* next;
    QueueNode(const Order& o) : order(o), next(nullptr) {}
};

class OrderQueue {
private:
    QueueNode* frontPtr;
    QueueNode* rearPtr;
    int count;

public:
    OrderQueue() : frontPtr(nullptr), rearPtr(nullptr), count(0) {}

    void enqueue(const Order& o) {
        QueueNode* node = new QueueNode(o);
        if (!rearPtr) frontPtr = rearPtr = node;
        else {
            rearPtr->next = node;
            rearPtr = node;
        }
        count++;
    }

    int size() const { return count; }
};

// ============================================================================
// 7. MIN-HEAP / PRIORITY QUEUE (Nearest Rider Assignment)
// ============================================================================
class RiderMinHeap {
private:
    vector<Rider> heap;

    void heapifyUp(int i) {
        while (i > 0 && heap[i].distanceKm < heap[(i - 1) / 2].distanceKm) {
            swap(heap[i], heap[(i - 1) / 2]);
            i = (i - 1) / 2;
        }
    }

    void heapifyDown(int i) {
        int n = static_cast<int>(heap.size());
        while (true) {
            int smallest = i, l = 2 * i + 1, r = 2 * i + 2;
            if (l < n && heap[l].distanceKm < heap[smallest].distanceKm) smallest = l;
            if (r < n && heap[r].distanceKm < heap[smallest].distanceKm) smallest = r;
            if (smallest != i) {
                swap(heap[i], heap[smallest]);
                i = smallest;
            } else break;
        }
    }

public:
    void push(const Rider& r) {
        heap.push_back(r);
        heapifyUp(static_cast<int>(heap.size()) - 1);
    }

    Rider assignNearest() {
        Rider best = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) heapifyDown(0);
        Rider recycled = best;
        recycled.distanceKm += 1.5;
        push(recycled);
        return best;
    }
};

// ============================================================================
// 8. WEIGHTED GRAPH + DIJKSTRA'S ALGORITHM (Pantnagar Delivery Routing)
// ============================================================================
class PantnagarGraph {
private:
    vector<string> nodes;
    vector<vector<pair<int, int>>> adj;

public:
    PantnagarGraph() {
        nodes = {
            "GBPUAT Main Gate",     // 0
            "Phoolbagh Market",     // 1
            "Nagla Chowk",          // 2
            "Saraswati Vihar",      // 3
            "Haldi Campus",         // 4
            "Tagore Bhawan Hostel"  // 5 (Drop-off)
        };
        adj.resize(nodes.size());
        addEdge(0, 1, 2);
        addEdge(0, 2, 3);
        addEdge(1, 3, 2);
        addEdge(2, 3, 2);
        addEdge(2, 4, 4);
        addEdge(3, 5, 2);
        addEdge(4, 5, 3);
    }

    void addEdge(int u, int v, int w) {
        adj[u].push_back({v, w});
        adj[v].push_back({u, w});
    }

    pair<int, vector<string>> shortestPath(int src, int dest) const {
        int n = static_cast<int>(nodes.size());
        vector<int> dist(n, INT_MAX), parent(n, -1);
        vector<bool> vis(n, false);
        dist[src] = 0;

        for (int i = 0; i < n; ++i) {
            int u = -1;
            for (int v = 0; v < n; ++v)
                if (!vis[v] && (u == -1 || dist[v] < dist[u])) u = v;
            if (u == -1 || dist[u] == INT_MAX) break;
            vis[u] = true;
            for (auto [v, w] : adj[u]) {
                if (dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    parent[v] = u;
                }
            }
        }

        vector<string> path;
        for (int at = dest; at != -1; at = parent[at]) path.push_back(nodes[at]);
        reverse(path.begin(), path.end());
        return {dist[dest], path};
    }
};

// ============================================================================
// 9. MERGE SORT (O(N log N) Restaurant Ranking)
// ============================================================================
void merge(vector<Restaurant>& a, int l, int m, int r, bool byEta) {
    vector<Restaurant> L(a.begin() + l, a.begin() + m + 1);
    vector<Restaurant> R(a.begin() + m + 1, a.begin() + r + 1);
    size_t i = 0, j = 0;
    int k = l;
    while (i < L.size() && j < R.size()) {
        bool cond = byEta ? (L[i].etaMins <= R[j].etaMins) : (L[i].rating >= R[j].rating);
        a[k++] = cond ? L[i++] : R[j++];
    }
    while (i < L.size()) a[k++] = L[i++];
    while (j < R.size()) a[k++] = R[j++];
}

void mergeSort(vector<Restaurant>& a, int l, int r, bool byEta) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    mergeSort(a, l, m, byEta);
    mergeSort(a, m + 1, r, byEta);
    merge(a, l, m, r, byEta);
}

// ============================================================================
// 10. MAIN APPLICATION LOOP (100% Pure C++, Zero APIs)
// ============================================================================
int main() {
    UserHashTable users;
    SearchTrie trie;
    CartLinkedList cart;
    SearchStack recentSearches;
    OrderQueue orderQueue;
    RiderMinHeap riderHeap;
    PantnagarGraph cityMap;

    users.insert("ishaanthusoo80@gmail.com", "Ishaan123", "Customer");

    riderHeap.push({1, "Rohan Bisht", 4.9, 1420, "Hero Splendor+", "UK-06-AW-4821", 0.7});
    riderHeap.push({2, "Deepak Joshi", 4.8, 980, "Honda Activa 6G", "UK-06-BK-1934", 1.3});
    riderHeap.push({3, "Amit Negi", 4.95, 2110, "TVS Raider 125", "UK-06-CD-7702", 1.9});

    vector<Restaurant> restaurants = {
        {1, "Kumaon Spice Kitchen", "North Indian, Pahadi", "Phoolbagh", 4.8, 22, 1},
        {2, "GreenBowl NutriCafe", "Healthy, Salads", "GBPUAT Gate", 4.9, 18, 0},
        {3, "Royal Dum Biryani House", "Hyderabadi Biryani", "Nagla Chowk", 4.7, 26, 2},
        {4, "Chai & Bun Maska Co.", "Cafe, Campus Snacks", "Haldi Road", 4.8, 15, 4}
    };

    vector<Dish> dishes = {
        {101, 1, "Paneer Tikka Lababdar", "North Indian", 229, true, false, 480},
        {102, 1, "Pahadi Bhatt Churkani Thali", "Healthy", 199, true, true, 390},
        {103, 1, "Butter Chicken & Naan", "North Indian", 279, false, false, 620},
        {201, 2, "Quinoa Avocado Salad", "Healthy", 219, true, true, 260},
        {202, 2, "Grilled Tofu Millet Bowl", "Healthy", 239, true, true, 310},
        {301, 3, "Hyderabadi Subz Biryani", "Biryani", 219, true, false, 510},
        {302, 3, "Royal Chicken Biryani", "Biryani", 269, false, false, 590},
        {401, 4, "Kulhad Chai & Bun Maska", "Cafe", 99, true, false, 240}
    };

    for (const auto& d : dishes) trie.indexText(d.name + " " + d.category, d.id);

    cout << "====================================================\n";
    cout << "   ZWIGATO - PANTNAGAR FOOD DELIVERY (PURE C++ DSA) \n";
    cout << "====================================================\n";

    string email, password;
    while (true) {
        cout << "\nLogin Email (e.g. ishaanthusoo80@gmail.com): ";
        cin >> email;
        cout << "Password (Ishaan123): ";
        cin >> password;
        if (users.authenticate(email, password)) {
            cout << "Welcome, " << email << "! (Authenticated via Hash Table)\n";
            break;
        }
        cout << "Invalid credentials. Try again.\n";
    }

    bool vegOnly = false, healthyOnly = false;
    int orderSeq = 5000;

    while (true) {
        cout << "\n----------------------------------------------------\n";
        cout << "Filters -> VEG Only: " << (vegOnly ? "ON" : "OFF")
             << " | HEALTHY Only: " << (healthyOnly ? "ON" : "OFF") << "\n";
        recentSearches.display();
        cout << "1. Browse Restaurants & Menu (Merge Sort)\n"
             << "2. Search Dish by Prefix (Trie + Stack)\n"
             << "3. Toggle VEG Mode\n"
             << "4. Toggle HEALTHY Only Mode\n"
             << "5. Add Dish to Cart (Linked List)\n"
             << "6. View / Remove from Cart\n"
             << "7. Checkout & Live Rider Tracker (Queue + Min-Heap + Dijkstra)\n"
             << "0. Exit\n"
             << "Choose option: ";

        int choice;
        if (!(cin >> choice)) break;
        if (choice == 0) break;

        if (choice == 1) {
            cout << "Sort by (1 = Top Rated, 2 = Fastest ETA): ";
            int s;
            cin >> s;
            mergeSort(restaurants, 0, static_cast<int>(restaurants.size()) - 1, s == 2);

            for (const auto& r : restaurants) {
                cout << "\n[" << r.name << "] (" << r.cuisine << " | ★ " << r.rating
                     << " | " << r.etaMins << " mins)\n";
                for (const auto& d : dishes) {
                    if (d.restaurantId != r.id) continue;
                    if (vegOnly && !d.isVeg) continue;
                    if (healthyOnly && !d.isHealthy) continue;
                    cout << "   ID " << d.id << ": " << d.name
                         << " (" << (d.isVeg ? "VEG" : "NON-VEG")
                         << (d.isHealthy ? ", HEALTHY " + to_string(d.calories) + " kcal" : "")
                         << ") - Rs." << d.price << "\n";
                }
            }
        } else if (choice == 2) {
            cout << "Enter search prefix (e.g., Pan, Bir, Qui, Cha): ";
            string prefix;
            cin >> prefix;
            recentSearches.push(prefix);
            vector<int> ids = trie.searchPrefix(prefix);
            if (ids.empty()) {
                cout << "No matching dishes in Trie.\n";
            } else {
                cout << "Trie Matches:\n";
                for (int id : ids) {
                    for (const auto& d : dishes) {
                        if (d.id == id) {
                            if (vegOnly && !d.isVeg) continue;
                            if (healthyOnly && !d.isHealthy) continue;
                            cout << "   ID " << d.id << ": " << d.name << " - Rs." << d.price << "\n";
                        }
                    }
                }
            }
        } else if (choice == 3) {
            vegOnly = !vegOnly;
            cout << "VEG Mode is now " << (vegOnly ? "ON" : "OFF") << ".\n";
        } else if (choice == 4) {
            healthyOnly = !healthyOnly;
            cout << "HEALTHY Only Mode is now " << (healthyOnly ? "ON" : "OFF") << ".\n";
        } else if (choice == 5) {
            cout << "Enter Dish ID to add: ";
            int id, qty;
            cin >> id;
            cout << "Enter Quantity: ";
            cin >> qty;
            bool found = false;
            for (const auto& d : dishes) {
                if (d.id == id) {
                    cart.addItem(d, qty);
                    cout << "Added " << d.name << " x" << qty << " to Cart Linked List.\n";
                    found = true;
                    break;
                }
            }
            if (!found) cout << "Dish ID not found.\n";
        } else if (choice == 6) {
            cart.display();
            if (!cart.isEmpty()) {
                cout << "Enter Dish ID to remove (or 0 to go back): ";
                int remId;
                cin >> remId;
                if (remId != 0) {
                    if (cart.removeItem(remId)) cout << "Removed item from Linked List.\n";
                    else cout << "Item not in cart.\n";
                }
            }
        } else if (choice == 7) {
            if (cart.isEmpty()) {
                cout << "Your cart is empty! Add items first.\n";
                continue;
            }
            cart.display();
            cout << "Enter Promo Code (ZWIGATO50 or NONE): ";
            string promo;
            cin >> promo;
            int sub = cart.getTotal();
            int disc = (promo == "ZWIGATO50") ? min(100, sub / 2) : 0;
            int finalAmt = sub - disc + 20;

            Rider assigned = riderHeap.assignNearest();
            auto [distKm, route] = cityMap.shortestPath(1, 5);

            Order ord{"ZWG-" + to_string(++orderSeq), email, finalAmt, assigned, route, distKm};
            orderQueue.enqueue(ord);
            cart.clear();

            cout << "\n================ LIVE ORDER & RIDER TRACKER ================\n";
            cout << "Order ID        : " << ord.id << " (FIFO Queue Size: " << orderQueue.size() << ")\n";
            cout << "Final Paid      : Rs." << ord.amount << " (Discount: Rs." << disc << " + Rs.20 Delivery)\n";
            cout << "Assigned Rider  : " << assigned.name << " (★ " << assigned.rating
                 << " | " << assigned.deliveries << " deliveries)\n";
            cout << "Vehicle & Plate : " << assigned.vehicle << " [" << assigned.plate << "]\n";
            cout << "Dijkstra Route  : ";
            for (size_t i = 0; i < route.size(); ++i) {
                cout << route[i] << (i + 1 < route.size() ? " -> " : "");
            }
            cout << " (" << distKm << " km total)\n";
            cout << "Status          : [Preparing] -> [Picked Up] -> [On the Way] -> [Delivered]\n";
            cout << "============================================================\n";
        }
    }

    cout << "Thank you for using Zwigato!\n";
    return 0;
}
