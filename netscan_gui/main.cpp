#include <QtWidgets>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>

// ==== POSIX / sockets cross-platform bits ====
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  // Só o MSVC entende #pragma comment; evite aviso no MinGW
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  using socklen_t = int;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <errno.h>
  #include <sys/select.h>
#endif

static void set_nonblock_socket(int fd){
#ifdef _WIN32
  u_long mode = 1; ioctlsocket(fd, FIONBIO, &mode);
#else
  int flags = fcntl(fd, F_GETFL, 0); fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}
static void close_socket(int fd){
#ifdef _WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}

// ===================== CORE SCANNERS =====================
static std::string tcp_connect_scan(const std::string& ip, int port, int timeout_ms){
  int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return "error";
  set_nonblock_socket(fd);

  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
#ifdef _WIN32
  inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
#else
  if(inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1){ close_socket(fd); return "error"; }
#endif

  int r;
#ifdef _WIN32
  r = ::connect(fd, (sockaddr*)&addr, sizeof(addr));
  int lastErr = WSAGetLastError();
  if(r==0){ close_socket(fd); return "open"; }
  if(lastErr != WSAEWOULDBLOCK && lastErr != WSAEINPROGRESS){
    if(lastErr == WSAECONNREFUSED){ close_socket(fd); return "closed"; }
    close_socket(fd); return "filtered";
  }
  fd_set wfds; FD_ZERO(&wfds); FD_SET((SOCKET)fd, &wfds);
  timeval tv{ timeout_ms/1000, (timeout_ms%1000)*1000 };
  int sel = select(0, nullptr, &wfds, nullptr, &tv);
  if(sel>0){
    int opt=0; socklen_t optlen=sizeof(opt);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&opt, &optlen);
    close_socket(fd);
    if(opt==0) return "open";
    if(opt==WSAECONNREFUSED) return "closed";
    return "filtered";
  } else { close_socket(fd); return "filtered"; }
#else
  r = ::connect(fd, (sockaddr*)&addr, sizeof(addr));
  if(r==0){ close_socket(fd); return "open"; }
  if(errno != EINPROGRESS){ if(errno==ECONNREFUSED){ close_socket(fd); return "closed";} close_socket(fd); return "filtered"; }
  fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
  timeval tv{ timeout_ms/1000, (timeout_ms%1000)*1000 };
  int sel = select(fd+1, nullptr, &wfds, nullptr, &tv);
  if(sel>0){
    int err=0; socklen_t len=sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    close_socket(fd);
    if(err==0) return "open";
    if(err==ECONNREFUSED) return "closed";
    return "filtered";
  } else { close_socket(fd); return "filtered"; }
#endif
}

static std::string udp_probe_scan(const std::string& ip, int port, int timeout_ms){
  int fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0) return "error";
#ifdef _WIN32
  DWORD t = timeout_ms; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
#else
  timeval tv{ timeout_ms/1000, (timeout_ms%1000)*1000 }; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
  sockaddr_in dst{}; dst.sin_family = AF_INET; dst.sin_port = htons(port);
#ifdef _WIN32
  inet_pton(AF_INET, ip.c_str(), &dst.sin_addr);
#else
  if(inet_pton(AF_INET, ip.c_str(), &dst.sin_addr) != 1){ close_socket(fd); return "error"; }
#endif
  char payload[1] = {0};
  sendto(fd, payload, sizeof(payload), 0, (sockaddr*)&dst, sizeof(dst));

  char buf[256]; sockaddr_in src{}; socklen_t sl = sizeof(src);
  int n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
  close_socket(fd);
#ifdef _WIN32
  if(n!=SOCKET_ERROR && n>0) return "open"; else return "open|filtered";
#else
  if(n>0) return "open"; else return "open|filtered";
#endif
}

// ===================== GUI =====================
class Worker : public QObject {
  Q_OBJECT
public:
  explicit Worker(QObject* parent=nullptr) : QObject(parent) {}
  void requestStop(){ stop_.store(true); }

signals:
  void resultReady(QString ip, QString proto, int port, QString state);
  void progress(int current, int total);
  void finished();

public slots:
  void startScan(QString targetsCsv, QString portsSpec, bool doTcp, bool doUdp, int timeoutMs){
    stop_.store(false);
    auto ips = expandTargets(targetsCsv);
    auto ports = parsePorts(portsSpec);
    const int modes = (doTcp?1:0) + (doUdp?1:0);
    const int total = (int)ips.size() * (int)ports.size() * modes;
    int done = 0;
    for(const auto& ip : ips){
      for(int p : ports){
        if(stop_) { emit finished(); return; }
        if(doTcp){
          auto st = QString::fromStdString(tcp_connect_scan(ip.toStdString(), p, timeoutMs));
          emit resultReady(ip, "tcp", p, st);
          emit progress(++done, total);
          if(stop_) { emit finished(); return; }
        }
        if(doUdp){
          auto st = QString::fromStdString(udp_probe_scan(ip.toStdString(), p, timeoutMs));
          emit resultReady(ip, "udp", p, st);
          emit progress(++done, total);
          if(stop_) { emit finished(); return; }
        }
      }
    }
    emit finished();
  }

private:
  std::atomic<bool> stop_{false};
  static QStringList expandTargets(const QString& csv){
    QStringList out; for(const auto& part : csv.split(',', Qt::SkipEmptyParts)) out << part.trimmed(); return out; }
  static QList<int> parsePorts(const QString& spec){
    QList<int> out; const auto parts = spec.split(',', Qt::SkipEmptyParts);
    for(const auto& p : parts){
      const auto s=p.trimmed(); int dash=s.indexOf('-');
      if(dash<0){ out<<s.toInt(); }
      else { int a=s.left(dash).toInt(); int b=s.mid(dash+1).toInt(); if(a>b) std::swap(a,b); for(int i=a;i<=b;++i) out<<i; }
    }
    std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(), out.end()), out.end()); return out;
  }
};

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(){ setupUi(); }
  ~MainWindow(){ onStop(); }

