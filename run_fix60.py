with open("output_test_bonus_small7.log", "r") as f:
    lines = f.readlines()
for i, l in enumerate(lines):
    if "return mp" in l:
        print(f"L{i-2}: {lines[i-2].strip()}")
        print(f"L{i-1}: {lines[i-1].strip()}")
        print(f"L{i}: {lines[i].strip()}")
        print(f"L{i+1}: {lines[i+1].strip()}")
        print(f"L{i+2}: {lines[i+2].strip()}")
        break
