#include <Windows.h>
#include <stdexcept>
#include <conio.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <deque>
#include <map>
#include <random>
#include "interception.h"

/* Strafe switch can happen again [time in ms] */
#define GRACE_PERIOD 70 

/* Controls how inaccurate can key switches happen
 * Higher number = more inaccurate */
#define KEYPRESS_RANDOM_DEVIATION 2

class context {
public:
    context();
    ~context();

    inline operator InterceptionContext() const noexcept { return context_; }

private:
    InterceptionContext context_;
};

context::context() {
    context_ = interception_create_context();
    if (!context_) {
        throw std::runtime_error("failed to create interception context");
    }
}

context::~context() {
    interception_destroy_context(context_);
}

enum ScanCode
{
    SCANCODE_ESC = 0x01,
    SCANCODE_A = 0x1E,
    SCANCODE_D = 0x20,
    SCANCODE_W = 0x11,
    SCANCODE_SPACE = 0x39,
    SCANCODE_LEFT = 0x4B,
    SCANCODE_RIGHT = 0x4D
};

struct entry {
    InterceptionDevice device;
    InterceptionKeyStroke stroke;

    entry(InterceptionDevice device, InterceptionKeyStroke const& ims)
        : device(device)
        , stroke(ims)
    {}
};

context ctx;


int speed_limit(int speed)
{
    // Precomputed soft MSL: 
    // for x in range(64):
    // ... print(f"{round(12*math.atan(x/12))},", end = " ")
    int pre_f[] = { 0, 1, 2, 3, 4, 5, 6, 6, 7, 8, 8, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17
    };
    if (speed >= 0)
    {
        if (speed < 64)
        {
            speed = pre_f[speed];
        }
    }
    else
    {
        if (speed > -64)
        {
            speed = pre_f[speed * -1] * -1;
        }
    }
    return speed;
}

int main() {

    std::cout << " ________  ___  __    _______       ___    ___ \n";
    std::cout << "|\\   __  \\|\\  \\|\\  \\ |\\  ___ \\     |\\  \\  /  /|\n";
    std::cout << "\\ \\  \\|\\  \\ \\  \\/  /|\\ \\   __/|    \\ \\  \\/  / /\n";
    std::cout << " \\ \\  \\\\\\  \\ \\   ___  \\ \\  \\_|/__   \\ \\    / / \n";
    std::cout << "  \\ \\  \\\\\\  \\ \\  \\\\ \\  \\ \\  \\_|\\ \\   \\/  /  /  \n";
    std::cout << "   \\ \\_______\\ \\__\\\\ \\__\\ \\_______\\__/  / /    \n";
    std::cout << "    \\|_______|\\|__| \\|__|\\|_______|\\___/ /     \n";
    std::cout << "                                  \\|___|/      \n";
    std::cout << "                                               \n";

    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

    bool isProfiling = true;
    bool isPaused = false;
    bool probablyAir = false;
    unsigned int strafeDir = 0; // 0: no strafing, 1: left, 2: right

    int strafeSwitchSpeed = 3; // this is used to preemptively switch strafes

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(-KEYPRESS_RANDOM_DEVIATION, KEYPRESS_RANDOM_DEVIATION); // define the range

    InterceptionDevice device; // device 1 is keyboard, device 11 is mouse
    InterceptionStroke stroke;

    InterceptionKeyStroke fake_d;
    fake_d.code = SCANCODE_D;
    fake_d.state = 0;

    InterceptionKeyStroke fake_d_end;
    fake_d_end.code = SCANCODE_D;
    fake_d_end.state = 1;

    InterceptionKeyStroke fake_a;
    fake_a.code = SCANCODE_A;
    fake_a.state = 0;

    InterceptionKeyStroke fake_a_end;
    fake_a_end.code = SCANCODE_A;
    fake_a_end.state = 1;

    InterceptionKeyStroke fake_w_end;
    fake_w_end.code = SCANCODE_W;
    fake_w_end.state = 1;

    interception_set_filter(ctx, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_MOVE | INTERCEPTION_FILTER_MOUSE_WHEEL);
    interception_set_filter(ctx, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);

    auto t_last = std::chrono::high_resolution_clock::now();

    while (interception_receive(ctx, device = interception_wait(ctx), &stroke, 1) > 0)
    {

        if (interception_is_mouse(device))
        {
            InterceptionMouseStroke& mstroke = *(InterceptionMouseStroke*)&stroke;

            mstroke.x = speed_limit(mstroke.x);
            mstroke.y = speed_limit(mstroke.y);

            interception_send(ctx, device, &stroke, 1);

            if (mstroke.rolling == -120)
            { // MWHEELDOWN
                probablyAir = true;
                interception_send(ctx, 1, (InterceptionStroke*)&fake_w_end, 1);
            }

            if (probablyAir)
            {
                auto t_now = std::chrono::high_resolution_clock::now();

                if (std::chrono::duration<double, std::milli>(t_now - t_last).count() > GRACE_PERIOD) {
                    int rand_deviation = distr(gen);
                    int switchspd = (strafeSwitchSpeed + rand_deviation);
                    switchspd = switchspd < 0 ? 0 : switchspd;
                    std::cout << switchspd << " ";
                    if (mstroke.x >= (-1 * switchspd) && strafeDir != 2)
                    {
                        strafeDir = 2;
                        interception_send(ctx, 1, (InterceptionStroke*)&fake_a_end, 1);
                        interception_send(ctx, 1, (InterceptionStroke*)&fake_d, 1);
                        t_last = t_now;
                    }
                    else if (mstroke.x <= switchspd && strafeDir != 1)
                    {
                        strafeDir = 1;
                        interception_send(ctx, 1, (InterceptionStroke*)&fake_d_end, 1);
                        interception_send(ctx, 1, (InterceptionStroke*)&fake_a, 1);
                        t_last = t_now;
                    }
                }
            }

        }
        else if (interception_is_keyboard(device))
        {
            InterceptionKeyStroke& kstroke = *(InterceptionKeyStroke*)&stroke;

            if (probablyAir) // Ignore all actual A and D inputs to not mess with the emulator
            {
                if (kstroke.code != SCANCODE_A && kstroke.code != SCANCODE_D)
                {
                    interception_send(ctx, device, &stroke, 1);
                }
            }
            else {
                interception_send(ctx, device, &stroke, 1);
            }

            if (kstroke.code == SCANCODE_W && kstroke.state == 0)
            {
                probablyAir = false;
                if (strafeDir == 2)
                {
                    interception_send(ctx, 1, (InterceptionStroke*)&fake_d_end, 1);
                }
                else if (strafeDir == 1)
                {
                    interception_send(ctx, 1, (InterceptionStroke*)&fake_a_end, 1);
                }
                strafeDir = 0;
            }

            if (kstroke.code == SCANCODE_LEFT && kstroke.state == 0)
            {
                strafeSwitchSpeed = strafeSwitchSpeed > 0 ? strafeSwitchSpeed - 1 : strafeSwitchSpeed;
                std::cout << "Speed threshold for strafe switch [-]: " << strafeSwitchSpeed << std::endl;
            }

            if (kstroke.code == SCANCODE_RIGHT && kstroke.state == 0)
            {
                strafeSwitchSpeed = strafeSwitchSpeed < 10 ? strafeSwitchSpeed + 1 : strafeSwitchSpeed;
                std::cout << "Speed threshold for strafe switch [+]: " << strafeSwitchSpeed << std::endl;
            }
        }
    }

    interception_destroy_context(ctx);

    return 0;
}