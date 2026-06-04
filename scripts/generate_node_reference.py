import json
import os
import shutil
import sys
from mdutils.mdutils import MdUtils
from mdutils.tools import Image

# Paths for build, data, and documentation directories
BUILD_PATH = "build"
HSD_DATA_PATH = "Hesiod/data"
NODE_SNAPSHOT_PATH = "docs/images/nodes/"
NODE_MARKDOWN_PATH = "docs/node_reference/nodes"
NODE_REFERENCE_PATH = "docs/node_reference"
NODE_EXAMPLES_PATH = "docs/examples"


def generate_snapshots():
    """Generate node snapshots using the Hesiod executable."""
    print("Generating snapshots...")
    # os.system(f"cd {BUILD_PATH} ; ./bin/hesiod --inventory")


def load_node_data():
    """Load node documentation data from a JSON file."""
    fname = os.path.join(HSD_DATA_PATH, "node_documentation.json")
    with open(fname, "r") as f:
        return json.load(f)


def generate_categories_markdown(data):
    """Generate markdown documentation for node categories."""
    md_file = MdUtils(file_name=os.path.join(NODE_MARKDOWN_PATH, "categories"),
                      title="Node Categories")
    md_file.new_header(level=1, title="Categories")

    # Dictionary to store primary categories and their subcategories
    # and dictionary to store nodes grouped by category
    cat = {}
    node_per_cat = {}

    # Organizing node data into categories
    for node_type, node_data in data.items():
        category = node_data["category"]
        node_per_cat.setdefault(category, []).append(node_type)

        node_cat = category.split("/")
        cat.setdefault(node_cat[0], []).extend(node_cat[1:])

    cat_table = ["Primary", "Secondary", "Nodes"]

    # Constructing category markdown table
    for primary in sorted(cat.keys()):
        secondary_list = sorted(set(cat[primary]))

        if secondary_list:
            for secondary in secondary_list:
                node_list = sorted(
                    node_per_cat.get(f"{primary}/{secondary}", []))
                cat_table.extend([primary, secondary, ", ".join(node_list)])
        else:
            node_list = sorted(node_per_cat.get(primary, []))
            cat_table.extend([primary, "", ", ".join(node_list)])

    md_file.new_table(columns=3,
                      rows=len(cat_table) // 3,
                      text=cat_table,
                      text_align="left")
    md_file.create_md_file()


def build_nav_summary(data):
    """Build the literate-nav SUMMARY.md body, grouping nodes by category.

    Returns the markdown text; does not write to disk (kept pure for testing).
    """
    # primary -> {secondary_or_None: [node_type, ...]}
    cat_tree = {}
    for node_type, node_info in data.items():
        parts = node_info["category"].split("/")
        primary = parts[0]
        secondary = parts[1] if len(parts) > 1 else None
        cat_tree.setdefault(primary, {}).setdefault(secondary, []).append(node_type)

    lines = ["* [Categories](categories.md)"]
    for primary in sorted(cat_tree):
        lines.append("* {}".format(primary))
        sub = cat_tree[primary]
        # nodes attached directly to the primary category (no secondary)
        for node_type in sorted(sub.get(None, [])):
            lines.append("    * [{0}](nodes/{0}.md)".format(node_type))
        # then each secondary category as a nested section
        for secondary in sorted(s for s in sub if s is not None):
            lines.append("    * {}".format(secondary))
            for node_type in sorted(sub[secondary]):
                lines.append("        * [{0}](nodes/{0}.md)".format(node_type))
    return "\n".join(lines) + "\n"


def generate_nav_summary(data):
    """Write docs/node_reference/SUMMARY.md for mkdocs-literate-nav."""
    fname = os.path.join(NODE_REFERENCE_PATH, "SUMMARY.md")
    with open(fname, "w", encoding="utf-8") as f:
        f.write(build_nav_summary(data))


def generate_node_markdown(data):
    """Generate markdown documentation for all nodes."""
    img_relative_path = os.path.relpath(NODE_SNAPSHOT_PATH, NODE_MARKDOWN_PATH)
    hsd_relative_path = os.path.relpath(NODE_EXAMPLES_PATH, NODE_MARKDOWN_PATH)

    for node_type, node_info in data.items():
        # print(node_info['label'])

        md_file = MdUtils(file_name=os.path.join(NODE_MARKDOWN_PATH,
                                                 node_info['label']),
                          title=node_info['label'] + " Node")

        md_file.new_paragraph(node_info["description"])
        md_file.new_paragraph()

        img_fname = node_type + "_settings.png"
        img_path_rel = os.path.join(img_relative_path, img_fname)
        md_file.new_paragraph(
            Image.Image.new_inline_image(text="img", path=img_path_rel))
        md_file.new_paragraph()

        # Category
        md_file.new_header(level=1, title="Category")
        md_file.new_paragraph(node_info["category"])

        # Generate tables for inputs, outputs, and parameters
        generate_ports_table(md_file, node_info, "input", "Inputs")
        generate_ports_table(md_file, node_info, "output", "Outputs")
        generate_parameters_table(md_file, node_info)

        # Handle node snapshot images
        md_file.new_header(level=1, title="Example")

        img_fname = node_type + "_hsd_example.png"

        if (os.path.isfile(os.path.join(NODE_SNAPSHOT_PATH, img_fname))):
            img_path_rel = os.path.join(img_relative_path, img_fname)
            md_file.new_paragraph(
                Image.Image.new_inline_image(text="img", path=img_path_rel))

            hsd_fname = node_type + '.hsd'
            md_file.new_paragraph(
                'Corresponding Hesiod file: [{}]({}). Use [Ctrl+I] in the node editor to import a hsd file within your current project. \n\n> **Note:** Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).\n> If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).'
                .format(hsd_fname, os.path.join(hsd_relative_path, hsd_fname)))
            md_file.new_paragraph()
        else:
            md_file.new_paragraph('No example available.')

        # add manual-input existing content
        fname = os.path.join(NODE_MARKDOWN_PATH, node_type + '.in.md')
        if os.path.exists(fname):
            with open(fname, 'r', encoding='utf-8') as f:
                existing_content = f.read()
            md_file.new_line(existing_content)
        else:
            # write an empty one if not existing
            with open(fname, 'w') as f:
                pass

        md_file.create_md_file()


def generate_ports_table(md_file, node_info, port_type, title):
    """Generate markdown table for input or output ports."""
    table = ["Name", "Type", "Description"]
    entries = [[p["caption"], p["data_type"], p["description"]]
               for p in node_info["ports"].values() if p["type"] == port_type]

    if entries:
        for entry in entries:
            table.extend(entry)

        md_file.new_header(level=1, title=title)
        md_file.new_table(columns=3,
                          rows=len(entries) + 1,
                          text=table,
                          text_align="left")


def generate_parameters_table(md_file, node_info):
    """Generate markdown table for node parameters."""
    table = ["Name", "Type", "Description"]
    parameters = node_info.get("parameters", {})

    if parameters is None:
        return

    entries = [[p["label"], p["type"], p["description"]]
               for p in parameters.values()]

    if entries:
        for entry in entries:
            table.extend(entry)

        md_file.new_header(level=1, title="Parameters")
        md_file.new_table(columns=3,
                          rows=len(entries) + 1,
                          text=table,
                          text_align="left")


def main():
    """Main function to generate markdown documentation."""
    generate_snapshots()  # Generate node snapshots

    print("Updating markdown documentation...")

    node_data = load_node_data()

    generate_nav_summary(node_data)
    generate_node_markdown(node_data)

    generate_categories_markdown(node_data)


if __name__ == "__main__":
    main()
