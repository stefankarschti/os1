# ELF-Based Structured Interface Metadata Specification

## 1. Overview

This document defines a mechanism for embedding **structured interface metadata** inside an ELF file. The goal is to expose a well-defined, introspectable interface consisting of:

- **Properties** (read-only / read-write data)
- **Methods** (callable functions)
- **Events** (subscription-based notifications)

This design separates:

ELF symbols → where code/data lives  
Interface metadata → what those symbols mean

---

## 2. Core Concept

An ELF file contains:

1. **Standard ELF structures**
   - `.text`, `.data`, `.bss`
   - `.symtab`, `.dynsym`

2. **Custom interface metadata section**
   - `.os.interface` (recommended)

The metadata describes how to interpret symbols as a structured interface.

---

## 3. ELF Section Definition

### 3.1 Section Name

Recommended:

.os.interface

Alternative (versioned):

.os.iface.v1

---

### 3.2 Section Contents

The section contains a serialized schema describing:

- Interface identity
- Version
- Properties
- Methods
- Events
- Types
- Permissions

---

## 4. Metadata Schema (Conceptual)

Example (YAML for clarity):

```yaml
interface: "com.example.counter"
version: 1

properties:
  - name: "value"
    type: "u64"
    access: "rw"
    symbol: "counter_value"

  - name: "max"
    type: "u64"
    access: "ro"
    symbol: "counter_max"

methods:
  - name: "increment"
    symbol: "counter_increment"
    args:
      - { name: "amount", type: "u64" }
    returns:
      type: "u64"

  - name: "reset"
    symbol: "counter_reset"
    args: []
    returns:
      type: "void"

events:
  - name: "changed"
    payload:
      - { name: "new_value", type: "u64" }
    add_symbol: "counter_changed_add"
    remove_symbol: "counter_changed_remove"
    list_symbol: "counter_changed_list"
```

---

## 5. Symbol Resolution Model

### 5.1 Address Calculation

runtime_address = module_load_base + symbol.st_value

Cases:

| Type              | Address Formula                    |
|------------------|-----------------------------------|
| Non-PIE          | symbol.st_value                   |
| PIE / shared obj | base + symbol.st_value            |

---

### 5.2 Required ELF Data

- `.symtab` or `.dynsym`
- Symbol names referenced by metadata

---

## 6. Properties

### 6.1 Definition

```yaml
name: "value"
type: "u64"
access: "rw"
symbol: "counter_value"
```

---

### 6.2 Property Models

#### A. Direct Memory Property

symbol: "counter_value"

Access:

read/write memory directly

**Pros**
- Fast
- Simple

**Cons**
- Unsafe
- No validation
- Concurrency issues

---

#### B. Accessor Property (Recommended)

```yaml
get_symbol: "counter_get_value"
set_symbol: "counter_set_value"
```

Access:

invoke functions instead of raw memory access

**Pros**
- Safe
- Validated
- Encapsulated

**Cons**
- Slight overhead

---

### 6.3 Property Fields

name  
type  
access (ro | rw)  
symbol OR (get_symbol / set_symbol)  
size  
alignment  
permissions (optional)

---

## 7. Methods

### 7.1 Definition

```yaml
- name: "increment"
  symbol: "counter_increment"
  calling_convention: "sysv_amd64"
  args:
    - { name: "amount", type: "u64" }
  returns:
    type: "u64"
```

---

### 7.2 Required Metadata

name  
symbol  
argument list  
return type  
calling convention  
permissions  
blocking behavior (optional)  
error model (optional)

---

### 7.3 ABI Considerations

Avoid relying indefinitely on raw C ABI.

Long-term:

→ define stable OS-level ABI  
→ define portable type system

---

## 8. Events

### 8.1 Concept

Events are modeled as structured subscription systems.

---

### 8.2 Definition

```yaml
- name: "changed"
  payload:
    - { name: "new_value", type: "u64" }
  add_symbol: "counter_changed_add"
  remove_symbol: "counter_changed_remove"
  list_symbol: "counter_changed_list"
```

---

### 8.3 Event Operations

add(callback) → returns subscription handle  
remove(handle)  
list() → returns current subscriptions  
emit(...) → internal

---

### 8.4 Runtime Model

obj.events.changed.add(fn)  
obj.events.changed.remove(handle)  
obj.events.changed.list()

---

## 9. Encoding Options

### 9.1 Option A: JSON / YAML (Recommended Initially)

.os.interface = UTF-8 JSON/YAML

**Pros**
- Easy to debug
- Easy to generate
- Flexible

**Cons**
- Larger size
- Runtime parsing cost

---

### 9.2 Option B: Binary Schema

Example:

```c
struct OsIfaceHeader {
    uint32_t magic;   // "OSIF"
    uint16_t version;
    uint16_t flags;
    uint32_t counts[];
};
```

**Pros**
- Fast
- Compact

**Cons**
- Harder to evolve

---

### 9.3 Option C: ELF Notes

.note.os.interface

**Pros**
- Standard ELF mechanism

**Cons**
- Less flexible
- Slightly harder tooling

---

## 10. Embedding Metadata (C Example)

```c
__attribute__((section(".os.interface"), used))
static const char os_interface_metadata[] =
"{"
"  \"interface\": \"com.example.counter\","
"  \"version\": 1,"
"  \"properties\": ["
"    {\"name\":\"value\", \"type\":\"u64\", \"access\":\"rw\", \"symbol\":\"counter_value\"}"
"  ],"
"  \"methods\": ["
"    {\"name\":\"increment\", \"symbol\":\"counter_increment\", \"args\":[\"u64\"], \"returns\":\"u64\"}"
"  ]"
"}";
```

---

## 11. Inspection Tools

```bash
readelf -S app
readelf -x .os.interface app
objdump -s -j .os.interface app
```

---

## 12. Runtime Resolution Flow

1. Load ELF  
2. Locate .os.interface section  
3. Parse metadata  
4. Resolve symbols  
5. Compute runtime addresses  
6. Build structured object  

---

## 13. Runtime API Model (OS Object)

obj = os.load("/bin/counter")

obj.properties.value.read()  
obj.properties.value.write(42)

obj.methods.increment(5)

sub = obj.events.changed.add(callback)  
obj.events.changed.remove(sub)

---

## 14. Security & Capability Model

NO unrestricted memory access

Instead:

- Require capabilities / handles  
- Enforce permissions at interface level  

---

## 15. Design Constraints & Risks

### 15.1 Stripped Symbols

Mitigation:

→ require export of interface symbols  
→ or embed symbol indices  

---

### 15.2 ASLR

Mitigation:

→ always resolve base at runtime  

---

### 15.3 Relocations

Mitigation:

→ resolve after relocation  

---

### 15.4 Local Variables

Mitigation:

→ restrict interface to globals/static  

---

## 16. Design Principle

Must describe semantics, not just structure:

type  
permissions  
ownership  
lifetime  
thread-safety  
blocking behavior  
error model  
versioning  

---

## 17. Summary

ELF = transport + symbol resolution  
.os.interface = structured interface definition  

Symbols give addresses  
Metadata gives meaning  

---

## 18. Next Steps

1. Minimal JSON schema (v1)  
2. Type system  
3. Calling convention spec  
4. Capability model  

Then implement:

→ ELF parser  
→ metadata parser  
→ runtime binder  
→ shell integration  
