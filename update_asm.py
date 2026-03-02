with open("ASM_TUTORIAL.md", "r") as f:
    lines = f.readlines()

new_content = """
### Conditional Assembly (`_if` / `_else` / `_endif`)
Allows compiling instructions conditionally based on static values. Supports comparison operators.

```lua
asm(
    _if 1 == 1
        _print "Condition is true"
    _else
        _print "Condition is false"
    _endif
)
```

### `junk`
Inserts garbage data to thwart disassembly. Can be a string, which is encoded as an `EXTRAARG` instruction sequence. Can also be an integer, which generates the specified number of `NOP` instructions.
```lua
asm(
    junk "some_random_string_data"
    junk 5
)
```

"""

for i, line in enumerate(lines):
    if "### `jmpx @label` / `JMP @label`" in line:
        lines.insert(i, new_content)
        break

with open("ASM_TUTORIAL.md", "w") as f:
    f.writelines(lines)
