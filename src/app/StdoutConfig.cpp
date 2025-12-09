#include <iostream>
#include <cstdlib>
#include <string_view>

namespace
{
    struct VixStdoutConfigurator
    {
        VixStdoutConfigurator()
        {
            const char *env = std::getenv("VIX_STDOUT_MODE");
            if (!env)
                return;

            std::string_view mode{env};

            if (mode == "line")
            {
                std::cout << std::unitbuf;
            }
        }
    };

    VixStdoutConfigurator g_vixStdoutConfigurator;
} // namespace
