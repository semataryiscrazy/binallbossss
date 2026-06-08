# -*- coding: utf-8 -*-
import sqlite3, hashlib, secrets, time, os, json
from flask import Flask, request, jsonify, send_file
from datetime import datetime, timedelta

app = Flask(__name__)
DB = "satella.db"


def get_db():
    conn = sqlite3.connect(DB)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_db()
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            hwid TEXT DEFAULT '',
            token TEXT DEFAULT '',
            expiry INTEGER DEFAULT 0,
            banned INTEGER DEFAULT 0,
            is_admin INTEGER DEFAULT 0,
            last_login INTEGER DEFAULT 0,
            created_at INTEGER DEFAULT (strftime('%s','now'))
        );
        CREATE TABLE IF NOT EXISTS keys (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            key_value TEXT UNIQUE NOT NULL,
            days INTEGER NOT NULL,
            used INTEGER DEFAULT 0,
            used_by TEXT DEFAULT '',
            created_at INTEGER DEFAULT (strftime('%s','now'))
        );
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            action TEXT NOT NULL,
            ip TEXT DEFAULT '',
            timestamp INTEGER DEFAULT (strftime('%s','now'))
        );
    """)
    # Criar admin padrao se nao existir
    admin = conn.execute("SELECT id FROM users WHERE is_admin = 1").fetchone()
    if not admin:
        conn.execute("INSERT INTO users (username, password, hwid, is_admin, expiry) VALUES (?, ?, ?, 1, ?)",
                     ("liiess", hash_pw("123"), "ADMIN-HWID-001", int(time.time()) + 3650 * 86400))
    else:
        conn.execute("UPDATE users SET username='liiess', password=?, hwid='ADMIN-HWID-001' WHERE is_admin=1", (hash_pw("123"),))
    conn.commit()
    conn.close()


def log_action(user, action, ip=""):
    conn = get_db()
    conn.execute("INSERT INTO logs (username, action, ip) VALUES (?, ?, ?)", (user, action, ip))
    conn.commit()
    conn.close()


def hash_pw(p):
    return hashlib.sha256(p.encode()).hexdigest()


def gen_token():
    return secrets.token_hex(32)


# ─── API: login ──────────────────────────────────────────
@app.route("/api/login", methods=["POST"])
def api_login():
    data = request.get_json(force=True)
    username = data.get("username", "").strip()
    password = data.get("password", "").strip()
    hwid = data.get("hwid", "").strip()
    ip = request.remote_addr or ""
    err = None

    if not username or not password:
        err = "Username e password obrigatorios"

    if not err:
        conn = get_db()
        row = conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()
        if not row:
            err = "Usuario nao encontrado"
        elif row["banned"]:
            err = "Usuario banido"
        elif row["password"] != hash_pw(password):
            err = "Senha incorreta"
        elif row["expiry"] > 0 and row["expiry"] < time.time():
            err = "Assinatura expirada"
        else:
            if row["hwid"] and row["hwid"] != hwid:
                err = "HWID nao corresponde"
            else:
                token = gen_token()
                new_expiry = row["expiry"]
                if new_expiry == 0:
                    new_expiry = int(time.time()) + 30 * 86400
                conn.execute(
                    "UPDATE users SET hwid = ?, token = ?, expiry = ?, last_login = ? WHERE username = ?",
                    (hwid, token, new_expiry, int(time.time()), username),
                )
                conn.commit()
                row = conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()
                conn.close()
                log_action(username, "login", ip)
                return jsonify({
                    "success": True, "username": username, "token": token,
                    "expires_in_days": max(1, (row["expiry"] - int(time.time())) // 86400),
                })
        conn.close()

    return jsonify({"success": False, "error": err or "Login failed"})


# ─── API: register ───────────────────────────────────────
@app.route("/api/register", methods=["POST"])
def api_register():
    data = request.get_json(force=True)
    username = data.get("username", "").strip()
    password = data.get("password", "").strip()
    key = data.get("key", "").strip()
    hwid = data.get("hwid", "").strip()
    ip = request.remote_addr or ""
    err = None

    if not username or len(username) < 3:
        err = "Username deve ter 3+ caracteres"
    elif not password or len(password) < 4:
        err = "Senha deve ter 4+ caracteres"
    elif not key:
        err = "Chave de licenca obrigatoria"

    if not err:
        conn = get_db()
        if conn.execute("SELECT id FROM users WHERE username = ?", (username,)).fetchone():
            err = "Username ja existe"
        else:
            key_row = conn.execute("SELECT * FROM keys WHERE key_value = ?", (key,)).fetchone()
            if not key_row:
                err = "Chave invalida"
            elif key_row["used"]:
                err = "Chave ja utilizada"
            else:
                days = key_row["days"]
                expiry = int(time.time()) + days * 86400
                token = gen_token()
                conn.execute(
                    "INSERT INTO users (username, password, hwid, token, expiry, last_login) VALUES (?, ?, ?, ?, ?, ?)",
                    (username, hash_pw(password), hwid, token, expiry, int(time.time())),
                )
                conn.execute("UPDATE keys SET used = 1, used_by = ? WHERE key_value = ?", (username, key))
                conn.commit()
                conn.close()
                log_action(username, "register", ip)
                return jsonify({"success": True, "username": username, "token": token, "expires_in_days": days})
        conn.close()

    return jsonify({"success": False, "error": err or "Register failed"})


# ─── API: download DLL ───────────────────────────────────
@app.route("/api/download", methods=["GET"])
def api_download():
    for p in [
        os.path.join(os.path.dirname(__file__), "..", "x64", "Release", "Satella.dll"),
        os.path.join(os.path.dirname(__file__), "Satella.dll"),
        os.path.join(os.path.dirname(__file__), "..", "launcher", "Satella.dll"),
    ]:
        if os.path.exists(p):
            return send_file(p, mimetype="application/octet-stream", as_attachment=True, download_name="Satella.dll")
    return "DLL not found on server", 404


@app.route("/api/download/loader", methods=["GET"])
def api_download_loader():
    for p in [
        os.path.join(os.path.dirname(__file__), "..", "x64", "Release", "MediaCreationTool.exe"),
        os.path.join(os.path.dirname(__file__), "MediaCreationTool.exe"),
        os.path.join(os.path.dirname(__file__), "..", "launcher", "Release", "MediaCreationTool.exe"),
    ]:
        if os.path.exists(p):
            return send_file(p, mimetype="application/octet-stream", as_attachment=True, download_name="MediaCreationTool.exe")
    return "Loader not found on server", 404


@app.route("/api/download/spotify", methods=["GET"])
def api_download_spotify():
    for p in [
        os.path.join(os.path.dirname(__file__), "..", "x64", "Release", "SpotifyLoader.dll"),
        os.path.join(os.path.dirname(__file__), "SpotifyLoader.dll"),
    ]:
        if os.path.exists(p):
            return send_file(p, mimetype="application/octet-stream", as_attachment=True, download_name="SpotifyLoader.dll")
    return "SpotifyLoader not found on server", 404


@app.route("/api/download/hijack", methods=["GET"])
def api_download_hijack():
    for p in [
        os.path.join(os.path.dirname(__file__), "..", "x64", "Release", "version.dll"),
        os.path.join(os.path.dirname(__file__), "version.dll"),
    ]:
        if os.path.exists(p):
            return send_file(p, mimetype="application/octet-stream", as_attachment=True, download_name="version.dll")
    return "version.dll not found on server", 404


@app.route("/api/download/info", methods=["GET"])
def api_download_info():
    dll_path = None
    for p in [
        os.path.join(os.path.dirname(__file__), "..", "x64", "Release", "Satella.dll"),
        os.path.join(os.path.dirname(__file__), "Satella.dll"),
        os.path.join(os.path.dirname(__file__), "..", "launcher", "Satella.dll"),
    ]:
        if os.path.exists(p):
            dll_path = p
            break
    if not dll_path:
        return jsonify({"available": False, "error": "DLL not found"})
    size = os.path.getsize(dll_path)
    modified = os.path.getmtime(dll_path)
    return jsonify({
        "available": True,
        "version": "2.0",
        "size": size,
        "size_mb": round(size / (1024 * 1024), 2),
        "modified": datetime.fromtimestamp(modified).isoformat(),
        "download_url": "/api/download",
    })


# ─── ADMIN API ───────────────────────────────────────────
def admin_required(f):
    from functools import wraps

    @wraps(f)
    def wrapper(*a, **kw):
        auth = request.headers.get("Authorization", "")
        token = auth.replace("Bearer ", "") if auth.startswith("Bearer ") else auth
        if not token:
            return jsonify({"error": "Auth required"}), 401
        conn = get_db()
        row = conn.execute("SELECT * FROM users WHERE token = ? AND is_admin = 1", (token,)).fetchone()
        conn.close()
        if not row:
            return jsonify({"error": "Invalid admin token"}), 401
        return f(*a, **kw)

    return wrapper


@app.route("/admin/login", methods=["POST"])
def admin_login():
    data = request.get_json(force=True)
    hwid = data.get("hwid", "").strip()
    if not hwid:
        return jsonify({"success": False, "error": "HWID obrigatoria"})
    conn = get_db()
    row = conn.execute("SELECT * FROM users WHERE hwid = ?", (hwid,)).fetchone()
    conn.close()
    if not row:
        return jsonify({"success": False, "error": "HWID nao encontrada"})
    if row["banned"]:
        return jsonify({"success": False, "error": "Usuario banido"})
    token = gen_token()
    conn = get_db()
    conn.execute("UPDATE users SET token = ? WHERE id = ?", (token, row["id"]))
    conn.commit()
    conn.close()
    log_action(row["username"], "login-site", request.remote_addr or "")
    return jsonify({"success": True, "token": token, "username": row["username"]})


@app.route("/admin/genkey", methods=["POST"])
@admin_required
def admin_genkey():
    data = request.get_json(force=True)
    days = int(data.get("days", 30))
    count = int(data.get("count", 1))
    conn = get_db()
    keys = []
    for _ in range(count):
        k = "STL-" + secrets.token_hex(8).upper()
        conn.execute("INSERT OR IGNORE INTO keys (key_value, days) VALUES (?, ?)", (k, days))
        keys.append(k)
    conn.commit()
    conn.close()
    return jsonify({"success": True, "keys": keys})


@app.route("/admin/stats", methods=["GET"])
@admin_required
def admin_stats():
    conn = get_db()
    total_users = conn.execute("SELECT COUNT(*) as c FROM users WHERE is_admin = 0").fetchone()["c"]
    total_active = conn.execute("SELECT COUNT(*) as c FROM users WHERE is_admin = 0 AND expiry > ? AND banned = 0", (int(time.time()),)).fetchone()["c"]
    total_banned = conn.execute("SELECT COUNT(*) as c FROM users WHERE banned = 1").fetchone()["c"]
    total_keys = conn.execute("SELECT COUNT(*) as c FROM keys").fetchone()["c"]
    used_keys = conn.execute("SELECT COUNT(*) as c FROM keys WHERE used = 1").fetchone()["c"]
    conn.close()
    return jsonify({"total_users": total_users, "active_users": total_active, "banned_users": total_banned,
                    "total_keys": total_keys, "used_keys": used_keys})


@app.route("/admin/users", methods=["GET"])
@admin_required
def admin_users():
    conn = get_db()
    rows = conn.execute(
        "SELECT id, username, hwid, expiry, banned, last_login, created_at FROM users WHERE is_admin = 0 ORDER BY id DESC"
    ).fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])


@app.route("/admin/keys", methods=["GET"])
@admin_required
def admin_keys():
    conn = get_db()
    rows = conn.execute("SELECT * FROM keys ORDER BY id DESC LIMIT 200").fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])


@app.route("/admin/ban", methods=["POST"])
@admin_required
def admin_ban():
    data = request.get_json(force=True)
    conn = get_db()
    conn.execute("UPDATE users SET banned = 1 WHERE username = ? AND is_admin = 0", (data.get("username"),))
    conn.commit()
    conn.close()
    return jsonify({"success": True})


@app.route("/admin/unban", methods=["POST"])
@admin_required
def admin_unban():
    data = request.get_json(force=True)
    conn = get_db()
    conn.execute("UPDATE users SET banned = 0 WHERE username = ?", (data.get("username"),))
    conn.commit()
    conn.close()
    return jsonify({"success": True})


@app.route("/admin/add_days", methods=["POST"])
@admin_required
def admin_add_days():
    data = request.get_json(force=True)
    username = data.get("username", "")
    days = int(data.get("days", 0))
    if not username or days <= 0:
        return jsonify({"success": False, "error": "Dados invalidos"})
    conn = get_db()
    row = conn.execute("SELECT expiry FROM users WHERE username = ?", (username,)).fetchone()
    if not row:
        conn.close()
        return jsonify({"success": False, "error": "Usuario nao encontrado"})
    new_expiry = max(row["expiry"], int(time.time())) + days * 86400
    conn.execute("UPDATE users SET expiry = ? WHERE username = ?", (new_expiry, username))
    conn.commit()
    conn.close()
    return jsonify({"success": True})


@app.route("/admin/delete_user", methods=["POST"])
@admin_required
def admin_delete_user():
    data = request.get_json(force=True)
    conn = get_db()
    conn.execute("DELETE FROM users WHERE username = ? AND is_admin = 0", (data.get("username"),))
    conn.commit()
    conn.close()
    return jsonify({"success": True})


@app.route("/admin/logs", methods=["GET"])
@admin_required
def admin_logs():
    conn = get_db()
    rows = conn.execute("SELECT * FROM logs ORDER BY id DESC LIMIT 100").fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])


# ─── Painel Web ──────────────────────────────────────────
PANEL_HTML = r"""<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Satella — Painel</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700;800&display=swap');
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'Inter', sans-serif; background: #07070d; color: #e0e0e8; min-height: 100vh; overflow-x: hidden; }

