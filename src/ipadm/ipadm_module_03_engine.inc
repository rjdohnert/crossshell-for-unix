// IP Helper Enumerator Engine
class IpAdminEngine {
public:
    static std::vector<IpInterfaceInfo> DiscoverInterfaces() {
        std::vector<IpInterfaceInfo> list;
        ULONG family = AF_UNSPEC;
        ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;
        ULONG bufSize = 15360;
        PIP_ADAPTER_ADDRESSES pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));

        if (!pAddresses) return list;

        if (GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize) == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));
        }

        if (GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize) == NO_ERROR) {
            PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
            int index = 0;

            while (pCurr) {
                std::wstring wsFriendly(pCurr->FriendlyName);
                const std::string friendlyName = WStringToUtf8(wsFriendly);

                if (!IsRealNetworkInterface(pCurr->IfType, friendlyName)) {
                    pCurr = pCurr->Next;
                    continue;
                }

                IpInterfaceInfo info;
                std::ostringstream nameOss;

                if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                    info.ifName = "lo0";
                    info.ifClass = "loopback";
                } else {
                    nameOss << "net" << index++;
                    info.ifName = nameOss.str();
                    info.ifClass = "ip";
                }

                info.state = (pCurr->OperStatus == IfOperStatusUp) ? "ok" : "down";
                info.active = (pCurr->OperStatus == IfOperStatusUp) ? "yes" : "no";
                info.overLink = "--";
                info.luid = pCurr->Luid;
                info.ifIndex = pCurr->IfIndex;

                list.push_back(info);
                pCurr = pCurr->Next;
            }
        }
        if (pAddresses) free(pAddresses);
        return list;
    }

    static std::vector<IpAddressObject> DiscoverAddressObjects(const std::vector<IpInterfaceInfo>& interfaces) {
        std::vector<IpAddressObject> addrs;

        PMIB_UNICASTIPADDRESS_TABLE pTable = nullptr;
        if (GetUnicastIpAddressTable(AF_UNSPEC, &pTable) == NO_ERROR) {
            for (ULONG i = 0; i < pTable->NumEntries; ++i) {
                const auto& row = pTable->Table[i];

                // Match with Interface
                std::string matchedIf = "unknown";
                for (const auto& iface : interfaces) {
                    if (iface.ifIndex == row.InterfaceIndex) {
                        matchedIf = iface.ifName;
                        break;
                    }
                }

                IpAddressObject obj;
                obj.ifName = matchedIf;
                obj.sockaddr = row.Address;
                obj.prefixLength = row.OnLinkPrefixLength;
                obj.ipAddress = FormatSockAddrCIDR(reinterpret_cast<const SOCKADDR*>(&row.Address), row.OnLinkPrefixLength);

                // Determine Address Type & Object Name
                std::ostringstream objName;
                if (row.Address.si_family == AF_INET) {
                    obj.type = (row.PrefixOrigin == IpPrefixOriginDhcp) ? "dhcp" : "static";
                    objName << matchedIf << "/v4";
                } else {
                    obj.type = (row.PrefixOrigin == IpPrefixOriginRouterAdvertisement) ? "addrconf" : "static";
                    objName << matchedIf << "/v6";
                }
                obj.addrobj = objName.str();

                // State
                if (row.DadState == IpDadStatePreferred) {
                    obj.state = "ok";
                } else if (row.DadState == IpDadStateDuplicate) {
                    obj.state = "duplicate";
                } else {
                    obj.state = "down";
                }

                addrs.push_back(obj);
            }
            FreeMibTable(pTable);
        }
        return addrs;
    }
};