private slots:
  void onStart(){
    if(workerThread_) return;
    btnStart_->setEnabled(false); btnStop_->setEnabled(true);
    progress_->setVisible(true); progress_->setRange(0,0);

    workerThread_ = new QThread(this);
    worker_ = new Worker();
    worker_->moveToThread(workerThread_);

    // Executa startScan NA THREAD DO WORKER
    connect(workerThread_, &QThread::started, worker_, [this]{
      worker_->startScan(
        targets_->text(),
        ports_->text(),
        cbTcp_->isChecked(),
        cbUdp_->isChecked(),
        timeout_->text().toInt()
      );
    });

    connect(worker_, &Worker::resultReady, this, &MainWindow::onResult);
    connect(worker_, &Worker::progress,    this, &MainWindow::onProgress);
    connect(worker_, &Worker::finished,    this, &MainWindow::onFinished);
    connect(worker_, &Worker::finished,    workerThread_, &QThread::quit);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(workerThread_, &QThread::finished, [this]{ workerThread_=nullptr; worker_=nullptr; });

    workerThread_->start();
  }
  void onStop(){ if(worker_) worker_->requestStop(); }
  void onResult(QString ip, QString proto, int port, QString state){
    int row = table_->rowCount(); table_->insertRow(row);
    table_->setItem(row,0,new QTableWidgetItem(ip));
    table_->setItem(row,1,new QTableWidgetItem(proto));
    table_->setItem(row,2,new QTableWidgetItem(QString::number(port)));
    table_->setItem(row,3,new QTableWidgetItem(state));
  }
  void onProgress(int current, int total){ progress_->setRange(0,total); progress_->setValue(current); }
  void onFinished(){ btnStart_->setEnabled(true); btnStop_->setEnabled(false); progress_->setVisible(false); }

private:
  QLineEdit *targets_, *ports_, *timeout_;
  QCheckBox *cbTcp_, *cbUdp_;
  QPushButton *btnStart_, *btnStop_;
  QProgressBar *progress_;
  QTableWidget *table_;
  QThread *workerThread_ = nullptr; Worker* worker_ = nullptr;

  void setupUi(){
    auto central = new QWidget(this);
    auto form = new QFormLayout;

    targets_ = new QLineEdit("127.0.0.1");
    ports_   = new QLineEdit("1-1024");
    timeout_ = new QLineEdit("800");
    cbTcp_   = new QCheckBox("TCP"); cbTcp_->setChecked(true);
    cbUdp_   = new QCheckBox("UDP");
    btnStart_ = new QPushButton("Iniciar");
    btnStop_  = new QPushButton("Parar"); btnStop_->setEnabled(false);
    progress_ = new QProgressBar; progress_->setVisible(false);

    form->addRow("Alvos (CSV):", targets_);
    form->addRow("Portas:", ports_);
    form->addRow("Timeout (ms):", timeout_);

    auto hb = new QHBoxLayout; hb->addWidget(cbTcp_); hb->addWidget(cbUdp_);
    auto protoBox = new QWidget; protoBox->setLayout(hb);
    form->addRow("Protocolos:", protoBox);

    auto hb2 = new QHBoxLayout; hb2->addWidget(btnStart_); hb2->addWidget(btnStop_); hb2->addWidget(progress_);
    auto ctlBox = new QWidget; ctlBox->setLayout(hb2);
    form->addRow(ctlBox);

    table_ = new QTableWidget(0,4);
    table_->setHorizontalHeaderLabels({"IP","Proto","Porta","Estado"});
    table_->horizontalHeader()->setStretchLastSection(true);

    auto v = new QVBoxLayout; v->addLayout(form); v->addWidget(table_);
    central->setLayout(v);
    setCentralWidget(central);
    resize(900, 520);
    setWindowTitle("NetScan GUI (TCP/UDP)");

    connect(btnStart_, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(btnStop_,  &QPushButton::clicked, this, &MainWindow::onStop);
  }
};

// ============== main() ==============
int main(int argc, char** argv){
#ifdef _WIN32
  WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
  QApplication app(argc, argv);
  MainWindow w; w.show();
  int rc = app.exec();
#ifdef _WIN32
  WSACleanup();
#endif
  return rc;
}

#include "main.moc"
