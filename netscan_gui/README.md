# NetScan GUI (TCP/UDP)

Ferramenta simples de **varredura de portas TCP e UDP** com **interface gráfica (Qt)**.  
Funciona em **Linux** (obrigatório) e também em **Windows** e **macOS**.

> ⚠️ Use **apenas** em redes e hosts para os quais você tem autorização.

---

## ✨ Recursos

- **TCP connect-scan** (não requer root): detecta `open`, `closed` e `filtered`.
- **UDP probe**: detecta `open` e `open|filtered` (ver nota abaixo).
- **Interface gráfica** (Qt Widgets): alvo(s), portas, timeout, seleção TCP/UDP, tabela de resultados.
- **Multiplataforma**: Linux, Windows, macOS.
- Código em **C++17** com **CMake** + **Qt 6**.

> 🔎 **UDP “closed”**: por padrão, UDP é silencioso; quando não há resposta, marcamos `open|filtered`.
> Para marcar `closed` quando chegar **ICMP Port Unreachable**, veja a seção **“UDP closed (opcional)”**.

---

## 📦 Pré-requisitos

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y g++ cmake qt6-base-dev build-essential
```

### Windows
- **Qt 6 (Desktop)** com **kit MinGW 64-bit** (ou MSVC 64-bit) — via Qt Online Installer  
- **CMake** (marque “Add CMake to PATH”): https://cmake.org/download/  
- **(Opcional) Python 3** para testes (`py -m http.server`, `udp_echo.py`)  
- **(Opcional) Qt Creator** (vem no instalador do Qt) – recomendado pela facilidade

> Dica: Se usar **MinGW**, o compilador já vem junto no próprio Qt.

### macOS
```bash
brew install cmake qt
# Caminho do Qt via Homebrew geralmente: /opt/homebrew/opt/qt
```

---

## 🚀 Build

### ✅ Opção A — **Qt Creator** (mais simples)

**Como abrir e rodar:**
1. **Iniciar** → digite **“Qt Creator”** → **Enter**  
2. **File → Open File or Project…** → selecione o `CMakeLists.txt` do projeto  
3. Na tela de kits, escolha **Desktop Qt 6.x MinGW 64-bit** (ou MSVC) → **Configure Project**  
4. Clique no **martelo (Build)** para compilar  
5. Clique no **Play (Run)** para executar a GUI

> **Teste rápido (TCP):**  
> Abra um PowerShell e rode `py -m http.server 8080 --bind 127.0.0.1`.  
> Na GUI: Alvo `127.0.0.1`, Porta `8080`, Timeout `800`, marque **TCP** → **Iniciar** → deve dar **open**.  
> Pare o servidor (Ctrl+C) e escaneie de novo → **closed**.

---

### 🔧 Opção B — **CMake pela linha de comando**

#### Linux
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./netscan_gui
```

#### Windows (MinGW)

**Abrir o prompt certo:**
- **Iniciar** → “**Qt 6.x (MinGW 64-bit) Command Prompt**” → **Enter**

**Compilar e rodar:**
```powershell
cd C:\caminho\para\repo
mkdir build; cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64"
cmake --build .
.\netscan_gui.exe
```

#### Windows (MSVC / Visual Studio 2022)
```powershell
cd C:\caminho\para\repo
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build . --config Release
.\Release\netscan_gui.exe
```

#### macOS
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./netscan_gui.app/Contents/MacOS/netscan_gui
```

---

## 🖱️ Uso (GUI)

- **Alvos (CSV)**: um ou mais IPs separados por vírgula.  
  Ex.: `127.0.0.1,192.168.1.10`
- **Portas**: lista/intervalo – ex.: `22,53,80,443` ou `1-1024`
- **Timeout (ms)**: 800–1200 (LAN); 1500–3000 (internet)
- Marque **TCP** e/ou **UDP** → **Iniciar**

### Interpretação dos estados

**TCP**
- `open` – handshake OK → serviço ouvindo  
- `closed` – conexão recusada (RST) → porta fechada  
- `filtered` – sem resposta/timeout/erro de rede → firewall/host silencioso

**UDP**
- `open` – houve resposta UDP  
- `open|filtered` – nenhuma resposta (pode estar aberto e silencioso **ou** filtrado)  
- `closed` – **apenas** se habilitar o modo opcional ICMP (abaixo)

---

## 🧪 Testes rápidos

### TCP aberto
```bash
# Linux/macOS
python3 -m http.server 8080 --bind 127.0.0.1
# Windows
py -m http.server 8080 --bind 127.0.0.1
```
Escaneie `127.0.0.1` porta `8080` (TCP) → **open**.  
Pare (Ctrl+C) e escaneie de novo → **closed**.

### UDP aberto (echo simples)
> Use uma porta livre (ex.: **9999**) para evitar conflito com mDNS (5353).

Crie `udp_echo.py`:
```python
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 9999))
print("UDP echo em 127.0.0.1:9999")
while True:
    data, addr = s.recvfrom(1024)
    s.sendto(b"ok", addr)
```
Rode:
```bash
# Windows
py udp_echo.py
# Linux/macOS
python3 udp_echo.py
```
Escaneie `127.0.0.1` porta `9999` (UDP) → **open**.  
Sem esse script, é normal ver **open|filtered** no UDP.

---

## 🔧 “UDP closed” por ICMP (opcional)

Para marcar UDP **closed** quando o host retorna **ICMP Port Unreachable**:
1. Abra `main.cpp`.
2. Substitua `udp_probe_scan(...)` por uma versão que usa `connect()` no socket e verifica o erro (`ECONNREFUSED` no Linux, `WSAECONNRESET` no Windows).
3. Recompile.

Isso melhora o diagnóstico de UDP sem precisar de libpcap.

---

## 🛠️ Problemas comuns

- **Tudo dá `filtered`** → aumente o **Timeout** (ex.: 1200–1500 ms) e verifique firewall/roteamento.  
- **`Could NOT find Qt6::Widgets`** → passe `-DCMAKE_PREFIX_PATH` com o caminho do kit Qt (ex.: `C:\Qt\6.9.2\mingw_64`).  
- **Erro AutoMoc / `main.moc`** → em `CMakeLists.txt` use `set(CMAKE_AUTOMOC ON)` e no fim do `main.cpp` inclua `#include "main.moc"`.  
- **Windows: Winsock não linka** → garanta `target_link_libraries(netscan_gui PRIVATE ws2_32)` no `CMakeLists.txt`.  
- **Checar porta no Windows**:
  ```powershell
  Test-NetConnection 127.0.0.1 -Port 8080
  ```

---

## 📁 Estrutura do projeto
```
.
├─ CMakeLists.txt
└─ main.cpp
```

---

## 🙏 Aviso legal
A varredura de portas pode ser interpretada como atividade intrusiva.  
Utilize **somente** em ambientes onde você possui permissão explícita.

---

## Apêndice — Atalhos rápidos (Windows)

**Abrir Qt Creator e rodar**
1. Iniciar → “**Qt Creator**” → Enter  
2. *File → Open File or Project…* → selecione `CMakeLists.txt`  
3. Kit **Desktop Qt 6.x MinGW 64-bit** → *Configure Project*  
4. **Build** (martelo) → **Run** (play)

**Abrir o Prompt do Qt (MinGW) e rodar sem Qt Creator**
1. Iniciar → “**Qt 6.x (MinGW 64-bit) Command Prompt**” → Enter  
2. No prompt, rode:
```powershell
cd C:\caminho\para\repo
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64"
cmake --build .
.\netscan_gui.exe
```
