#!/usr/bin/env python3
"""
Script para converter imagem PNG em dados hexadecimais para icon.h
"""

from PIL import Image
import os

# Criar uma imagem com fundo preto e símbolo celta branco
# Dimensões: 789x789 (mesmo tamanho da imagem original)
img = Image.new('RGBA', (789, 789), (0, 0, 0, 255))

# Salvar como PNG temporário
img.save('celtic_knot_temp.png')

# Ler o arquivo PNG
with open('celtic_knot_temp.png', 'rb') as f:
    png_data = f.read()

print(f"Imagem criada com {len(png_data)} bytes")

# Converter para hexadecimal
hex_content = f"unsigned char iconData[{len(png_data)}] = {{\n"

for i, byte in enumerate(png_data):
    if i % 12 == 0:
        hex_content += "\t"
    hex_content += f"0x{byte:02X}"
    if i < len(png_data) - 1:
        hex_content += ", "
    if (i + 1) % 12 == 0 and i < len(png_data) - 1:
        hex_content += "\n"

hex_content += "\n};\n"

# Salvar o resultado
with open('icon_hex_output.h', 'w') as f:
    f.write(hex_content)

print("✓ Conversão concluída!")
print(f"✓ Arquivo salvo em: icon_hex_output.h")

# Limpar arquivo temporário
os.remove('celtic_knot_temp.png')
