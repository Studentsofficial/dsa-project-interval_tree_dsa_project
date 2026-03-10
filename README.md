# MemSys — Memory Management System

> An interval tree-powered memory allocator with a real-time web UI, built with C++, Flask, and vanilla JavaScript.

---

## What is this?

Ever wondered how operating systems make sure two processes don't accidentally share the same chunk of memory? That's exactly what this project simulates.

**MemSys** is a memory management system where every allocation request is checked against an **interval tree** (implemented in C++) before being approved. If two processes would overlap in memory — the system catches it instantly and tells you exactly what's conflicting.

It's a full-stack DSA project — the C++ does the heavy lifting, Flask acts as the middleman, and a dark-themed web UI lets you interact with everything in real time.

---

## How it works

```
Browser UI  →  Flask API  →  C++ Interval Tree
   (HTML)        (Python)       (interval_tree_cli.exe)
```

1. You enter a **start address**, **offset (size)**, and **process ID** in the UI
2. The UI sends a request to Flask
3. Flask calls the C++ binary to check if the range `[start, start+offset]` overlaps with anything already allocated
4. If no overlap → memory is allocated and saved
5. If overlap → request is rejected and you see exactly which process caused the conflict

---

## Project Structure

```
DSA_Interval_Tree/
│
├── interval_tree.cpp       ← C++ interval tree + CLI wrapper (compile this)
├── interval_tree_cli.exe   ← compiled binary (generated after compiling)
├── memtree_state.txt       ← C++ persists tree state here between calls
│
├── app.py                  ← Flask backend (all API routes)
├── flask_state.json        ← Flask persists allocations here (auto-created)
│
└── index.html              ← Frontend UI (open this in browser)
```

---

## Features

- **Overlap detection** using an interval tree — O(log n) checks
- **Visual memory map** — see all allocations as colored bars in real time
- **Capacity management** — set total memory size, get warned when running low
- **Persistent state** — restart Flask and nothing is lost
- **Free by dropdown** — select any active process and free it in one click
- **Conflict details** — when an allocation fails, you see exactly which process it clashes with

---

## Getting Started

### 1. Prerequisites

- **GCC** (for compiling C++) — [MinGW](https://www.mingw-w64.org/) on Windows
- **Python 3** with pip
- A modern browser

### 2. Compile the C++

```bash
g++ -o interval_tree_cli interval_tree.cpp
```

This creates `interval_tree_cli.exe` on Windows (or `interval_tree_cli` on Mac/Linux).

### 3. Install Python dependencies

```bash
pip install flask flask-cors
```

### 4. Start the Flask server

```bash
python app.py
```

You should see:
```
Running on http://127.0.0.1:5000
```

Leave this terminal open.

### 5. Open the UI

In a **second terminal**, run:

```bash
python -m http.server 8080
```

Then open your browser and go to:

```
http://localhost:8080/index.html
```

---

## API Reference

| Method | Endpoint | What it does |
|--------|----------|--------------|
| `GET`  | `/status` | Returns all current allocations |
| `POST` | `/allocate` | Tries to allocate memory for a process |
| `POST` | `/free` | Frees memory for a specific process |
| `POST` | `/reset` | Clears all allocations |

### Example — Allocate

**Request:**
```json
POST /allocate
{
  "start_address": 1000,
  "offset": 512,
  "process_id": "P1"
}
```

**Success response:**
```json
{
  "success": true,
  "message": "Allocated [1000, 1512] for process 'P1'"
}
```

**Overlap response:**
```json
{
  "success": false,
  "error": "Overlap detected",
  "conflict_pid": "P1",
  "conflict_range": [1000, 1512]
}
```

### Example — Free

```json
POST /free
{ "process_id": "P1" }
```

---

## The Interval Tree (C++)

The core data structure is an **augmented BST** where each node stores:
- An interval `[low, high]`
- A `max` value — the maximum `high` across its entire subtree

This `max` value makes overlap checking efficient — if `node.left.max < query.low`, we can skip the entire left subtree.

**Overlap condition:**
```
A overlaps B  ⟺  A.low <= B.high  AND  A.high >= B.low
```

The C++ binary accepts 4 commands via command line:

```bash
interval_tree_cli check  <low> <high>   # does [low,high] overlap anything?
interval_tree_cli insert <low> <high>   # add interval to tree
interval_tree_cli remove <low> <high>   # remove interval from tree
interval_tree_cli status                # dump all intervals as JSON
```

State is saved to `memtree_state.txt` so the tree persists between subprocess calls from Flask.

---

## Why Flask + subprocess instead of a Python binding?

Honestly, it's the simplest approach that works without any extra setup. No need for `ctypes`, `cffi`, or compiling a shared library — Flask just runs the binary and reads its JSON output from stdout. It adds a tiny bit of latency but for a project like this, it's completely fine.

---

## Known Limitations

- No memory compaction or defragmentation
- Each process can only hold one allocation at a time
- State is stored in flat files, not a real database
- The interval tree is not self-balancing (no AVL/Red-Black)

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Data structure | C++ (interval tree) |
| Backend API | Python / Flask |
| Frontend | HTML + CSS + Vanilla JS |
| Communication | REST (JSON over HTTP) |
| State persistence | JSON file (Flask) + TXT file (C++) |

---

## License

MIT — do whatever you want with it. Would appreciate a star if it helped! ⭐