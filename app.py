from flask import Flask, request, jsonify
from flask_cors import CORS
import subprocess
import json
import os
import sqlite3

app = Flask(__name__)
CORS(app)

# ─── DATABASE SETUP ───────────────────────────────────────────────────────────

DB_FILE = "intervals.db"

def get_db():
    """Open a connection to the SQLite database."""
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row   # access columns by name, e.g. row["start"]
    return conn

def init_db():
    """Create the allocations table if it doesn't already exist."""
    conn = get_db()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS allocations (
            process_id TEXT PRIMARY KEY,
            start      INTEGER NOT NULL,
            end        INTEGER NOT NULL,
            offset     INTEGER NOT NULL
        )
    """)
    conn.commit()
    conn.close()

# Initialize DB on startup
init_db()


# ─── HELPER FUNCTIONS ─────────────────────────────────────────────────────────

def get_all_allocations():
    """Return all allocations as a list of dicts."""
    conn = get_db()
    rows = conn.execute("SELECT * FROM allocations").fetchall()
    conn.close()
    return [dict(r) for r in rows]

def get_allocation(pid):
    """Return a single allocation by process_id, or None."""
    conn = get_db()
    row = conn.execute(
        "SELECT * FROM allocations WHERE process_id = ?", (pid,)
    ).fetchone()
    conn.close()
    return dict(row) if row else None

def insert_allocation(pid, start, end, offset):
    """Insert a new allocation row."""
    conn = get_db()
    conn.execute(
        "INSERT INTO allocations (process_id, start, end, offset) VALUES (?, ?, ?, ?)",
        (pid, start, end, offset)
    )
    conn.commit()
    conn.close()

def delete_allocation(pid):
    """Delete an allocation by process_id."""
    conn = get_db()
    conn.execute("DELETE FROM allocations WHERE process_id = ?", (pid,))
    conn.commit()
    conn.close()

def clear_all_allocations():
    """Delete every allocation row."""
    conn = get_db()
    conn.execute("DELETE FROM allocations")
    conn.commit()
    conn.close()


# ─── C++ INTERVAL TREE BRIDGE ─────────────────────────────────────────────────

def call_cpp_interval_tree(command, args):
    binary = os.environ.get("CPP_BINARY", "./interval_tree_cli")
    cmd = [binary, command] + [str(a) for a in args]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        return json.loads(result.stdout)
    except FileNotFoundError:
        return mock_interval_tree(command, args)
    except Exception as e:
        return {"error": str(e)}


def mock_interval_tree(command, args):
    """Pure-Python fallback when C++ binary is not compiled yet."""
    allocations = {r["process_id"]: r for r in get_all_allocations()}

    if command == "check":
        low, high = int(args[0]), int(args[1])
        for pid, alloc in allocations.items():
            if alloc["start"] <= high and alloc["end"] >= low:
                return {"overlaps": True, "conflict_pid": pid,
                        "conflict_range": [alloc["start"], alloc["end"]]}
        return {"overlaps": False}
    elif command == "insert":
        return {"success": True}
    elif command == "remove":
        return {"success": True}
    elif command == "status":
        return {"intervals": [
            {"low": v["start"], "high": v["end"], "pid": k}
            for k, v in allocations.items()
        ]}
    return {"error": "Unknown command"}


# ─── ROUTES ───────────────────────────────────────────────────────────────────

@app.route("/status", methods=["GET"])
def status():
    rows = get_all_allocations()
    result = [
        {"process_id": r["process_id"], "start": r["start"],
         "end": r["end"], "offset": r["offset"]}
        for r in rows
    ]
    return jsonify({"allocations": result})


@app.route("/allocate", methods=["POST"])
def allocate():
    data = request.get_json()
    try:
        start  = int(data["start_address"])
        offset = int(data["offset"])
        pid    = str(data["process_id"]).strip()
    except (KeyError, ValueError, TypeError) as e:
        return jsonify({"success": False, "error": f"Invalid input: {e}"}), 400

    if not pid:
        return jsonify({"success": False, "error": "process_id cannot be empty"}), 400
    if offset <= 0:
        return jsonify({"success": False, "error": "offset must be positive"}), 400
    if get_allocation(pid):
        return jsonify({"success": False, "error": f"Process '{pid}' already has an allocation"}), 409

    end   = start + offset
    check = call_cpp_interval_tree("check", [start, end])

    if check.get("error"):
        return jsonify({"success": False, "error": check["error"]}), 500

    if check.get("overlaps"):
        return jsonify({
            "success": False,
            "error": "Overlap detected",
            "conflict_pid": check.get("conflict_pid"),
            "conflict_range": check.get("conflict_range")
        }), 409

    call_cpp_interval_tree("insert", [start, end])
    insert_allocation(pid, start, end, offset)   # ← saved to DB

    return jsonify({
        "success": True,
        "message": f"Allocated [{start}, {end}] for process '{pid}'"
    })


@app.route("/free", methods=["POST"])
def free_memory():
    data = request.get_json()
    pid  = str(data.get("process_id", "")).strip()

    alloc = get_allocation(pid)
    if not alloc:
        return jsonify({"success": False, "error": f"No allocation found for process '{pid}'"}), 404

    call_cpp_interval_tree("remove", [alloc["start"], alloc["end"]])
    delete_allocation(pid)   # ← removed from DB

    return jsonify({"success": True, "message": f"Freed memory for process '{pid}'"})


@app.route("/reset", methods=["POST"])
def reset():
    clear_all_allocations()   # wipe DB table (C++ reads from this DB — no .txt file anymore)
    return jsonify({"success": True, "message": "All allocations cleared"})


if __name__ == "__main__":
    app.run(debug=True, port=5000)