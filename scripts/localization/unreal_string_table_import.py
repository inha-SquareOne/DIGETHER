"""Import a complete String Table CSV into an existing String Table asset and save it.

Run in the Unreal Editor (or headless with -run=pythonscript) with two arguments:
the /Game asset path and an absolute CSV path. The CSV must contain every key of
the table (for example the `table` output of sheet_pipeline.py), because the
engine import replaces the table's entries with the CSV rows. Back up the asset
and check its lock/owner before running.
"""

import argparse
from pathlib import Path

import unreal


def import_table(asset_path, csv_path):
    if not asset_path.startswith("/Game/"):
        raise ValueError("String Table asset path must start with /Game/")
    source = Path(csv_path)
    if not source.is_absolute() or not source.is_file():
        raise ValueError("CSV path must be an existing absolute file: " + csv_path)

    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset or not isinstance(asset, unreal.StringTable):
        raise ValueError("Asset was not found or is not a String Table: " + asset_path)
    if not unreal.DigetherStringTableEditorLibrary.import_string_table_from_csv_file(asset, str(source)):
        raise RuntimeError("String Table CSV import failed: " + csv_path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("String Table save failed: " + asset_path)
    unreal.log("Imported and saved String Table: {} <- {}".format(asset_path, source))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_path", help="String Table asset path, for example /Game/Frontend/Localization/ST_Session")
    parser.add_argument("csv_path", help="Absolute path of the complete table CSV")
    arguments = parser.parse_args()
    import_table(arguments.asset_path, arguments.csv_path)


if __name__ == "__main__":
    main()
