#include <iodrivers_base/Forward.hpp>
#include <iodrivers_base/Driver.hpp>
#include <thread>
#include <vector>

using namespace std;
using namespace iodrivers_base;

struct QuitGuard
{
    bool& quitFlag;
    Driver& driver1;
    Driver& driver2;

    ~QuitGuard() {
        quitFlag = true;
    }
};

typedef int (Driver::*ReadMode)(uint8_t*, int, base::Time const&);

static int forwardData(Driver& from, ReadMode mode, Driver& to,
                        uint8_t* buffer, int buffer_size, base::Time timeout)
{
    int size = 0;
    try {
        size = (from.*mode)(buffer, buffer_size, timeout);
    }
    catch(TimeoutError&) {
    }
    if (size != 0) {
        to.writePacket(buffer, size);
    }
    return size;
}

void iodrivers_base::forward(bool raw_mode,
                Driver& driver1, Driver& driver2,
                base::Time timeout1,
                base::Time timeout2,
                size_t const buffer_size)
{
    vector<uint8_t> buffer(buffer_size);
    int fd1 = driver1.getFileDescriptor();
    int fd2 = driver2.getFileDescriptor();
    int select_nfd = std::max(fd1, fd2) + 1;


    ReadMode readMode = raw_mode ? static_cast<ReadMode>(&Driver::readRaw) :
                                   static_cast<ReadMode>(&Driver::readPacket);

    while (!driver1.eof() && !driver2.eof()) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd1, &set);
        FD_SET(fd2, &set);

        timeval timeout_spec = {
            static_cast<time_t>(10),
            suseconds_t(0)
        };

        int ret = select(select_nfd, &set, NULL, NULL, &timeout_spec);
        if (ret < 0 && errno != EINTR)
            throw UnixError("forward(): error in select()");
        else if (ret == 0)
            continue;

        if (FD_ISSET(fd1, &set)) {
            forwardData(driver1, readMode, driver2, &buffer[0], buffer_size, timeout1);
        }

        if (FD_ISSET(fd2, &set)) {
            forwardData(driver2, readMode, driver1, &buffer[0], buffer_size, timeout1);
        }
    }
}
