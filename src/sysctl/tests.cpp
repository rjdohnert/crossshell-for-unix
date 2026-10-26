
#include "sysctl_app.hpp"
#include "telemetry_provider.hpp"
#include "storage_provider.hpp"
#include "reporter.hpp"
#include "options.hpp"
#include "mib_tree.hpp"
#include "sysctl.hpp"

using namespace sysctl;


#include <iostream>
#include <initializer_list>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class CapturedOutput {
public:
    std::ostringstream output;
    std::ostringstream error;

private:
    std::streambuf* m_stdout;
    std::streambuf* m_stderr;

public:
    CapturedOutput()
        : m_stdout(std::cout.rdbuf(output.rdbuf())),
          m_stderr(std::cerr.rdbuf(error.rdbuf())) {}

    ~CapturedOutput() {
        std::cout.rdbuf(m_stdout);
        std::cerr.rdbuf(m_stderr);
    }
};

struct Result {
    int status;
    std::string output;
    std::string error;
};

Result Run(SysctlApp& app, std::initializer_list<std::string> arguments) {
    std::vector<std::string> storage{"sysctl"};
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& argument : storage) argv.push_back(argument.data());
    CapturedOutput captured;
    const int status = app.Run(static_cast<int>(argv.size()), argv.data());
    return {status, captured.output.str(), captured.error.str()};
}

Result Run(std::initializer_list<std::string> arguments) {
    SysctlApp app;
    return Run(app, arguments);
}

std::vector<std::string> OutputNames(const std::string& output) {
    std::istringstream lines(output);
    std::vector<std::string> names;
    std::string line;
    while (std::getline(lines, line)) {
        const auto equals = line.find('=');
        Require(equals != std::string::npos, "List output is not name=value");
        names.push_back(line.substr(0, equals));
    }
    return names;
}

class TemporaryFile {
public:
    char path[MAX_PATH]{};

    TemporaryFile() {
        char directory[MAX_PATH]{};
        const DWORD length = GetTempPathA(MAX_PATH, directory);
        Require(length > 0 && length < MAX_PATH, "Cannot locate temporary directory");
        Require(GetTempFileNameA(directory, "sct", 0, path) != 0, "Cannot create test file");
    }

    ~TemporaryFile() { DeleteFileA(path); }

    void Write(const std::string& text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
        file.close();
        Require(!file.fail(), "Cannot write test file");
    }
};

void TestTree() {
    MibTree tree;
    std::string value = "before";
    tree.RegisterNode("test.writable", "Writable test value", [&]() { return value; },
                      [&](const std::string& next) { value = next; return true; });
    auto writable = std::static_pointer_cast<SysctlLeafNode>(tree.FindNode("test.writable"));
    Require(writable && writable->IsWritable(), "Moved setter lost its writable status");
    Require(writable->SetValue("after") && writable->GetValue() == "after", "Setter did not run");

    tree.RegisterNode("test.readonly", "", []() { return "fixed"; });
    auto readonly = std::static_pointer_cast<SysctlLeafNode>(tree.FindNode("test.readonly"));
    Require(!readonly->IsWritable() && !readonly->SetValue("changed"), "Read-only leaf accepted a write");
    Require(tree.FindNode("") == tree.GetRoot(), "Root lookup failed");
    Require(tree.FindNode("test")->IsDirectory(), "Directory lookup failed");
    for (const auto& path : {"test.", ".test", "test..writable", "test.writable.", "test.writable.child"}) {
        Require(!tree.FindNode(path), "Malformed lookup matched a node");
    }
    for (const auto& path : {"", ".", ".test", "test.", "test..bad",
                             "test.writable.child", "test.writable", "test"}) {
        bool rejected = false;
        try {
            tree.RegisterNode(path, "", []() { return "bad"; });
        } catch (const std::invalid_argument&) {
            rejected = true;
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected, "Invalid or colliding registration was accepted");
    }
    Require(writable->GetValue() == "after" && tree.FindNode("test.writable") == writable,
            "Rejected registration replaced an existing node");

    MibTree windows;
    WindowsTelemetryFactory::Populate(windows);
    auto hostname = std::static_pointer_cast<SysctlLeafNode>(windows.FindNode("kern.hostname"));
    Require(hostname->IsWritable(), "Hostname setter is not writable");
}

