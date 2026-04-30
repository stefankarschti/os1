// Minimal AML namespace loader and evaluator used for ACPI device discovery,
// PCI INTx routing through _PRT, and simple device power hooks.
#include "platform/acpi_aml.hpp"

#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "mm/virtual_memory.hpp"

#if defined(OS1_HOST_TEST)
#include <string.h>
#else
#include "util/string.h"
#endif

namespace
{
constexpr size_t kAmlMaxTableLength = 1ull << 20;
constexpr size_t kAmlMaxObjects = 512;
constexpr size_t kAmlMaxIntegerOverrides = 64;
constexpr uint32_t kAcpiDefaultSta = 0x0Fu;
constexpr uint8_t kAmlExtOpPrefix = 0x5Bu;
constexpr uint8_t kAmlScopeOp = 0x10u;
constexpr uint8_t kAmlBufferOp = 0x11u;
constexpr uint8_t kAmlPackageOp = 0x12u;
constexpr uint8_t kAmlMethodOp = 0x14u;
constexpr uint8_t kAmlAliasOp = 0x06u;
constexpr uint8_t kAmlNameOp = 0x08u;
constexpr uint8_t kAmlBytePrefix = 0x0Au;
constexpr uint8_t kAmlWordPrefix = 0x0Bu;
constexpr uint8_t kAmlDwordPrefix = 0x0Cu;
constexpr uint8_t kAmlStringPrefix = 0x0Du;
constexpr uint8_t kAmlQwordPrefix = 0x0Eu;
constexpr uint8_t kAmlDualNamePrefix = 0x2Eu;
constexpr uint8_t kAmlMultiNamePrefix = 0x2Fu;
constexpr uint8_t kAmlRootChar = 0x5Cu;
constexpr uint8_t kAmlParentChar = 0x5Eu;
constexpr uint8_t kAmlReturnOp = 0xA4u;
constexpr uint8_t kAmlStoreOp = 0x70u;
constexpr uint8_t kAmlNoopOp = 0xA3u;
constexpr uint8_t kAmlExtOpDevice = 0x82u;
constexpr uint8_t kAmlExtOpProcessor = 0x83u;
constexpr uint8_t kAmlExtOpPowerResource = 0x84u;
constexpr uint8_t kAmlExtOpThermalZone = 0x85u;
constexpr uint8_t kAmlExtOpOperationRegion = 0x80u;
constexpr uint8_t kAmlExtOpField = 0x81u;
constexpr uint8_t kAmlExtOpIndexField = 0x86u;
constexpr uint8_t kAmlExtOpBankField = 0x87u;
constexpr uint8_t kAmlExtOpDataRegion = 0x88u;
constexpr uint8_t kAmlExtOpMutex = 0x01u;
constexpr uint8_t kAmlExtOpEvent = 0x02u;
constexpr uint8_t kResourceSmallIrq = 0x04u;
constexpr uint8_t kResourceSmallIo = 0x08u;
constexpr uint8_t kResourceSmallFixedIo = 0x09u;
constexpr uint8_t kResourceSmallEndTag = 0x0Fu;
constexpr uint8_t kResourceLargeMemory32Fixed = 0x06u;

struct [[gnu::packed]] AcpiSdtHeader
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

enum class AmlObjectKind : uint8_t
{
    Name = 1,
    Method = 2,
    Device = 3,
};

enum class AmlValueKind : uint8_t
{
    None = 0,
    Integer = 1,
    String = 2,
    Buffer = 3,
    Package = 4,
    NamePath = 5,
};

struct AmlObject
{
    bool active;
    AmlObjectKind kind;
    uint8_t method_arg_count;
    uint32_t data_length;
    char path[kAcpiDevicePathBytes];
    const uint8_t* data;
};

struct AmlIntegerOverride
{
    bool active;
    char path[kAcpiDevicePathBytes];
    uint64_t value;
};

struct AmlNamespaceState
{
    bool loaded;
    AmlObject objects[kAmlMaxObjects];
    size_t object_count;
    AmlIntegerOverride integer_overrides[kAmlMaxIntegerOverrides];
};

struct AmlValue
{
    AmlValueKind kind;
    uint64_t integer;
    const uint8_t* data;
    uint32_t length;
    uint8_t package_count;
    char namepath[kAcpiDevicePathBytes];
};

AmlNamespaceState g_namespace{};
AcpiPciRoute g_routes[kAcpiMaxPciRoutes]{};
size_t g_route_count = 0;
const char* g_last_error = "ok";
char g_last_error_buffer[96]{};
char g_last_object_path[kAcpiDevicePathBytes]{};

void set_error_with_byte(const char* prefix, uint8_t value, const char* scope = nullptr)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    size_t cursor = 0;
    while((nullptr != prefix) && (0 != prefix[cursor]) && (cursor + 4 < sizeof(g_last_error_buffer)))
    {
        g_last_error_buffer[cursor] = prefix[cursor];
        ++cursor;
    }
    g_last_error_buffer[cursor++] = '-';
    g_last_error_buffer[cursor++] = kHex[(value >> 4) & 0xFu];
    g_last_error_buffer[cursor++] = kHex[value & 0xFu];
    if((nullptr != scope) && (0 != scope[0]) && (cursor + 1 < sizeof(g_last_error_buffer)))
    {
        g_last_error_buffer[cursor++] = '@';
        for(size_t i = 0; (0 != scope[i]) && (cursor + 1 < sizeof(g_last_error_buffer)); ++i)
        {
            g_last_error_buffer[cursor++] = scope[i];
        }
    }
    g_last_error_buffer[cursor] = 0;
    g_last_error = g_last_error_buffer;
}

