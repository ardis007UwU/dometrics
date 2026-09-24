// main.cpp — single-binary entry point for the Dometrics CLI + daemon.
#include "cli/cli_router.hpp"

#include <iostream>

int main(int argc, char** argv) {
  try {
    return dometrics::cli::run(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "dometrics: fatal error: " << e.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "dometrics: unknown fatal error\n";
    return 1;
  }
}