void TestCli() {
    for (const auto& help : {"-h", "--help", "/?"}) {
        const auto result = Run({help});
        Require(result.status == 0 && result.error.empty() &&
                result.output.find("usage: sysctl") == 0, "Help did not succeed");
    }
    const auto read = Run({"kern.ostype"});
    Require(read.status == 0 && read.output == "kern.ostype=Windows_NT\n" && read.error.empty(),
            "Default read format changed");
    const auto value = Run({"-enq", "kern.ostype"});
    Require(value.status == 0 && value.output == "Windows_NT\n", "Grouped value-only options failed");
    const auto description = Run({"-d", "kern.ostype"});
    Require(description.status == 0 && description.output == "kern.ostype: Operating system type\n",
            "Description output failed");
    const auto directory = Run({"hw"});
    Require(directory.status == 0 && directory.output.find("hw.ncpu=") != std::string::npos,
            "Directory traversal failed");
    const auto all = Run({"-a"});
    const auto tables = Run({"-A"});
    const auto defaults = Run({});
    Require(all.status == 0 && tables.status == 0 && defaults.status == 0 &&
            all.error.empty() && defaults.error.empty() &&
            OutputNames(all.output) == OutputNames(tables.output) &&
            OutputNames(all.output) == OutputNames(defaults.output) &&
            all.output.find("hw.diskcount=") != std::string::npos &&
            all.output.find("fs.volume.count=") != std::string::npos &&
            tables.error.find("use typeperf") != std::string::npos &&
            all.output.find("vm.loadavg=") == std::string::npos,
            "Default listing, scalar listing, or table guidance failed");
    const auto obsoleteWrite = Run({"-w", "kern.ostype"});
    Require(obsoleteWrite.status == 0 && obsoleteWrite.output == read.output,
            "OpenBSD's obsolete -w flag must be a no-op");
    const auto mixed = Run({"missing.node", "kern.ostype"});
    Require(mixed.status != 0 && mixed.output == read.output && !mixed.error.empty(),
            "Query failure was lost or prevented subsequent reads");
    const auto ignored = Run({"-i", "missing.node", "missing.node=value", "kern.ostype"});
    Require(ignored.status == 0 && ignored.error.empty() && ignored.output == read.output,
            "Ignore-unknown option failed");
    const auto terminator = Run({"--", "-n"});
    Require(terminator.status != 0 && terminator.error.find("top level name -n") != std::string::npos,
            "Option terminator was not respected");
    const auto afterOperand = Run({"kern.ostype", "-n"});
    Require(afterOperand.status != 0 && afterOperand.output == read.output &&
            afterOperand.error.find("top level name -n") != std::string::npos,
            "Options after the first operand were not treated as names");
    const auto missingChild = Run({"kern.missing"});
    Require(missingChild.status != 0 &&
            missingChild.error == "sysctl: name missing in kern.missing is unknown\n",
            "Unknown MIB component diagnostic changed");
    const auto unavailable = Run({"-i", "vm.loadavg", "hw.usermem", "kern.ostype"});
    Require(unavailable.status != 0 && unavailable.output == read.output &&
            unavailable.error.find("not available on Windows") != std::string::npos,
            "Unavailable compatibility names returned misleading values or were ignored by -i");

    for (const auto& arguments : {
            std::initializer_list<std::string>{"-f"}, {"-f", ""}, {"--bogus"}, {"-x"},
            {""}, {"=bad"}, {"kern."}, {"kern.ostype=bad"}, {"kern=value"},
            {"-i", "kern.ostype=bad"}}) {
        Require(Run(arguments).status != 0, "Invalid invocation returned success");
    }

    const auto nameOnly = Run({"-N", "kern.ostype"});
    Require(nameOnly.status == 0 && nameOnly.output == "kern.ostype\n", "-N did not print name only");
    const auto wildcard = Run({"kern.*"});
    Require(wildcard.status == 0 && wildcard.output.find("kern.ostype=Windows_NT\n") != std::string::npos,
            "Wildcard query kern.* failed");

    const auto release = Run({"-n", "kern.osrelease"});
    const auto version = Run({"-n", "kern.version"});
    OSVERSIONINFOW expected{};
    expected.dwOSVersionInfoSize = sizeof(expected);
    using GetVersion = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto getVersion = reinterpret_cast<GetVersion>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    Require(getVersion && getVersion(&expected) == 0, "Cannot obtain expected OS release");
    const auto expectedRelease = std::to_string(expected.dwMajorVersion) + "." +
        std::to_string(expected.dwMinorVersion) + "." + std::to_string(expected.dwBuildNumber);
    Require(release.status == 0 && release.output == expectedRelease + "\n",
            "OS release does not match Windows");
    Require(version.status == 0 && version.output ==
            "Microsoft Windows [Version " + expectedRelease + "] NT Kernel Engine\n",
            "Kernel version is not based on the live OS release");
}

