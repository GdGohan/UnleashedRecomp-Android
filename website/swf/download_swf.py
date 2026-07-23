import os
import requests
from urllib.parse import urlparse

BASE_URL = "https://sonic.sega.jp/SonicWorldAdventure/"

SWF_FILES = [
    "contents/objects/version.swf",
    "contents/scripts/swfobject/expressinstall.swf",

    "day.swf",
    "night.swf",
    "preloader.swf",
    "SWA2.swf",
    "SWA_sound.swf",

    "swf/action.swf",
    "swf/action_ps3.swf",
    "swf/action_wii.swf",
    "swf/character.swf",
    "swf/character_01.swf",
    "swf/character_02.swf",
    "swf/character_03.swf",
    "swf/character_04.swf",
    "swf/character_05.swf",
    "swf/character_06.swf",
    "swf/character_07.swf",
    "swf/movie.swf",
    "swf/screen.swf",
    "swf/sound.swf",
    "swf/story.swf",
    "swf/SWA2.swf",
    "swf/wallpaper.swf",
    "swf/world.swf",
    "swf/world_ps3.swf",
    "swf/world_ps3_apotoshd.swf",
    "swf/world_ps3_apotoshd_day.swf",
    "swf/world_ps3_apotoshd_night.swf",
    "swf/world_ps3_chunnan.swf",
    "swf/world_ps3_chunnan_day.swf",
    "swf/world_ps3_chunnan_night.swf",
    "swf/world_wii.swf",
    "swf/world_wii_apotos.swf",
    "swf/world_wii_spagonia.swf",
]


def baixar_arquivo(caminho):
    url = BASE_URL + caminho
    destino = os.path.join("download", caminho)

    pasta = os.path.dirname(destino)
    os.makedirs(pasta, exist_ok=True)

    print(f"Baixando: {url}")

    try:
        resposta = requests.get(
            url,
            timeout=60,
            headers={
                "User-Agent": "Mozilla/5.0"
            }
        )

        if resposta.status_code == 200:
            with open(destino, "wb") as arquivo:
                arquivo.write(resposta.content)

            print(f"OK: {destino}")
            return True

        print(f"ERRO HTTP {resposta.status_code}: {url}")
        return False

    except requests.RequestException as erro:
        print(f"ERRO: {erro}")
        return False


def main():
    total = len(SWF_FILES)
    sucesso = 0

    for i, arquivo in enumerate(SWF_FILES, start=1):
        print(f"\n[{i}/{total}]")
        if baixar_arquivo(arquivo):
            sucesso += 1

    print("\n==============================")
    print(f"Concluído: {sucesso}/{total} arquivos baixados")
    print("==============================")


if __name__ == "__main__":
    main()