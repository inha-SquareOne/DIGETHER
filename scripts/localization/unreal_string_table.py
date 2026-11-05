"""Export an existing String Table from the Unreal Editor, without modifying it.

Run from the Unreal Python console with two arguments: the /Game asset path and
an absolute output CSV path. The destination must not already exist. Import and
asset saving are intentionally not performed by this script.
"""

import argparse
import os
from pathlib import Path
import shutil
import tempfile

import unreal


def export_table(asset_path, destination):
    if not asset_path.startswith("/Game/"):
        raise ValueError("String Table asset path must start with /Game/")
    output = Path(destination)
    if not output.is_absolute():
        raise ValueError("CSV destination must be an absolute path")
    if not output.parent.is_dir():
        raise ValueError("CSV destination directory does not exist")

    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset or not isinstance(asset, unreal.StringTable):
        raise ValueError("Asset was not found or is not a String Table: " + asset_path)
    library = unreal.StringTableLibrary
    table_id = library.get_table_id(asset)
    if not library.is_registered_table_id(table_id):
        raise RuntimeError("Loaded String Table was not registered: " + asset_path)

    # The engine exporter accepts a filename and may overwrite it. Export first
    # to our own unique file, then use exclusive creation for the requested path.
    temporary_path = None
    output_created = False
    try:
        with tempfile.NamedTemporaryFile(prefix="ue_string_table_", suffix=".csv", delete=False) as temporary:
            temporary_path = temporary.name
        if not library.export_table_to_csv_file(table_id, temporary_path):
            raise RuntimeError("Unreal String Table CSV export failed")
        with open(temporary_path, "rb") as source, open(output, "xb") as target:
            output_created = True
            shutil.copyfileobj(source, target)
        output_created = False
    finally:
        if temporary_path and os.path.exists(temporary_path):
            os.unlink(temporary_path)
        if output_created:
            os.unlink(output)

    namespace = library.get_table_namespace(table_id)
    unreal.log("Exported String Table: {} -> {}".format(asset_path, output))
    unreal.log("Table ID: {}; namespace: {}".format(table_id, namespace))
    unreal.log("CSV does not contain the namespace; use the actual exported PO identity for translations.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_path", help="String Table asset path, for example /Game/Frontend/Localization/ST_Session")
    parser.add_argument("output_csv", help="New absolute output CSV path")
    arguments = parser.parse_args()
    export_table(arguments.asset_path, arguments.output_csv)


if __name__ == "__main__":
    main()
