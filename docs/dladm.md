# dladm

## What it does

Reports and summarizes data-link information in a Solaris `dladm`-style layout for Windows hosts. It exposes physical adapters, virtual NICs, and basic link properties while filtering out synthetic Windows interfaces that are not real networking links.

## Usage

- `dladm.exe show-link [link]`
- `dladm.exe show-phys [link]`
- `dladm.exe show-linkprop [-p prop] [link]`
- `dladm.exe show-vnic [link]`
- `dladm.exe show-aggr [link]`
- `dladm.exe show-vlan [link]`
- `dladm.exe set-linkprop -p prop=value link`
- `dladm.exe help`

## Subcommands

- `show-link`: summary of data links, class, MTU, state, and over-link
- `show-phys`: physical links with media type, state, speed, duplex, and device name
- `show-linkprop`: link-property view for MTU, speed, duplex, and MAC address
- `show-vnic`: virtual NIC information and bounds to a physical link
- `show-aggr`: aggregation or NIC team summary
- `show-vlan`: VLAN bindings and tag IDs
- `set-linkprop`: simulated property update entry point

## Notes

- This is a Windows-native approximation of the Solaris `dladm` command, not a byte-for-byte clone.
- Real adapters are filtered from the richer Windows interface enumeration so synthetic WFP, tunnel, and QoS entries do not pollute the default output.
- The output is designed to be human-readable, with compact tables similar to Solaris output conventions.
- Some features are emulated because Windows does not expose a direct Solaris-style data-link registry.

## Examples

- `dladm.exe show-link`
- `dladm.exe show-phys`
- `dladm.exe show-linkprop -p mtu net0`
- `dladm.exe show-vnic`
- `dladm.exe show-aggr`
- `dladm.exe show-vlan`

## UNIX origin

The original `dladm` utility comes from Oracle Solaris and is used to inspect and configure data links, VLANs, link aggregations, and virtual interfaces.
