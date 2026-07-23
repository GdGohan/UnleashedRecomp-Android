import os
import subprocess
import imageio_ffmpeg


def converter_para_flash32_perfeito(diretorio_entrada, diretorio_saida=None):
  if not diretorio_saida:
    diretorio_saida = diretorio_entrada

  os.makedirs(diretorio_saida, exist_ok=True)
  ffmpeg_exe = imageio_ffmpeg.get_ffmpeg_exe()

  arquivos_flv = [
      f for f in os.listdir(diretorio_entrada) if f.lower().endswith(".flv")
  ]

  if not arquivos_flv:
    print("Nenhum arquivo .flv encontrado na pasta.")
    return

  print(
      f"Encontrados {len(arquivos_flv)} arquivo(s). Ajustando para o Flash"
      " Player 32..."
  )

  for arquivo in arquivos_flv:
    caminho_entrada = os.path.join(diretorio_entrada, arquivo)
    nome_base = os.path.splitext(arquivo)[0]
    caminho_saida = os.path.join(diretorio_saida, f"{nome_base}.flv")

    comando = [
        ffmpeg_exe,
        "-i",
        caminho_entrada,
        "-c:v",
        "libx264",
        "-profile:v",
        "baseline",
        "-level",
        "3.0",
        "-pix_fmt",
        "yuv420p",
        "-c:a",
        "aac",
        "-ar",
        "44100",
        "-b:a",
        "128k",
        # Força o formato estrito de FLV legado e corrige o atraso de áudio/vídeo
        "-fflags",
        "+genpts",
        "-async",
        "1",
        "-f",
        "flv",
        caminho_saida,
        "-y",
    ]

    print(f"Processando: {arquivo} -> {os.path.basename(caminho_saida)}")

    try:
      subprocess.run(comando, check=True)
      print(f" [SUCESSO] {arquivo}\n")
    except subprocess.CalledProcessError as e:
      print(f" [ERRO] Falha ao processar {arquivo}\n")


converter_para_flash32_perfeito("./movies", "./movies_conv")