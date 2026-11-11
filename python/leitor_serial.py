import serial
import csv
import sys

# ================= CONFIGURAÇÕES =================
# Windows: 'COM6' (veja no Gerenciador de Dispositivos)
# Mac:     '/dev/cu.usbmodemXXXX' (veja com: ls /dev/cu.usbmodem*)
# Também dá para passar a porta na linha de comando:
#   python leitor_serial.py /dev/cu.usbmodem1102
PORTA_SERIAL = 'COM6'
BAUD_RATE = 115200
ARQUIVO_CSV = 'dados_cinematicos.csv'
# =================================================

COLUNAS = ['ax', 'ay', 'az', 'vx', 'vy', 'vz', 'px', 'py', 'pz']


def linha_valida(dados):
    """Aceita só linhas com 9 números (descarta cabeçalho, mensagens '#' e lixo)."""
    if len(dados) != len(COLUNAS):
        return False
    try:
        [float(d) for d in dados]
        return True
    except ValueError:
        return False


def iniciar_leitura(porta):
    conexao = None
    try:
        conexao = serial.Serial(porta, BAUD_RATE, timeout=1)
        print(f"Conectado na porta {porta} a {BAUD_RATE} bps.")
        print(f"Gravando dados em '{ARQUIVO_CSV}'. Pressione Ctrl+C para parar.")

        with open(ARQUIVO_CSV, mode='w', newline='') as arquivo:
            escritor_csv = csv.writer(arquivo)
            escritor_csv.writerow(COLUNAS)   # cabeçalho fixo, mesmo se a placa já estava rodando
            n = 0

            while True:
                linha = conexao.readline().decode('utf-8', errors='ignore').strip()
                if not linha:
                    continue

                if linha.startswith('#'):        # mensagens da placa (calibração etc.)
                    print(linha)
                    continue

                dados = linha.split(',')
                if linha_valida(dados):
                    escritor_csv.writerow(dados)
                    arquivo.flush()
                    n += 1
                    print(f"[{n}] a=({dados[0]}, {dados[1]}, {dados[2]})  "
                          f"v=({dados[3]}, {dados[4]}, {dados[5]})  "
                          f"p=({dados[6]}, {dados[7]}, {dados[8]})")

    except serial.SerialException as e:
        print(f"Erro ao abrir a porta serial: {e}")
        print("Verifique se a placa está conectada e se a porta está correta.")
    except KeyboardInterrupt:
        print("\nLeitura encerrada pelo usuário.")
    finally:
        if conexao is not None and conexao.is_open:
            conexao.close()
            print("Porta serial fechada.")


if __name__ == "__main__":
    porta = sys.argv[1] if len(sys.argv) > 1 else PORTA_SERIAL
    iniciar_leitura(porta)
