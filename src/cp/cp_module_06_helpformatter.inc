class HelpFormatter {
public:
    static void printHelp() {
        std::cout << R"(cp(1)                   CrossShell for UNIX Reference Manual                  cp(1)

    NAME
        cp - copy files and directory subtrees

    SYNOPSIS
        cp [OPTIONS] SOURCE... DEST

    DESCRIPTION
        The cp command copies the contents of SOURCE to DEST. If multiple sources
        are specified, DEST must be a directory. Supports recursive subtree copies,
        attribute and ACL preservation, and dereference options.

    OPTIONS
        -f, --force
            Unlink existing destination files without prompting (overrides -i).

        -i, --interactive
            Prompt before overwriting an existing destination file.

        -p, --preserve
            Preserve timestamps and Windows file attributes.

        -e
            Preserve file security descriptors and extended attributes.

        -r, -R, --recursive
            Copy directories recursively.

        -L, --dereference
            Always follow symbolic links.

        -P, --no-dereference
            Never follow symbolic links; copy the link itself.

        -v, --verbose
            Display detailed execution diagnostics.

        --no-progress
            Disable interactive terminal progress bar indicators.

        -h, --help
            Display this reference manual.

    EXAMPLES
        cp kernel.sys C:\Backup\
            Copy single file to backup directory.

        cp -r /opt/app D:\Deployment\
            Recursively copy directory tree.

        cp -ip file1.dat file2.dat
            Interactively copy with preserved attributes.

    CrossShell for UNIX                                                    cp(1)
)";
    }
};

// ============================================================================
// Class: ArgumentParser
// Parses HP-UX compound arguments (e.g., -rip, -fe) and long options.
// ============================================================================
