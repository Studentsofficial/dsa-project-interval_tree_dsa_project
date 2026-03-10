#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
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

public:
    IntervalTree() : root(NULL) {}

    void insert(int l, int h) { root = insert(root, l, h); }

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


// ─── HELPER: build tree from argv pairs starting at index `from` ──────────────

IntervalTree buildTreeFromArgs(int argc, char* argv[], int from) {
    IntervalTree tree;
    // args come in pairs: low high low high ...
    for (int i = from; i + 1 < argc; i += 2) {
        int l = atoi(argv[i]);
        int h = atoi(argv[i + 1]);
        tree.insert(l, h);
    }
    return tree;
}


// ─── MAIN ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "{\"error\":\"No command provided\"}" << endl;
        return 1;
    }

    string cmd = argv[1];

    // ── check low high [l1 h1 l2 h2 ...] ────────────────────────────────────
    // Flask passes all currently-allocated intervals as trailing arg pairs.
    if (cmd == "check") {
        if (argc < 4) {
            cout << "{\"error\":\"check requires low high\"}" << endl;
            return 1;
        }
        int l = atoi(argv[2]), h = atoi(argv[3]);

        IntervalTree  tree = buildTreeFromArgs(argc, argv, 4);
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
    // Persistence is handled entirely by Flask/Python.
    // The C++ binary just acknowledges.
    if (cmd == "insert") {
        if (argc < 4) {
            cout << "{\"error\":\"insert requires low high\"}" << endl;
            return 1;
        }
        cout << "{\"success\":true}" << endl;
        return 0;
    }

    // ── remove low high ─────────────────────────────────────────────────────
    // Persistence is handled entirely by Flask/Python.
    if (cmd == "remove") {
        if (argc < 4) {
            cout << "{\"error\":\"remove requires low high\"}" << endl;
            return 1;
        }
        cout << "{\"success\":true}" << endl;
        return 0;
    }

    // ── status [l1 h1 l2 h2 ...] ────────────────────────────────────────────
    // Flask passes all currently-allocated intervals as trailing arg pairs.
    if (cmd == "status") {
        IntervalTree           tree      = buildTreeFromArgs(argc, argv, 2);
        vector<pair<int,int> > intervals = tree.getAllIntervals();

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