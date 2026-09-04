#include <exception>
#include <iostream>

void runCoreTests();
void runWorldTests();
void runWorldGenerationTests();

int main()
{
    try
    {
        runCoreTests();
        runWorldTests();
        runWorldGenerationTests();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Paladin test failure: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout << "All Paladin tests passed.\n";

    return 0;
}