[[nodiscard]] bool checksum_valid(const void* data, size_t length)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for(size_t i = 0; i < length; ++i)
    {
        sum = static_cast<uint8_t>(sum + bytes[i]);
    }
    return 0 == sum;
}

[[nodiscard]] bool signature_equals(const char* left, const char* right)
{
    return 0 == memcmp(left, right, 4);
}

[[nodiscard]] bool map_table(VirtualMemory& kernel_vm,
                             uint64_t physical_address,
                             const AcpiSdtHeader*& header)
{
    header = nullptr;
    if((0 == physical_address) || !map_direct_range(kernel_vm, physical_address, sizeof(AcpiSdtHeader)))
    {
        return false;
    }

    header = kernel_physical_pointer<const AcpiSdtHeader>(physical_address);
    if((header->length < sizeof(AcpiSdtHeader)) || (header->length > kAmlMaxTableLength))
    {
        return false;
    }
    if(!map_direct_range(kernel_vm, physical_address, header->length))
    {
        return false;
    }
    if(!checksum_valid(header, header->length))
    {
        return false;
    }
    return true;
}

[[nodiscard]] bool is_name_lead(uint8_t value)
{
    return ((value >= 'A') && (value <= 'Z')) || ('_' == value);
}

[[nodiscard]] bool is_name_start(uint8_t value)
{
    return is_name_lead(value) || (kAmlRootChar == value) || (kAmlParentChar == value) ||
           (kAmlDualNamePrefix == value) || (kAmlMultiNamePrefix == value);
}

void set_root_path(char* path)
{
    path[0] = '\\';
    path[1] = 0;
}

[[nodiscard]] size_t path_length(const char* path)
{
    size_t length = 0;
    while((nullptr != path) && (0 != path[length]))
    {
        ++length;
    }
    return length;
}

[[nodiscard]] bool path_equals(const char* left, const char* right)
{
    return 0 == strcmp(left, right);
}

[[nodiscard]] const char* last_path_segment(const char* path)
{
    if(nullptr == path)
    {
        return nullptr;
    }

    const char* segment = path;
    for(size_t i = 0; 0 != path[i]; ++i)
    {
        if('.' == path[i])
        {
            segment = path + i + 1;
        }
    }
    return segment;
}

bool path_parent(const char* path, char* parent)
{
    if((nullptr == path) || (nullptr == parent) || (0 == path[0]))
    {
        return false;
    }
    size_t length = path_length(path);
    if((1 == length) && ('\\' == path[0]))
    {
        set_root_path(parent);
        return true;
    }

    while((length > 0) && ('.' != path[length - 1]))
    {
        --length;
    }
    if(length <= 1)
    {
        set_root_path(parent);
        return true;
    }

    memcpy(parent, path, length - 1);
    parent[length - 1] = 0;
    return true;
}

bool append_segment(const char* base, const char segment[4], char* output)
{
    if((nullptr == base) || (nullptr == output))
    {
        return false;
    }

    const size_t base_length = path_length(base);
    if((0 == base_length) || (base_length + 5 >= kAcpiDevicePathBytes))
    {
        return false;
    }

    memcpy(output, base, base_length);
    size_t cursor = base_length;
    if(!((1 == base_length) && ('\\' == base[0])))
    {
        output[cursor++] = '.';
    }
    memcpy(output + cursor, segment, 4);
    cursor += 4;
    output[cursor] = 0;
    return true;
}

bool parse_name_string(const uint8_t*& cursor,
                       const uint8_t* end,
                       const char* scope,
                       char* path)
{
    if((cursor >= end) || (nullptr == scope) || (nullptr == path))
    {
        return false;
    }

    char base[kAcpiDevicePathBytes]{};
    if(kAmlRootChar == *cursor)
    {
        set_root_path(base);
        ++cursor;
    }
    else
    {
        strlcpy(base, scope, sizeof(base));
    }

    while((cursor < end) && (kAmlParentChar == *cursor))
    {
        char next_base[kAcpiDevicePathBytes]{};
        if(!path_parent(base, next_base))
        {
            return false;
        }
        strlcpy(base, next_base, sizeof(base));
        ++cursor;
    }

    uint8_t segment_count = 1;
    if(cursor >= end)
    {
        return false;
    }
    if(0x00u == *cursor)
    {
        strlcpy(path, base, kAcpiDevicePathBytes);
        ++cursor;
        return true;
    }
    if(kAmlDualNamePrefix == *cursor)
    {
        segment_count = 2;
        ++cursor;
    }
    else if(kAmlMultiNamePrefix == *cursor)
    {
        ++cursor;
        if(cursor >= end)
        {
            return false;
        }
        segment_count = *cursor++;
    }

    char next_path[kAcpiDevicePathBytes]{};
    strlcpy(next_path, base, sizeof(next_path));
    for(uint8_t i = 0; i < segment_count; ++i)
    {
        if((end - cursor) < 4)
        {
            return false;
        }
        if(!append_segment(next_path, reinterpret_cast<const char*>(cursor), path))
        {
            return false;
        }
        strlcpy(next_path, path, sizeof(next_path));
        cursor += 4;
    }
    return true;
}

[[nodiscard]] bool parse_pkg_length(const uint8_t*& cursor,
                                    const uint8_t* end,
                                    const uint8_t*& package_start,
                                    const uint8_t*& package_end)
{
    if(cursor >= end)
    {
        return false;
    }
    package_start = cursor;
    const uint8_t lead = *cursor++;
    const uint8_t extra_bytes = static_cast<uint8_t>((lead >> 6) & 0x3u);
    uint32_t length = (0 == extra_bytes) ? static_cast<uint32_t>(lead & 0x3Fu)
                                         : static_cast<uint32_t>(lead & 0x0Fu);
    for(uint8_t i = 0; i < extra_bytes; ++i)
    {
        if(cursor >= end)
        {
            return false;
        }
        length |= static_cast<uint32_t>(*cursor++) << (4u + (8u * i));
    }
    package_end = package_start + length;
    return (package_end >= cursor) && (package_end <= end);
}

