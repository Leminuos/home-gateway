#ifndef __BACKLIGHT_PWM_H__
#define __BACKLIGHT_PWM_H__

#include <string>

class BacklightPwm {
    public:
        BacklightPwm() = default;

        ~BacklightPwm() = default;

        int init(const std::string &chipPath, int channel, unsigned int periodNs);

        void deinit();

        int setBrightness(int percent);

    private:
        std::string mChannelPath;
        unsigned int mPeriodNs = 0;

        int writeSysfs(const std::string &filePath, const std::string &value);
};

#endif // __BACKLIGHT_PWM_H__
