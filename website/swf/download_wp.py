import os
import requests
from urllib.parse import urlparse

# Prefixo original que será pesquisado no Wayback
TARGET_URL = "http://sonic.sega.jp/SonicWorldAdventure/download/*"

# Pasta de destino
PASTA_DESTINO = "download/download"

# API CDX do Wayback Machine
CDX_URL = "https://web.archive.org/cdx/search/cdx"

os.makedirs(PASTA_DESTINO, exist_ok=True)

print("Consultando o Wayback Machine...")

params = {
    "url": TARGET_URL,
    "output": "json",
    "fl": "original,statuscode,mimetype",
    "filter": "statuscode:200",
    "collapse": "urlkey",
}

try:

    resposta = requests.get(
        CDX_URL,
        params=params,
        timeout=60
    )

    resposta.raise_for_status()

    dados = resposta.json()

except Exception as erro:

    print(f"Erro ao consultar o Wayback Machine: {erro}")
    exit()


# Remove o cabeçalho
if dados and dados[0][0] == "original":
    dados = dados[1:]


# Lista de URLs FLV
urls = []

for item in dados:

    if not item:
        continue

    url = item[0]

    if url not in urls:
        urls.append(url)


print(f"\nArquivos encontrados: {len(urls)}")


# Baixa diretamente das URLs originais
for url_original in urls:

    nome_arquivo = os.path.basename(
        urlparse(url_original).path
    )

    if not nome_arquivo:
        continue

    caminho = os.path.join(
        PASTA_DESTINO,
        nome_arquivo
    )

    print(f"\nArquivo: {nome_arquivo}")
    print(f"URL: {url_original}")

    if os.path.exists(caminho):

        print("Já existe. Pulando.")
        continue

    try:

        resposta_download = requests.get(
            url_original,
            stream=True,
            timeout=120
        )

        if resposta_download.status_code != 200:

            print(
                f"Erro HTTP: "
                f"{resposta_download.status_code}"
            )

            continue

        with open(caminho, "wb") as arquivo:

            for bloco in resposta_download.iter_content(
                chunk_size=1024 * 1024
            ):

                if bloco:
                    arquivo.write(bloco)

        tamanho_mb = os.path.getsize(caminho) / (
            1024 * 1024
        )

        print(
            f"OK: {nome_arquivo} "
            f"({tamanho_mb:.2f} MB)"
        )

    except requests.RequestException as erro:

        print(
            f"Erro ao baixar {nome_arquivo}: {erro}"
        )


print("\nDownload concluído.")