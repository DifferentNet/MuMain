"""Generates the web build's packet send functions from OpenMU's packet definitions.

    python tools/web/gen_web_packets.py --xml-dir <packets>/contentFiles/any/net10.0
        --bindings-dir src/source/Dotnet --out-dir <build>/generated/web

The desktop client sends packets through MUnique.Client.Library (C#, ClientLibrary/), whose
Send<Packet> exports are generated from the same XML by GenerateExtensionsDotNet.xslt. A browser
can't load that library, so this writes C++ with the same behaviour:

- WebPacketBuilders.generated.h: Build<Packet>(...) for every packet, returning its bytes, laid out
  like OpenMU's generated <Packet>Ref structs (GenerateRefStructs.xslt): header, then each field.
- WebPacketFunctions.generated.cpp: Send<Packet>(handle, ...) for every export the C# library has
  (packets without Structure[] fields), with its parameters in the same order and C types, and a
  name -> function table that Core/Platform/posix/PlatformLibrary.cpp hands out on the web.

The parameter types are checked against the client's own bindings (PacketBindings_*.h), so a
mismatch fails the build instead of corrupting packets. Standard library only.
"""
import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET

NS = "{http://www.munique.net/OpenMU/PacketDefinitions}"
NAMESPACES = ["ConnectServer", "ClientToServer", "ChatServer"]

# XML type -> (C parameter type, size in bytes or None)
SCALARS = {
    "Byte": ("BYTE", 1), "Boolean": ("BYTE", 1),
    "ShortLittleEndian": ("uint16_t", 2), "ShortBigEndian": ("uint16_t", 2),
    "IntegerLittleEndian": ("uint32_t", 4), "IntegerBigEndian": ("uint32_t", 4),
    "LongLittleEndian": ("uint64_t", 8), "LongBigEndian": ("uint64_t", 8),
    "Float": ("float", 4), "Double": ("double", 8),
}


def text(element, tag):
    child = element.find(NS + tag)
    return child.text.strip() if child is not None and child.text is not None else None


def clean_name(name):
    return re.sub(r"[ ()/\-:]", "", name)


def lower_name(name):
    result = name[0].lower() + name[1:]
    return "class_" if result == "class" else result


class Field:
    def __init__(self, element):
        self.index = int(text(element, "Index"))
        self.type = text(element, "Type")
        self.name = clean_name(text(element, "Name"))
        self.length = text(element, "Length")
        self.length = int(self.length) if self.length is not None else None
        self.left_shifted = text(element, "LeftShifted")
        self.left_shifted = int(self.left_shifted) if self.left_shifted is not None else None
        self.default = text(element, "DefaultValue")
        self.type_name = text(element, "TypeName")
        self.param = lower_name(self.name)

    def c_type(self):
        if self.type in SCALARS:
            return SCALARS[self.type][0]
        if self.type == "Enum":
            # Enums are written as one byte (GenerateRefStructs.xslt); passing them as their
            # integer type keeps the WebAssembly signature the same as the client's enum type.
            return "uint32_t" if "." in (self.type_name or "") else "BYTE"
        if self.type == "String":
            return "const char16_t*"
        if self.type == "Binary":
            return "const BYTE*"
        raise ValueError(f"unsupported field type {self.type}")

    def params(self):
        if self.type == "Binary":
            return [(self.c_type(), self.param), ("uint32_t", self.param + "ByteLength")]
        return [(self.c_type(), self.param)]


class Packet:
    def __init__(self, element):
        self.name = clean_name(text(element, "Name"))
        self.header = text(element, "HeaderType")
        self.code = int(text(element, "Code"), 16)
        subcode = text(element, "SubCode")
        self.subcode = int(subcode, 16) if subcode is not None else None
        length = text(element, "Length")
        self.length = int(length) if length is not None else None
        fields = element.find(NS + "Fields")
        self.fields = [Field(f) for f in fields.findall(NS + "Field")] if fields is not None else []

    def exported(self):
        return all(f.type != "Structure[]" for f in self.fields)

    def ordered_fields(self):
        # GenerateExtensionsDotNet.xslt sorts the parameters by DefaultValue (as text, stable):
        # fields without one come first.
        return sorted(self.fields, key=lambda f: f.default or "")

    def header_type_byte(self):
        return int(self.header[1], 16) | 0xC0  # "C1Header..." -> 0xC1

    def is_c2_or_c4(self):
        return self.header[1] in "24"

    def variable_field(self):
        if self.length is not None:
            return None
        variable = [f for f in self.fields if f.type in ("String", "Binary") and f.length is None]
        if len(variable) != 1:
            raise ValueError(f"{self.name}: expected one variable-length field, got {len(variable)}")
        return variable[0]


