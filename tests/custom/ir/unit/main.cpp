#include "test.hpp"
#include <set>

int main(int argc, char** argv) {
    using namespace test;
    try {
        std::vector<Case> cases;
        builderCases(cases); printerCases(cases); verifierCases(cases); graphCases(cases);
        std::sort(cases.begin(), cases.end(), [](const Case& a, const Case& b) { return a.id < b.id; });
        std::set<std::string> ids;
        for (const auto& c : cases) {
            if (c.id.empty() || c.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") != std::string::npos ||
                !ids.insert(c.id).second) throw std::runtime_error("invalid/duplicate case ID: " + c.id);
            if (c.oracle != "none" && c.oracle != "accept" && c.oracle != "reject-parse" && c.oracle != "reject-verify")
                throw std::runtime_error("invalid oracle: " + c.id);
        }
        if (argc == 2 && std::string(argv[1]) == "--list") {
            for (const auto& c : cases) std::cout << c.id << '\t' << c.oracle << '\t' << c.diagnostic << '\n';
            return 0;
        }
        if (argc != 5 || std::string(argv[1]) != "--case" || std::string(argv[3]) != "--output-dir") {
            std::cerr << "Usage: ir-unit --list | --case <id> --output-dir <directory>\n";
            return 2;
        }
        const auto it = std::find_if(cases.begin(), cases.end(), [&](const Case& c) { return c.id == argv[2]; });
        if (it == cases.end()) { std::cerr << "Unknown case: " << argv[2] << '\n'; return 2; }
        std::filesystem::create_directories(argv[4]);
        Context context{argv[4]};
        try { it->run(context); }
        catch (const InfrastructureError&) { throw; }
        catch (const std::exception& e) { context.check(false, std::string("unexpected exception: ") + e.what()); }
        if (!context.assertions) context.check(false, "case executed no assertions");
        std::cout << (context.failures ? "FAIL " : "PASS ") << it->id << " assertions=" << context.assertions << " assertions_failed=" << context.failures << '\n';
        return context.failures ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << '\n'; return 2;
    }
}