void TestBatch() {
    TemporaryFile file;
    file.Write(" \r\n  # comment\r\n kern.ostype # inline comment\r\n");
    for (const auto& arguments : {
            std::initializer_list<std::string>{"-f", file.path},
            {"-f" + std::string(file.path)}, {"-nf" + std::string(file.path)}}) {
        const auto result = Run(arguments);
        Require(result.status == 0 && result.error.empty() &&
                result.output.find("Windows_NT\n") != std::string::npos,
                "Batch whitespace, comments, or attached file argument failed");
    }
    file.Write(" kern.ostype = bad\r\nmissing.node\r\nkern.ostype\r\n");
    const auto failure = Run({"-f", file.path});
    Require(failure.status != 0 && failure.output == "kern.ostype=Windows_NT\n" &&
            failure.error.find("read-only") != std::string::npos &&
            failure.error.find("top level name missing") != std::string::npos,
            "Batch did not trim assignments, report errors, or continue");
    Require(Run({"-a", "-f", file.path}).status == 0,
            "Explicit listing must take precedence over -f as on OpenBSD");
    const std::string missing = std::string(file.path) + ".missing";
    Require(Run({"-f", missing}).status != 0, "Missing batch file returned success");
    Require(Run({"-a", "-f", missing}).status == 0, "Listing tried to open the ignored input file");
    file.Write("kern.ostype\n");
    const auto ordered = Run({"-f", file.path, "hw.byteorder"});
    Require(ordered.status == 0 && ordered.output == "kern.ostype=Windows_NT\nhw.byteorder=1234\n",
            "Batch entries were not processed before command-line queries");
    file.Write(" # empty configuration\n");
    const auto empty = Run({"-f", file.path});
    Require(empty.status == 0 && empty.output.empty() && empty.error.empty(),
            "An empty batch file unexpectedly listed all values");
}

void TestWritesAndNotices() {
    MibTree tree;
    std::string value = "before";
    tree.RegisterNode("test.value", "", [&]() { return value; }, [&](const std::string& next) {
        if (next == "fail") throw std::runtime_error("setter failed");
        value = next;
        return true;
    });
    tree.RegisterNode("test.table", "", []() { return "use a table reader"; }, nullptr,
                      SysctlLeafNode::Kind::Table);
    tree.RegisterNode("test.unavailable", "", []() { return "not supported by this OS"; }, nullptr,
                      SysctlLeafNode::Kind::Unavailable);
    SysctlApp app(std::move(tree));
    const auto write = Run(app, {"test.value=after"});
    Require(write.status == 0 && write.output == "test.value: before -> after\n",
            "Default write output changed");
    const auto numbered = Run(app, {"-n", "test.value=next"});
    Require(numbered.status == 0 && numbered.output == "next\n", "Value-only write output changed");
    const auto quiet = Run(app, {"-nq", "test.value=quiet"});
    Require(quiet.status == 0 && quiet.output.empty() && quiet.error.empty() && value == "quiet",
            "Quiet did not override -n");
    const auto failure = Run(app, {"-q", "test.value=fail", "test.value"});
    Require(failure.status != 0 && failure.output == "test.value=quiet\n" &&
            failure.error.find("test.value: setter failed") != std::string::npos,
            "Setter exceptions were hidden or stopped subsequent queries");
    const auto all = Run(app, {});
    Require(all.status == 0 && all.output == "test.value=quiet\n" && all.error.empty(),
            "Scalar listing included tables or unsupported values");
    const auto tables = Run(app, {"-A"});
    Require(tables.status == 0 && tables.output == all.output &&
            tables.error.find("use a table reader") != std::string::npos &&
            tables.error.find("not supported by this OS") != std::string::npos,
            "Extended listing did not distinguish tables and unsupported values");
    Require(Run(app, {"test.table"}).status == 0 && Run(app, {"test.unavailable"}).status != 0,
            "Table guidance and unavailable queries have incorrect exit status");
}