def emitted(c_type):
    """The C++ spelling of a binding type: BYTE (a Windows-style typedef) as std::uint8_t."""
    return c_type.replace("BYTE", "std::uint8_t")


def write_field(field):
    """C++ statements writing one field into std::vector<uint8_t> p."""
    i, p = field.index, field.param
    t = field.type
    if t in ("Byte", "Enum"):
        value = f"static_cast<std::uint8_t>({p})"
        if field.length is not None or field.left_shifted is not None:
            bits = field.length if field.length is not None else 8
            return [f"SetByteValue(p.data() + {i}, {value}, {bits}, {field.left_shifted or 0});"]
        return [f"p[{i}] = {value};"]
    if t == "Boolean":
        return [f"SetBoolean(p.data() + {i}, {p} != 0, {field.left_shifted or 0});"]
    if t in SCALARS:
        size = SCALARS[t][1]
        if t == "Float":
            return [f"WriteFloat(p.data() + {i}, {p});"]
        if t == "Double":
            return [f"WriteDouble(p.data() + {i}, {p});"]
        endian = "BigEndian" if t.endswith("BigEndian") else "LittleEndian"
        return [f"WriteUInt{size * 8}{endian}(p.data() + {i}, {p});"]
    if t == "String":
        if field.length is not None:
            return [f"WriteString(p.data() + {i}, {field.length}, {p}Utf8);"]
        return [f"WriteString(p.data() + {i}, {p}Utf8.size() + 1, {p}Utf8);"]
    if t == "Binary":
        if field.length is not None:
            return [f"WriteBinary(p.data() + {i}, {field.length}, {p}, {p}ByteLength);"]
        return [f"WriteBinary(p.data() + {i}, {p}ByteLength, {p}, {p}ByteLength);"]
    raise ValueError(f"unsupported field type {t}")


def builder(packet):
    params = [f"{emitted(c)} {n}" for f in packet.ordered_fields() for c, n in f.params()]
    lines = [f"inline std::vector<std::uint8_t> Build{packet.name}({', '.join(params)})", "{"]
    for f in packet.fields:
        if f.type == "String":
            lines.append(f"    const std::string {f.param}Utf8 = ToUtf8({f.param});")
    variable = packet.variable_field()
    if variable is None:
        lines.append(f"    std::vector<std::uint8_t> p({packet.length}, 0);")
    elif variable.type == "String":
        lines.append(f"    std::vector<std::uint8_t> p({variable.index} + {variable.param}Utf8.size() + 1, 0);")
    else:
        lines.append(f"    std::vector<std::uint8_t> p({variable.index} + static_cast<std::size_t>({variable.param}ByteLength), 0);")
    lines.append(f"    p[0] = 0x{packet.header_type_byte():02X};")
    if packet.is_c2_or_c4():
        lines.append(f"    WriteUInt16BigEndian(p.data() + 1, static_cast<std::uint16_t>(p.size()));")
        code_index = 3
    else:
        lines.append(f"    p[1] = static_cast<std::uint8_t>(p.size());")
        code_index = 2
    lines.append(f"    p[{code_index}] = 0x{packet.code:02X};")
    if packet.subcode is not None:
        lines.append(f"    p[{code_index + 1}] = 0x{packet.subcode:02X};")
    for f in packet.fields:
        lines += ["    " + s for s in write_field(f)]
    lines += ["    return p;", "}", ""]
    return lines


