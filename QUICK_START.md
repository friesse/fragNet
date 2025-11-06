# Game Coordinator Server - Quick Start Guide

## 🚀 Quick Setup

### 1. Build the Server

**Linux:**
```bash
cd game-coordinator-server/gc-server-main
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Windows:**
```cmd
cd game-coordinator-server\gc-server-main
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A Win32 ..
cmake --build . --config Release
```

### 2. Configure Binding

**Option A: Environment Variables (Recommended)**
```bash
# Linux/Mac
export GC_BIND_IP="0.0.0.0"
export GC_PORT=21818
./start_gc_server.sh

# Windows
set GC_BIND_IP=0.0.0.0
set GC_PORT=21818
start_gc_server.bat
```

**Option B: Edit Source Code**

Edit `gc_server/main.cpp` lines 11-12:
```cpp
const char* BIND_IP = "0.0.0.0";  // Your server IP or 0.0.0.0
const uint16 GAME_PORT = 21818;
```
Then rebuild.

### 3. Configure Database

Edit `gc_server/networking.cpp` lines 56, 64, 73:
```cpp
// Replace these placeholders:
"mysql_connection_ip"       -> "79.72.94.86"
"mysql_connection_username" -> "classiccounter_user"
"mysql_connection_password" -> "ClassicC0unter!DB2025"
```

### 4. Run the Server

**Linux:**
```bash
chmod +x start_gc_server.sh
./start_gc_server.sh
```

**Windows:**
```cmd
start_gc_server.bat
```

**Or directly:**
```bash
./build/gc_server/gc_server  # Linux
build\Release\gc_server.exe   # Windows
```

---

## 📋 Configuration Options

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GC_BIND_IP` | `0.0.0.0` | IP address to bind to |
| `GC_PORT` | `21818` | UDP port for GC traffic |
| `SteamAppId` | `730` | Steam App ID (CS:GO) |

### Binding Options

| IP | Effect |
|----|--------|
| `0.0.0.0` | All interfaces (public server) |
| `192.168.x.x` | LAN only |
| `[Public IP]` | Specific interface |
| `127.0.0.1` | Localhost only (testing) |

---

## ✅ Verify It's Working

### Check Console Output

**SUCCESS ✅:**
```
Initializing Steam Game Server on 0.0.0.0:21818
Steam Game Server initialized successfully
Attempting to bind GC network socket to 0.0.0.0:21818
Created a listen socket on (0) 0.0.0.0:21818
Connected to classiccounter DB successfully!
Connected to ollum_inventory DB successfully!
Connected to ollum_ranked DB successfully!
```

**FAILURE ❌:**
```
Created a listen socket on (2130706433) 127.0.0.1:21818
Socket bound to 127.0.0.1 (LOCALHOST ONLY - not accessible from network!)
```

### Check Network Binding

**Linux:**
```bash
netstat -tulpn | grep 21818
# Should show: udp 0.0.0.0:21818 (not 127.0.0.1:21818)
```

**Windows:**
```cmd
netstat -ano | findstr 21818
```

### Test From Remote Machine

```bash
# Linux
nc -u -v [server_ip] 21818

# Windows PowerShell  
Test-NetConnection -ComputerName [server_ip] -Port 21818 -InformationLevel Detailed
```

---

## 🔥 Firewall Rules

### Ubuntu/Debian
```bash
sudo ufw allow 21818/udp
sudo ufw reload
```

### CentOS/RHEL
```bash
sudo firewall-cmd --permanent --add-port=21818/udp
sudo firewall-cmd --reload
```

### Windows (PowerShell as Admin)
```powershell
New-NetFirewallRule -DisplayName "CS:GO GC Server" -Direction Inbound -LocalPort 21818 -Protocol UDP -Action Allow
```

---

## 🐛 Common Issues

### Issue: Still Binding to 127.0.0.1

**Fix 1:** Use your actual server IP instead of 0.0.0.0:
```bash
export GC_BIND_IP="79.72.94.86"  # Your real IP
```

**Fix 2:** Use iptables forwarding (workaround):
```bash
sudo iptables -t nat -A PREROUTING -i eth0 -p udp --dport 21818 -j DNAT --to-destination 127.0.0.1:21818
```

**Fix 3:** See `NETWORK_BINDING_FIX.md` for Alternative Solutions

### Issue: Database Connection Failed

Check:
1. Database credentials in `networking.cpp`
2. Database server is running: `sudo systemctl status mariadb`
3. Firewall allows port 3306: `sudo ufw allow 3306/tcp`
4. Network connectivity: `telnet 79.72.94.86 3306`

### Issue: Port Already in Use

Find and kill the process:
```bash
# Linux
sudo netstat -tulpn | grep 21818
sudo kill [PID]

# Windows
netstat -ano | findstr 21818
taskkill /PID [PID] /F
```

Or use a different port:
```bash
export GC_PORT=27015
```

---

## 📁 Project Structure

```
gc-server-main/
├── gc_server/              # Main server code
│   ├── main.cpp           # Entry point (BIND_IP configured here)
│   ├── networking.cpp     # Network & DB (DB credentials here)
│   ├── networking_inventory.cpp
│   └── ...
├── steamworks/            # Steam SDK
├── items/                 # Item definitions
├── build/                 # Build output
├── start_gc_server.sh     # Linux startup script
├── start_gc_server.bat    # Windows startup script
├── QUICK_START.md         # This file
└── NETWORK_BINDING_FIX.md # Detailed troubleshooting
```

---

## 🔗 Related Files

- **Binding configuration:** `gc_server/main.cpp` (lines 11-22)
- **Network init:** `gc_server/networking.cpp` (lines 107-152)
- **Database config:** `gc_server/networking.cpp` (lines 42-82)
- **Detailed guide:** `NETWORK_BINDING_FIX.md`

---

## 📞 Support Checklist

If you need help, provide:

1. **Console output** (first 50 lines)
2. **Binding check:** 
   ```bash
   netstat -tulpn | grep 21818
   ```
3. **Server OS:** `uname -a` (Linux) or `systeminfo` (Windows)
4. **Build output:** Any errors from cmake/make
5. **Network config:**
   ```bash
   ip addr show  # Linux
   ipconfig      # Windows
   ```

---

## ⚡ Performance Tips

1. **Use specific IP** instead of 0.0.0.0 if possible (slightly faster)
2. **Check firewall logs** for dropped packets
3. **Monitor with:** `watch -n 1 'netstat -su | grep -i udp'`
4. **Increase UDP buffers** (Linux):
   ```bash
   sudo sysctl -w net.core.rmem_max=2097152
   sudo sysctl -w net.core.wmem_max=2097152
   ```

---

## 🎯 Next Steps

1. ✅ Get server binding to 0.0.0.0
2. Configure game coordinator client to connect
3. Test with CS:GO 2016 client
4. Set up auto-restart (systemd/Windows Service)
5. Configure monitoring/logging
6. Implement rate limiting (if needed)

---

**Good luck! 🚀**

For detailed troubleshooting, see: `NETWORK_BINDING_FIX.md`
