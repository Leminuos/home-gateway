#include "BacklightPwm.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int BacklightPwm::writeSysfs(const std::string &filePath, const std::string &value)
{
    int fd = open(filePath.c_str(), O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open '%s': %s\n", filePath.c_str(), strerror(errno));
        return -1;
    }

    ssize_t n = write(fd, value.c_str(), value.size());
    close(fd);
    if (n < 0) {
        fprintf(stderr, "Failed to write '%s' to '%s': %s\n",
                value.c_str(), filePath.c_str(), strerror(errno));
        return -1;
    }
    return 0;
}

int BacklightPwm::init(const std::string &chipPath, int channel, unsigned int periodNs)
{
    mChannelPath = chipPath + "/pwm" + std::to_string(channel);

    /* Export channel nếu chưa được export */
    struct stat st;
    if (stat(mChannelPath.c_str(), &st) != 0) {
        if (writeSysfs(chipPath + "/export", std::to_string(channel)) < 0) {
            return -1;
        }
        /* Chờ udev tạo thư mục pwmN */
        usleep(100000);
    }

    /* Set duty=0 trước, sau đó set period để tránh vi phạm rule duty_cycle <= period */
    if (writeSysfs(mChannelPath + "/period", std::to_string(periodNs)) < 0) {
        return -1;
    }

    if (writeSysfs(mChannelPath + "/duty_cycle", "0") < 0) {
        return -1;
    }

    mPeriodNs = periodNs;

    if (writeSysfs(mChannelPath + "/enable", "1") < 0) {
        return -1;
    }
    return 0;
}

void BacklightPwm::deinit()
{
    if (mChannelPath.empty()) {
        return;
    }
    writeSysfs(mChannelPath + "/enable", "0");
    writeSysfs(mChannelPath + "/duty_cycle", "0");
}

int BacklightPwm::setBrightness(int percent)
{
    if (mPeriodNs == 0) {
        return -1;
    }
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    unsigned long long duty = (unsigned long long)mPeriodNs * (unsigned)percent / 100ULL;
    return writeSysfs(mChannelPath + "/duty_cycle", std::to_string(duty));
}
