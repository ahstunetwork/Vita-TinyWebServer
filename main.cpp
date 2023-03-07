#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

const int MAX_CONNECTION = 1024;

// form file_type to http_file_type
std::unordered_map<std::string, std::string> umap;

void init_umap() {
    umap["html"] = "text/html; charset=utf-8";
    umap["htm"] = "text/html; charset=utf-8";
    umap["jpg"] = "image/jpeg";
    umap["jpeg"] = "image/jpeg";
    umap["gif"] = "image/gif";
    umap["png"] = "image/png";
    umap["css"] = "text/css";
    umap["au"] = "audio/basic";
    umap["wav"] = "audio/wav";
    umap["avi"] = "video/x-msvideo";
    umap["mov"] = "video/quicktime";
    umap["qt"] = "video/quicktime";
    umap["mpeg"] = "video/mpeg";
    umap["mpe"] = "video/mpeg";
    umap["vrml"] = "model/vrml";
    umap["wrl"] = "model/vrml";
    umap["mid"] = "audio/midi";
    umap["midi"] = "audio/midi";
    umap["mp3"] = "audio/mpeg";
    umap["ogg"] = "application/ogg";
    umap["pac"] = "application/x-ns-proxy-autoconfig";
}

// 通过文件名获取文件的类型
std::string get_file_type(const std::string& file) {
    std::string dot = file.substr(file.find_last_of('.') + 1);
    // if (dot.empty())
    //     return "text/plain; charset=utf-8";
    // if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
    //     return "text/html; charset=utf-8";
    // if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
    //     return "image/jpeg";
    // if (strcmp(dot, ".gif") == 0)
    //     return "image/gif";
    // if (strcmp(dot, ".png") == 0)
    //     return "image/png";
    // if (strcmp(dot, ".css") == 0)
    //     return "text/css";
    // if (strcmp(dot, ".au") == 0)
    //     return "audio/basic";
    // if (strcmp(dot, ".wav") == 0)
    //     return "audio/wav";
    // if (strcmp(dot, ".avi") == 0)
    //     return "video/x-msvideo";
    // if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
    //     return "video/quicktime";
    // if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
    //     return "video/mpeg";
    // if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
    //     return "model/vrml";
    // if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
    //     return "audio/midi";
    // if (strcmp(dot, ".mp3") == 0)
    //     return "audio/mpeg";
    // if (strcmp(dot, ".ogg") == 0)
    //     return "application/ogg";
    // if (strcmp(dot, ".pac") == 0)
    //     return "application/x-ns-proxy-autoconfig";
    auto it = umap.find(dot);
    if (it != umap.end()) {
        return it->second;
    }
    return "text/plain; charset=utf-8";
}




// 发送错误信息
void send_error(int cfd, int status, char *title, char *text)
{
	char buf[4096] = {0};

	sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
	sprintf(buf+strlen(buf), "Content-Type:%s\r\n", "text/html");
	sprintf(buf+strlen(buf), "Content-Length:%d\r\n", -1);
	sprintf(buf+strlen(buf), "Connection: close\r\n");
	send(cfd, buf, strlen(buf), 0);
	send(cfd, "\r\n", 2, 0);

	memset(buf, 0, sizeof(buf));

	sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
	sprintf(buf+strlen(buf), "<body bgcolor=\"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
	sprintf(buf+strlen(buf), "%s\n", text);
	sprintf(buf+strlen(buf), "<hr>\n</body>\n</html>\n");
	send(cfd, buf, strlen(buf), 0);
	
	return ;
}



void sys_err(const std::string& msg) {
    std::cout << msg << std::endl;
    exit(-1);
}

int accept4_connection(int epfd,
                       int lfd,
                       sockaddr_in& client_addr,
                       socklen_t& client_addr_len) {
    int cfd =
        ::accept4(lfd, (sockaddr*)&client_addr, &client_addr_len, O_NONBLOCK);
    epoll_event ev_temp;
    ev_temp.data.fd = cfd;
    ev_temp.events = EPOLLIN;
    ::epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev_temp);
    return cfd;
}

