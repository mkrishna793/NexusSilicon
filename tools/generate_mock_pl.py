import os

nodes_path = "D:/MFE Engine/benchmarks/ibm01.nodes"
pl_path = "D:/MFE Engine/benchmarks/ibm01.pl"

if not os.path.exists(nodes_path):
    print(f"Error: {nodes_path} does not exist.")
    exit(1)

print(f"[*] Reading nodes from {nodes_path}...")
cells = []

with open(nodes_path, "r") as f:
    parsing_cells = False
    for line in f:
        # Strip comments and whitespaces
        line = line.split("#")[0].strip()
        if not line:
            continue
        
        # Check headers
        if any(h in line for h in ["UCLA", "NumNodes", "NumTerminals", ":"]):
            if "NumTerminals" in line:
                parsing_cells = True
            continue
            
        if parsing_cells:
            tokens = line.split()
            if len(tokens) >= 3:
                cell_name = tokens[0]
                is_terminal = (len(tokens) >= 4 and "terminal" in tokens[3])
                cells.append((cell_name, is_terminal))

print(f"[+] Found {len(cells)} cells in nodes file.")

print(f"[*] Writing mock pl file to {pl_path}...")
with open(pl_path, "w") as out:
    out.write("UCLA pl 1.0\n")
    for cell_name, is_terminal in cells:
        fixed_flag = " FIXED" if is_terminal else ""
        out.write(f"{cell_name} 0.0 0.0 : N{fixed_flag}\n")

print("[+] pl file generated successfully!")
