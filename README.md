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
                   ↕
               SQLite DB
             (intervals.db)
```

1. You enter a **start address**, **offset (size)**, and **process ID** in the UI
2. The UI sends a request to Flask
3. Flask reads the current allocations from `intervals.db` and passes them to the C++ binary as command-line arguments
4. The C++ binary builds the interval tree in memory and checks for overlaps — no DB access needed
5. If no overlap → Flask saves the new allocation to the database
6. If overlap → request is rejected and you see exactly which process caused the conflict

---

## Project Structure

```
DSA_Interval_Tree/
│
├── interval_tree.cpp       ← C++ interval tree + CLI wrapper (compile this)
├── interval_tree_cli.exe   ← compiled binary (generated after compiling)
│
├── app.py                  ← Flask backend (all API routes + DB read/write)
├── intervals.db            ← SQLite database (auto-created on first run)
├── flask_state.json        ← Flask runtime config (auto-created)
│
└── index.html              ← Frontend UI (open this in browser)
```

---

## Features

- **Overlap detection** using an interval tree — O(log n) checks
- **Visual memory map** — see all allocations as colored bars in real time
- **Capacity management** — set total memory size, get warned when running low
- **Persistent state** — backed by SQLite; restart Flask and nothing is lost
- **Free by dropdown** — select any active process and free it in one click
- **Conflict details** — when an allocation fails, you see exactly which process it clashes with
- **Pure-Python fallback** — Flask falls back to a Python overlap checker if the C++ binary isn't compiled yet

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

Flask will automatically create `intervals.db` on first launch. Leave this terminal open.

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
interval_tree_cli check  <low> <high> [<l1> <h1> ...]   # overlap check; existing intervals passed as args
interval_tree_cli insert <low> <high>                    # ack only; Flask handles DB write
interval_tree_cli remove <low> <high>                    # ack only; Flask handles DB delete
interval_tree_cli status [<l1> <h1> ...]                 # dump intervals; existing intervals passed as args
```

### Persistence Architecture

State is owned entirely by **Flask / SQLite**. C++ is a pure in-memory algorithm:

| Who | Does what |
|-----|-----------|
| Flask (`app.py`) | All DB reads and writes — `SELECT`, `INSERT`, `DELETE`, `CREATE TABLE` |
| C++ (`interval_tree_cli`) | Pure algorithm — receives intervals as CLI args, builds tree in memory, returns JSON |

The C++ binary has **zero database dependency** — no SQLite headers, no file I/O.

---

## Why Flask + subprocess instead of a Python binding?

Honestly, it's the simplest approach that works without any extra setup. No need for `ctypes`, `cffi`, or compiling a shared library — Flask just runs the binary and reads its JSON output from stdout. It adds a tiny bit of latency but for a project like this, it's completely fine.

---

## Known Limitations

- No memory compaction or defragmentation
- Each process can only hold one allocation at a time
- The interval tree is not self-balancing (no AVL/Red-Black)

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Data structure | C++ (interval tree, pure in-memory) |
| Backend API | Python / Flask |
| Frontend | HTML + CSS + Vanilla JS |
| Communication | REST (JSON over HTTP) |
| State persistence | SQLite (`intervals.db`) — owned by Flask |

---

## License

MIT — do whatever you want with it. Would appreciate a star if it helped! ⭐