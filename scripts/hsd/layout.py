COL_SPACING = 320.0
ROW_SPACING = 200.0


def layout_positions(node_ids, links):
    """Assign (x, y) scene positions by longest-path depth from roots.

    `links` is a list of (from_id, to_id). Returns {node_id: (x, y)}.
    Deterministic: depth sets the column; insertion order sets the row.
    """
    succ = {n: [] for n in node_ids}
    indeg = {n: 0 for n in node_ids}
    for a, b in links:
        if a in succ and b in indeg:
            succ[a].append(b)
            indeg[b] += 1

    # longest-path depth via repeated relaxation (graphs are small/acyclic)
    depth = {n: 0 for n in node_ids}
    for _ in range(len(node_ids)):
        changed = False
        for a, b in links:
            if a in depth and b in depth and depth[b] < depth[a] + 1:
                depth[b] = depth[a] + 1
                changed = True
        if not changed:
            break

    rows = {}
    pos = {}
    for n in node_ids:                      # stable: spec/insertion order
        col = depth[n]
        row = rows.get(col, 0)
        rows[col] = row + 1
        pos[n] = (col * COL_SPACING, row * ROW_SPACING)
    return pos
