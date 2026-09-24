#pragma once
// cli_router.hpp — command dispatch for the `dometrics` CLI.
#include <string>
#include <vector>

namespace dometrics::cli {

int run(int argc, char** argv);

void print_usage();
void print_help();

} // namespace dometrics::cli