int get_line(int cfd, char* buf, int size) {
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {
        n = ::recv(cfd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = ::recv(cfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {
                    ::recv(cfd, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    if (-1 == n) {
        i = n;
    }
    return i;
}

int dissconnection(int epfd, int cfd) {
    int ret = ::epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, nullptr);
    if (ret != 0) {
        sys_err("epoll_ctl disconnection error");
    }
    return ret;
}

void send_response(int cfd,
                   int error_no,
                   std::string file_desc,
                   std::string file_type,
                   int file_len) {
    std::vector<char> buf(1024);
    ::sprintf(buf.data(), "HTTP/1.1 %d %s\r\n", error_no, file_desc.c_str());
    ::sprintf(buf.data() + strlen(buf.data()), "%s\r\n", file_type.c_str());
    ::sprintf(buf.data() + strlen(buf.data()), "Content-Length:%d\r\n",
              file_len);
    std::cout << "send_response\n" << buf.data() << std::endl;
    send(cfd, buf.data(), strlen(buf.data()), 0);
    send(cfd, "\r\n", 2, 0);
}

// 发送服务器本地文件给浏览器
void send_file(int cfd,  std::string file) {
    std::cout << "file = " << file << std::endl;
    int filefd = ::open(file.c_str(), O_RDONLY);
    if (filefd == -1) {
        file = "page_404.html";
        filefd = ::open(file.c_str(), O_RDONLY);
    } else {
        // std::cout << "filename = " << file << std::endl
        //           << "filefd = " << filefd << std::endl;
        int n = 0;
        std::vector<char> buf(1024);
        while ((n = ::read(filefd, buf.data(), 1024)) > 0) {
            int ret = ::send(cfd, buf.data(), n, 0);
            if( ret == -1 ) {
                std::cout << "send return -1, errorno = " << errno << std::endl;
                if( errno == EAGAIN ) {
                    continue; 
                } else if( errno == EINTR ) {
                    continue;;
                } else {
                    sys_err("error ::send");
                }
            }
            // std::cout << "ret = " << ret << std::endl;
        }
    }
}

void http_request(int cfd, std::string file) {
    struct stat sbuf;
    int ret = ::stat(file.c_str(), &sbuf);
    if (ret != 0) {
        std::cout << "redirection -> 404" << std::endl;
        file = "page_404.html";
        ret = ::stat(file.c_str(), &sbuf);
        std::cout << "file not found" << std::endl;
    }
    if (S_ISREG(sbuf.st_mode)) {
        std::cout << file << " is reg" << std::endl;

        // 回发应答协议头
        // 回发应答内容

        std::string content_type = "Content-Type: " + get_file_type(file);
        std::cout << "file type = " << content_type << std::endl;

        send_response(cfd, 200, "ok", content_type.c_str(), sbuf.st_size);
        std::cout << "send_response ok" << std::endl;
        send_file(cfd, file);
        std::cout << "send_file ok" << std::endl;
    } else if (S_ISDIR(sbuf.st_mode)) {
        std::cout << file << " is dir" << std::endl;
        
    }
}

// 处理读请求，这个读请求一定是http请求
int do_read_http(int cfd, int epfd) {
    // 1. get_line 读取 http 协议行
    // 2. 从行中拆分 GET 文件名 协议版本信息
    // 3. 判断文件是否存在 ::stat
    // 4. 判断是文件还是目录
    // 5. 是文件 -- open -- read -- 写给brower
    //      5.1 按照 http 协议应答消息来写
    //      5.2 协议版本--请求码--请求状态
    // 6. 写文件数据

    std::vector<char> line(1024);
    int read_len = get_line(cfd, line.data(), 1024);

    std::cout << "begin" << std::endl;
    std::cout << line.data() << std::endl;
    std::cout << "end--" << std::endl;

    if (read_len == 0) {
        sys_err("client error");
        // close
        dissconnection(epfd, cfd);
    } else if (read_len > 0) {
        // GET /hello.c HTTP/1.1
        // strtok 可以拆分，但有更好的方法
        std::vector<char> method(16);
        std::vector<char> path(256);
        std::vector<char> protocol(16);

        ::sscanf(line.data(), "%[^ ] %[^ ] %[^ ]", method.data(), path.data(),
                 protocol.data());
        std::cout << "method = " << method.data() << std::endl;
        std::cout << "path   = " << path.data() << std::endl;
        std::cout << "protoc = " << protocol.data() << std::endl;

        // while (1) {
        // 	char buf[1024] = {0};
        // 	int len = get_line(cfd, buf, sizeof(buf));
        // 	if (buf[0] == '\n') {
        // 		break;
        // 	} else if (len == -1)
        // 		break;
        // }
        while (read_len > 0) {
            std::vector<char> idle(1024);
            read_len = ::read(cfd, idle.data(), 1024);
            if (idle.front() == '\n') {
                break;
            } else if (read_len == -1) {
                break;
            }
        }
        if (::strncasecmp(method.data(), "GET", 3) == 0) {
            // 确定是 get 方法
            std::string file;
            for (const auto& ch : path) {
                if (ch != '\0') {
                    file += ch;
                }
            }
            if (!file.empty()) {
                file.erase(file.begin());
            }
            std::cout << "file = " << file << file.length() << std::endl;
            http_request(cfd, file);
        }
    }

    return 0;
}

int do_read(int epfd, int cfd) {
    std::cout << "listen some reading" << std::endl;
    std::vector<char> buf(1024);
    int read_len = ::read(cfd, buf.data(), 1024);
    if (read_len == 0) {
        dissconnection(epfd, cfd);
        return 0;
    }
    std::cout << "read_len = " << read_len << " info = " << buf.data()
              << std::endl;
    return read_len;
}

void* epoll_run(int port) {
    // socket
    int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        sys_err("socket create error");
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 端口复用
    int opt = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    bind(lfd, (sockaddr*)&addr, sizeof(addr));
    listen(lfd, MAX_CONNECTION);

    // epoll
    int epfd = epoll_create(MAX_CONNECTION);
    epoll_event ev_temp;
    std::vector<epoll_event> evs(MAX_CONNECTION);

    ev_temp.data.fd = lfd;
    ev_temp.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev_temp);

    while (true) {
        evs.clear();
        int nready = ::epoll_wait(epfd, evs.data(), MAX_CONNECTION, -1);
        if (nready < 0) {
            sys_err("epoll_wait error");
        }
        for (int i = 0; i < nready; i++) {
            auto fd = evs[i].data.fd;
            auto event = evs[i].events;
            if (fd == lfd) {
                // do accept
                std::cout << "accept new connection" << std::endl;
                sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                accept4_connection(epfd, lfd, client_addr, client_addr_len);
            } else {
                if (event == EPOLLIN) {
                    do_read_http(fd, epfd);
                    // do_read(epfd, fd);
                } else if (event == EPOLLOUT) {
                    // do write
                } else {
                    std::cout << "disconnection" << std::endl;
                    dissconnection(epfd, fd);
                }
            }
        }
    }
}

void init_map() {}

int main(int argc, char* argv[]) {
    // 保证命令行参数正确
    if (argc < 3) {
        printf("server port path\n");
        exit(-1);
    }

    // initial umap
    init_umap();

    // 获取 port
    int port = atoi(argv[1]);

    // 改变程序工作目录
    int ret = ::chdir(argv[2]);
    if (ret != 0) {
        printf("chdir error\n");
        exit(-1);
    }

    // 启动服务器
    epoll_run(port);
}