# Comunicação Bidirecional MQTT com ESP32

Este projeto implementa um sistema de comunicação **Full-Duplex** entre dois dispositivos ESP32. O objetivo é o espelhamento de estado: um botão pressionado na **Placa A** acende o LED na **Placa B**, e vice-versa.

## 👥 Membros da Equipe
* **Joel Rodrigues**
* **Ian Pessoa**
* **Fagner Timoteo**
* **Enzo Albuquerque**

---

## 🔗 Repositório Complementar

⚠️ **ATENÇÃO:** Este repositório contém o código para a **Placa A**.

Para o sistema funcionar completamente com dois ESP32, você precisa gravar o código complementar na segunda placa.
* **Acesse o repositório da Placa B aqui:** https://github.com/enzo-gois/Acender-LED-do-ESP32-via-MQTT-placa-B

---

## 🧠 Entendendo o Código

O código implementa a lógica para controlar um LED em um ESP32 e responder a um botão nele conectado, utilizando o protocolo MQTT para comunicação.
O projeto utiliza o **ESP-IDF** e a biblioteca **ESP-MQTT**.

### Variáveis e Constantes
De início, criamos variáveis importantes que definem o comportamento do hardware e da conexão:

```c
#define TAG "MQTT_LED"
#define BROKER_URI "mqtt://192.168.3.6:1883" 
#define LED_GPIO     23
#define BUTTON_GPIO  22 

static esp_mqtt_client_handle_t mqtt_client = NULL;
```

* **TAG:** Constante usada para identificar os logs no terminal referentes à conexão do sistema.
* **BROKER_URI:** Endereço IP do broker + porta. É o servidor responsável por receber e distribuir as mensagens.
* **LED_GPIO e BUTTON_GPIO:** Constantes que definem os pinos físicos utilizados no ESP32.
* **mqtt_client:** Variável que gerencia a sessão e conexão do cliente MQTT.

---

### Lógica do Cliente MQTT (`mqtt_event_handler_cb`)
Esta função é o "cérebro" da recepção de dados. Ela opera como uma máquina de estados baseada nos eventos da rede:

1. **Conexão Estabelecida (`MQTT_EVENT_CONNECTED`):**
   * O ESP32 se inscreve no tópico da **outra placa** (ex: `esp32/tp1`) para escutar comandos.

2. **Dados Recebidos (`MQTT_EVENT_DATA`):**
   * O sistema verifica se a mensagem chegou no tópico esperado.
   * **Controle do LED:**
     * Se chegar a mensagem **"off"**: O código entende que o botão da outra placa foi pressionado (lógica pull-up) e **Acende o LED** (`gpio_set_level 1`).
     * Se chegar a mensagem **"on"**: O código entende que o botão foi solto e **Apaga o LED** (`gpio_set_level 0`).

---

### Tasks do FreeRTOS
Para garantir que o sistema não trave esperando o botão ou a rede, utilizamos uma tarefa dedicada rodando em paralelo:

#### Task de Leitura (`button_task`)
Responsável apenas por monitorar o hardware (Botão Local).
1. Lê continuamente o estado do pino `BUTTON_GPIO`.
2. Detecta mudanças de estado (se estava solto e foi apertado, ou vice-versa).
3. **Publicação:** Assim que o estado muda, ela publica imediatamente a mensagem ("on" ou "off") no tópico de saída desta placa (ex: `esp32/tp2`), avisando a rede que houve uma ação.

---

### Função Principal (`app_main`)
É o ponto de entrada da aplicação, responsável pela inicialização de todos os componentes:

1. **Inicialização do Sistema:** Configura o NVS (necessário para o Wi-Fi), as interfaces de rede e o loop de eventos padrão.
2. **Conexão Wi-Fi:** Chama a função `example_connect()` que utiliza as credenciais configuradas no `menuconfig` para conectar à rede.
3. **Configuração de GPIO:** Define o pino do LED como saída e o do botão como entrada (ativando o resistor de pull-up interno).
4. **Início do MQTT:** Configura a URI do broker e inicia o cliente MQTT.
5. **Criação das Tasks:** Cria a task `button_task` para rodar simultaneamente com a comunicação Wi-Fi.

---

## ⚙️ Configuração do Ambiente (Essencial)

Para que os dois ESP32 conversem, é necessário configurar o endereço do servidor e liberar o acesso externo no PC:

### 1. Configurar o Mosquitto (Permitir acesso externo)
1. Vá à pasta de instalação (ex: `C:\Program Files\mosquitto`).
2. Crie ou edite o arquivo `mosquitto.conf` adicionando:
   ```text
   listener 1883
   allow_anonymous true
   ```
3. Rode o broker pelo terminal (Admin):
   ```cmd
   mosquitto -c mosquitto.conf -v
   ```

### 2. Firewall e IP
1. **Firewall:** Abra a porta **1883** no Firewall do Windows (Entrada) ou desative-o temporariamente.
2. **Configurar o IP:** No código `main.c`, localize a linha `#define BROKER_URI` e substitua pelo IPv4 do seu computador.

---

## 🚀 Como Executar

### Passo 1: Gravar a Placa A (Este Repositório)
1. Configure o Wi-Fi: `idf.py menuconfig` -> *Example Connection Configuration*.
2. Compile e grave:
   ```bash
   idf.py -p (PORTA_USB) flash monitor
   ```

### Passo 2: Gravar a Placa B (Outro Repositório)
1. Baixe o código do repositório complementar (link no topo).
2. Configure o Wi-Fi e o IP nele também.
3. Grave na segunda placa.

---

## 🧪 Teste Prático (Com Duas Placas)

Com ambos os códigos gravados e as placas ligadas (alimentadas via USB):

1. **Verificação Inicial:**
   * Certifique-se de que ambas as placas conectaram ao Wi-Fi e ao Broker MQTT (o LED da placa pode piscar ou você pode verificar via monitor serial se aparece `MQTT_EVENT_CONNECTED`).

2. **Teste A -> B:**
   * Pressione o botão na **Placa A**.
   * O LED na **Placa B** deve acender **instantaneamente**.
   * Solte o botão na **Placa A**. O LED na **Placa B** deve apagar.

3. **Teste B -> A:**
   * Pressione o botão na **Placa B**.
   * O LED na **Placa A** deve acender **instantaneamente**.

---

## 💻 Simulação (Caso tenha apenas 1 Placa)

Se você tiver apenas este código gravado em uma placa física, use o PC para simular a segunda placa:

1. **Para ver o botão desta placa:**
   ```cmd
   mosquitto_sub -h localhost -t "esp32/tp2" -v
   ```
   *Ao apertar o botão na placa, a mensagem aparece aqui.*

2. **Para acender o LED desta placa:**
   ```cmd
   mosquitto_pub -h localhost -t "esp32/tp1" -m "off"
   ```
   *O LED da placa deve acender.*