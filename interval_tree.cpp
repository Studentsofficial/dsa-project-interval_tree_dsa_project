#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "sqlite3.h"
using namespace std;


// ─── DATA STRUCTURES ─────────────────────────────────────────────────────────

struct Interval { int low, high; };

struct Node {
    Interval i;
    int      max;
    Node*    left;
    Node*    right;
};

struct OverlapResult {
    bool overlaps;
    int  conflict_low;
    int  conflict_high;
};


// ─── INTERVAL TREE ───────────────────────────────────────────────────────────

class IntervalTree {
private:
    Node* root;

    Node* createNode(int l, int h) {
        Node* n   = new Node();
        n->i.low  = l;
        n->i.high = h;
        n->max    = h;
        n->left   = NULL;
        n->right  = NULL;
        return n;
    }

    Node* insert(Node* node, int l, int h) {
        if (!node) return createNode(l, h);
        if (l < node->i.low)
            node->left  = insert(node->left,  l, h);
        else
            node->right = insert(node->right, l, h);
        if (node->max < h) node->max = h;
        return node;
    }

    Node* findOverlap(Node* node, int l, int h) {
        if (!node) return NULL;
        if (node->i.low <= h && node->i.high >= l)
            return node;
        if (node->left && node->left->max >= l)
            return findOverlap(node->left, l, h);
        return findOverlap(node->right, l, h);
    }

    void collect(Node* node, vector<Node*>& out) {
        if (!node) return;
        collect(node->left,  out);
        out.push_back(node);
        collect(node->right, out);
    }

    Node* removeNode(Node* node, int l, int h) {
        if (!node) return NULL;
        if (node->i.low == l && node->i.high == h) {
            vector<Node*> rest;
            collect(node->left,  rest);
            collect(node->right, rest);
            delete node;
            Node* newRoot = NULL;
            for (int i = 0; i < (int)rest.size(); i++) {
                newRoot = insert(newRoot, rest[i]->i.low, rest[i]->i.high);
                delete rest[i];
            }
            return newRoot;
        }
        if (l < node->i.low)
            node->left  = removeNode(node->left,  l, h);
        else
            node->right = removeNode(node->right, l, h);
        node->max = node->i.high;
        if (node->left)  node->max = max(node->max, node->left->max);
        if (node->right) node->max = max(node->max, node->right->max);
        return node;
    }

public:
    IntervalTree() : root(NULL) {}

    void insert(int l, int h) { root = insert(root, l, h); }
    void remove(int l, int h) { root = removeNode(root, l, h); }

    OverlapResult checkOverlap(int l, int h) {
        OverlapResult res;
        Node* found = findOverlap(root, l, h);
        if (found) {
            res.overlaps       = true;
            res.conflict_low   = found->i.low;
            res.conflict_high  = found->i.high;
        } else {
            res.overlaps       = false;
            res.conflict_low   = -1;
            res.conflict_high  = -1;
        }
        return res;
    }

    vector<pair<int,int> > getAllIntervals() {
        vector<Node*> nodes;
        collect(root, nodes);
        vector<pair<int,int> > out;
        for (int i = 0; i < (int)nodes.size(); i++)
            out.push_back(make_pair(nodes[i]->i.low, nodes[i]->i.high));
        return out;
    }
};


// ─── SQLITE HELPERS ──────────────────────────────────────────────────────────

const string DB_FILE = "intervals.db";

/**
 * Open intervals.db and rebuild the IntervalTree from the allocations table.
 * Python/Flask owns all writes to the DB; this function is read-only.
 */
IntervalTree loadFromDB() {
    IntervalTree tree;
    sqlite3* db = NULL;

    if (sqlite3_open(DB_FILE.c_str(), &db) != SQLITE_OK) {
        // Can't open DB — return empty tree (safe fallback)
        if (db) sqlite3_close(db);
        return tree;
    }

    sqlite3_stmt* stmt = NULL;
    const char*   sql  = "SELECT start, end FROM allocations";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int l = sqlite3_column_int(stmt, 0);
            int h = sqlite3_column_int(stmt, 1);
            tree.insert(l, h);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return tree;
}


// ─── MAIN ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "{\"error\":\"No command provided\"}" << endl;
        return 1;
    }

    string cmd = argv[1];

    // ── check low high ──────────────────────────────────────────────────────
    if (cmd == "check") {
        if (argc < 4) {
            cout << "{\"error\":\"check requires low high\"}" << endl;
            return 1;
        }
        int l = atoi(argv[2]), h = atoi(argv[3]);

        IntervalTree  tree = loadFromDB();
        OverlapResult res  = tree.checkOverlap(l, h);

        if (res.overlaps) {
            cout << "{\"overlaps\":true,\"conflict_range\":["
                 << res.conflict_low << "," << res.conflict_high << "]}"
                 << endl;
        } else {
            cout << "{\"overlaps\":false}" << endl;
        }
        return 0;
    }

    // ── insert low high ─────────────────────────────────────────────────────
    // Persistence is handled entirely by Flask/Python (insert_allocation).
    // The C++ binary no longer writes state; just acknowledge success.
    if (cmd == "insert") {
        if (argc < 4) {
            cout << "{\"error\":\"insert requires low high\"}" << endl;
            return 1;
        }
        cout << "{\"success\":true}" << endl;
        return 0;
    }

    // ── remove low high ─────────────────────────────────────────────────────
    // Persistence is handled entirely by Flask/Python (delete_allocation).
    if (cmd == "remove") {
        if (argc < 4) {
            cout << "{\"error\":\"remove requires low high\"}" << endl;
            return 1;
        }
        cout << "{\"success\":true}" << endl;
        return 0;
    }

    // ── status ──────────────────────────────────────────────────────────────
    if (cmd == "status") {
        IntervalTree             tree      = loadFromDB();
        vector<pair<int,int> >   intervals = tree.getAllIntervals();

        cout << "{\"intervals\":[";
        for (int i = 0; i < (int)intervals.size(); i++) {
            cout << "[" << intervals[i].first << "," << intervals[i].second << "]";
            if (i + 1 < (int)intervals.size()) cout << ",";
        }
        cout << "]}" << endl;
        return 0;
    }

    cout << "{\"error\":\"Unknown command\"}" << endl;
    return 1;
}