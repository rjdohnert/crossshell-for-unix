#include "engine.hpp"
#include "selftest.hpp"

static int runInternalSelfTests() {
    int passed = 0;
    int failed = 0;

    auto assert_eq = [&](const std::string& actual, const std::string& expected, const std::string& test_name) {
        if (actual == expected) {
            std::cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            std::cerr << "FAIL: " << test_name << " (Expected: '" << expected << "', Actual: '" << actual << "')\n";
            failed++;
        }
    };

    auto assert_true = [&](bool condition, const std::string& test_name) {
        if (condition) {
            std::cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            std::cerr << "FAIL: " << test_name << " (Expected: true, Actual: false)\n";
            failed++;
        }
    };

    std::cout << "--- Running CrossShellTCSH Internal Self-Tests ---\n";

    // 1. Quoting and String Utilities
    {
        TcshEngine shell(false);
        assert_eq(shell.stripOuterQuotes("\"hello\""), "hello", "stripOuterQuotes double quotes");
        assert_eq(shell.stripOuterQuotes("'world'"), "world", "stripOuterQuotes single quotes");
        assert_eq(shell.stripOuterQuotes("unquoted"), "unquoted", "stripOuterQuotes unquoted");
        assert_eq(shell.normalizePathSeparators("foo/bar/baz"), "foo\\bar\\baz", "normalizePathSeparators");
    }

    // 2. Wildcard Matching
    {
        assert_true(wildcard_match("*.txt", "readme.txt"), "wildcard_match simple star");
        assert_true(wildcard_match("file?.log", "file1.log"), "wildcard_match question mark");
        assert_true(!wildcard_match("*.cpp", "tcsh.h"), "wildcard_match mismatch");
        assert_true(wildcard_match("src/*/*.cpp", "src/dir/file.cpp"), "wildcard_match nested path");
    }

    // 3. Variable Assignment and Scope
    {
        TcshEngine shell(false);
        shell.executeCommandLine("set myvar = testvalue");
        assert_eq(shell.getVar("myvar"), "testvalue", "set scalar variable");

        shell.executeCommandLine("set mylist = ( alpha beta gamma )");
        assert_eq(shell.getVar("mylist"), "alpha beta gamma", "set list variable");

        shell.executeCommandLine("setenv TCSH_TEST_VAR custom_val");
        char buf[256] = {};
        GetEnvironmentVariableA("TCSH_TEST_VAR", buf, sizeof(buf));
        assert_eq(std::string(buf), "custom_val", "setenv sets environment variable");

        shell.executeCommandLine("unsetenv TCSH_TEST_VAR");
        DWORD len = GetEnvironmentVariableA("TCSH_TEST_VAR", buf, sizeof(buf));
        assert_true(len == 0, "unsetenv removes environment variable");

        shell.executeCommandLine("unset myvar");
        assert_eq(shell.getVar("myvar"), "", "unset removes shell variable");
    }

    // 4. Arithmetic (@) Evaluation
    {
        TcshEngine shell(false);
        shell.executeCommandLine("@ x = 10 + 5");
        assert_eq(shell.getVar("x"), "15", "@ addition");

        shell.executeCommandLine("@ y = 3 * 4 + 2");
        assert_eq(shell.getVar("y"), "14", "@ multiplication precedence");

        shell.executeCommandLine("@ z = (20 - 5) / 3");
        assert_eq(shell.getVar("z"), "5", "@ parenthesized division");

        shell.executeCommandLine("@ m = 17 % 5");
        assert_eq(shell.getVar("m"), "2", "@ modulo");

        shell.executeCommandLine("@ sub = 10 - 25");
        assert_eq(shell.getVar("sub"), "-15", "@ subtraction negative result");
    }

    // 5. Conditional Expressions (test)
    {
        TcshEngine shell(false);
        shell.executeCommandLine("test \"hello\" == \"hello\"");
        assert_eq(shell.getVar("status"), "0", "test equality returns 0");

        shell.executeCommandLine("test \"apple\" != \"orange\"");
        assert_eq(shell.getVar("status"), "0", "test inequality returns 0");

        shell.executeCommandLine("test \"alpha\" == \"beta\"");
        assert_eq(shell.getVar("status"), "1", "test false equality returns 1");
    }

    // 6. Aliases
    {
        TcshEngine shell(false);
        shell.executeCommandLine("alias myecho echo");
        shell.executeCommandLine("unalias myecho");
        assert_eq(shell.getVar("status"), "0", "alias and unalias command");
    }

    // 7. Directory Stack (pushd, popd, dirs)
    {
        TcshEngine shell(false);
        shell.executeCommandLine("dirs");
        assert_eq(shell.getVar("status"), "0", "dirs executes successfully");
    }

    // 8. Status Codes & Logic Sequencing
    {
        TcshEngine shell(false);
        shell.executeCommandLine("true");
        assert_eq(shell.getVar("status"), "0", "true returns 0");

        shell.executeCommandLine("false");
        assert_eq(shell.getVar("status"), "1", "false returns 1");

        shell.executeCommandLine("set seq = 0");
        shell.executeCommandLine("true && set seq = 1");
        assert_eq(shell.getVar("seq"), "1", "&& sequence on success");

        shell.executeCommandLine("false && set seq = 2");
        assert_eq(shell.getVar("seq"), "1", "&& sequence skipped on failure");

        shell.executeCommandLine("false || set seq = 3");
        assert_eq(shell.getVar("seq"), "3", "|| sequence on failure");
    }

    // 9. One-shot -c command execution
    {
        TcshEngine shell(false);
        int code = shell.executeCommandString("set result = success", "tcsh");
        assert_eq(std::to_string(code), "0", "executeCommandString returns 0");
        assert_eq(shell.getVar("result"), "success", "executeCommandString sets variable");
    }

    // 10. echotc Builtin
    {
        TcshEngine shell(false);
        shell.executeCommandLine("echotc normal");
        assert_eq(shell.getVar("status"), "0", "echotc normal succeeds");

        shell.executeCommandLine("echotc bold");
        assert_eq(shell.getVar("status"), "0", "echotc bold succeeds");

        shell.executeCommandLine("echotc non_existent_cap");
        assert_eq(shell.getVar("status"), "1", "echotc invalid cap returns 1");
    }

    // 11. ls-F Builtin
    {
        TcshEngine shell(false);
        shell.executeCommandLine("ls-F src");
        assert_eq(shell.getVar("status"), "0", "ls-F src succeeds");
    }

    // 12. onintr Builtin
    {
        TcshEngine shell(false);
        shell.executeCommandLine("onintr -");
        assert_eq(shell.getVar("status"), "0", "onintr - succeeds");

        shell.executeCommandLine("onintr cleanup_handler");
        assert_eq(shell.getVar("status"), "0", "onintr label succeeds");

        shell.executeCommandLine("onintr");
        assert_eq(shell.getVar("status"), "0", "onintr restore succeeds");
    }

    std::cout << "\n--- Self-Test Summary: " << passed << " passed, " << failed << " failed ---\n";
    return (failed == 0) ? 0 : 1;
}

