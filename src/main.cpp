#include "core/Application.h"
#include "debug/CrashReporter.h"

#include <cstdlib>
#include <exception>

int main()
{
    Paladin::CrashReporter::install();
    try
    {
        Paladin::Application app;
        return app.run();
    }
    catch (const std::exception& exception)
    {
        Paladin::CrashReporter::reportFatal(exception.what());
    }
    catch (...)
    {
        Paladin::CrashReporter::reportFatal(
            "Unhandled non-standard exception in application"
        );
    }
    return EXIT_FAILURE;
}
