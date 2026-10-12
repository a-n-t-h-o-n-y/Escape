#pragma once

#include "test.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace contract {
inline void checked(bool ok)
{
    if (!ok) throw std::system_error(errno ? errno : EIO,std::generic_category());
}
class Fd {
    int value_=-1;
public:
    explicit Fd(int value=-1) : value_{value} {}
    Fd(Fd const&)=delete;
    Fd& operator=(Fd const&)=delete;
    Fd(Fd&& other) noexcept : value_{std::exchange(other.value_,-1)} {}
    ~Fd() { if (value_>=0) ::close(value_); }
    auto get() const -> int { return value_; }
    auto release() -> int { return std::exchange(value_,-1); }
    void close() { if (value_>=0) ::close(std::exchange(value_,-1)); }
};
class File {
    std::FILE* value_;
public:
    explicit File(std::FILE* value=std::tmpfile()) : value_{value} { checked(value_!=nullptr); }
    File(File const&)=delete;
    File& operator=(File const&)=delete;
    File(File&& other) noexcept : value_{std::exchange(other.value_,nullptr)} {}
    ~File() { if (value_) std::fclose(value_); }
    auto get() const -> std::FILE* { return value_; }
    auto contents() -> std::string
    {
        checked(std::fflush(value_)==0);
        std::rewind(value_);
        std::string result;
        std::array<char,4096> bytes;
        while (auto count=std::fread(bytes.data(),1,bytes.size(),value_)) result.append(bytes.data(),count);
        checked(!std::ferror(value_));
        return result;
    }
};
inline void write_fd(int fd,std::string_view bytes)
{
    while (!bytes.empty()) {
        auto count=::write(fd,bytes.data(),bytes.size());
        if (count<0 && errno==EINTR) continue;
        checked(count>0);
        bytes.remove_prefix(static_cast<std::size_t>(count));
    }
}
struct Pipe {
    Fd writer;
    File reader;
    Pipe() : Pipe(make()) {}
    void write(std::string_view bytes) { write_fd(writer.get(),bytes); }
    void eof() { writer.close(); }
private:
    static auto make() -> std::array<int,2>
    {
        std::array<int,2> fds;
        checked(::pipe(fds.data())==0);
        return fds;
    }
    explicit Pipe(std::array<int,2> fds) : writer{fds[1]},reader{open_reader(fds[0])} {}
    static auto open_reader(int fd) -> std::FILE*
    {
        Fd owned{fd};
        auto* result=::fdopen(fd,"r");
        checked(result!=nullptr);
        owned.release();
        return result;
    }
};
inline auto duplicated_file(int fd,char const* mode) -> std::FILE*
{
    Fd copy{::dup(fd)};
    checked(copy.get()>=0);
    auto* result=::fdopen(copy.get(),mode);
    checked(result!=nullptr);
    copy.release();
    return result;
}
struct Pty {
    Fd master;
    Fd slave;
    File input;
    File output;
    Pty() : master{open_master()},slave{open_slave(master.get())},
            input{duplicated_file(slave.get(),"r")},output{duplicated_file(slave.get(),"w")}
    {
        checked(::fcntl(master.get(),F_SETFL,::fcntl(master.get(),F_GETFL)|O_NONBLOCK)==0);
        resize(80,24);
    }
    void write(std::string_view bytes) { write_fd(master.get(),bytes); }
    auto drain() -> std::string
    {
        std::string result;
        std::array<char,4096> bytes;
        while (true) {
            auto count=::read(master.get(),bytes.data(),bytes.size());
            if (count>0) { result.append(bytes.data(),static_cast<std::size_t>(count)); continue; }
            if (count<0 && errno==EINTR) continue;
            if (count==0 || (count<0 && (errno==EAGAIN || errno==EWOULDBLOCK))) return result;
            checked(false);
        }
    }
    void resize(unsigned short width,unsigned short height)
    {
        winsize size{};
        size.ws_col=width;
        size.ws_row=height;
        checked(::ioctl(slave.get(),TIOCSWINSZ,&size)==0);
    }
    auto settings() const -> termios
    {
        termios result{};
        checked(::tcgetattr(slave.get(),&result)==0);
        return result;
    }
    void settings(termios const& value) { checked(::tcsetattr(slave.get(),TCSANOW,&value)==0); }
private:
    static auto open_master() -> int
    {
        auto fd=::posix_openpt(O_RDWR|O_NOCTTY);
        checked(fd>=0);
        return fd;
    }
    static auto open_slave(int master) -> int
    {
        checked(::grantpt(master)==0 && ::unlockpt(master)==0);
        auto* path=::ptsname(master);
        checked(path!=nullptr);
        auto fd=::open(path,O_RDWR|O_NOCTTY);
        checked(fd>=0);
        return fd;
    }
};
inline auto same_settings(termios const& a,termios const& b) -> bool
{
    return a.c_iflag==b.c_iflag && a.c_oflag==b.c_oflag && a.c_cflag==b.c_cflag &&
           a.c_lflag==b.c_lflag && std::memcmp(a.c_cc,b.c_cc,sizeof(a.c_cc))==0 &&
           ::cfgetispeed(&a)==::cfgetispeed(&b) && ::cfgetospeed(&a)==::cfgetospeed(&b);
}
struct EnvironmentVariable {
    std::string name;
    bool present;
    std::string previous;
    EnvironmentVariable(char const* key,char const* value) : name{key},present{std::getenv(key)!=nullptr},
        previous{present ? std::getenv(key) : ""}
    {
        checked(value ? ::setenv(key,value,1)==0 : ::unsetenv(key)==0);
    }
    ~EnvironmentVariable()
    {
        if (present) ::setenv(name.c_str(),previous.c_str(),1);
        else ::unsetenv(name.c_str());
    }
};
struct StdoutCapture {
    File file;
    Fd previous;
    StdoutCapture() : previous{::dup(STDOUT_FILENO)}
    {
        checked(previous.get()>=0);
        checked(std::fflush(stdout)==0);
        checked(::dup2(::fileno(file.get()),STDOUT_FILENO)>=0);
    }
    ~StdoutCapture()
    {
        std::fflush(stdout);
        ::dup2(previous.get(),STDOUT_FILENO);
        std::clearerr(stdout);
    }
};
} // namespace contract