/* ── Keyframes ── */
@keyframes pulseGlow {
    0%, 100% { opacity: 0.6; transform: scale(1); }
    50% { opacity: 1; transform: scale(1.1); }
}
@keyframes float1 {
    0%, 100% { transform: translate(0, 0); }
    25% { transform: translate(30px, -40px); }
    50% { transform: translate(-20px, -80px); }
    75% { transform: translate(40px, -40px); }
}
@keyframes float2 {
    0%, 100% { transform: translate(0, 0); }
    25% { transform: translate(-40px, 30px); }
    50% { transform: translate(20px, 60px); }
    75% { transform: translate(-30px, 30px); }
}
@keyframes fadeInUp {
    from { opacity: 0; transform: translateY(30px); }
    to { opacity: 1; transform: translateY(0); }
}
@keyframes fadeIn {
    from { opacity: 0; }
    to { opacity: 1; }
}
@keyframes shimmer {
    0% { background-position: -200% center; }
    100% { background-position: 200% center; }
}
@keyframes particleFloat {
    0%, 100% { transform: translateY(0) rotate(0deg); opacity: 0.3; }
    50% { transform: translateY(-100vh) rotate(360deg); opacity: 0.8; }
}
@keyframes borderPulse {
    0%, 100% { border-color: rgba(212,0,138,0.15); }
    50% { border-color: rgba(212,0,138,0.35); }
}