void TestMemoryAndBootTime() {
    const auto epoch = Run({"-n", "kern.boottime"});
    const auto date = Run({"kern.boottime"});
    Require(epoch.status == 0 && date.status == 0, "Boot-time query failed");
    const auto boot = std::stoll(epoch.output);
    const auto now = std::time(nullptr);
    Require(boot > 0 && boot <= now &&
            date.output.find("kern.boottime=") == 0 && date.output.size() == 39 &&
            date.output[17] == ' ', "Boot time is not a ctime date with a numeric -n value");
    Require(std::llabs(static_cast<long long>(now) - boot -
            static_cast<long long>(GetTickCount64() / 1000)) <= 2, "Boot epoch is not based on uptime");
    const auto load = Run({"-n", "vm.memory_load"});
    const auto percentage = std::stoul(load.output);
    Require(load.status == 0 && percentage <= 100 && load.output.find('%') == std::string::npos,
            "Memory load must be an integer percentage, not a Unix load average");
    const auto total = Run({"-n", "vm.swap_total"});
    const auto free = Run({"-n", "vm.swap_free"});
    Require(total.status == 0 && free.status == 0 && std::stoull(free.output) <= std::stoull(total.output),
            "Invalid actual pagefile counters");
    const auto commit = Run({"-n", "vm.commit_limit"});
    Require(commit.status == 0 && std::stoull(commit.output) >= std::stoull(total.output),
            "Commit limit and actual pagefile size were confused");
    const auto cpu = Run({"-n", "hw.ncpu"});
    const auto cpuspeed = Run({"-n", "hw.cpuspeed"});
    Require(cpuspeed.status == 0 && std::stoul(cpuspeed.output) > 0, "hw.cpuspeed is not a positive integer");
    const auto physmem = Run({"-n", "hw.physmem"});
    const auto physmem64 = Run({"-n", "hw.physmem64"});
    Require(physmem64.status == 0 && physmem64.output == physmem.output, "hw.physmem64 does not match hw.physmem");
    const auto domain = Run({"kern.domainname"});
    Require(domain.status == 0, "kern.domainname query failed");
    const auto ip6fwd = Run({"-n", "net.inet6.ip6.forwarding"});
    Require(ip6fwd.status == 0 && (ip6fwd.output == "0\n" || ip6fwd.output == "1\n"),
            "net.inet6.ip6.forwarding is not 0 or 1");

    Require(cpu.status == 0 && std::stoul(cpu.output) == GetActiveProcessorCount(ALL_PROCESSOR_GROUPS),
            "CPU count does not span processor groups");
}