def sender(packet):
    fields = packet.ordered_fields()
    params = ["std::int32_t handle"] + [f"{emitted(c)} {n}" for f in fields for c, n in f.params()]
    args = [n for f in fields for _, n in f.params()]
    return [f"void Send{packet.name}({', '.join(params)})", "{",
            f"    const std::vector<std::uint8_t> p = Build{packet.name}({', '.join(args)});",
            "    mu::web::SendPacket(handle, p.data(), p.size());", "}", ""]


def binding_types(bindings_dir):
    """Function name -> list of parameter C types, from the client's PacketBindings_*.h."""
    result = {}
    for ns in NAMESPACES:
        with open(os.path.join(bindings_dir, f"PacketBindings_{ns}.h"), encoding="utf-8-sig") as f:
            for m in re.finditer(r"typedef void\(CORECLR_DELEGATE_CALLTYPE\* (\w+)\)\(([^)]*)\);", f.read()):
                result[m.group(1)] = [t.strip() for t in m.group(2).split(",")][1:]
    return result


def normalize(c_type, enum_names):
    return "ENUM" if c_type in enum_names else c_type


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--xml-dir", required=True)
    parser.add_argument("--bindings-dir", required=True)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    packets = []
    for ns in NAMESPACES:
        root = ET.parse(os.path.join(args.xml_dir, ns, f"{ns}Packets.xml")).getroot()
        # The C# library exports every packet of these files, whichever way it is meant to travel.
        packets += [Packet(element) for element in root.iter(NS + "Packet")]

    bindings = binding_types(args.bindings_dir)
    exported = [p for p in packets if p.exported() and f"Send{p.name}" in bindings]
    missing = sorted(set(bindings) - {f"Send{p.name}" for p in exported})
    if missing:
        sys.exit(f"gen_web_packets: no packet definition for {', '.join(missing)}")
    errors = []
    for p in exported:
        ours = [c for f in p.ordered_fields() for c, _ in f.params()]
        theirs = bindings[f"Send{p.name}"]
        if len(ours) != len(theirs) or any(
                o != t and not (o in ("BYTE", "uint32_t") and re.fullmatch(r"[A-Z]\w*", t) and t not in ("BYTE",))
                for o, t in zip(ours, theirs)):
            errors.append(f"Send{p.name}: generated ({', '.join(ours)}) vs bindings ({', '.join(theirs)})")
    if errors:
        sys.exit("gen_web_packets: parameter mismatch\n  " + "\n  ".join(errors))

    os.makedirs(args.out_dir, exist_ok=True)
    header = ["// Generated by tools/web/gen_web_packets.py from OpenMU's packet definitions. Don't edit.",
              "#pragma once", "", '#include "Core/Platform/web/WebPacketWriting.h"', "",
              "namespace mu::web::packets", "{"]
    for p in packets:
        if all(f.type != "Structure[]" for f in p.fields):
            header += builder(p)
    header += ["} // namespace mu::web::packets", ""]
    write_if_changed(os.path.join(args.out_dir, "WebPacketBuilders.generated.h"), "\n".join(header))

    source = ["// Generated by tools/web/gen_web_packets.py from OpenMU's packet definitions. Don't edit.",
              '#include "Core/Platform/web/WebNetwork.h"', '#include "WebPacketBuilders.generated.h"', "",
              "namespace", "{", "using namespace mu::web::packets;", ""]
    for p in exported:
        source += sender(p)
    source += ["} // namespace", "",
               "const std::vector<mu::web::NetworkSymbol>& mu::web::GeneratedPacketFunctions()", "{",
               "    static const std::vector<NetworkSymbol> functions = {"]
    source += [f'        {{"Send{p.name}", reinterpret_cast<void*>(&Send{p.name})}},' for p in exported]
    source += ["    };", "    return functions;", "}", ""]
    write_if_changed(os.path.join(args.out_dir, "WebPacketFunctions.generated.cpp"), "\n".join(source))
    print(f"gen_web_packets: {len(exported)} send functions, {len(packets)} packet builders")


def write_if_changed(path, content):
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            if f.read() == content:
                return
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)


if __name__ == "__main__":
    main()