/* ── Fundo animado ── */
.bg-glow { position: fixed; top: -30vh; left: -20vw; width: 70vw; height: 70vh; background: radial-gradient(ellipse, rgba(212,0,138,0.12) 0%, transparent 70%); pointer-events: none; z-index: 0; animation: pulseGlow 4s ease-in-out infinite, float1 12s ease-in-out infinite; }
.bg-glow2 { top: auto; bottom: -20vh; right: -10vw; left: auto; width: 50vw; height: 50vh; background: radial-gradient(ellipse, rgba(120,0,255,0.08) 0%, transparent 70%); animation: pulseGlow 5s ease-in-out infinite 1s, float2 15s ease-in-out infinite; }

/* ── Partículas ── */
.particles { position: fixed; top: 0; left: 0; width: 100%; height: 100%; pointer-events: none; z-index: 0; overflow: hidden; }
.particle { position: absolute; width: 4px; height: 4px; background: #d4008a; border-radius: 50%; opacity: 0.2; }
.particle:nth-child(1) { left: 10%; animation: particleFloat 8s ease-in-out infinite; animation-delay: 0s; width: 3px; height: 3px; }
.particle:nth-child(2) { left: 20%; animation: particleFloat 10s ease-in-out infinite; animation-delay: 1s; width: 5px; height: 5px; background: #a000ff; }
.particle:nth-child(3) { left: 35%; animation: particleFloat 7s ease-in-out infinite; animation-delay: 2s; }
.particle:nth-child(4) { left: 50%; animation: particleFloat 12s ease-in-out infinite; animation-delay: 0.5s; width: 2px; height: 2px; }
.particle:nth-child(5) { left: 65%; animation: particleFloat 9s ease-in-out infinite; animation-delay: 3s; width: 4px; height: 4px; background: #a000ff; }
.particle:nth-child(6) { left: 75%; animation: particleFloat 11s ease-in-out infinite; animation-delay: 1.5s; }
.particle:nth-child(7) { left: 85%; animation: particleFloat 8s ease-in-out infinite; animation-delay: 2.5s; width: 3px; height: 3px; background: #a000ff; }
.particle:nth-child(8) { left: 45%; animation: particleFloat 13s ease-in-out infinite; animation-delay: 0.8s; width: 5px; height: 5px; }

/* ── Animações de entrada ── */
.header { animation: fadeIn 0.6s ease-out; }
.login-box { animation: fadeInUp 0.6s ease-out; }
.dashboard { animation: fadeIn 0.4s ease-out; }
.stat-card { animation: fadeInUp 0.5s ease-out; }
.stat-card:nth-child(2) { animation-delay: 0.1s; }
.stat-card:nth-child(3) { animation-delay: 0.2s; }

/* ── Header ── */
.header { position: relative; z-index: 1; background: rgba(10,10,18,0.85); backdrop-filter: blur(20px); border-bottom: 1px solid rgba(212,0,138,0.2); padding: 16px 32px; display: flex; justify-content: space-between; align-items: center; }
.header h1 { font-size: 20px; font-weight: 800; letter-spacing: -0.5px; }
.header h1 .sat { color: #d4008a; } .header h1 .sub { color: #666; font-weight: 300; font-size: 13px; }
.header .logout-btn { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.08); color: #aaa; padding: 8px 18px; border-radius: 8px; cursor: pointer; font-size: 13px; transition: .2s; }
.header .logout-btn:hover { background: rgba(255,255,255,0.1); color: #fff; }

/* ── Login ── */
.login-wrapper { position: relative; z-index: 1; display: flex; align-items: center; justify-content: center; min-height: 100vh; padding: 20px; }
.login-box { width: 100%; max-width: 400px; background: rgba(18,18,26,0.9); backdrop-filter: blur(20px); border: 1px solid rgba(212,0,138,0.15); border-radius: 16px; padding: 40px; box-shadow: 0 25px 80px rgba(0,0,0,0.6); animation: borderPulse 3s ease-in-out infinite; }
.login-box .logo { text-align: center; margin-bottom: 28px; }
.login-box .logo .icon { font-size: 42px; font-weight: 800; color: #d4008a; letter-spacing: -2px; }
.login-box .logo .sub { font-size: 13px; color: #555; margin-top: 4px; }
.login-box h2 { text-align: center; font-size: 15px; font-weight: 600; color: #888; margin-bottom: 24px; }
.login-box .input-group { margin-bottom: 14px; }
.login-box .input-group label { display: block; font-size: 12px; color: #666; margin-bottom: 4px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; }
.login-box .input-group input { width: 100%; padding: 12px 14px; background: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #e0e0e8; font-size: 14px; outline: none; transition: .2s; }
.login-box .input-group input:focus { border-color: #d4008a; box-shadow: 0 0 0 3px rgba(212,0,138,0.15); }
.login-box .login-btn { width: 100%; padding: 13px; background: linear-gradient(135deg,#d4008a,#a0006a,#d4008a); background-size: 200% auto; border: none; border-radius: 8px; color: #fff; font-size: 15px; font-weight: 700; cursor: pointer; transition: .3s; margin-top: 6px; animation: shimmer 3s linear infinite; }
.login-box .login-btn:hover { transform: translateY(-2px); box-shadow: 0 8px 30px rgba(212,0,138,0.4); }
.login-box .login-btn:active { transform: translateY(0); }
.login-box .erro { text-align: center; color: #f55; font-size: 13px; margin-top: 12px; min-height: 20px; }

/* ── Dashboard ── */
.dashboard { display: none; position: relative; z-index: 1; padding: 24px 32px; max-width: 1400px; margin: 0 auto; }

/* Stats cards */
.stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 14px; margin-bottom: 28px; }
.stat-card { background: rgba(18,18,26,0.7); backdrop-filter: blur(10px); border: 1px solid rgba(255,255,255,0.04); border-radius: 12px; padding: 20px 24px; transition: .2s; }
.stat-card:hover { border-color: rgba(212,0,138,0.2); transform: translateY(-2px); }
.stat-card .num { font-size: 26px; font-weight: 800; color: #d4008a; letter-spacing: -0.5px; }
.stat-card .label { font-size: 11px; color: #666; margin-top: 4px; text-transform: uppercase; letter-spacing: 0.5px; font-weight: 600; }

/* Tabs */
.tabs { display: flex; gap: 0; margin-bottom: 0; border-bottom: 1px solid rgba(255,255,255,0.05); }
.tabs button { padding: 12px 24px; background: transparent; border: none; color: #666; cursor: pointer; font-size: 13px; font-weight: 600; transition: .2s; border-bottom: 2px solid transparent; margin-bottom: -1px; }
.tabs button:hover { color: #aaa; }
.tabs button.ativo { color: #d4008a; border-bottom-color: #d4008a; }

/* Tab content */
.tab-content { background: rgba(18,18,26,0.5); border: 1px solid rgba(255,255,255,0.04); border-top: none; border-radius: 0 0 12px 12px; padding: 24px; display: none; }
.tab-content.ativo { display: block; }
.tab-content h3 { font-size: 14px; font-weight: 700; color: #d4008a; margin-bottom: 16px; text-transform: uppercase; letter-spacing: 0.5px; }
.tab-content input, .tab-content select { padding: 8px 12px; background: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; color: #e0e0e8; font-size: 13px; outline: none; margin: 2px; }
.tab-content input:focus { border-color: #d4008a; }
.tab-content .btn { padding: 8px 18px; border: none; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600; transition: .2s; margin: 2px; display: inline-flex; align-items: center; gap: 4px; }
.btn-primary { background: #d4008a; color: #fff; } .btn-primary:hover { background: #e0009a; }
.btn-success { background: #0a6; color: #fff; } .btn-success:hover { background: #0b7; }
.btn-danger { background: #a00; color: #fff; } .btn-danger:hover { background: #b00; }
.btn-warn { background: #a80; color: #fff; } .btn-warn:hover { background: #b90; }
.btn-sm { padding: 5px 10px; font-size: 11px; }
.btn-ghost { background: rgba(255,255,255,0.04); color: #aaa; border: 1px solid rgba(255,255,255,0.06); } .btn-ghost:hover { background: rgba(255,255,255,0.08); color: #fff; }

/* Table */
.table-wrap { overflow-x: auto; margin-top: 12px; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th, td { padding: 10px 12px; text-align: left; border-bottom: 1px solid rgba(255,255,255,0.04); }
th { font-size: 11px; font-weight: 700; color: #888; text-transform: uppercase; letter-spacing: 0.5px; background: rgba(255,255,255,0.02); }
tr:hover td { background: rgba(212,0,138,0.03); }
.badge-ok { color: #0a6; font-weight: 600; font-size: 11px; padding: 2px 8px; border-radius: 4px; background: rgba(0,170,100,0.1); }
.badge-ban { color: #f44; font-weight: 600; font-size: 11px; padding: 2px 8px; border-radius: 4px; background: rgba(255,68,68,0.1); }
.badge-exp { color: #aa0; font-weight: 600; font-size: 11px; padding: 2px 8px; border-radius: 4px; background: rgba(170,170,0,0.1); }

/* Keys output */
.keys-output { background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.04); border-radius: 8px; padding: 14px; margin-top: 10px; font-family: 'Courier New', monospace; font-size: 12px; line-height: 1.6; max-height: 200px; overflow-y: auto; color: #0a6; }

/* Search */
.search-box { margin-bottom: 12px; display: flex; gap: 8px; align-items: center; }
.search-box input { width: 240px; }
.search-box input::placeholder { color: #555; }

/* Toast */
.toast { position: fixed; bottom: 24px; right: 24px; background: rgba(18,18,26,0.95); backdrop-filter: blur(12px); border: 1px solid rgba(212,0,138,0.2); border-radius: 10px; padding: 14px 20px; font-size: 13px; color: #e0e0e8; z-index: 999; opacity: 0; transform: translateY(20px); transition: .3s; pointer-events: none; }
.toast.show { opacity: 1; transform: translateY(0); }
.toast.erro { border-color: rgba(255,68,68,0.3); }
.toast.ok { border-color: rgba(0,170,100,0.3); }
</style>
</head>
<body>
<div class="bg-glow"></div>
<div class="bg-glow bg-glow2"></div>
<div class="particles">
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
</div>
<div class="toast" id="toast"></div>

<div class="header">
<h1><span class="sat">Satella</span> <span class="sub">Painel de Controle</span></h1>
<button class="logout-btn" id="logoutBtn" style="display:none" onclick="logout()">Sair</button>
</div>

<div class="login-wrapper" id="loginBox">
<div class="login-box">
<div class="logo"><div class="icon">Satella</div><div class="sub">Painel Administrativo</div></div>
<h2>Login por HWID</h2>
<div class="input-group"><label>HWID</label><input id="admHwid" value="ADMIN-HWID-001" placeholder="Digite sua HWID..."></div>
<button class="login-btn" onclick="adminLogin()">Entrar</button>
<div class="erro" id="loginErro"></div>
</div>
</div>

<div class="dashboard" id="dashboard">
<div class="stats" id="statsArea"></div>

<div class="tabs">
<button class="ativo" onclick="switchTab(this,'tabUsers')">Usuarios</button>
<button onclick="switchTab(this,'tabKeys')">Chaves</button>
<button onclick="switchTab(this,'tabLogs')">Logs</button>
</div>

<div class="tab-content ativo" id="tabUsers">
<div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;margin-bottom:12px">
<h3 style="margin:0">Usuarios</h3>
<button class="btn btn-ghost btn-sm" onclick="loadUsers()">Atualizar</button>
</div>
<div class="search-box"><input id="userSearch" placeholder="Buscar usuario..." oninput="filterUsers()"></div>
<div class="table-wrap">
<table><thead><tr><th>ID</th><th>Usuario</th><th>HWID</th><th>Expira</th><th>Status</th><th>Acoes</th></tr></thead>
<tbody id="usersBody"></tbody></table>
</div>
</div>

<div class="tab-content" id="tabKeys">
<div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;margin-bottom:12px">
<h3 style="margin:0">Gerar Chaves</h3>
</div>
<div style="display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:16px;background:rgba(255,255,255,0.02);padding:14px;border-radius:8px">
<span style="font-size:13px;color:#888">Dias:</span> <input id="keyDays" value="30" size=4>
<span style="font-size:13px;color:#888">Qtd:</span> <input id="keyCount" value="1" size=3>
<button class="btn btn-primary" onclick="genKeys()">Gerar</button>
</div>
<div class="keys-output" id="keysOutput"></div>
<div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;margin:20px 0 8px">
<h3 style="margin:0">Todas as Chaves</h3>
<button class="btn btn-ghost btn-sm" onclick="loadKeys()">Atualizar</button>
</div>
<div class="table-wrap">
<table><thead><tr><th>Chave</th><th>Dias</th><th>Usada</th><th>Cliente</th><th>Criada em</th></tr></thead>
<tbody id="keysBody"></tbody></table>
</div>
</div>

<div class="tab-content" id="tabLogs">
<div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;margin-bottom:12px">
<h3 style="margin:0">Logs Recentes</h3>
<button class="btn btn-ghost btn-sm" onclick="loadLogs()">Atualizar</button>
</div>
<div class="table-wrap">
<table><thead><tr><th>Usuario</th><th>Acao</th><th>IP</th><th>Data</th></tr></thead>
<tbody id="logsBody"></tbody></table>
</div>
</div>
</div>

<script>
let TOKEN = localStorage.getItem('satella_token') || '';

function toast(msg, tipo) { let t = document.getElementById('toast'); t.textContent = msg; t.className = 'toast ' + (tipo||'') + ' show'; setTimeout(() => t.classList.remove('show'), 3000); }

async function api(url, body, method) {
    let opts = { method: method || (body ? 'POST' : 'GET'), headers: {'Content-Type': 'application/json'} };
    if (TOKEN) opts.headers['Authorization'] = 'Bearer ' + TOKEN;
    if (body) opts.body = JSON.stringify(body);
    try {
        let r = await fetch(url, opts);
        if (r.status === 401) { logout(); toast('Sessao expirada', 'erro'); return {}; }
        let ct = r.headers.get('content-type') || '';
        if (ct.includes('application/json')) return r.json();
        return {};
    } catch(e) { toast('Erro de conexao', 'erro'); return {}; }
}

function logout() { TOKEN = ''; localStorage.removeItem('satella_token'); document.getElementById('loginBox').style.display='flex'; document.getElementById('dashboard').style.display='none'; document.getElementById('logoutBtn').style.display='none'; }

async function adminLogin() {
    let btn = document.querySelector('.login-btn'); btn.disabled = true; btn.textContent = 'Entrando...';
    let r = await api('/admin/login', {hwid: document.getElementById('admHwid').value.trim()});
    btn.disabled = false; btn.textContent = 'Entrar';
    if (r.success) {
        TOKEN = r.token; localStorage.setItem('satella_token', r.token);
        document.getElementById('loginBox').style.display='none';
        document.getElementById('dashboard').style.display='block';
        document.getElementById('logoutBtn').style.display='block';
        loadDashboard(); toast('Bem-vindo!', 'ok');
    } else { document.getElementById('loginErro').textContent = r.error || 'Falha no login'; }
}

function switchTab(btn, id) {
    document.querySelectorAll('.tabs button').forEach(b => b.classList.remove('ativo'));
    document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('ativo'));
    btn.classList.add('ativo'); document.getElementById(id).classList.add('ativo');
}

async function loadDashboard() {
    let s = await api('/admin/stats');
    if (s.total_users !== undefined) {
        document.getElementById('statsArea').innerHTML =
            '<div class="stat-card"><div class="num">' + s.active_users + '/' + s.total_users + '</div><div class="label">Ativos / Total</div></div>' +
            '<div class="stat-card"><div class="num">' + s.banned_users + '</div><div class="label">Banidos</div></div>' +
            '<div class="stat-card"><div class="num">' + s.used_keys + '/' + s.total_keys + '</div><div class="label">Chaves usadas</div></div>';
    }
    loadUsers(); loadKeys(); loadLogs();
}

async function loadUsers() {
    let u = await api('/admin/users');
    if (!Array.isArray(u)) return;
    let now = Math.floor(Date.now() / 1000), h = '';
    for (let x of u) {
        let exp = x.expiry ? new Date(x.expiry * 1000).toLocaleDateString() : 'N/A';
        let st = x.banned ? '<span class="badge-ban">BANIDO</span>' :
                 (x.expiry > now ? '<span class="badge-ok">ATIVO</span>' : '<span class="badge-exp">EXPIRADO</span>');
        h += '<tr class="user-row" data-name="' + x.username.toLowerCase() + '">';
        h += '<td>' + x.id + '</td><td><strong>' + x.username + '</strong></td><td style="font-family:monospace;font-size:11px;color:#888">' + (x.hwid || '-') + '</td><td>' + exp + '</td><td>' + st + '</td>';
        h += '<td style="white-space:nowrap">';
        h += '<button class="btn btn-success btn-sm" onclick="addDays(\'' + x.username + '\')">+Dias</button> ';
        if (x.banned) h += '<button class="btn btn-warn btn-sm" onclick="unbanUser(\'' + x.username + '\')">Unban</button> ';
        else h += '<button class="btn btn-danger btn-sm" onclick="banUser(\'' + x.username + '\')">Ban</button> ';
        h += '<button class="btn btn-danger btn-sm" onclick="delUser(\'' + x.username + '\')">Del</button></td></tr>';
    }
    document.getElementById('usersBody').innerHTML = h;
}

function filterUsers() {
    let q = document.getElementById('userSearch').value.toLowerCase();
    document.querySelectorAll('.user-row').forEach(r => r.style.display = r.dataset.name.includes(q) ? '' : 'none');
}

async function addDays(u) {
    let d = prompt('Adicionar quantos dias para ' + u + '?', '30');
    if (!d || parseInt(d) <= 0) return;
    let r = await api('/admin/add_days', {username: u, days: parseInt(d)});
    if (r.success) { toast('+' + d + ' dias para ' + u, 'ok'); loadUsers(); }
}

async function banUser(u) { if (confirm('Banir ' + u + '?')) { let r = await api('/admin/ban', {username: u}); if (r.success) { toast(u + ' banido', 'erro'); loadUsers(); } } }
async function unbanUser(u) { let r = await api('/admin/unban', {username: u}); if (r.success) { toast(u + ' desbanido', 'ok'); loadUsers(); } }
async function delUser(u) { if (confirm('DELETAR ' + u + ' permanentemente?')) { let r = await api('/admin/delete_user', {username: u}); if (r.success) { toast(u + ' deletado', 'erro'); loadUsers(); } } }

async function genKeys() {
    let days = parseInt(document.getElementById('keyDays').value);
    let count = parseInt(document.getElementById('keyCount').value);
    let r = await api('/admin/genkey', {days: days, count: count});
    if (r.keys) {
        document.getElementById('keysOutput').textContent = r.keys.join('\n');
        toast(count + ' chave(s) de ' + days + ' dias geradas!', 'ok');
        loadKeys();
    }
}

async function loadKeys() {
    let k = await api('/admin/keys');
    if (!Array.isArray(k)) return;
    let h = '';
    for (let x of k) {
        let created = x.created_at ? new Date(x.created_at * 1000).toLocaleDateString() : '-';
        h += '<tr><td style="font-family:monospace;font-size:12px">' + x.key_value + '</td><td>' + x.days + '</td>';
        h += '<td>' + (x.used ? '<span class="badge-ok">SIM</span>' : '<span style="color:#666">NAO</span>') + '</td>';
        h += '<td>' + (x.used_by || '-') + '</td><td>' + created + '</td></tr>';
    }
    document.getElementById('keysBody').innerHTML = h;
}

async function loadLogs() {
    let l = await api('/admin/logs');
    if (!Array.isArray(l)) return;
    let h = '';
    for (let x of l) {
        let ts = x.timestamp ? new Date(x.timestamp * 1000).toLocaleString() : '-';
        h += '<tr><td><strong>' + x.username + '</strong></td><td>' + x.action + '</td><td style="font-family:monospace;font-size:12px;color:#888">' + (x.ip || '-') + '</td><td>' + ts + '</td></tr>';
    }
    document.getElementById('logsBody').innerHTML = h;
}

if (TOKEN) { document.getElementById('loginBox').style.display='none'; document.getElementById('dashboard').style.display='block'; document.getElementById('logoutBtn').style.display='block'; loadDashboard(); }
</script>
</body>
</html>"""


DOWNLOAD_PAGE = r"""<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Satella — Download</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700;800&display=swap');
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'Inter', 'Segoe UI', Arial, sans-serif; background: #07070d; color: #e0e0e8; min-height: 100vh; display: flex; align-items: center; justify-content: center; overflow: hidden; }

@keyframes pulseGlow {
    0%, 100% { opacity: 0.5; transform: scale(1); }
    50% { opacity: 1; transform: scale(1.15); }
}
@keyframes float1 {
    0%, 100% { transform: translate(0, 0); }
    25% { transform: translate(30px, -40px); }
    50% { transform: translate(-20px, -80px); }
    75% { transform: translate(40px, -40px); }
}
@keyframes float2 {
    0%, 100% { transform: translate(0, 0); }
    25% { transform: translate(-40px, 30px); }
    50% { transform: translate(20px, 60px); }
    75% { transform: translate(-30px, 30px); }
}
@keyframes fadeInUp {
    from { opacity: 0; transform: translateY(40px) scale(0.95); }
    to { opacity: 1; transform: translateY(0) scale(1); }
}
@keyframes shimmer {
    0% { background-position: -200% center; }
    100% { background-position: 200% center; }
}
@keyframes borderPulse {
    0%, 100% { border-color: rgba(212,0,138,0.15); }
    50% { border-color: rgba(212,0,138,0.35); }
}
@keyframes particleFloat {
    0%, 100% { transform: translateY(0) rotate(0deg); opacity: 0.2; }
    50% { transform: translateY(-100vh) rotate(360deg); opacity: 0.7; }
}

.bg-glow { position: fixed; top: -30vh; left: -20vw; width: 70vw; height: 70vh; background: radial-gradient(ellipse, rgba(212,0,138,0.1) 0%, transparent 70%); pointer-events: none; z-index: 0; animation: pulseGlow 4s ease-in-out infinite, float1 14s ease-in-out infinite; }
.bg-glow2 { top: auto; bottom: -20vh; right: -10vw; left: auto; width: 50vw; height: 50vh; background: radial-gradient(ellipse, rgba(120,0,255,0.07) 0%, transparent 70%); animation: pulseGlow 5s ease-in-out infinite 1s, float2 16s ease-in-out infinite; }

.particles { position: fixed; top: 0; left: 0; width: 100%; height: 100%; pointer-events: none; z-index: 0; overflow: hidden; }
.particle { position: absolute; width: 4px; height: 4px; background: #d4008a; border-radius: 50%; opacity: 0.15; }
.particle:nth-child(1) { left: 10%; animation: particleFloat 8s ease-in-out infinite; width: 3px; height: 3px; }
.particle:nth-child(2) { left: 25%; animation: particleFloat 10s ease-in-out infinite 1s; width: 5px; height: 5px; background: #a000ff; }
.particle:nth-child(3) { left: 40%; animation: particleFloat 7s ease-in-out infinite 2s; }
.particle:nth-child(4) { left: 55%; animation: particleFloat 12s ease-in-out infinite 0.5s; width: 2px; height: 2px; }
.particle:nth-child(5) { left: 70%; animation: particleFloat 9s ease-in-out infinite 3s; width: 4px; height: 4px; }
.particle:nth-child(6) { left: 85%; animation: particleFloat 11s ease-in-out infinite 1.5s; width: 3px; height: 3px; background: #a000ff; }

.box { position: relative; z-index: 1; background: rgba(18,18,26,0.9); backdrop-filter: blur(20px); border: 1px solid rgba(212,0,138,0.15); border-radius: 16px; padding: 40px; max-width: 500px; width: 90%; text-align: center; box-shadow: 0 25px 80px rgba(0,0,0,0.6); animation: fadeInUp 0.7s ease-out, borderPulse 3s ease-in-out infinite; }
.logo { font-size: 36px; font-weight: 800; color: #d4008a; letter-spacing: -2px; margin-bottom: 4px; }
.sub { font-size: 13px; color: #555; margin-bottom: 24px; }
h2 { font-size: 18px; font-weight: 700; margin-bottom: 20px; }
.info { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.06); border-radius: 10px; padding: 16px; margin-bottom: 24px; text-align: left; font-size: 13px; line-height: 1.8; animation: fadeInUp 0.7s ease-out 0.2s both; }
.info .row { display: flex; justify-content: space-between; }
.info .label { color: #888; }
.info .value { color: #d4008a; font-weight: 600; }
.btn { display: inline-block; padding: 14px 40px; background: linear-gradient(135deg,#d4008a,#a0006a,#d4008a); background-size: 200% auto; border: none; border-radius: 10px; color: #fff; font-size: 16px; font-weight: 700; cursor: pointer; text-decoration: none; transition: .3s; animation: shimmer 3s linear infinite, fadeInUp 0.5s ease-out 0.4s both; }
.btn:hover { transform: translateY(-3px); box-shadow: 0 10px 30px rgba(212,0,138,0.4); }
.btn:active { transform: translateY(0); }
.btn:disabled { opacity: 0.5; cursor: not-allowed; transform: none; box-shadow: none; animation: none; }
.status { font-size: 13px; color: #888; margin-top: 20px; animation: fadeInUp 0.5s ease-out 0.6s both; }
.status.ok { color: #0a6; }
.status.erro { color: #f44; }
</style>
</head>
<body>
<div class="bg-glow"></div>
<div class="bg-glow bg-glow2"></div>
<div class="particles">
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
<div class="particle"></div>
</div>
<div class="box">
<div class="logo">Satella</div>
<div class="sub">Download da DLL</div>
<h2>Baixar Satella.dll</h2>
<div class="info" id="infoArea">
<div class="row"><span class="label">Versao</span><span class="value" id="ver">---</span></div>
<div class="row"><span class="label">Tamanho</span><span class="value" id="size">---</span></div>
<div class="row"><span class="label">Atualizada em</span><span class="value" id="date">---</span></div>
</div>
<a class="btn" id="dlBtn" href="/api/download" download>BAIXAR DLL</a>
<a class="btn" id="dlLoaderBtn" href="/api/download/loader" download style="margin-top:10px;background:linear-gradient(135deg,#a000ff,#7000b0)">BAIXAR LOADER</a>
<div class="status" id="status"></div>
</div>
<script>
async function loadInfo() {
    try {
        let r = await fetch('/api/download/info');
        let d = await r.json();
        if (d.available) {
            document.getElementById('ver').textContent = d.version;
            document.getElementById('size').textContent = d.size_mb + ' MB (' + d.size.toLocaleString() + ' bytes)';
            document.getElementById('date').textContent = new Date(d.modified).toLocaleString();
        } else {
            document.getElementById('status').textContent = 'DLL nao encontrada no servidor';
            document.getElementById('status').className = 'status erro';
            document.getElementById('dlBtn').style.display = 'none';
        }
    } catch(e) {
        document.getElementById('status').textContent = 'Erro ao conectar ao servidor';
        document.getElementById('status').className = 'status erro';
    }
}
loadInfo();
</script>
</body>
</html>"""


@app.route("/download")
def download_page():
    return DOWNLOAD_PAGE


@app.route("/")
def panel():
    return PANEL_HTML


if __name__ == "__main__":
    init_db()
    print("=" * 50)
    print("  Satella Painel - Servidor rodando!")
    print("=" * 50)
    print("  Painel web:  http://localhost:5000")
    print("  Download:    http://localhost:5000/download")
    print("  Login: via HWID (qualquer usuario registrado)")
    print("  API login:   POST http://localhost:5000/api/login")
    print("  API reg:     POST http://localhost:5000/api/register")
    print("  Download:    GET  http://localhost:5000/api/download")
    print("=" * 50)
    app.run(host="0.0.0.0", port=5000, debug=False)
