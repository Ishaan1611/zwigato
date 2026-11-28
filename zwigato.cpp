// Zwigato: food delivery website written in C++17. No libraries, no HTML files.
// The C++ program is the web server AND the page generator.
// Build:  g++ -std=c++17 -O2 zwigato.cpp -o zwigato      Run: ./zwigato   Open: http://localhost:8080
// (Linux / macOS / WSL. It uses POSIX sockets.)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
using namespace std;

// =====================  DATA STRUCTURES (all hand-written)  =====================
// 1. LINKED LIST: menu of every restaurant, order history of every customer
template<class T> struct List {
  struct N { T v; N* nx; }; N *h = 0, *t = 0; int n = 0;
  void pushFront(T v) { h = new N{v, h}; if (!t) t = h; n++; }
  void pushBack(T v)  { N* x = new N{v, 0}; if (t) t->nx = x; else h = x; t = x; n++; }
};
// 2. QUEUE (FIFO, linked): one queue of incoming orders per restaurant
template<class T> struct Queue {
  struct N { T v; N* nx; }; N *f = 0, *b = 0;
  void enqueue(T v) { N* x = new N{v, 0}; if (b) b->nx = x; else f = x; b = x; }
  bool dequeue(T& v) { if (!f) return false; v = f->v; N* d = f; f = f->nx; if (!f) b = 0; delete d; return true; }
  bool empty() { return !f; }
};
// 3. STACK (array based): cart "Undo" and "Recent searches"
template<class T, int CAP> struct Stack {
  T a[CAP]; int top = 0;
  bool push(T v) { if (top == CAP) return false; a[top++] = v; return true; }
  bool pop(T& v) { if (!top) return false; v = a[--top]; return true; }
  void pushUnique(T v) {                       // newest on top, no duplicates, oldest dropped when full
    for (int i = 0; i < top; i++) if (a[i] == v) { for (int j = i; j < top - 1; j++) a[j] = a[j + 1]; top--; break; }
    if (top == CAP) { for (int j = 0; j < top - 1; j++) a[j] = a[j + 1]; top--; }
    a[top++] = v;
  }
};
// 4. HASH TABLE (separate chaining): users by email, sessions by cookie id
struct HT {
  struct E { string k; int v; E* nx; }; E* b[64] = {0};
  static unsigned h(const string& s) { unsigned x = 5381; for (unsigned char c : s) x = x * 33 + c; return x % 64; }
  void put(const string& k, int v) { for (E* e = b[h(k)]; e; e = e->nx) if (e->k == k) { e->v = v; return; } unsigned i = h(k); b[i] = new E{k, v, b[i]}; }
  int get(const string& k) { for (E* e = b[h(k)]; e; e = e->nx) if (e->k == k) return e->v; return -1; }
};
// 5. TRIE: search by prefix over restaurant names and dish names
struct Trie {
  struct N { N* c[27] = {0}; vector<int> refs; }; N* root = new N;
  static int ix(char ch) { ch = tolower((unsigned char)ch); return (ch >= 'a' && ch <= 'z') ? ch - 'a' : 26; }
  void insert(const string& text, int ref) {
    size_t i = 0;
    while (i < text.size()) {
      while (i < text.size() && !isalnum((unsigned char)text[i])) i++;
      N* n = root;
      while (i < text.size() && isalnum((unsigned char)text[i])) { int k = ix(text[i++]); if (!n->c[k]) n->c[k] = new N; n = n->c[k]; if (std::find(n->refs.begin(), n->refs.end(), ref) == n->refs.end()) n->refs.push_back(ref); }
    }
  }
  vector<int> find(const string& prefix) { N* n = root; for (char ch : prefix) { if (!isalnum((unsigned char)ch)) break; n = n->c[ix(ch)]; if (!n) return {}; } return n->refs; }
};
// 6. MIN-HEAP (priority queue): picks the fastest delivery first
struct HeapItem { int key, rid, did; };
struct MinHeap {
  vector<HeapItem> a;
  void push(HeapItem x) { a.push_back(x); int i = a.size() - 1; while (i && a[(i - 1) / 2].key > a[i].key) { swap(a[i], a[(i - 1) / 2]); i = (i - 1) / 2; } }
  bool pop(HeapItem& out) {
    if (a.empty()) return false; out = a[0]; a[0] = a.back(); a.pop_back(); int i = 0, n = a.size();
    for (;;) { int l = 2 * i + 1, r = l + 1, m = i; if (l < n && a[l].key < a[m].key) m = l; if (r < n && a[r].key < a[m].key) m = r; if (m == i) break; swap(a[i], a[m]); i = m; }
    return true;
  }
};

// =====================  DATA MODEL  =====================
struct Dish { int id; string n; int p; char dt; bool gf, fl, sp, out; int sold; };   // dt: V veg, G vegan, N non-veg
struct Rest { int id; string n, c; int eta; double km, rate; string em; int hue; bool far; List<Dish> menu; Queue<int> q; int next = 0; };
struct Order { int id, rid, uid, total, st; string uname; vector<pair<int,int>> it; };   // st: 0 placed 1 cooking 2 on the way 3 delivered 4 cancelled
struct User { string email, name, pass; int role, rid; bool veg; List<int> hist; Stack<int, 64> undo; int cart[1300]; int crid; string promo; int tip; Stack<string, 6> recent; };

struct DD { const char *n; int p; char dt; const char* fl; };
struct RD { const char *n, *c; int eta; double km, rate; const char* em; int hue; DD d[4]; };
static const RD RAW[12] = {
 {"Welcome Restaurant","North Indian",22,1.1,4.7,"🍛",20,{{"Dal Makhani",190,'V',"s"},{"Paneer Tikka Masala",240,'V',"g"},{"Garlic Butter Naan",55,'V',""},{"Gulab Jamun",70,'V',""}}},
 {"Fartyal Restaurant","Homestyle Thali",18,.8,4.5,"🍱",45,{{"Kumaoni Rajma Chawal",130,'G',"gs"},{"Deluxe Veg Thali",180,'V',"fs"},{"Aloo Gutke",110,'G',"g"},{"Bal Mithai",95,'V',"f"}}},
 {"The Pizza Bite","Italian & Pizza",25,1.5,4.6,"🍕",8,{{"Farmhouse Overload Pizza",260,'V',"fs"},{"Peri Peri Paneer Pizza",280,'V',""},{"White Sauce Alfredo Pasta",190,'V',"s"},{"Choco Lava Cake",95,'V',""}}},
 {"Satnaam Ji Burger & Momos","Fast Food & Chinese",16,.5,4.4,"🥟",35,{{"Crispy Aloo Tikki Burger",70,'V',""},{"Steamed Veg Momos (8 pcs)",80,'G',"s"},{"Tandoori Kurkure Momos",120,'V',"f"},{"Schezwan Hakka Noodles",140,'G',""}}},
 {"Moments Cafe","Cafe & Shakes",20,.9,4.8,"☕",28,{{"Hazelnut Cold Coffee",130,'V',"gs"},{"Pesto Penne Pasta",195,'V',"f"},{"Avocado & Herb Toast",165,'G',"s"},{"Blueberry Cheesecake",160,'V',"f"}}},
 {"Kesari Sweets & Snacks","Street Food & Mithai",15,.7,4.7,"🥠",50,{{"Hyderabadi Veg Dum Biryani",165,'V',"gfs"},{"Crispy Punjabi Samosa (2 pcs)",40,'G',""},{"Kulhad Masala Chai",30,'V',"g"},{"Chole Bhature",110,'V',"s"}}},
 {"The Chocolate Room","Desserts & Cafe",24,1.4,4.6,"🍫",15,{{"Signature Belgian Hot Chocolate",155,'V',"gs"},{"Four-Cheese Margherita",230,'V',"f"},{"Nutella Waffle Sundae",185,'V',"fs"},{"Brownie Sizzler",210,'V',""}}},
 {"Chotu Momos","Momos & Soups",14,.4,4.5,"🥡",160,{{"Juicy Chicken Steamed Momos",95,'N',"s"},{"Fiery Chilli Garlic Momos",110,'G',"f"},{"Himalayan Hot & Sour Soup",75,'G',"g"},{"Paneer Afghani Momos",125,'V',"s"}}},
 {"Nainital Lakeview Sushi & Bowls","Asian & Sushi",38,6.2,4.9,"🍣",190,{{"Avocado Cucumber Maki Roll",290,'G',"gfs"},{"Spicy Salmon Crunch Roll",360,'N',"f"},{"Teriyaki Tofu Donburi Bowl",260,'G',"gs"},{"Miso Wakame Soup",120,'G',"g"}}},
 {"Rudrapur Woodfire Artisans","Italian & Pizza",35,5.8,4.8,"🍕",5,{{"Burrata & Sweet Basil Pizza",340,'V',"fs"},{"Truffle Mushroom Sourdough",320,'V',"f"},{"Smoked Chicken Pesto Pizza",360,'N',"s"},{"Classic Espresso Tiramisu",190,'V',""}}},
 {"Kumaon Royal Kebabs","North Indian",40,6.5,4.7,"🍢",25,{{"Charcoal Chicken Seekh Platter",310,'N',"gfs"},{"Awadhi Mutton Dum Biryani",340,'N',"gf"},{"Dahi Ke Sholay",210,'V',"s"},{"Malai Broccoli Tikka",230,'V',"gf"}}},
 {"Haldwani Vegan Kitchen","Homestyle Thali",36,6.9,4.8,"🥗",130,{{"Quinoa & Roasted Millet Bowl",240,'G',"gfs"},{"Jackfruit Kathal Biryani",220,'G',"gf"},{"Almond-Milk Matcha Latte",155,'G',"gs"},{"Coconut Jaggery Chia Pudding",145,'G',"g"}}}};