[[nodiscard]] uint16_t read_u16(const uint8_t* bytes)
{
    return static_cast<uint16_t>(bytes[0]) | static_cast<uint16_t>(bytes[1] << 8);
}

[[nodiscard]] uint32_t read_u32(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

[[nodiscard]] uint64_t read_u64(const uint8_t* bytes)
{
    uint64_t value = 0;
    for(size_t i = 0; i < 8; ++i)
    {
        value |= static_cast<uint64_t>(bytes[i]) << (i * 8u);
    }
    return value;
}

bool store_object(AmlObjectKind kind,
                  const char* path,
                  const uint8_t* data,
                  uint32_t data_length,
                  uint8_t method_arg_count)
{
    if((nullptr == path) || (0 == path[0]))
    {
        return false;
    }

    for(size_t i = 0; i < g_namespace.object_count; ++i)
    {
        if(g_namespace.objects[i].active && path_equals(g_namespace.objects[i].path, path))
        {
            g_namespace.objects[i].kind = kind;
            g_namespace.objects[i].data = data;
            g_namespace.objects[i].data_length = data_length;
            g_namespace.objects[i].method_arg_count = method_arg_count;
            strlcpy(g_last_object_path, path, sizeof(g_last_object_path));
            return true;
        }
    }

    if(g_namespace.object_count >= kAmlMaxObjects)
    {
        return false;
    }

    AmlObject& object = g_namespace.objects[g_namespace.object_count++];
    object = {};
    object.active = true;
    object.kind = kind;
    object.method_arg_count = method_arg_count;
    object.data = data;
    object.data_length = data_length;
    strlcpy(object.path, path, sizeof(object.path));
    strlcpy(g_last_object_path, path, sizeof(g_last_object_path));
    return true;
}

[[nodiscard]] AmlObject* find_object(const char* path)
{
    for(size_t i = 0; i < g_namespace.object_count; ++i)
    {
        if(g_namespace.objects[i].active && path_equals(g_namespace.objects[i].path, path))
        {
            return &g_namespace.objects[i];
        }
    }
    return nullptr;
}

[[nodiscard]] const AmlIntegerOverride* find_integer_override(const char* path)
{
    for(size_t i = 0; i < kAmlMaxIntegerOverrides; ++i)
    {
        if(g_namespace.integer_overrides[i].active && path_equals(g_namespace.integer_overrides[i].path, path))
        {
            return &g_namespace.integer_overrides[i];
        }
    }
    return nullptr;
}

bool set_integer_override(const char* path, uint64_t value)
{
    for(size_t i = 0; i < kAmlMaxIntegerOverrides; ++i)
    {
        if(g_namespace.integer_overrides[i].active && path_equals(g_namespace.integer_overrides[i].path, path))
        {
            g_namespace.integer_overrides[i].value = value;
            return true;
        }
    }

    for(size_t i = 0; i < kAmlMaxIntegerOverrides; ++i)
    {
        if(!g_namespace.integer_overrides[i].active)
        {
            g_namespace.integer_overrides[i].active = true;
            g_namespace.integer_overrides[i].value = value;
            strlcpy(g_namespace.integer_overrides[i].path,
                    path,
                    sizeof(g_namespace.integer_overrides[i].path));
            return true;
        }
    }
    return false;
}

bool evaluate_term(const char* scope,
                   const uint8_t*& cursor,
                   const uint8_t* end,
                   bool resolve_names,
                   AmlValue& value);

bool resolve_value(AmlValue& value)
{
    if(AmlValueKind::NamePath != value.kind)
    {
        return true;
    }

    AmlObject* object = find_object(value.namepath);
    if(nullptr == object)
    {
        return false;
    }

    const uint8_t* cursor = object->data;
    const uint8_t* end = object->data + object->data_length;
    switch(object->kind)
    {
        case AmlObjectKind::Name:
        {
            if(const AmlIntegerOverride* override = find_integer_override(object->path))
            {
                value = {};
                value.kind = AmlValueKind::Integer;
                value.integer = override->value;
                return true;
            }
            char scope[kAcpiDevicePathBytes]{};
            path_parent(object->path, scope);
            return evaluate_term(scope, cursor, end, true, value);
        }
        case AmlObjectKind::Method:
        {
            if(0 != object->method_arg_count)
            {
                return false;
            }
            char scope[kAcpiDevicePathBytes]{};
            path_parent(object->path, scope);
            value = {};
            while(cursor < end)
            {
                const uint8_t opcode = *cursor++;
                switch(opcode)
                {
                    case kAmlReturnOp:
                        if(!evaluate_term(scope, cursor, end, true, value))
                        {
                            return false;
                        }
                        return true;
                    case kAmlStoreOp:
                    {
                        AmlValue source{};
                        if(!evaluate_term(scope, cursor, end, true, source) ||
                           (AmlValueKind::Integer != source.kind))
                        {
                            return false;
                        }
                        AmlValue target{};
                        if(!evaluate_term(scope, cursor, end, false, target) ||
                           (AmlValueKind::NamePath != target.kind))
                        {
                            return false;
                        }
                        if(!set_integer_override(target.namepath, source.integer))
                        {
                            return false;
                        }
                        break;
                    }
                    case kAmlNoopOp:
                        break;
                    default:
                    {
                        AmlValue ignored{};
                        if(!evaluate_term(scope, --cursor, end, false, ignored))
                        {
                            return false;
                        }
                        break;
                    }
                }
            }
            value.kind = AmlValueKind::None;
            return true;
        }
        case AmlObjectKind::Device:
            return true;
    }
    return false;
}

bool evaluate_term(const char* scope,
                   const uint8_t*& cursor,
                   const uint8_t* end,
                   bool resolve_names,
                   AmlValue& value)
{
    if(cursor >= end)
    {
        return false;
    }

    value = {};
    const uint8_t opcode = *cursor;
    switch(opcode)
    {
        case 0x00u:
            ++cursor;
            value.kind = AmlValueKind::Integer;
            value.integer = 0;
            return true;
        case 0x01u:
            ++cursor;
            value.kind = AmlValueKind::Integer;
            value.integer = 1;
            return true;
        case 0xFFu:
            ++cursor;
            value.kind = AmlValueKind::Integer;
            value.integer = ~0ull;
            return true;
        case kAmlBytePrefix:
            if((end - cursor) < 2)
            {
                return false;
            }
            value.kind = AmlValueKind::Integer;
            value.integer = cursor[1];
            cursor += 2;
            return true;
        case kAmlWordPrefix:
            if((end - cursor) < 3)
            {
                return false;
            }
            value.kind = AmlValueKind::Integer;
            value.integer = read_u16(cursor + 1);
            cursor += 3;
            return true;
        case kAmlDwordPrefix:
            if((end - cursor) < 5)
            {
                return false;
            }
            value.kind = AmlValueKind::Integer;
            value.integer = read_u32(cursor + 1);
            cursor += 5;
            return true;
        case kAmlQwordPrefix:
            if((end - cursor) < 9)
            {
                return false;
            }
            value.kind = AmlValueKind::Integer;
            value.integer = read_u64(cursor + 1);
            cursor += 9;
            return true;
        case kAmlStringPrefix:
        {
            ++cursor;
            const uint8_t* start = cursor;
            while((cursor < end) && (0 != *cursor))
            {
                ++cursor;
            }
            if(cursor >= end)
            {
                return false;
            }
            value.kind = AmlValueKind::String;
            value.data = start;
            value.length = static_cast<uint32_t>(cursor - start);
            ++cursor;
            return true;
        }
        case kAmlBufferOp:
        {
            ++cursor;
            const uint8_t* pkg_start = nullptr;
            const uint8_t* pkg_end = nullptr;
            if(!parse_pkg_length(cursor, end, pkg_start, pkg_end))
            {
                return false;
            }
            AmlValue length_value{};
            if(!evaluate_term(scope, cursor, pkg_end, true, length_value) ||
               (AmlValueKind::Integer != length_value.kind))
            {
                return false;
            }
            value.kind = AmlValueKind::Buffer;
            value.data = cursor;
            value.length = static_cast<uint32_t>(pkg_end - cursor);
            cursor = pkg_end;
            (void)length_value;
            return true;
        }
        case kAmlPackageOp:
        {
            ++cursor;
            const uint8_t* pkg_start = nullptr;
            const uint8_t* pkg_end = nullptr;
            if(!parse_pkg_length(cursor, end, pkg_start, pkg_end) || (cursor >= pkg_end))
            {
                return false;
            }
            value.kind = AmlValueKind::Package;
            value.package_count = *cursor++;
            value.data = cursor;
            value.length = static_cast<uint32_t>(pkg_end - cursor);
            cursor = pkg_end;
            return true;
        }
        default:
            break;
    }

    if(!is_name_start(opcode))
    {
        return false;
    }

    char path[kAcpiDevicePathBytes]{};
    if(!parse_name_string(cursor, end, scope, path))
    {
        return false;
    }

    value.kind = AmlValueKind::NamePath;
    strlcpy(value.namepath, path, sizeof(value.namepath));
    if(resolve_names)
    {
        return resolve_value(value);
    }
    return true;
}

bool skip_term(const char* scope, const uint8_t*& cursor, const uint8_t* end)
{
    AmlValue ignored{};
    return evaluate_term(scope, cursor, end, false, ignored);
}

bool parse_term_list(const char* scope, const uint8_t* cursor, const uint8_t* end)
{
    while(cursor < end)
    {
        const uint8_t opcode = *cursor++;
        switch(opcode)
        {
            case kAmlScopeOp:
            {
                const uint8_t* pkg_start = nullptr;
                const uint8_t* pkg_end = nullptr;
                if(!parse_pkg_length(cursor, end, pkg_start, pkg_end))
                {
                    g_last_error = "parse-scope-pkg";
                    return false;
                }
                char next_scope[kAcpiDevicePathBytes]{};
                if(!parse_name_string(cursor, pkg_end, scope, next_scope))
                {
                    g_last_error = "parse-scope-path";
                    return false;
                }
                if(!parse_term_list(next_scope, cursor, pkg_end))
                {
                    if(0 == strcmp(g_last_error, "ok"))
                    {
                        g_last_error = "parse-scope-body";
                    }
                    return false;
                }
                cursor = pkg_end;
                break;
            }
            case kAmlNameOp:
            {
                char path[kAcpiDevicePathBytes]{};
                if(!parse_name_string(cursor, end, scope, path))
                {
                    g_last_error = "parse-name-path";
                    return false;
                }
                const uint8_t* data_start = cursor;
                if(!skip_term(scope, cursor, end) ||
                   !store_object(AmlObjectKind::Name,
                                 path,
                                 data_start,
                                 static_cast<uint32_t>(cursor - data_start),
                                 0))
                {
                    g_last_error = "parse-name-data";
                    return false;
                }
                break;
            }
            case kAmlMethodOp:
            {
                const uint8_t* pkg_start = nullptr;
                const uint8_t* pkg_end = nullptr;
                if(!parse_pkg_length(cursor, end, pkg_start, pkg_end))
                {
                    g_last_error = "parse-method-pkg";
                    return false;
                }
                char path[kAcpiDevicePathBytes]{};
                if(!parse_name_string(cursor, pkg_end, scope, path) || (cursor >= pkg_end))
                {
                    g_last_error = "parse-method-path";
                    return false;
                }
                const uint8_t method_flags = *cursor++;
                if(!store_object(AmlObjectKind::Method,
                                 path,
                                 cursor,
                                 static_cast<uint32_t>(pkg_end - cursor),
                                 static_cast<uint8_t>(method_flags & 0x7u)))
                {
                    g_last_error = "parse-method-store";
                    return false;
                }
                cursor = pkg_end;
                break;
            }
            case kAmlAliasOp:
            {
                const uint8_t* source_start = cursor;
                char ignored[kAcpiDevicePathBytes]{};
                char alias_path[kAcpiDevicePathBytes]{};
                if(!parse_name_string(cursor, end, scope, ignored) ||
                   !parse_name_string(cursor, end, scope, alias_path) ||
                   !store_object(AmlObjectKind::Name,
                                 alias_path,
                                 source_start,
                                 static_cast<uint32_t>(cursor - source_start - path_length(alias_path)),
                                 0))
                {
                    g_last_error = "parse-alias";
                    return false;
                }
                break;
            }
            case kAmlExtOpPrefix:
            {
                if(cursor >= end)
                {
                    g_last_error = "parse-ext-prefix";
                    return false;
                }
                const uint8_t ext = *cursor++;
                switch(ext)
                {
                    case kAmlExtOpDevice:
                    {
                        const uint8_t* pkg_start = nullptr;
                        const uint8_t* pkg_end = nullptr;
                        if(!parse_pkg_length(cursor, end, pkg_start, pkg_end))
                        {
                            g_last_error = "parse-device-pkg";
                            return false;
                        }
                        char path[kAcpiDevicePathBytes]{};
                        if(!parse_name_string(cursor, pkg_end, scope, path))
                        {
                            g_last_error = "parse-device-path";
                            return false;
                        }
                        if(!store_object(AmlObjectKind::Device, path, cursor, 0, 0))
                        {
                            g_last_error = "parse-device-store";
                            return false;
                        }
                        if(!parse_term_list(path, cursor, pkg_end))
                        {
                            if(0 == strcmp(g_last_error, "ok"))
                            {
                                g_last_error = "parse-device-body";
                            }
                            return false;
                        }
                        cursor = pkg_end;
                        break;
                    }
                    case kAmlExtOpProcessor:
                    case kAmlExtOpPowerResource:
                    case kAmlExtOpThermalZone:
                    case kAmlExtOpField:
                    case kAmlExtOpIndexField:
                    case kAmlExtOpBankField:
                    case kAmlExtOpDataRegion:
                    {
                        const uint8_t* pkg_start = nullptr;
                        const uint8_t* pkg_end = nullptr;
                        if(!parse_pkg_length(cursor, end, pkg_start, pkg_end))
                        {
                            g_last_error = "parse-ext-pkg";
                            return false;
                        }
                        cursor = pkg_end;
                        break;
                    }
                    case kAmlExtOpOperationRegion:
                    {
                        if((end - cursor) < 4)
                        {
                            g_last_error = "parse-opregion-name";
                            return false;
                        }
                        cursor += 4;
                        if(cursor >= end)
                        {
                            g_last_error = "parse-opregion-space";
                            return false;
                        }
                        ++cursor;
                        if(!skip_term(scope, cursor, end) || !skip_term(scope, cursor, end))
                        {
                            g_last_error = "parse-opregion-terms";
                            return false;
                        }
                        break;
                    }
                    case kAmlExtOpMutex:
                    case kAmlExtOpEvent:
                    {
                        char ignored[kAcpiDevicePathBytes]{};
                        if(!parse_name_string(cursor, end, scope, ignored))
                        {
                            g_last_error = "parse-ext-name";
                            return false;
                        }
                        if(kAmlExtOpMutex == ext)
                        {
                            if(cursor >= end)
                            {
                                g_last_error = "parse-mutex-sync";
                                return false;
                            }
                            ++cursor;
                        }
                        break;
                    }
                    default:
                        set_error_with_byte("parse-unsupported-ext", ext, scope);
                        return false;
                }
                break;
            }
            default:
                set_error_with_byte("parse-unsupported-op", opcode, scope);
                return false;
        }
    }
    return true;
}

void decode_eisa_id(uint32_t value, char output[kAcpiHardwareIdBytes])
{
    output[0] = static_cast<char>('@' + ((value >> 26) & 0x1Fu));
    output[1] = static_cast<char>('@' + ((value >> 21) & 0x1Fu));
    output[2] = static_cast<char>('@' + ((value >> 16) & 0x1Fu));
    const uint16_t product = static_cast<uint16_t>(value & 0xFFFFu);
    for(size_t i = 0; i < 4; ++i)
    {
        const uint8_t nibble = static_cast<uint8_t>((product >> ((3u - i) * 4u)) & 0xFu);
        output[3 + i] = (nibble < 10u) ? static_cast<char>('0' + nibble)
                                       : static_cast<char>('A' + (nibble - 10u));
    }
    output[7] = 0;
}

bool evaluate_child(const char* device_path, const char* child_name, AmlValue& value)
{
    char child_path[kAcpiDevicePathBytes]{};
    if(!append_segment(device_path, child_name, child_path))
    {
        return false;
    }

    value = {};
    value.kind = AmlValueKind::NamePath;
    strlcpy(value.namepath, child_path, sizeof(value.namepath));
    return resolve_value(value);
}

bool has_object(const char* device_path, const char* child_name)
{
    char child_path[kAcpiDevicePathBytes]{};
    return append_segment(device_path, child_name, child_path) && (nullptr != find_object(child_path));
}

bool parse_resources(const uint8_t* data,
                     size_t length,
                     AcpiResourceInfo* resources,
                     uint8_t& resource_count)
{
    if(nullptr == resources)
    {
        g_last_error = "resources-null";
        return false;
    }

    resource_count = 0;
    const uint8_t* cursor = data;
    const uint8_t* end = data + length;
    while(cursor < end)
    {
        const uint8_t header = *cursor++;
        if(0 == (header & 0x80u))
        {
            const uint8_t type = static_cast<uint8_t>((header >> 3) & 0x0Fu);
            const uint8_t item_length = header & 0x07u;
            if((end - cursor) < item_length)
            {
                g_last_error = "resources-small-length";
                return false;
            }
            if(kResourceSmallEndTag == type)
            {
                return true;
            }
            if(resource_count >= kAcpiMaxDeviceResources)
            {
                cursor += item_length;
                continue;
            }
            AcpiResourceInfo& resource = resources[resource_count];
            switch(type)
            {
                case kResourceSmallIrq:
                    if(item_length >= 2)
                    {
                        const uint16_t irq_mask = read_u16(cursor);
                        for(uint8_t irq = 0; irq < 16; ++irq)
                        {
                            if(0 != (irq_mask & (1u << irq)))
                            {
                                resource = {};
                                resource.kind = AcpiResourceKind::Irq;
                                resource.flags = (item_length >= 3) ? cursor[2] : 0;
                                resource.base = irq;
                                resource.length = 1;
                                ++resource_count;
                                break;
                            }
                        }
                    }
                    break;
                case kResourceSmallIo:
                    if(item_length >= 7)
                    {
                        resource = {};
                        resource.kind = AcpiResourceKind::Io;
                        resource.base = read_u16(cursor + 1);
                        resource.length = cursor[6];
                        ++resource_count;
                    }
                    break;
                case kResourceSmallFixedIo:
                    if(item_length >= 3)
                    {
                        resource = {};
                        resource.kind = AcpiResourceKind::Io;
                        resource.base = read_u16(cursor);
                        resource.length = cursor[2];
                        ++resource_count;
                    }
                    break;
                default:
                    break;
            }
            cursor += item_length;
            continue;
        }

        if((end - cursor) < 2)
        {
            g_last_error = "resources-large-header";
            return false;
        }
        const uint8_t type = header & 0x7Fu;
        const uint16_t item_length = read_u16(cursor);
        cursor += 2;
        if((end - cursor) < item_length)
        {
            g_last_error = "resources-large-length";
            return false;
        }
        if((kResourceLargeMemory32Fixed == type) && (item_length >= 9) &&
           (resource_count < kAcpiMaxDeviceResources))
        {
            AcpiResourceInfo& resource = resources[resource_count++];
            resource = {};
            resource.kind = AcpiResourceKind::Memory;
            resource.flags = cursor[0];
            resource.base = read_u32(cursor + 1);
            resource.length = read_u32(cursor + 5);
        }
        cursor += item_length;
    }
    return true;
}

bool package_element_at(const char* scope,
                        const uint8_t* package_data,
                        size_t package_length,
                        uint8_t index,
                        AmlValue& value)
{
    const uint8_t* cursor = package_data;
    const uint8_t* end = package_data + package_length;
    for(uint8_t current = 0; current <= index; ++current)
    {
        if(cursor >= end)
        {
            g_last_error = "package-cursor";
            return false;
        }
        if(!evaluate_term(scope, cursor, end, false, value))
        {
            g_last_error = "package-evaluate";
            return false;
        }
    }
    return true;
}

bool find_device_index(const AcpiDeviceInfo* devices, size_t device_count, const char* path, uint16_t& index)
{
    for(size_t i = 0; i < device_count; ++i)
    {
        if(devices[i].active && path_equals(devices[i].path, path))
        {
            index = static_cast<uint16_t>(i);
            return true;
        }
    }
    index = kAcpiDeviceIndexNone;
    return false;
}

uint8_t inherit_bus_number(const AcpiDeviceInfo* devices, size_t device_count, const char* path)
{
    char parent[kAcpiDevicePathBytes]{};
    if(!path_parent(path, parent))
    {
        return 0xFFu;
    }

    while(!((1 == path_length(parent)) && ('\\' == parent[0])))
    {
        for(size_t i = 0; i < device_count; ++i)
        {
            if(devices[i].active && path_equals(devices[i].path, parent) && (0xFFu != devices[i].bus_number))
            {
                return devices[i].bus_number;
            }
        }

        char next_parent[kAcpiDevicePathBytes]{};
        if(!path_parent(parent, next_parent))
        {
            break;
        }
        strlcpy(parent, next_parent, sizeof(parent));
    }
    return 0xFFu;
}

bool resolve_route_irq(const char* path, uint32_t& irq, uint16_t& flags)
{
    AmlValue crs{};
    if(!evaluate_child(path, "_CRS", crs) || (AmlValueKind::Buffer != crs.kind))
    {
        g_last_error = "route-crs";
        return false;
    }

    AcpiResourceInfo resources[kAcpiMaxDeviceResources]{};
    uint8_t resource_count = 0;
    if(!parse_resources(crs.data, crs.length, resources, resource_count))
    {
        return false;
    }

    for(uint8_t i = 0; i < resource_count; ++i)
    {
        if(AcpiResourceKind::Irq == resources[i].kind)
        {
            irq = static_cast<uint32_t>(resources[i].base);
            flags = resources[i].flags;
            return true;
        }
    }
    g_last_error = "route-no-irq";
    return false;
}

bool build_routes_for_device(const AcpiDeviceInfo& device,
                             const AcpiDeviceInfo* devices,
                             size_t device_count,
                             AcpiPciRoute* routes,
                             size_t& route_count)
{
    AmlValue prt{};
    if(!evaluate_child(device.path, "_PRT", prt) || (AmlValueKind::Package != prt.kind))
    {
        g_last_error = "route-prt";
        return true;
    }

    char scope[kAcpiDevicePathBytes]{};
    strlcpy(scope, device.path, sizeof(scope));
    for(uint8_t i = 0; i < prt.package_count; ++i)
    {
        if(route_count >= kAcpiMaxPciRoutes)
        {
            g_last_error = "route-table-full";
            return false;
        }

        AmlValue row{};
        if(!package_element_at(scope, prt.data, prt.length, i, row) || (AmlValueKind::Package != row.kind))
        {
            g_last_error = "route-row";
            return false;
        }

        AmlValue address{};
        AmlValue pin{};
        AmlValue source{};
        AmlValue source_index{};
        if(!package_element_at(scope, row.data, row.length, 0, address) ||
           !package_element_at(scope, row.data, row.length, 1, pin) ||
           !package_element_at(scope, row.data, row.length, 2, source) ||
           !package_element_at(scope, row.data, row.length, 3, source_index) ||
           (AmlValueKind::Integer != address.kind) || (AmlValueKind::Integer != pin.kind))
        {
            g_last_error = "route-elements";
            return false;
        }

        AcpiPciRoute& route = routes[route_count];
        route = {};
        route.active = true;
        route.source_is_gsi = false;
        route.bus_number = device.bus_number;
        route.slot = static_cast<uint8_t>((address.integer >> 16) & 0xFFu);
        const uint16_t function_field = static_cast<uint16_t>(address.integer & 0xFFFFu);
        route.function = (0xFFFFu == function_field) ? 0xFFu : static_cast<uint8_t>(function_field & 0xFFu);
        route.pin = static_cast<uint8_t>(pin.integer & 0xFFu);
        route.source_device_index = kAcpiDeviceIndexNone;

        if((AmlValueKind::Integer == source.kind) && (0 == source.integer) &&
           (AmlValueKind::Integer == source_index.kind))
        {
            route.source_is_gsi = true;
            route.irq = static_cast<uint32_t>(source_index.integer);
        }
        else if(AmlValueKind::NamePath == source.kind)
        {
            uint32_t irq = 0;
            uint16_t irq_flags = 0;
            if(!resolve_route_irq(source.namepath, irq, irq_flags))
            {
                return false;
            }
            route.irq = irq;
            route.flags = irq_flags;
            strlcpy(route.source_path, source.namepath, sizeof(route.source_path));
            (void)find_device_index(devices, device_count, source.namepath, route.source_device_index);
        }
        else
        {
            g_last_error = "route-source";
            return false;
        }

        ++route_count;
    }
    return true;
}
}  // namespace

