# Radioberry VFO Deck

O **Radioberry VFO Deck** é um display auxiliar dedicado à sintonia e controle de rádios SDR baseados em Radioberry, utilizando um microcontrolador **Raspberry Pi Pico** com display, encoders e botões físicos.

<p align="center">
  <img src="vfodeck.jpg" width="1024">
</p>

A comunicação com o software de rádio é feita via **USB MIDI**, permitindo controle e visualização de estado em **piHPSDR**, **DeskHPSDR** e outros softwares compatíveis, com integração através de **TCI → MIDI**.

---

## Objetivo

- Fornecer uma interface física dedicada para sintonia (VFO)
- Exibir a frequência atual em destaque
- Controlar funções do rádio usando encoders e botões
- Reduzir dependência de mouse e teclado
- Operar de forma independente da interface gráfica principal

---

## Arquitetura Geral



Radioberry
↑
piHPSDR / DeskHPSDR (TCI)
↑
Bridge no PC (TCI → MIDI)
↑
Raspberry Pi Pico (USB MIDI)
↑
Display + Encoders + Botões


- O estado do rádio é obtido via **TCI**
- A Pico não acessa TCI diretamente
- Um bridge no PC converte eventos TCI em mensagens MIDI

---

## Tela 0 – Sintonia

A tela principal do sistema é dedicada à sintonia.

### Informações exibidas
- Frequência atual (Hz)
- VFO ativo (A / B)
- Modo de operação
- STEP de sintonia
- RIT / XIT
- Estado de TX / RX
- Indicadores de bloqueio e split
- (Opcional) nível de sinal

### Controles na tela
- BAND − / BAND +
- MODE
- FILTER
- AGC
- RIT / XIT
- SPLIT
- A/B
- LOCK
- MENU

---

## Entradas Físicas

### Encoders
- **Encoder 1**: Sintonia do VFO  
  - Push: alterna VFO A/B
- **Encoder 2**: STEP de sintonia
- **Encoder 3**: Filtro
- **Encoder 4**: Navegação / Menu
- **Encoder 5**: Função configurável (AF / RF / SQL)

### Botões
- MODE
- BAND +
- BAND −
- A/B
- STEP
- LOCK
- PTT (opcional)

---

## Comunicação MIDI

### Pico → PC (MIDI OUT)
- Encoders: Control Change (CC)
- Botões: Note ou CC

### PC → Pico (MIDI IN)
- SysEx para atualização de estado:
  - Frequência
  - VFO ativo
  - Modo
  - STEP
  - PTT
  - Estados auxiliares

### Formato SysEx (proposto)


F0 7D <CMD> <DATA...> F7


---

## Estado do Projeto

- [x] Estrutura base do firmware
- [x] Leitura de encoders e botões
- [x] Navegação por páginas
- [ ] Implementação da Tela 0 (Sintonia)
- [ ] Recepção MIDI (SysEx)
- [ ] Bridge TCI → MIDI
- [ ] Integração completa com piHPSDR / DeskHPSDR

---

## Requisitos

- Raspberry Pi Pico
- Display gráfico compatível
- Encoders rotativos com push
- Botões físicos
- PC rodando piHPSDR ou DeskHPSDR
- Bridge TCI → MIDI no PC