struct Cat { const char *n, *em, *kw; };
static const Cat CATS[] = {{"Momos","🥟","momo"},{"Pizza","🍕","pizza"},{"Thali","🍱","thali"},{"Biryani","🍚","biryani"},{"Cafe","☕","cafe|coffee"},{"Desserts","🍰","dessert|chocolate|cake|mithai"},{"Sushi","🍣","sushi|asian"},{"Burger","🍔","burger"},{"Noodles","🍜","noodle"},{"Soup","🍲","soup"}};
static const char* EMO[][2] = {{"momo","🥟"},{"pizza","🍕"},{"pasta","🍝"},{"penne","🍝"},{"biryani","🍚"},{"chawal","🍚"},{"naan","🫓"},{"bhature","🫓"},{"toast","🫓"},{"coffee","☕"},{"chai","☕"},{"latte","☕"},{"chocolate","🍫"},{"brownie","🍫"},{"cake","🍰"},{"tiramisu","🍰"},{"pudding","🍰"},{"sundae","🍰"},{"waffle","🍰"},{"jamun","🍮"},{"mithai","🍮"},{"sushi","🍣"},{"maki","🍣"},{"soup","🍲"},{"burger","🍔"},{"noodle","🍜"},{"thali","🍱"},{"tikka","🍢"},{"kebab","🍢"},{"seekh","🍢"},{"samosa","🥟"},{"bowl","🥗"},{"paneer","🍛"},{"dal","🍛"}};

Rest R[12]; Trie TR; HT users, sessions; vector<User*> U; vector<Order> O; int freqSold[1300];
int curUser = -1;   // user of the request being handled

// =====================  SMALL HELPERS  =====================
static string esc(const string& s) { string o; for (char c : s) { if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c; } return o; }
static string lower(string s) { for (auto& c : s) c = tolower((unsigned char)c); return s; }
static bool anyKw(const string& text, const string& kws) {                    // "a|b|c" keyword match
  string t = lower(text); size_t i = 0;
  while (i <= kws.size()) { size_t j = kws.find('|', i); if (j == string::npos) j = kws.size(); if (t.find(kws.substr(i, j - i)) != string::npos) return true; i = j + 1; }
  return false;
}
static string emo(const string& n) { string t = lower(n); for (auto& e : EMO) if (t.find(e[0]) != string::npos) return e[1]; return "🍽️"; }
static int eff(const Dish& d) { return d.fl ? d.p * 3 / 4 : d.p; }
static string inr(int n) { return "₹" + to_string(n); }
static Dish* findDish(int did) { int rid = did / 100; if (rid < 1 || rid > 12) return 0; for (auto* x = R[rid - 1].menu.h; x; x = x->nx) if (x->v.id == did) return &x->v; return 0; }
static string urlDecode(const string& s) { string o; for (size_t i = 0; i < s.size(); i++) { if (s[i] == '+') o += ' '; else if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), 0, 16); i += 2; } else o += s[i]; } return o; }
static string param(const string& q, const string& k) {
  size_t i = 0; while (i < q.size()) { size_t j = q.find('&', i); if (j == string::npos) j = q.size(); size_t e = q.find('=', i);
    if (e != string::npos && e < j && q.substr(i, e - i) == k) return urlDecode(q.substr(e + 1, j - e - 1)); i = j + 1; } return "";
}
static string enc(const string& s) { string o; char b[4]; for (unsigned char c : s) { if (isalnum(c)) o += c; else { snprintf(b, 4, "%%%02X", c); o += b; } } return o; }