void acpi_namespace_reset()
{
    memset(&g_namespace, 0, sizeof(g_namespace));
    memset(g_routes, 0, sizeof(g_routes));
    g_route_count = 0;
    memset(g_last_object_path, 0, sizeof(g_last_object_path));
}

bool acpi_namespace_load(VirtualMemory& kernel_vm,
                         const AcpiDefinitionBlock* definition_blocks,
                         size_t definition_block_count)
{
    if((nullptr == definition_blocks) || (0 == definition_block_count))
    {
        g_last_error = "bad-arguments";
        return false;
    }

    g_last_error = "ok";
    acpi_namespace_reset();
    set_root_path(g_namespace.objects[0].path);
    for(size_t i = 0; i < definition_block_count; ++i)
    {
        if(!definition_blocks[i].active)
        {
            continue;
        }

        const AcpiSdtHeader* header = nullptr;
        if(!map_table(kernel_vm, definition_blocks[i].physical_address, header))
        {
            g_last_error = "map-table";
            return false;
        }
        if(!signature_equals(header->signature, definition_blocks[i].signature))
        {
            g_last_error = "signature-mismatch";
            return false;
        }

        const auto* aml = reinterpret_cast<const uint8_t*>(header) + sizeof(AcpiSdtHeader);
        const size_t aml_length = header->length - sizeof(AcpiSdtHeader);
        char root[kAcpiDevicePathBytes]{};
        set_root_path(root);
        if(!parse_term_list(root, aml, aml + aml_length))
        {
            acpi_namespace_reset();
            return false;
        }
    }

    g_namespace.loaded = true;
    g_last_error = "ok";
    return true;
}

