"""메인 메뉴·대기실 에셋을 만든다(에디터 Python).

실행:
  UnrealEditor-Cmd.exe <프로젝트>.uproject -run=pythonscript -script=<이 파일> -unattended -nosplash -nop4

- SourceArt/Frontend/MainMenuBackground.png -> /Game/Frontend/Textures/T_FrontendBackground (UI 텍스처, 밉맵 없음)
- SourceArt/Frontend/UI/*.png -> /Game/Frontend/UI/Textures/ (버튼·패널 9-slice 텍스처, scripts/art/make_ui_placeholders.py가 자리표시를 만든다)
- /Game/Frontend/Maps/L_MainMenu, L_Lobby: 빈 맵. GameMode는 DefaultEngine.ini의 GameModeMapPrefixes가 정한다.
이미 있는 맵은 건드리지 않는다. 텍스처는 원본에서 다시 가져온다.
"""

import os

import unreal

TEXTURE_DIR = "/Game/Frontend/Textures"
TEXTURE_NAME = "T_FrontendBackground"
UI_TEXTURE_DIR = "/Game/Frontend/UI/Textures"
MAP_DIR = "/Game/Frontend/Maps"
MAP_NAMES = ("L_MainMenu", "L_Lobby")


def import_ui_texture(source: str, destination_dir: str, name: str) -> None:
    """UI용 텍스처로 가져온다: UI LOD 그룹, 밉맵 없음, 압축 없음, 스트리밍 안 함."""
    if not os.path.isfile(source):
        raise RuntimeError(f"원본이 없습니다: {source}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", destination_dir)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.load_asset(f"{destination_dir}/{name}")
    if texture is None:
        raise RuntimeError(f"텍스처를 가져오지 못했습니다: {name}")
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
        raise RuntimeError(f"텍스처를 저장하지 못했습니다: {name}")
    unreal.log(f"[Frontend] texture saved: {destination_dir}/{name}")


def import_textures(project_dir: str) -> None:
    source_dir = os.path.join(project_dir, "SourceArt", "Frontend")
    import_ui_texture(os.path.join(source_dir, "MainMenuBackground.png"), TEXTURE_DIR, TEXTURE_NAME)
    ui_dir = os.path.join(source_dir, "UI")
    for file_name in sorted(os.listdir(ui_dir)):
        if file_name.lower().endswith(".png"):
            import_ui_texture(os.path.join(ui_dir, file_name), UI_TEXTURE_DIR, os.path.splitext(file_name)[0])


def create_maps() -> None:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name in MAP_NAMES:
        path = f"{MAP_DIR}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"[Frontend] map exists, skipped: {path}")
            continue
        world = tools.create_asset(name, MAP_DIR, unreal.World, unreal.WorldFactory())
        if world is None or not unreal.EditorAssetLibrary.save_loaded_asset(world, False):
            raise RuntimeError(f"맵을 만들지 못했습니다: {path}")
        unreal.log(f"[Frontend] map created: {path}")


def main() -> None:
    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    import_textures(project_dir)
    create_maps()
    unreal.log("[Frontend] done")


main()
