# Atividade: Acender LED do Esp32 via MQTT

## Membros da equipe
* *Joel Rodrigues*
* *Ian Pessoa*
* *Fagner Timoteo*
* *Enzo Albuquerque*

---

## Entendendo o código
O código implementa a lógica para controlar um LED em um ESP32 e responder a um botão nele conectado, utilizando o protocolo MQTT para comunicação. O objetivo é permitir que o pressionamento de um botão em uma placa controle o LED de outra (ou de um cliente MQTT externo), e vice-versa, replicando o estado instantaneamente.

O projeto utiliza o *ESP-IDF* e a biblioteca *ESP-MQTT*.

### Variáveis e Constantes
De início, criamos variáveis importantes que definem o comportamento do hardware e da conexão:

```
#define TAG "MQTT_LED"
#define BROKER_URI "mqtt://192.168.3.6:1883" 
#define LED_GPIO     23
#define BUTTON_GPIO  0 

static bool led_on = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;

```
* *TAG:* Constante usada para identificar os logs no terminal referentes a conexao do sistema.
* *BROKER_URI:* Endereço IP do broker + porta. É o servidor responsável por receber e distribuir as mensagens.
* *LED_GPIO e BUTTON_GPIO:* Constantes que definem os pinos físicos utilizados no ESP32.
* *led_on:* Variável de estado que controla logicamente se o LED deve estar aceso ou apagado.
* *esp_mqtt_client_handle_t mqtt_client:* Variável que gerencia a sessão e conexão do cliente MQTT.

---

### Lógica do Cliente MQTT (```mqtt_event_handler_cb```)
Esta função é o "cérebro" da comunicação. Ela opera como uma máquina de estados baseada nos eventos da rede:

1. *Conexão Estabelecida (MQTT_EVENT_CONNECTED):*
   * O ESP32 se inscreve no tópico esp32/led para receber comandos de luz.
   * O ESP32 se inscreve no tópico esp32/button para saber quando botões são pressionados na rede.

2. *Dados Recebidos (MQTT_EVENT_DATA):*
   * *Controle do LED:* Se chegar a mensagem "on" ou "off" no tópico esp32/led, o pino físico é alterado imediatamente.
   * *Monitoramento do Botão:*
     * Ao receber led_on no tópico esp32/button, o sistema registra a pendência e o timestamp.
     * Ao receber led_off no tópico esp32/button, o sistema entende que o botão foi solto e envia o comando para desligar o LED.

---

### Tasks do FreeRTOS
Para garantir que o sistema não trave esperando o botão ou a rede, utilizamos duas tarefas rodando em paralelo:

#### 1. Task de Leitura (```button_task```)
Responsável apenas por olhar o hardware.
* Lê o estado do pino (definido em BUTTON_GPIO).
* Quando detecta "pressionado", publica a mensagem led_on no tópico esp32/button.
* Quando detecta "solto", publica a mensagem led_off.

#### 2. Task de Interpretação (```button_interpreter_task```)
Responsável pela lógica temporal.
* Monitora se existe uma ação de botão pendente (button_pending_on).
* Valida o tempo decorrido e publica a mensagem definitiva on no tópico esp32/led para acender efetivamente as luzes.

---

### Função Principal (```app_main```)
É o ponto de entrada da aplicação, responsável pela inicialização de todos os componentes:
1. *Inicialização do Sistema:* Configura o NVS (necessário para o Wi-Fi), as interfaces de rede e o loop de eventos padrão.
2. *Conexão Wi-Fi:* Chama a função example_connect() que utiliza as credenciais configuradas no menuconfig para conectar à rede.
3. *Configuração de GPIO:* Define o pino do LED como saída e o do botão como entrada (ativando o resistor de pull-up interno).
4. *Início do MQTT:* Configura a URI do broker e inicia o cliente MQTT.
5. *Criação das Tasks:* Cria as tasks button_task e button_interpreter_task para rodarem simultaneamente, alocando a memória necessária para elas.

---

## Configuração do Broker MQTT (Essencial)
Para que os dois ESP32 (ou o ESP32 e o PC) conversem, é necessário configurar o endereço do servidor e liberar o acesso externo no PC:

1. *Configurar o Mosquitto (Permitir acesso externo):*
   * Vá à pasta de instalação (ex: C:\Program Files\mosquitto).
   * Crie ou edite o arquivo mosquitto.conf adicionando:
   ```
     text
     listener 1883
     allow_anonymous true
    ```
     
   * Rode o broker pelo terminal: 
   ```
   mosquitto -c mosquitto.conf -v
   ```

2. *Firewall:*
   * Abra a porta *1883* no Firewall do Windows (Entrada) ou desative-o temporariamente.

3. *Configurar o IP:*
   * No código, localize a linha 
   ```
   #define BROKER_URI.
   ```
   * Substitua pelo IPv4 do seu computador (verifique com o comando ipconfig no terminal).
   * *Atenção:* Os dispositivos devem estar na mesma rede Wi-Fi.

---

## Como Executar

### Opção 1: Com duas placas ESP32

1. *Configurar o Wi-Fi (Menuconfig):*
   * Abra o terminal do ESP-IDF e digite: 
   ```
   idf.py menuconfig
   ```
   * Navegue até a opção: *Example Connection Configuration*.
   * Em *WiFi SSID*, digite o nome da sua rede (apenas redes 2.4GHz).
   * Em *WiFi Password*, digite a senha da rede.
   * Pressione S para salvar e Esc (ou Q) para sair.

2. *Upload:*
   * Compile e faça o upload para a *Placa A*:
        ```
         idf.py -p (PORTA_USB_1) flash monitor
        ```
     
   * Faça o upload para a *Placa B* (em outra porta):
     ```
     idf.py -p (PORTA_USB_2) flash monitor
     ```
     

3. *Teste:*
   * Pressione o botão na *Placa A* e observe o LED acender na *Placa B* (e vice-versa).

### Opção 2: Com apenas uma placa + Cliente MQTT (PC ou Celular)
Caso não possua duas placas, você pode simular a segunda usando um software (ex: *MQTTX* no PC ou *MyMQTT* no celular).

1. Faça o upload do código para o seu ESP32 (configurando o Wi-Fi como explicado acima).
2. Mantenha o Mosquitto rodando no PC.
3. *Para acender o LED físico do ESP32 remotamente:*
   * Publique a mensagem on no tópico esp32/led (via terminal: mosquitto_pub -h localhost -t "esp32/led" -m "on").
   * O ESP32 receberá o comando e acenderá o LED.
4. *Para monitorar o botão físico do ESP32:*
   * Inscreva-se no tópico esp32/button (via terminal: mosquitto_sub -h localhost -t "esp32/button" -v).
   * Ao pressionar o botão físico no ESP32, você verá a mensagem chegar no PC instantaneamente.