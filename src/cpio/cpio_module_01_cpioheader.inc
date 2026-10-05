struct CpioHeader {
    char magic[6];     // "070701"
    char ino[8];       // Inode number
    char mode[8];      // File mode / permissions
    char uid[8];       // User ID
    char gid[8];       // Group ID
    char nlink[8];     // Number of links
    char mtime[8];     // Modification time
    char filesize[8];  // File size
    char maj[8];       // Major device number
    char min[8];       // Minor device number
    char rmaj[8];      // Real major device number
    char rmin[8];      // Real minor device number
    char namesize[8];  // Length of path (including trailing '\0')
    char chksum[8];    // Checksum (0 for newc)
};
#pragma pack(pop)

