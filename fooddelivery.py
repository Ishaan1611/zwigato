from collections import deque
from dataclasses import dataclass, field
from enum import Enum
import itertools


# ---------- Trie (used for search) ----------
class TrieNode:
    __slots__ = ("children", "refs")

    def __init__(self):
        self.children = {}
        self.refs = set()  # ids of everything whose word passes through this node


class Trie:
    def __init__(self):
        self.root = TrieNode()

    def insert(self, text, ref):
        # index every word, so "pizza" finds "Domino's Pizza"
        for word in text.lower().split():
            node = self.root
            for ch in word:
                node = node.children.setdefault(ch, TrieNode())
                node.refs.add(ref)

    def search_prefix(self, prefix):
        node = self.root
        for ch in prefix.lower():
            if ch not in node.children:
                return set()
            node = node.children[ch]
        return node.refs


# ---------- Models ----------
class Status(Enum):
    PENDING = "PENDING"
    PROCESSING = "PROCESSING"
    DELIVERED = "DELIVERED"
    CANCELLED = "CANCELLED"


@dataclass
class FoodItem:
    item_id: int
    name: str
    price: float


@dataclass
class Restaurant:
    rest_id: int
    name: str
    menu: dict = field(default_factory=dict)  # item_id -> FoodItem (HashMap)


@dataclass
class Order:
    order_id: int
    rest_id: int
    items: list  # list of (FoodItem, qty)
    status: Status = Status.PENDING

    @property
    def total(self):
        return sum(i.price * q for i, q in self.items)


# ---------- System ----------
class FoodDeliverySystem:
    def __init__(self):
        self.restaurants = {}            # HashMap: rest_id -> Restaurant
        self.orders = {}                 # HashMap: order_id -> Order
        self.pending = deque()           # Queue (FIFO) of pending order_ids
        self.rest_trie = Trie()          # search by restaurant name
        self.item_trie = Trie()          # search by food item name
        self.item_index = {}             # item key -> (rest_id, FoodItem)
        self._order_seq = itertools.count(1)
        self._item_seq = itertools.count(1)

    # --- Food items / restaurants ---
    def add_restaurant(self, rest_id, name):
        self.restaurants[rest_id] = Restaurant(rest_id, name)
        self.rest_trie.insert(name, rest_id)

    def add_food_item(self, rest_id, name, price):
        item = FoodItem(next(self._item_seq), name, price)
        self.restaurants[rest_id].menu[item.item_id] = item
        key = (rest_id, item.item_id)
        self.item_index[key] = item
        self.item_trie.insert(name, key)
        return item.item_id

    # --- Place order ---
    def place_order(self, rest_id, cart):
        """cart = {item_id: qty}"""
        rest = self.restaurants.get(rest_id)
        if not rest:
            raise ValueError("Restaurant not found")
        items = []
        for item_id, qty in cart.items():
            if item_id not in rest.menu:
                raise ValueError(f"Item {item_id} not on menu")
            items.append((rest.menu[item_id], qty))
        order = Order(next(self._order_seq), rest_id, items)
        self.orders[order.order_id] = order
        self.pending.append(order.order_id)      # enqueue
        return order.order_id

    # --- Cancel order ---
    def cancel_order(self, order_id):
        order = self.orders.get(order_id)         # O(1) lookup
        if not order or order.status != Status.PENDING:
            return False                          # can't cancel once processing
        order.status = Status.CANCELLED           # lazy deletion from queue
        return True

    # --- Process order ---
    def process_next_order(self):
        while self.pending:
            order = self.orders[self.pending.popleft()]   # dequeue
            if order.status == Status.CANCELLED:
                continue                                  # skip cancelled ones
            order.status = Status.PROCESSING
            return order
        return None

    def deliver(self, order_id):
        order = self.orders[order_id]
        if order.status == Status.PROCESSING:
            order.status = Status.DELIVERED

    # --- Maintain pending orders ---
    def get_pending_orders(self):
        return [self.orders[i] for i in self.pending
                if self.orders[i].status == Status.PENDING]

    # --- Search restaurants / items ---
    def search_restaurants(self, prefix):
        return [self.restaurants[r] for r in self.rest_trie.search_prefix(prefix)]

    def search_items(self, prefix):
        return [(self.restaurants[r].name, self.item_index[(r, i)])
                for (r, i) in self.item_trie.search_prefix(prefix)]


# ---------- Demo ----------
if __name__ == "__main__":
    s = FoodDeliverySystem()
    s.add_restaurant(1, "Pizza Palace")
    s.add_restaurant(2, "Spice Garden")
    pz = s.add_food_item(1, "Margherita Pizza", 250)
    pa = s.add_food_item(1, "Pasta", 180)
    bi = s.add_food_item(2, "Chicken Biryani", 220)

    o1 = s.place_order(1, {pz: 2, pa: 1})
    o2 = s.place_order(2, {bi: 1})
    print("Pending:", [o.order_id for o in s.get_pending_orders()])

    s.cancel_order(o1)
    print("After cancel:", [o.order_id for o in s.get_pending_orders()])

    o = s.process_next_order()
    print("Processing order", o.order_id, "total", o.total)
    s.deliver(o.order_id)

    print("Restaurants 'pi':", [r.name for r in s.search_restaurants("pi")])
    print("Items 'pi':", [(r, i.name) for r, i in s.search_items("pi")])