const char* acpi_namespace_last_error()
{
    return g_last_error;
}

const char* acpi_namespace_last_object()
{
    return g_last_object_path;
}

bool acpi_build_device_info(AcpiDeviceInfo* devices,
                            size_t& device_count,
                            AcpiPciRoute* routes,
                            size_t& route_count)
{
    if((nullptr == devices) || (nullptr == routes) || !g_namespace.loaded)
    {
        g_last_error = "build-arguments";
        return false;
    }

    memset(devices, 0, sizeof(AcpiDeviceInfo) * kAcpiMaxDevices);
    memset(routes, 0, sizeof(AcpiPciRoute) * kAcpiMaxPciRoutes);
    device_count = 0;
    route_count = 0;

    for(size_t i = 0; i < g_namespace.object_count; ++i)
    {
        const AmlObject& object = g_namespace.objects[i];
        if(!object.active || (AmlObjectKind::Device != object.kind))
        {
            continue;
        }
        if(device_count >= kAcpiMaxDevices)
        {
            g_last_error = "device-table-full";
            return false;
        }

        AcpiDeviceInfo& device = devices[device_count];
        device = {};
        device.active = true;
        device.status = kAcpiDefaultSta;
        device.bus_number = 0xFFu;
        strlcpy(device.path, object.path, sizeof(device.path));

        const char* last_segment = last_path_segment(object.path);
        last_segment = ((nullptr == last_segment) || ('\\' == last_segment[0])) ? object.path + 1
                                              : last_segment;
        memcpy(device.name, last_segment, 4);
        device.name[4] = 0;

        AmlValue value{};
        if(evaluate_child(object.path, "_HID", value))
        {
            device.flags |= kAcpiDeviceHasHid;
            if(AmlValueKind::Integer == value.kind)
            {
                device.hid_eisa_id = static_cast<uint32_t>(value.integer);
                decode_eisa_id(device.hid_eisa_id, device.hardware_id);
            }
            else if(AmlValueKind::String == value.kind)
            {
                const size_t copy_length = (value.length < (kAcpiHardwareIdBytes - 1)) ? value.length
                                                                                      : (kAcpiHardwareIdBytes - 1);
                memcpy(device.hardware_id, value.data, copy_length);
                device.hardware_id[copy_length] = 0;
            }
        }

        if(evaluate_child(object.path, "_UID", value) && (AmlValueKind::Integer == value.kind))
        {
            device.flags |= kAcpiDeviceHasUid;
            device.uid = value.integer;
        }
        if(evaluate_child(object.path, "_ADR", value) && (AmlValueKind::Integer == value.kind))
        {
            device.flags |= kAcpiDeviceHasAdr;
            device.adr = value.integer;
        }
        if(evaluate_child(object.path, "_BBN", value) && (AmlValueKind::Integer == value.kind))
        {
            device.flags |= kAcpiDeviceHasBbn;
            device.bus_number = static_cast<uint8_t>(value.integer & 0xFFu);
        }
        else
        {
            device.bus_number = inherit_bus_number(devices, device_count, object.path);
        }
        if(evaluate_child(object.path, "_STA", value) && (AmlValueKind::Integer == value.kind))
        {
            device.status = static_cast<uint32_t>(value.integer);
        }
        if(evaluate_child(object.path, "_CRS", value) && (AmlValueKind::Buffer == value.kind))
        {
            device.flags |= kAcpiDeviceHasCrs;
            if(!parse_resources(value.data, value.length, device.resources, device.resource_count))
            {
                g_last_error = "device-crs";
                return false;
            }
        }
        if(has_object(object.path, "_PRT"))
        {
            device.flags |= kAcpiDeviceHasPrt;
        }
        if(has_object(object.path, "_PS0"))
        {
            device.flags |= kAcpiDeviceHasPs0;
        }
        if(has_object(object.path, "_PS3"))
        {
            device.flags |= kAcpiDeviceHasPs3;
        }

        ++device_count;
    }

    for(size_t i = 0; i < device_count; ++i)
    {
        if(0 != (devices[i].flags & kAcpiDeviceHasPrt))
        {
            if(!build_routes_for_device(devices[i], devices, device_count, routes, route_count))
            {
                if(0 == strcmp(g_last_error, "ok"))
                {
                    g_last_error = "device-routes";
                }
                return false;
            }
        }
    }

    memset(g_routes, 0, sizeof(g_routes));
    for(size_t i = 0; i < route_count; ++i)
    {
        g_routes[i] = routes[i];
    }
    g_route_count = route_count;
    return true;
}