// =====================  CSS (street-poster design, same as the HTML version)  =====================
static const char* CSS = R"CSS(
:root{--bg:#FFE45E;--card:#fff;--ink:#111;--mut:#4A4636;--or:#FF4D2E;--gr:#0FA968;--bd:#111;--pk:#FF7AB6;--bl:#3A56F0;--mn:#5CE1A8;--line:rgba(17,17,17,.18);--hard:6px 6px 0 var(--bd)}
@media(prefers-color-scheme:dark){:root{--bg:#171717;--card:#242424;--ink:#F6F6F0;--mut:#B8B8A8;--bd:#FFE45E;--line:rgba(246,246,240,.18)}}
*{box-sizing:border-box;font-family:'Space Grotesk',system-ui,sans-serif}
h1,h2,h3,.logo,.bt,.stk{font-family:'Archivo Black',Impact,sans-serif;font-weight:400;margin:0}
body{margin:0;background:var(--bg) radial-gradient(color-mix(in srgb,var(--bd) 20%,transparent) 1.3px,transparent 1.6px) 0 0/22px 22px;color:var(--ink);font-size:15px;line-height:1.5;overflow-x:hidden}
a{color:inherit;text-decoration:none}.wrap{max-width:1160px;margin:0 auto;padding:0 18px 120px}
.nav{position:sticky;top:12px;z-index:30;margin:12px auto 0;max-width:1124px;display:flex;justify-content:space-between;align-items:center;gap:8px;padding:9px 12px 9px 18px;border:2.5px solid var(--bd);border-radius:18px;box-shadow:var(--hard);background:var(--card)}
.logo{font-size:23px;text-transform:uppercase;display:flex;align-items:center;gap:9px}.logo b{display:grid;place-items:center;width:32px;height:32px;border-radius:10px;background:var(--or);color:#fff;border:2.5px solid var(--bd);transform:rotate(-7deg);font-size:18px}
.btn{display:inline-flex;align-items:center;gap:6px;font-weight:700;font-size:13px;border:2.5px solid var(--bd);border-radius:12px;padding:8px 14px;background:var(--card);color:var(--ink);box-shadow:3px 3px 0 var(--bd);cursor:pointer}
.btn:hover{transform:translate(-1px,-1px);box-shadow:4px 4px 0 var(--bd)}.btn:active{transform:translate(3px,3px);box-shadow:none}
.btn.p{background:var(--or);color:#111}.btn.on{background:var(--ink);color:var(--bg)}.btn.sm{padding:5px 11px;font-size:12px}.btn.w{width:100%;justify-content:center;padding:13px}
.mq{margin:18px 0 0;overflow:hidden;border:2.5px solid var(--bd);background:#111;color:#FFE45E;border-radius:14px;box-shadow:4px 4px 0 var(--bd)}.mq div{display:flex;width:max-content;padding:9px 0;font:400 14px 'Archivo Black';text-transform:uppercase;white-space:nowrap;animation:mqs 28s linear infinite}.mq span{padding:0 24px}@keyframes mqs{to{transform:translateX(-50%)}}
.bento{display:grid;gap:18px;margin:18px 0}@media(min-width:900px){.bento{grid-template-columns:1.7fr 1fr}}
.hero{position:relative;background:var(--bl);color:#fff;border:2.5px solid var(--bd);border-radius:24px;box-shadow:9px 9px 0 var(--bd);padding:clamp(22px,4vw,42px);overflow:hidden}
.hero::after{content:"";position:absolute;inset:0;background:radial-gradient(rgba(255,255,255,.2) 2px,transparent 2.5px) 0 0/18px 18px;pointer-events:none}.hero>*{position:relative;z-index:1}
.hero h1{font-size:clamp(40px,6.6vw,80px);line-height:.95;text-transform:uppercase;color:#FFE45E;-webkit-text-stroke:2.5px #111;paint-order:stroke fill;text-shadow:5px 5px 0 #111;max-width:12ch}.hero h1 em{font-style:normal;color:#fff}
.hero p{margin:12px 0 18px;max-width:46ch}.stk{position:absolute!important;z-index:2!important;width:100px;height:100px;border-radius:50%;display:grid;place-items:center;text-align:center;background:#FFE45E;color:#111;font-size:13px;line-height:1.05;text-transform:uppercase;border:2.5px solid #111;box-shadow:4px 4px 0 #111;transform:rotate(12deg);right:22px;top:18px}
@media(max-width:899px){.stk{display:none}}
.sb{display:flex;gap:6px;background:#fff;border:2.5px solid #111;border-radius:14px;box-shadow:4px 4px 0 #111;padding:4px 4px 4px 14px;max-width:560px;align-items:center}.sb input{flex:1;min-width:0;border:0;outline:0;font:600 15px 'Space Grotesk';color:#111;background:transparent;padding:8px 0}.sb .btn{box-shadow:none}
.moods{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}.moods a{background:#fff;color:#111;border:2.5px solid #111;border-radius:10px;box-shadow:2px 2px 0 #111;padding:4px 12px;font-weight:700;font-size:12.5px}
.side{display:grid;gap:18px}.fl{background:var(--pk);color:#111;border:2.5px solid var(--bd);border-radius:22px;box-shadow:var(--hard);padding:20px;transform:rotate(1deg)}.fl h3{font-size:22px;text-transform:uppercase;margin:4px 0}.fl s{opacity:.7}.fl .pr{font:400 30px 'Archivo Black'}
.lv{background:var(--card);border:2.5px solid var(--bd);border-radius:22px;box-shadow:var(--hard);padding:18px 20px;transform:rotate(-1deg)}.seg{display:grid;grid-template-columns:repeat(4,1fr);gap:5px;margin:10px 0}.seg i{height:12px;border:2px solid var(--bd);border-radius:6px}.seg i.on{background:var(--mn)}
.promo{overflow:hidden;border:2.5px solid var(--bd);border-radius:22px;box-shadow:var(--hard);margin:6px 0}.pt{display:flex;width:400%;animation:slide 20s infinite}.ps{width:25%;padding:24px 28px;min-height:150px;color:#111;display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap}.ps h3{font-size:clamp(22px,3.6vw,32px);text-transform:uppercase}
.ps .code{display:inline-block;border:2.5px dashed #111;background:#fff;border-radius:10px;padding:2px 12px;font-weight:800;letter-spacing:.08em;margin-top:8px}
@keyframes slide{0%,20%{transform:translateX(0)}25%,45%{transform:translateX(-25%)}50%,70%{transform:translateX(-50%)}75%,95%{transform:translateX(-75%)}100%{transform:translateX(0)}}
.sec{margin:28px 0 14px;display:flex;justify-content:space-between;align-items:end;gap:10px}.sec h2{font-size:clamp(24px,3.6vw,38px);text-transform:uppercase;background:linear-gradient(transparent 62%,var(--pk) 62%);padding:0 6px}.sec small{font-weight:700;color:var(--mut)}
.cats{display:flex;gap:14px;overflow-x:auto;padding:10px 2px 12px}.cat{flex:none;display:grid;justify-items:center;gap:6px;font-weight:700;font-size:11px;text-transform:uppercase}.cat span{width:64px;height:64px;border-radius:18px;display:grid;place-items:center;font-size:30px;background:var(--card);border:2.5px solid var(--bd);box-shadow:3px 3px 0 var(--bd)}.cat.on span{background:var(--pk)}.cat:hover span{transform:rotate(-6deg)}
.pills{display:flex;gap:8px;flex-wrap:wrap;margin:6px 0 4px}
.grid{display:grid;gap:20px;grid-template-columns:repeat(auto-fill,minmax(285px,1fr))}.rc{position:relative;display:block;background:var(--card);border:2.5px solid var(--bd);border-radius:20px;box-shadow:var(--hard);overflow:hidden}.rc:nth-child(3n+1){transform:rotate(-.7deg)}.rc:nth-child(3n+2){transform:rotate(.5deg)}.rc:hover{transform:translate(-3px,-3px);box-shadow:10px 10px 0 var(--bd)}
.art{height:140px;position:relative;display:grid;place-items:center;border-bottom:2.5px solid var(--bd);overflow:hidden}.art::before{content:"";position:absolute;inset:0;background:radial-gradient(rgba(255,255,255,.38) 2px,transparent 2.5px) 0 0/16px 16px}.art b{font-size:70px;filter:drop-shadow(4px 4px 0 var(--bd));position:relative}
.bd{position:absolute;top:12px;font-size:11.5px;font-weight:800;border:2px solid var(--bd);background:#fff;color:#111;border-radius:8px;box-shadow:2px 2px 0 var(--bd);padding:1px 9px}.bd.l{left:12px}.bd.r{right:12px;background:#FFE45E}
.rb{padding:14px 18px 16px}.rb h3{font-size:17px;text-transform:uppercase}.rb .sub{color:var(--mut);font-size:13px}.mt{margin-top:8px;font-weight:700;font-size:12.5px}.rt{background:#FFE45E;color:#111;border:2px solid var(--bd);border-radius:8px;padding:0 8px}
.mh{background:var(--card);border:2.5px solid var(--bd);border-radius:20px;box-shadow:var(--hard);overflow:hidden;margin:14px 0}.mh .in{padding:16px 22px}
.dish{display:flex;align-items:center;gap:14px;padding:14px 20px;border-top:2px dashed var(--line)}.dish.out{opacity:.5}.tile{width:62px;height:62px;border-radius:14px;display:grid;place-items:center;font-size:32px;background:#FFE45E;border:2.5px solid var(--bd);box-shadow:3px 3px 0 var(--bd);flex:none}
.nm{flex:1;min-width:0;font-weight:800}.vm{display:inline-block;width:14px;height:14px;border:1.6px solid var(--gr);border-radius:4px;position:relative;top:2px}.vm::after{content:"";position:absolute;inset:2.5px;border-radius:50%;background:var(--gr)}.vm.n{border-color:#D92D20}.vm.n::after{background:#D92D20}
.tg span{font-size:10px;font-weight:800;text-transform:uppercase;border:2px solid var(--bd);border-radius:6px;padding:0 7px;margin-right:4px;display:inline-block}.tg .f{background:#FFE45E;color:#111}.tg .s{background:var(--pk);color:#111}
.stp{display:inline-flex;align-items:center;gap:4px;background:var(--or);color:#111;border:2px solid var(--bd);border-radius:12px;box-shadow:2px 2px 0 var(--bd);padding:2px}.stp a{width:28px;text-align:center;font-weight:800;font-size:17px}
.box{background:var(--card);border:2.5px solid var(--bd);border-radius:18px;box-shadow:var(--hard);padding:18px 20px;margin:14px 0}.box h4{margin:0 0 8px;text-transform:uppercase;font-size:12px;letter-spacing:.06em;color:var(--mut)}
.bl{display:flex;justify-content:space-between;margin:5px 0}.bt{display:flex;justify-content:space-between;font-size:24px;border-top:2px dashed var(--bd);margin-top:8px;padding-top:10px}
.cols{display:grid;gap:18px;grid-template-columns:repeat(auto-fit,minmax(270px,1fr))}.tk{border:2px solid var(--bd);border-radius:14px;padding:10px 14px;margin:10px 0;background:var(--card)}.tk.void{opacity:.5;text-decoration:line-through}
.empty{border:2.5px dashed var(--bd);border-radius:18px;padding:24px;text-align:center;background:var(--card)}
input,select{font:600 14px 'Space Grotesk';padding:9px 12px;border:2.5px solid var(--bd);border-radius:12px;background:var(--card);color:var(--ink);margin:4px 0;max-width:100%}
.auth{min-height:100vh;display:grid;place-items:center;padding:20px}.auth .box{width:min(440px,100%)}.auth input,.auth select{width:100%}.err{color:#D92D20;font-weight:700}
.cartbar{position:fixed;left:50%;transform:translateX(-50%);bottom:20px;z-index:35;background:#FFE45E;color:#111;border:2.5px solid #111;border-radius:14px;box-shadow:5px 5px 0 #111;padding:13px 22px;font-weight:800;max-width:calc(100% - 24px);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.bn{display:none}@media(max-width:700px){.cartbar{bottom:78px}.bn{display:grid;grid-template-columns:repeat(5,1fr);position:fixed;left:0;right:0;bottom:0;z-index:36;background:var(--card);border-top:3px solid var(--bd);padding:6px}.bn a{display:grid;justify-items:center;font-size:11px;font-weight:700;padding:4px 0}.bn a span{font-size:21px}}
)CSS";

// =====================  PAGE BUILDERS  =====================
static string shell(const string& title, const string& body, bool refresh = false) {
  string h = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>" + title + "</title>";
  if (refresh) h += "<meta http-equiv=\"refresh\" content=\"6\">";
  h += "<link href=\"https://fonts.googleapis.com/css2?family=Archivo+Black&family=Space+Grotesk:wght@400;500;700&display=swap\" rel=\"stylesheet\"><style>"; h += CSS; h += "</style></head><body>" + body + "</body></html>";
  return h;
}
static string art(const Rest& r, int h = 140) { return "<span class=\"art\" style=\"height:" + to_string(h) + "px;background:linear-gradient(135deg,hsl(" + to_string(r.hue) + " 92% 66%),hsl(" + to_string((r.hue + 38) % 360) + " 88% 54%))\"><b>" + r.em + "</b></span>"; }
static int cartCount(User* u) { int n = 0; if (u->crid) for (auto* x = R[u->crid - 1].menu.h; x; x = x->nx) n += u->cart[x->v.id]; return n; }
struct Bill { int sub = 0, saved = 0, disc = 0, del = 0, tax = 0, total = 0; };
static Bill bill(User* u) {
  Bill b; if (!u->crid) return b; Rest& r = R[u->crid - 1];
  for (auto* x = r.menu.h; x; x = x->nx) { int q = u->cart[x->v.id]; b.sub += eff(x->v) * q; b.saved += (x->v.p - eff(x->v)) * q; }
  string c = u->promo; bool free = false;
  if (c == "ZWIGATO50" && b.sub >= 149) b.disc = 50; else if (c == "FIRST20") b.disc = min(100, b.sub / 5); else if (c == "FREEDEL") free = true;
  b.del = free ? 0 : (r.far ? 35 : 20); b.tax = (b.sub - b.disc) * 5 / 100; b.total = b.sub - b.disc + b.del + b.tax + u->tip; return b;
}
static string nav(User* u) {
  string h = "<div class=\"nav\"><a class=\"logo\" href=\"/\"><b>Z</b>Zwigato</a><div style=\"display:flex;gap:8px;align-items:center;flex-wrap:wrap\">";
  if (u->role == 0) {
    h += "<a class=\"btn sm" + string(u->veg ? " on" : "") + "\" href=\"/veg\">🌿 VEG " + (u->veg ? "ON" : "OFF") + "</a><a class=\"btn sm\" href=\"/orders\">🧾 Orders</a><a class=\"btn p sm\" href=\"/cart\">🛒 Bag · " + to_string(cartCount(u)) + "</a>";
  } else h += "<span class=\"btn sm\">🏪 " + R[u->rid - 1].n + "</span>";
  return h + "<a class=\"btn sm\" href=\"/logout\">Log out</a></div></div>";
}
static string bottomNav(User* u) {
  return "<nav class=\"bn\"><a href=\"/\"><span>🏠</span>Home</a><a href=\"/search\"><span>🔍</span>Search</a><a href=\"/#offers\"><span>🎟️</span>Offers</a><a href=\"/cart\"><span>🛒</span>Bag " + (cartCount(u) ? to_string(cartCount(u)) : "") + "</a><a href=\"/ai\"><span>✨</span>AI</a></nav>";
}
static string cartBar(User* u) {
  int n = cartCount(u); if (!n) return ""; Bill b = bill(u);
  return "<a class=\"cartbar\" href=\"/cart\">🛒 " + to_string(n) + " item" + (n > 1 ? "s" : "") + " · " + inr(b.total) + " · " + R[u->crid - 1].n + " →</a>";
}
static const char* ST[] = {"Order placed", "Kitchen cooking", "Rider on the way", "Delivered", "Cancelled"};
static string tracker(Order& o) {
  string h = "<div class=\"seg\">"; for (int i = 0; i < 4; i++) h += string("<i class=\"") + (o.st != 4 && i <= o.st ? "on" : "") + "\"></i>"; h += "</div>";
  return "<b>#" + to_string(o.id) + " · " + R[o.rid - 1].n + "</b><div>" + ST[o.st] + " · " + inr(o.total) + "</div>" + h;
}
static string restCard(Rest& r, User* u, const string& match) {
  bool fl = false; for (auto* x = r.menu.h; x; x = x->nx) if (x->v.fl) fl = true;
  return "<a class=\"rc\" href=\"/r/" + to_string(r.id) + "\">" + art(r) + "<span class=\"bd l\">" + (r.eta <= 20 ? "🚀 " : "⏱ ") + to_string(r.eta) + " min</span>" + (fl ? "<span class=\"bd r\">⚡ -25%</span>" : "") +
         "<div class=\"rb\"><h3>" + esc(r.n) + "</h3><div class=\"sub\">" + esc(r.c) + (r.far ? " · Farther gem" : "") + "</div><div class=\"mt\"><span class=\"rt\">★ " + to_string(r.rate).substr(0, 3) + "</span> 📍 " + to_string(r.km).substr(0, 3) + " km" + (match.empty() ? "" : " · " + esc(match)) + "</div></div></a>";
}

// ---------- customer home ----------
static string home(User* u, const string& q) {
  string cat = param(q, "cat"), f = param(q, "f"); string b = nav(u) + "<main class=\"wrap\">";
  string it; for (const char* s : {"⚡ Flash drops -25%", "🛵 Live rider tracking", "🌿 Veg mode", "🎟️ ZWIGATO50 · FIRST20 · FREEDEL", "🥟 Momos from ₹60", "🚀 Fastest in 14 min", "⚡ Flash drops -25%", "🛵 Live rider tracking", "🌿 Veg mode", "🎟️ ZWIGATO50 · FIRST20 · FREEDEL", "🥟 Momos from ₹60", "🚀 Fastest in 14 min"}) it += string("<span>") + s + "</span>";
  b += "<div class=\"mq\"><div>" + it + "</div></div>";
  // flash deal card = first flash dish found by walking the menu linked lists
  string flash = "<h3>No flash deals</h3>"; for (int i = 0; i < 12; i++) { bool done = false; for (auto* x = R[i].menu.h; x && !done; x = x->nx) if (x->v.fl) { flash = "<small><b>⚡ FLASH DROP</b></small><h3>" + esc(x->v.n) + "</h3><div>" + esc(R[i].n) + " · " + to_string(R[i].eta) + " min</div><div class=\"pr\"><s>" + inr(x->v.p) + "</s> " + inr(eff(x->v)) + "</div><a class=\"btn sm\" style=\"margin-top:8px\" href=\"/r/" + to_string(R[i].id) + "\">Grab this deal →</a>"; done = true; } if (done) break; }
  string live = "<b>Live tracker</b><div class=\"seg\"><i></i><i></i><i></i><i></i></div><div style=\"color:var(--mut)\">Place an order and your rider appears here.</div>";
  for (auto* x = u->hist.h; x; x = x->nx) if (O[x->v - 1].st < 3) { live = "<b>🟢 Live tracker</b><br>" + tracker(O[x->v - 1]); break; }
  b += "<section class=\"bento\"><div class=\"hero\"><span class=\"stk\">Free<br>delivery<br>₹149+</span><h1>Hungry? <em>Feed the beast.</em></h1><p>Search a dish or kitchen, or tell our AI your mood. Live rider tracking on every order.</p>"
       "<form class=\"sb\" action=\"/search\"><span>🔍</span><input name=\"q\" placeholder=\"Search momos, dal makhani, pizza…\" aria-label=\"Search\"><button type=\"button\" class=\"btn sm\" style=\"padding:6px 9px\" onclick=\"v()\" aria-label=\"Voice search\">🎤</button><button class=\"btn p sm\">Search</button></form>"
       "<div class=\"moods\"><a href=\"/ai?m=happy\">😊 Celebrating</a><a href=\"/ai?m=tired\">😴 Tired</a><a href=\"/ai?m=light\">⚡ Light</a><a href=\"/ai?m=fast\">🚀 Fastest</a><a href=\"/ai?m=vegan\">🌱 Vegan</a></div></div>"
       "<div class=\"side\"><div class=\"fl\">" + flash + "</div><div class=\"lv\">" + live + "</div></div></section>";
  b += "<section class=\"promo\" id=\"offers\"><div class=\"pt\"><div class=\"ps\" style=\"background:var(--pk)\"><div><small><b>OFFER</b></small><h3>Flat ₹50 off</h3>On orders of ₹149+<br><span class=\"code\">ZWIGATO50</span></div><a class=\"btn\" href=\"/promo?c=ZWIGATO50\">Use code</a></div>"
       "<div class=\"ps\" style=\"background:var(--mn)\"><div><small><b>OFFER</b></small><h3>20% off</h3>Up to ₹100<br><span class=\"code\">FIRST20</span></div><a class=\"btn\" href=\"/promo?c=FIRST20\">Use code</a></div>"
       "<div class=\"ps\" style=\"background:#7FA2FF\"><div><small><b>OFFER</b></small><h3>Free delivery</h3>Zero delivery fee today<br><span class=\"code\">FREEDEL</span></div><a class=\"btn\" href=\"/promo?c=FREEDEL\">Use code</a></div>"
       "<div class=\"ps\" style=\"background:#FFE45E\"><div><small><b>OFFER</b></small><h3>⚡ Flash drops</h3>Signature dishes at -25%</div><a class=\"btn\" href=\"/?f=flash\">See deals</a></div></div></section>";
  b += "<div class=\"sec\"><h2>What’s on your mind?</h2></div><div class=\"cats\">";
  for (auto& c : CATS) b += string("<a class=\"cat") + (cat == c.n ? " on" : "") + "\" href=\"/?cat=" + c.n + "\"><span>" + c.em + "</span>" + c.n + "</a>";
  b += "</div><div class=\"pills\">";
  const char* FL[][2] = {{"fast","Fastest"},{"top","Top rated"},{"far","Farther gems"},{"vegan","Vegan"},{"gf","Gluten-free"},{"flash","Flash deals"},{"u22","Under 22 min"},{"r46","4.6+"}};
  for (auto& p : FL) b += string("<a class=\"btn sm") + (f == p[0] ? " on" : "") + "\" href=\"/?f=" + p[0] + (cat.empty() ? "" : "&cat=" + cat) + "\">" + p[1] + "</a>";
  if (!f.empty() || !cat.empty()) b += "<a class=\"btn sm\" href=\"/\">✕ Clear</a>";
  b += "</div>";
  // filter, then order with a HEAP (fastest) or std::sort (top rated)
  vector<int> ids; for (int i = 0; i < 12; i++) {
    Rest& r = R[i]; bool ok = true, anyV = false, anyG = false, anyFl = false, anyVg = false;
    for (auto* x = r.menu.h; x; x = x->nx) { if (x->v.dt != 'N') anyV = true; if (x->v.gf) anyG = true; if (x->v.fl) anyFl = true; if (x->v.dt == 'G') anyVg = true; }
    if (u->veg && !anyV) ok = false;
    if (!cat.empty()) { ok = false; for (auto& c : CATS) if (cat == c.n) { if (anyKw(r.c, c.kw)) ok = true; for (auto* x = r.menu.h; x; x = x->nx) if (anyKw(x->v.n, c.kw)) ok = true; } if (u->veg && !anyV) ok = false; }
    if (f == "far" && !r.far) ok = false; if (f == "vegan" && !anyVg) ok = false; if (f == "gf" && !anyG) ok = false; if (f == "flash" && !anyFl) ok = false; if (f == "u22" && r.eta > 22) ok = false; if (f == "r46" && r.rate < 4.6) ok = false;
    if (ok) ids.push_back(i);
  }
  if (f == "fast") { MinHeap h; for (int i : ids) h.push({R[i].eta, i, 0}); ids.clear(); HeapItem t; while (h.pop(t)) ids.push_back(t.rid); }
  else if (f == "top") sort(ids.begin(), ids.end(), [](int a, int c) { return R[a].rate > R[c].rate; });
  b += "<div class=\"sec\"><h2>" + string(cat.empty() ? "All kitchens" : cat + " kitchens") + "</h2><small>" + to_string(ids.size()) + " kitchens</small></div>";
  if (ids.empty()) b += "<div class=\"empty\">No kitchen matches that. Try clearing a filter.</div>"; else { b += "<div class=\"grid\">"; for (int i : ids) b += restCard(R[i], u, ""); b += "</div>"; }
  b += "<div class=\"sec\"><h2>Explore more</h2></div><div class=\"pills\"><a class=\"btn\" href=\"/veg\">🌿 Pure veg</a><a class=\"btn\" href=\"/?f=u22\">🚀 Under 20 min</a><a class=\"btn\" href=\"/?f=flash\">⚡ Flash deals</a><a class=\"btn\" href=\"/?f=far\">🧭 Farther gems</a><a class=\"btn\" href=\"/?f=vegan\">🌱 Vegan picks</a><a class=\"btn\" href=\"/ai\">✨ Ask AI</a></div></main>";
  b += "<script>function v(){var S=window.SpeechRecognition||window.webkitSpeechRecognition;if(!S)return alert('Voice search is not supported in this browser');var r=new S();r.onresult=function(e){location='/search?q='+encodeURIComponent(e.results[0][0].transcript)};r.start()}</script>";
  return shell("Zwigato", b + cartBar(u) + bottomNav(u));
}
// ---------- restaurant menu ----------
static string menuPage(User* u, int rid, const string& q) {
  Rest& r = R[rid - 1]; string t = param(q, "t"); string b = nav(u) + "<main class=\"wrap\"><a class=\"btn sm\" style=\"margin-top:16px\" href=\"/\">← All kitchens</a><div class=\"mh\">" + art(r, 120) + "<div class=\"in\"><h2>" + esc(r.n) + "</h2>" + esc(r.c) + " · ★ " + to_string(r.rate).substr(0, 3) + " · ⏱ " + to_string(r.eta) + " min · 📍 " + to_string(r.km).substr(0, 3) + " km</div><div style=\"padding:0 20px 14px;display:flex;gap:8px;flex-wrap:wrap\">";
  for (const char* x : {"All", "Specials", "Flash", "Vegan", "Gluten-free"}) b += string("<a class=\"btn sm") + (t == x ? " on" : "") + "\" href=\"/r/" + to_string(rid) + "?t=" + x + "\">" + x + "</a>";
  b += "</div>";
  int shown = 0;
  for (auto* x = r.menu.h; x; x = x->nx) {
    Dish& d = x->v; if (u->veg && d.dt == 'N') continue; if (t == "Specials" && !d.sp) continue; if (t == "Flash" && !d.fl) continue; if (t == "Vegan" && d.dt != 'G') continue; if (t == "Gluten-free" && !d.gf) continue;
    int n = (u->crid == rid) ? u->cart[d.id] : 0; shown++;
    b += string("<div class=\"dish") + (d.out ? " out" : "") + "\"><span class=\"tile\">" + emo(d.n) + "</span><div class=\"nm\"><span class=\"vm" + (d.dt == 'N' ? " n" : "") + "\"></span> " + esc(d.n) + "<div class=\"tg\">" + (d.dt == 'G' ? "<span>Vegan</span>" : "") + (d.gf ? "<span>Gluten-free</span>" : "") + (d.sp ? "<span class=\"s\">★ Special</span>" : "") + (d.fl ? "<span class=\"f\">⚡ Flash -25%</span>" : "") + "</div></div><div style=\"text-align:right;font-weight:800\">" + (d.fl ? "<s style=\"font-weight:500\">" + inr(d.p) + "</s><br>" : "") + inr(eff(d)) + "</div>";
    if (d.out) b += "<span>Sold out</span>"; else if (n) b += "<span class=\"stp\"><a href=\"/add?d=" + to_string(d.id) + "&n=-1&back=" + to_string(rid) + "\">−</a><b>" + to_string(n) + "</b><a href=\"/add?d=" + to_string(d.id) + "&n=1&back=" + to_string(rid) + "\">+</a></span>";
    else b += "<a class=\"btn p sm\" href=\"/add?d=" + to_string(d.id) + "&n=1&back=" + to_string(rid) + "\">Add</a>";
    b += "</div>";
  }
  if (!shown) b += "<div class=\"empty\" style=\"margin:14px\">No dishes match this filter.</div>";
  return shell(esc(r.n), b + "</div></main>" + cartBar(u) + bottomNav(u));
}
// ---------- cart ----------
static string cartPage(User* u) {
  string b = nav(u) + "<main class=\"wrap\"><div class=\"sec\"><h2>Your bag</h2></div>";
  if (!u->crid || !cartCount(u)) return shell("Bag", b + "<div class=\"empty\">Your bag is empty. <a class=\"btn p sm\" href=\"/\">Browse kitchens</a></div></main>" + bottomNav(u));
  Rest& r = R[u->crid - 1]; Bill bl = bill(u); b += "<div class=\"box\"><h4>From " + esc(r.n) + " · ⏱ " + to_string(r.eta) + " min</h4>";
  for (auto* x = r.menu.h; x; x = x->nx) { int q = u->cart[x->v.id]; if (!q) continue; b += "<div class=\"dish\" style=\"padding:10px 0\"><span class=\"tile\">" + emo(x->v.n) + "</span><div class=\"nm\">" + esc(x->v.n) + "<br><small>" + inr(eff(x->v)) + " × " + to_string(q) + "</small></div><span class=\"stp\"><a href=\"/add?d=" + to_string(x->v.id) + "&n=-1&back=cart\">−</a><b>" + to_string(q) + "</b><a href=\"/add?d=" + to_string(x->v.id) + "&n=1&back=cart\">+</a></span></div>"; }
  b += "<a class=\"btn sm\" href=\"/undo\">↶ Undo last change (stack)</a></div>";
  b += "<div class=\"box\"><h4>🎟️ Promo code</h4><form action=\"/promo\"><input name=\"c\" value=\"" + esc(u->promo) + "\" placeholder=\"ZWIGATO50 · FIRST20 · FREEDEL\"> <button class=\"btn p sm\">Apply</button></form></div>";
  b += "<div class=\"box\"><h4>🛵 Tip your rider</h4>"; for (int t : {0, 20, 30, 50}) b += string("<a class=\"btn sm") + (u->tip == t ? " on" : "") + "\" href=\"/tip?t=" + to_string(t) + "\">" + inr(t) + "</a> "; b += "</div>";
  b += "<div class=\"box\"><h4>🧾 Bill</h4><div class=\"bl\"><span>Subtotal</span><span>" + inr(bl.sub) + "</span></div>" + (bl.saved ? "<div class=\"bl\"><span>⚡ Flash savings</span><span>−" + inr(bl.saved) + "</span></div>" : "") + (bl.disc ? "<div class=\"bl\"><span>🎟️ " + u->promo + "</span><span>−" + inr(bl.disc) + "</span></div>" : "") +
       "<div class=\"bl\"><span>Delivery</span><span>" + (bl.del ? inr(bl.del) : "FREE") + "</span></div><div class=\"bl\"><span>GST & packaging</span><span>" + inr(bl.tax) + "</span></div><div class=\"bl\"><span>Tip</span><span>" + inr(u->tip) + "</span></div><div class=\"bt\"><span>Total</span><span>" + inr(bl.total) + "</span></div><br><a class=\"btn p w\" href=\"/pay\">Pay " + inr(bl.total) + " & place order →</a></div></main>" + bottomNav(u);
  return shell("Bag", b);
}
// ---------- orders (customer) ----------
static string ordersPage(User* u) {
  string b = nav(u) + "<main class=\"wrap\"><div class=\"sec\"><h2>Your orders</h2><small>Updates every 6 seconds</small></div>"; int n = 0;
  for (auto* x = u->hist.h; x; x = x->nx) { Order& o = O[x->v - 1]; n++; string its; for (auto& p : o.it) { Dish* d = findDish(p.first); if (d) its += to_string(p.second) + "× " + esc(d->n) + ", "; }
    b += "<div class=\"box\">" + tracker(o) + "<div style=\"color:var(--mut)\">" + its + "</div>" + (o.st == 0 ? "<br><a class=\"btn sm\" href=\"/cancel?o=" + to_string(o.id) + "\">Cancel order</a>" : "") + "</div>"; }
  if (!n) b += "<div class=\"empty\">No orders yet.</div>";
  return shell("Orders", b + "</main>" + bottomNav(u), true);
}
// ---------- search (TRIE) + recent searches (STACK) ----------
static string searchPage(User* u, const string& q) {
  string b = nav(u) + "<main class=\"wrap\"><div class=\"sec\"><h2>Search</h2></div><form class=\"sb\" action=\"/search\" style=\"border-radius:14px\"><span>🔍</span><input name=\"q\" value=\"" + esc(q) + "\" placeholder=\"Search dishes or kitchens\" autofocus><button type=\"button\" class=\"btn sm\" onclick=\"v()\">🎤</button><button class=\"btn p sm\">Go</button></form>";
  if (q.empty()) {
    if (u->recent.top) { b += "<div class=\"sec\"><h2>Your recent searches</h2></div><div class=\"pills\">"; for (int i = u->recent.top - 1; i >= 0; i--) b += "<a class=\"btn sm\" href=\"/search?q=" + enc(u->recent.a[i]) + "\">🕘 " + esc(u->recent.a[i]) + "</a>"; b += "</div>"; }
    b += "<div class=\"sec\"><h2>What’s on your mind?</h2></div><div class=\"cats\">"; for (auto& c : CATS) b += string("<a class=\"cat\" href=\"/search?q=") + c.n + "\"><span>" + c.em + "</span>" + c.n + "</a>"; b += "</div>";
  } else {
    u->recent.pushUnique(q); string first = q.substr(0, q.find(' ')); vector<int> refs = TR.find(first); string lq = lower(q); int shown = 0; b += "<div class=\"sec\"><h2>Results</h2></div>";
    for (int ref : refs) {
      if (ref < 0) { Rest& r = R[-ref - 1]; if (lower(r.n + " " + r.c).find(lq) == string::npos) continue; b += "<a class=\"box\" style=\"display:block\" href=\"/r/" + to_string(r.id) + "\"><b>" + r.em + " " + esc(r.n) + "</b><br>" + esc(r.c) + " · ⏱ " + to_string(r.eta) + " min</a>"; shown++; }
      else { Dish* d = findDish(ref); if (!d || lower(d->n).find(lq) == string::npos || (u->veg && d->dt == 'N')) continue; Rest& r = R[ref / 100 - 1]; b += "<a class=\"box\" style=\"display:block\" href=\"/r/" + to_string(r.id) + "\"><b>" + emo(d->n) + " " + esc(d->n) + "</b> · " + inr(eff(*d)) + "<br>" + esc(r.n) + " · ⏱ " + to_string(r.eta) + " min</a>"; shown++; }
    }
    if (!shown) b += "<div class=\"empty\">Nothing matches “" + esc(q) + "”. Try a shorter word.</div>";
  }
  b += "</main><script>function v(){var S=window.SpeechRecognition||window.webkitSpeechRecognition;if(!S)return alert('Voice search is not supported in this browser');var r=new S();r.onresult=function(e){location='/search?q='+encodeURIComponent(e.results[0][0].transcript)};r.start()}</script>";
  return shell("Search", b + cartBar(u) + bottomNav(u));
}
// ---------- AI mood concierge (HEAP picks the fastest kitchens) ----------
static string aiPage(User* u, const string& m) {
  string b = nav(u) + "<main class=\"wrap\"><div class=\"sec\"><h2>✨ Zwigato AI</h2><small>Mood and speed matcher</small></div><div class=\"moods\" style=\"margin-bottom:14px\"><a class=\"btn\" href=\"/ai?m=happy\">😊 Celebrating</a><a class=\"btn\" href=\"/ai?m=tired\">😴 Tired</a><a class=\"btn\" href=\"/ai?m=light\">⚡ Light</a><a class=\"btn\" href=\"/ai?m=fast\">🚀 Fastest</a><a class=\"btn\" href=\"/ai?m=vegan\">🌱 Vegan</a></div>";
  if (m.empty()) return shell("AI", b + "<div class=\"box\">Namaste! Pick a mood above and I’ll choose dishes from the fastest kitchens.</div></main>" + cartBar(u) + bottomNav(u));
  const char* kw = "dal|chocolate|momo|rajma|chai|biryani|soup"; string msg = "Warm, comforting plates:";
  if (m == "happy") { kw = "pizza|sundae|waffle|platter|roll"; msg = "Let’s celebrate. Chef specials:"; } else if (m == "light") { kw = "quinoa|avocado|soup|maki|tikka|donburi|bowl"; msg = "Clean, energising picks:"; }
  else if (m == "vegan") { kw = "*"; msg = "Vegan favourites:"; } else if (m == "fast") { kw = "*"; msg = "Fastest kitchens, dispatching in 14–20 minutes:"; }
  MinHeap h; for (int i = 0; i < 12; i++) for (auto* x = R[i].menu.h; x; x = x->nx) { Dish& d = x->v; if (d.out || (u->veg && d.dt == 'N')) continue; bool ok = string(kw) == "*" ? (m == "fast" ? d.sp : d.dt == 'G') : anyKw(d.n, kw); if (ok) h.push({R[i].eta, i + 1, d.id}); }
  b += "<div class=\"box\"><b>" + msg + "</b>"; HeapItem t; int k = 0;
  while (k < 3 && h.pop(t)) { Dish* d = findDish(t.did); k++; b += "<div class=\"dish\" style=\"padding:10px 0\"><span class=\"tile\">" + emo(d->n) + "</span><div class=\"nm\">" + esc(d->n) + " · " + inr(eff(*d)) + "<br><small>" + esc(R[t.rid - 1].n) + " · ⏱ " + to_string(t.key) + " min</small></div><a class=\"btn p sm\" href=\"/add?d=" + to_string(d->id) + "&n=1&back=ai\">+ Add</a></div>"; }
  if (!k) b += "<div>Nothing matches right now.</div>";
  return shell("AI", b + "</div></main>" + cartBar(u) + bottomNav(u));
}
// ---------- restaurant dashboard (customers' orders arrive in this restaurant's QUEUE) ----------
static string ownerPage(User* u) {
  Rest& r = R[u->rid - 1]; int wait = 0, cook = 0, done = 0, rev = 0; string inc, ck, dn;
  for (auto* x = r.q.f; x; x = x->nx) { Order& o = O[x->v - 1]; string its; for (auto& p : o.it) { Dish* d = findDish(p.first); if (d) its += to_string(p.second) + "× " + esc(d->n) + ", "; }
    if (o.st == 0) wait++; inc += string("<div class=\"tk") + (o.st == 4 ? " void" : "") + "\"><b>#" + to_string(o.id) + " · " + esc(o.uname) + "</b><br>" + its + " " + inr(o.total) + (o.st == 4 ? " (void)" : "") + "</div>"; }
  for (auto& o : O) if (o.rid == r.id) { string its; for (auto& p : o.it) { Dish* d = findDish(p.first); if (d) its += to_string(p.second) + "× " + esc(d->n) + ", "; }
    if (o.st == 1 || o.st == 2) { cook++; ck += "<div class=\"tk\"><b>#" + to_string(o.id) + " · " + esc(o.uname) + "</b><br>" + its + "<br><i>" + ST[o.st] + "</i><br><a class=\"btn p sm\" href=\"/o/step?o=" + to_string(o.id) + "\">" + (o.st == 1 ? "Send rider →" : "Mark delivered ✓") + "</a></div>"; }
    if (o.st == 3) { done++; rev += o.total; dn += "<div class=\"tk\"><b>#" + to_string(o.id) + " · " + esc(o.uname) + "</b> · " + inr(o.total) + "</div>"; } }
  string b = nav(u) + "<main class=\"wrap\"><div class=\"sec\"><h2>" + r.em + " " + esc(r.n) + "</h2><small>Kitchen command centre</small></div><div class=\"cols\"><div class=\"box\"><h4>Waiting</h4><b style=\"font-size:34px\">" + to_string(wait) + "</b></div><div class=\"box\"><h4>Cooking / on the way</h4><b style=\"font-size:34px\">" + to_string(cook) + "</b></div><div class=\"box\"><h4>Delivered</h4><b style=\"font-size:34px\">" + to_string(done) + "</b></div><div class=\"box\"><h4>Revenue</h4><b style=\"font-size:34px\">" + inr(rev) + "</b></div></div>";
  b += "<div class=\"sec\"><h2>Orders</h2></div><a class=\"btn p\" href=\"/o/cook\">Cook next order (dequeue)</a><div class=\"cols\"><div class=\"box\"><h4>Incoming queue (first in, first out)</h4>" + (inc.empty() ? "<div>No incoming orders.</div>" : inc) + "</div><div class=\"box\"><h4>Cooking</h4>" + (ck.empty() ? "<div>Nothing cooking.</div>" : ck) + "</div><div class=\"box\"><h4>Delivered</h4>" + (dn.empty() ? "<div>No deliveries yet.</div>" : dn) + "</div></div>";
  b += "<div class=\"sec\"><h2>Menu</h2></div><div class=\"mh\">"; for (auto* x = r.menu.h; x; x = x->nx) { Dish& d = x->v; b += "<div class=\"dish\"><span class=\"tile\">" + emo(d.n) + "</span><div class=\"nm\">" + esc(d.n) + "<br><small>" + inr(eff(d)) + (d.fl ? " (was " + inr(d.p) + ")" : "") + " · sold " + to_string(d.sold) + "</small></div><a class=\"btn sm" + string(d.out ? " on" : "") + "\" href=\"/o/stock?d=" + to_string(d.id) + "\">" + (d.out ? "Sold out" : "In stock") + "</a><a class=\"btn sm" + string(d.fl ? " p" : "") + "\" href=\"/o/flash?d=" + to_string(d.id) + "\">⚡ Flash -25%</a></div>"; }
  b += "</div><div class=\"box\"><h4>Add a dish (inserted at the front of the linked list)</h4><form action=\"/o/add\"><input name=\"n\" placeholder=\"Dish name\" required> <input name=\"p\" type=\"number\" min=\"10\" placeholder=\"Price ₹\" required> <select name=\"t\"><option value=\"V\">Veg</option><option value=\"G\">Vegan</option><option value=\"N\">Non-veg</option></select> <button class=\"btn p sm\">Publish dish</button></form></div></main>";
  return shell("Kitchen", b);
}
static string loginPage(const string& err = "") {
  string b = "<div class=\"auth\"><div class=\"box\"><div class=\"logo\"><b>Z</b>Zwigato</div><h2 style=\"margin:14px 0 4px;font-size:28px\">Sign in to your table</h2><p style=\"color:var(--mut);margin:0 0 12px\">Order fresh meals or manage your restaurant.</p>"
             "<form method=\"post\" action=\"/login\"><select name=\"role\"><option value=\"0\">🛵 Customer</option><option value=\"1\">👨‍🍳 Restaurant partner</option></select><input name=\"name\" placeholder=\"Full name (sign up only)\"><select name=\"rid\">";
  for (int i = 0; i < 12; i++) b += "<option value=\"" + to_string(i + 1) + "\">" + esc(R[i].n) + "</option>";
  b += "</select><input name=\"email\" type=\"email\" placeholder=\"Email\" required><input name=\"pass\" type=\"password\" placeholder=\"Password (min 6 characters)\" required><div class=\"err\">" + esc(err) + "</div>"
       "<button class=\"btn p w\" name=\"mode\" value=\"login\">Log in</button><br><br><button class=\"btn w\" name=\"mode\" value=\"signup\">Create account</button></form><br><a class=\"btn sm\" href=\"/guest\">🛵 Demo customer</a> <a class=\"btn sm\" href=\"/demo-owner\">👨‍🍳 Demo partner</a>"
       "<p style=\"color:var(--mut);font-size:12px\">Demo build: accounts live in memory and vanish when the server stops. For a restaurant account, pick your restaurant in the list.</p></div></div>";
  return shell("Zwigato · Log in", b);
}

// =====================  ROUTER  =====================
struct Resp { string body, loc, cookie; };
static int newUser(const string& email, const string& name, const string& pass, int role, int rid) {
  User* u = new User; u->email = email; u->name = name; u->pass = pass; u->role = role; u->rid = rid; u->veg = false; u->crid = 0; u->tip = 20; memset(u->cart, 0, sizeof u->cart);
  U.push_back(u); int ix = U.size() - 1; if (!email.empty()) users.put(email, ix); return ix;
}
static string startSession(Resp& rs, int ix) { string sid = to_string(rand()) + to_string(rand()); sessions.put(sid, ix); rs.cookie = "sid=" + sid + "; Path=/; HttpOnly"; return sid; }
static void addCart(User* u, int did, int n) {
  Dish* d = findDish(did); if (!d || d->out) return; int rid = did / 100;
  if (u->crid && u->crid != rid) { memset(u->cart, 0, sizeof u->cart); u->undo.top = 0; u->promo = ""; }
  u->crid = rid; int q = u->cart[did] + n; if (q < 0) q = 0; if (q != u->cart[did]) { u->undo.push(did * 10 + (n > 0 ? 1 : 2)); u->cart[did] = q; }
  if (!cartCount(u)) { u->crid = 0; u->promo = ""; }
}
static void route(const string& method, const string& path, const string& q, const string& body, const string& cookie, Resp& rs) {
  string sid; size_t cp = cookie.find("sid="); if (cp != string::npos) { sid = cookie.substr(cp + 4); sid = sid.substr(0, sid.find(';')); }
  int ix = sid.empty() ? -1 : sessions.get(sid); User* u = ix >= 0 ? U[ix] : 0;
  if (path == "/login" && method == "POST") {
    string mode = param(body, "mode"), email = lower(param(body, "email")), pass = param(body, "pass"), name = param(body, "name"); int role = atoi(param(body, "role").c_str()), rid = atoi(param(body, "rid").c_str());
    if (email.find('@') == string::npos || pass.size() < 6) { rs.body = loginPage("Enter a valid email and a password of 6+ characters."); return; }
    int e = users.get(email);
    if (mode == "signup") { if (e >= 0) { rs.body = loginPage("An account with this email already exists."); return; } if (name.empty()) { rs.body = loginPage("Please enter your name."); return; } e = newUser(email, name, pass, role, role ? rid : 0); }
    else { if (e < 0 || U[e]->pass != pass) { rs.body = loginPage("Wrong email or password."); return; } if (U[e]->role != role) { rs.body = loginPage("That is a different account type. Switch the selector."); return; } }
    startSession(rs, e); rs.loc = "/"; return;
  }
  if (path == "/guest") { startSession(rs, newUser("", "guest", "", 0, 0)); rs.loc = "/"; return; }
  if (path == "/demo-owner") { startSession(rs, newUser("", "Demo owner", "", 1, 1)); rs.loc = "/"; return; }
  if (!u) { rs.body = loginPage(); return; }
  if (path == "/logout") { sessions.put(sid, -1); rs.cookie = "sid=; Path=/; Max-Age=0"; rs.loc = "/"; return; }
  if (u->role == 1) {                                   // ---- restaurant interface ----
    Rest& r = R[u->rid - 1];
    if (path == "/o/cook") { int id; while (r.q.dequeue(id)) { if (O[id - 1].st == 4) continue; O[id - 1].st = 1; break; } rs.loc = "/"; return; }   // lazy deletion of cancelled orders
    if (path == "/o/step") { Order& o = O[atoi(param(q, "o").c_str()) - 1]; if (o.rid == r.id && o.st == 1) o.st = 2; else if (o.rid == r.id && o.st == 2) o.st = 3; rs.loc = "/"; return; }
    if (path == "/o/stock" || path == "/o/flash") { Dish* d = findDish(atoi(param(q, "d").c_str())); if (d && d->id / 100 == r.id) { if (path == "/o/stock") d->out = !d->out; else d->fl = !d->fl; } rs.loc = "/"; return; }
    if (path == "/o/add") { string n = param(q, "n"); int p = atoi(param(q, "p").c_str()); string t = param(q, "t"); if (!n.empty() && p > 0 && r.next < 99) { Dish d{r.id * 100 + r.next++, n, p, t.empty() ? 'V' : t[0], false, false, true, false, 0}; r.menu.pushFront(d); TR.insert(n, d.id); } rs.loc = "/"; return; }
    rs.body = ownerPage(u); return;
  }
  // ---- customer interface ----
  if (path == "/veg") { u->veg = !u->veg; rs.loc = "/"; return; }
  if (path == "/add") { addCart(u, atoi(param(q, "d").c_str()), atoi(param(q, "n").c_str())); string b = param(q, "back"); rs.loc = b == "cart" ? "/cart" : b == "ai" ? "/ai" : "/r/" + b; return; }
  if (path == "/undo") { int a; if (u->undo.pop(a)) { int did = a / 10; u->cart[did] += (a % 10 == 1) ? -1 : 1; if (u->cart[did] < 0) u->cart[did] = 0; if (!cartCount(u)) u->crid = 0; } rs.loc = "/cart"; return; }
  if (path == "/promo") { string c = param(q, "c"); for (auto& ch : c) ch = toupper((unsigned char)ch); if (c == "ZWIGATO50" || c == "FIRST20" || c == "FREEDEL" || c.empty()) u->promo = c; rs.loc = u->crid ? "/cart" : "/"; return; }
  if (path == "/tip") { u->tip = atoi(param(q, "t").c_str()); rs.loc = "/cart"; return; }
  if (path == "/pay") {
    if (u->crid && cartCount(u)) { Bill b = bill(u); Order o; o.id = O.size() + 1; o.rid = u->crid; o.uid = ix; o.total = b.total; o.st = 0; o.uname = u->name;
      for (auto* x = R[u->crid - 1].menu.h; x; x = x->nx) if (u->cart[x->v.id]) { o.it.push_back({x->v.id, u->cart[x->v.id]}); x->v.sold += u->cart[x->v.id]; }
      O.push_back(o); R[o.rid - 1].q.enqueue(o.id); u->hist.pushFront(o.id); memset(u->cart, 0, sizeof u->cart); u->crid = 0; u->promo = ""; u->undo.top = 0; }
    rs.loc = "/orders"; return;
  }
  if (path == "/cancel") { int id = atoi(param(q, "o").c_str()); if (id >= 1 && id <= (int)O.size() && O[id - 1].uid == ix && O[id - 1].st == 0) O[id - 1].st = 4; rs.loc = "/orders"; return; }
  if (path == "/cart") { rs.body = cartPage(u); return; }
  if (path == "/orders") { rs.body = ordersPage(u); return; }
  if (path == "/search") { rs.body = searchPage(u, param(q, "q")); return; }
  if (path == "/ai") { rs.body = aiPage(u, param(q, "m")); return; }
  if (path.rfind("/r/", 0) == 0) { int id = atoi(path.c_str() + 3); if (id >= 1 && id <= 12) { rs.body = menuPage(u, id, q); return; } }
  rs.body = home(u, q);
}

// =====================  HTTP SERVER (plain sockets)  =====================
int main() {
  srand(time(0));
  for (int i = 0; i < 12; i++) {                         // load the array of raw data into linked lists + trie
    const RD& d = RAW[i]; Rest& r = R[i]; r.id = i + 1; r.n = d.n; r.c = d.c; r.eta = d.eta; r.km = d.km; r.rate = d.rate; r.em = d.em; r.hue = d.hue; r.far = d.km > 5; r.next = 4;
    TR.insert(r.n, -(i + 1));
    for (int j = 0; j < 4; j++) { const DD& x = d.d[j]; Dish dish{(i + 1) * 100 + j, x.n, x.p, x.dt, strchr(x.fl, 'g') != 0, strchr(x.fl, 'f') != 0, strchr(x.fl, 's') != 0, false, 0}; r.menu.pushBack(dish); TR.insert(x.n, dish.id); }
  }
  int s = socket(AF_INET, SOCK_STREAM, 0), on = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
  sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(8080);
  if (bind(s, (sockaddr*)&a, sizeof a) < 0 || listen(s, 16) < 0) { perror("bind"); return 1; }
  printf("Zwigato is running. Open http://localhost:8080\n");
  for (;;) {
    int c = accept(s, 0, 0); if (c < 0) continue; string req; char buf[4096]; ssize_t n;
    while ((n = read(c, buf, sizeof buf)) > 0) { req.append(buf, n); size_t he = req.find("\r\n\r\n"); if (he == string::npos) continue; size_t cl = req.find("Content-Length:"); size_t len = cl == string::npos ? 0 : atoi(req.c_str() + cl + 15); if (req.size() >= he + 4 + len) break; }
    size_t e1 = req.find("\r\n"); string line = e1 == string::npos ? "" : req.substr(0, e1); size_t sp1 = line.find(' '), sp2 = line.find(' ', sp1 + 1);
    if (sp1 == string::npos || sp2 == string::npos) { close(c); continue; }
    string method = line.substr(0, sp1), target = line.substr(sp1 + 1, sp2 - sp1 - 1), path = target, q; size_t qm = target.find('?'); if (qm != string::npos) { path = target.substr(0, qm); q = target.substr(qm + 1); }
    string cookie; size_t ck = req.find("\r\nCookie:"); if (ck != string::npos) cookie = req.substr(ck + 9, req.find("\r\n", ck + 9) - ck - 9);
    size_t he = req.find("\r\n\r\n"); string body = he == string::npos ? "" : req.substr(he + 4);
    Resp rs; route(method, path, q, body, cookie, rs); string out;
    if (!rs.loc.empty()) out = "HTTP/1.1 302 Found\r\nLocation: " + rs.loc + "\r\n" + (rs.cookie.empty() ? "" : "Set-Cookie: " + rs.cookie + "\r\n") + "Content-Length: 0\r\nConnection: close\r\n\r\n";
    else out = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n" + (rs.cookie.empty() ? "" : "Set-Cookie: " + rs.cookie + "\r\n") + "Content-Length: " + to_string(rs.body.size()) + "\r\nConnection: close\r\n\r\n" + rs.body;
    size_t sent = 0; while (sent < out.size()) { ssize_t w = write(c, out.data() + sent, out.size() - sent); if (w <= 0) break; sent += w; }
    close(c);
  }
}