void TestStorage() {
    const std::vector<BYTE> descriptor{'x', 0, ' ', 'm', 'o', 'd', 'e', 'l', ' ', 0};
    Require(WindowsStorageFactory::DescriptorString(descriptor, 2) == "model" &&
            WindowsStorageFactory::DescriptorString(descriptor, 0).empty(),
            "Storage descriptor strings were not trimmed or missing fields were fabricated");
    for (const auto offset : {DWORD{10}, DWORD{11}}) {
        bool rejected = false;
        try { WindowsStorageFactory::DescriptorString(descriptor, offset); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, "Out-of-bounds storage descriptor string was accepted");
    }
    bool rejected = false;
    try { WindowsStorageFactory::DescriptorString({'x', 'y'}, 1); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "Unterminated storage descriptor string was accepted");

    MibTree tree;
    WindowsStorageFactory::Populate(tree);
    const auto get = [&](const std::string& path) {
        const auto node = tree.FindNode(path);
        Require(node && !node->IsDirectory(), "Missing storage parameter");
        const auto leaf = std::static_pointer_cast<SysctlLeafNode>(node);
        Require(!leaf->IsWritable(), "Storage telemetry unexpectedly permits writes");
        return leaf->GetValue();
    };
    const auto count = std::stoull(get("hw.diskcount"));
    std::istringstream names(get("hw.disknames"));
    std::string name;
    size_t found = 0;
    while (std::getline(names, name, ',')) {
        ++found;
        const std::string prefix = "hw.disk." + name + ".";
        const auto path = get(prefix + "path");
        Require(path == "\\\\.\\" + name, "Physical disk name and path disagree");
        DeviceHandle disk(Utf8ToWide(path));
        DISK_GEOMETRY_EX expected{};
        DWORD returned = 0;
        Require(DeviceIoControl(disk.Get(), IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &expected,
                                sizeof(expected), &returned, nullptr) != 0,
                "Cannot obtain independent disk geometry");
        Require(std::stoull(get(prefix + "size")) == static_cast<ULONGLONG>(expected.DiskSize.QuadPart) &&
                std::stoul(get(prefix + "sectorsize")) == expected.Geometry.BytesPerSector,
                "Physical disk size or sector size does not match Windows");
        for (const auto& field : {"model", "vendor", "serial", "revision", "bus", "removable"}) {
            get(prefix + field);
        }
    }
    Require(found == count, "Disk count and comma-separated disk names disagree");
    std::istringstream volumes(get("fs.volume.names"));
    found = 0;
    while (std::getline(volumes, name, ',')) {
        ++found;
        Require(name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z', "Invalid volume name");
        const std::string prefix = "fs.volume." + name + ".";
        const auto root = get(prefix + "path");
        ULARGE_INTEGER available{}, total{}, free{};
        Require(GetDiskFreeSpaceExW(Utf8ToWide(root).c_str(), &available, &total, &free) != 0,
                "Cannot obtain independent volume space");
        Require(std::stoull(get(prefix + "size")) == total.QuadPart &&
                std::stoull(get(prefix + "free")) <= total.QuadPart &&
                std::stoull(get(prefix + "available")) <= total.QuadPart,
                "Volume capacity, free space, or quota-available bytes are invalid");
        Require(!get(prefix + "filesystem").empty(), "Volume filesystem was not reported");
        get(prefix + "type");
        get(prefix + "label");
    }
    Require(found == std::stoull(get("fs.volume.count")), "Volume count and names disagree");
    const auto readonly = Run({"hw.diskcount=0", "fs.volume.count=0"});
    Require(readonly.status != 0 && readonly.error.find("read-only") != std::string::npos,
            "Storage parameters accepted a write");
}

void TestEncoding() {
    const std::string text = "host-\xc3\xa9";
    Require(WideToUtf8(Utf8ToWide(text)) == text, "UTF-8 conversion lost non-ASCII characters");
    bool rejected = false;
    try { Utf8ToWide("\xff"); }
    catch (const std::system_error&) { rejected = true; }
    Require(rejected, "Invalid UTF-8 was silently accepted");
}

void TestGetterFailure() {
    MibTree tree;
    tree.RegisterNode("test.bad", "", []() -> std::string {
        throw std::runtime_error("telemetry unavailable");
    });
    tree.RegisterNode("test.good", "", []() { return "ok"; });
    CapturedOutput captured;
    CliOptions options;
    SysctlPrinterVisitor visitor(options);
    tree.GetRoot()->Accept(visitor);
    Require(!visitor.Succeeded() && captured.output.str() == "test.good=ok\n" &&
            captured.error.str().find("test.bad: telemetry unavailable") != std::string::npos,
            "Getter failure produced partial output, was hidden, or stopped traversal");
}

}

int main() {
    try {
        TestTree();
        TestCli();
        TestBatch();
        TestWritesAndNotices();
        TestMemoryAndBootTime();
        TestStorage();
        TestEncoding();
        TestGetterFailure();
        std::cout << "PASS: MIB, OpenBSD-style CLI, writes, batch files, boot/memory telemetry, "
                     "physical disks, volumes, encoding, and error handling\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << "\n";
        return 1;
    }
}