bool acpi_resolve_pci_route(uint8_t bus,
                            uint8_t slot,
                            uint8_t function,
                            uint8_t pin,
                            uint32_t& irq,
                            uint16_t& flags)
{
    bool source_is_gsi = false;
    return acpi_resolve_pci_route_details(bus, slot, function, pin, irq, flags, source_is_gsi);
}

bool acpi_resolve_pci_route_details(uint8_t bus,
                                    uint8_t slot,
                                    uint8_t function,
                                    uint8_t pin,
                                    uint32_t& irq,
                                    uint16_t& flags,
                                    bool& source_is_gsi)
{
    for(size_t i = 0; i < g_route_count; ++i)
    {
        const AcpiPciRoute& route = g_routes[i];
        if(route.active && (route.bus_number == bus) && (route.slot == slot) && (route.pin == pin) &&
           ((0xFFu == route.function) || (route.function == function)))
        {
            irq = route.irq;
            flags = route.flags;
            source_is_gsi = route.source_is_gsi;
            return true;
        }
    }
    return false;
}

bool acpi_set_device_power_state(const char* path, AcpiPowerState state)
{
    if(nullptr == path)
    {
        return false;
    }
    const char* method = (AcpiPowerState::D0 == state) ? "_PS0" : "_PS3";
    AmlValue ignored{};
    return evaluate_child(path, method, ignored);
}

bool acpi_read_named_integer(const char* path, uint64_t& value)
{
    if(nullptr == path)
    {
        return false;
    }

    AmlValue resolved{};
    resolved.kind = AmlValueKind::NamePath;
    strlcpy(resolved.namepath, path, sizeof(resolved.namepath));
    if(!resolve_value(resolved) || (AmlValueKind::Integer != resolved.kind))
    {
        return false;
    }
    value = resolved.integer;
    return true;
}