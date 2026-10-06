#include "help_formatter.hpp"

void printHelp() {
    std::cout <<
R"(rpmbuild(1)                CrossShell for UNIX Reference Manual               rpmbuild(1)

NAME
    rpmbuild - build binary and source RPM packages

SYNOPSIS
    rpmbuild [OPTIONS] SPEC_FILE

DESCRIPTION
    Parses a Windows-oriented RPM spec file, executes build stages,
    and creates binary or source packages. The default build mode
    is binary packaging.

OPTIONS
    Build Modes:
        -ba
            Build both binary and source packages.
        -bb
            Build binary package only (from spec).
        -bs
            Build source RPM package only.
        -bp
            Execute the %prep stage only (unpack and patch).
        -bc
            Execute the %build stage only (compile sources).
        -bi
            Execute the %install stage only (stage to BUILDROOT).
        -bl
            Verify %files list against BUILDROOT.

    Configuration and Directives:
        --buildroot=DIR
            Override the default build root directory.
        --define="MACRO VALUE", -D "MACRO VALUE"
            Define build macro MACRO with value VALUE.
        --target=ARCH
            Target binary architecture (default: x86_64).
        --output=FILE
            Set explicit output file name.
        --clean
            Remove BUILDROOT directory tree after completion.
        -v, --verbose
            Enable detailed compilation telemetry.
        -?, --help
            Display this comprehensive reference manual and exit.
        --version
            Display version information and exit.

EXAMPLES
    rpmbuild -bb package.spec
        Build a binary package from specification file.

    rpmbuild -ba --clean package.spec
        Build binary and source packages and clean BUILDROOT.

    rpmbuild -bp package.spec
        Execute only the preparation stage.

    rpmbuild -bl package.spec
        Verify staged files against the %files list.

EXIT STATUS
    0   Successful build or completed stage check.
    1   Missing mode/spec, parse, stage, BUILDROOT, or packaging failure.

    CrossShell for UNIX                                              rpmbuild(1)
)";
}
