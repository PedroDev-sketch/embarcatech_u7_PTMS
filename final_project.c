#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/time.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include <string.h>

//Os botões A e B
#define btna 5
#define btnb 6

//Os pinos responsáveis pela saída do LED
#define pin_vermelho 13
#define pin_azul 12
#define pin_verde 11

//Pino do buzzer
#define pin_buzzer 21

//Os eixos x e y estão invertidos, ou seja, o x será vertical e o y horizontal
#define vrx 26
#define vry 27
#define sw 22

//Os pinos responsáveis pelo I2C
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

uint atual;

/*
    Pinos usados no projeto:
    5, 6, 14, 15, 22, 26, 27
*/

#define MAX_MESSAGE_LENGTH 16

typedef struct {
    int time_seconds; //Tempo de lembrete em segundos
    char message[MAX_MESSAGE_LENGTH]; //Mensagem do Lembrete
} Reminder;

Reminder current_reminder = {0, ""}; //Lembrete atual
bool reminder_active = false;
alarm_id_t reminder_alarm_id = 0;
absolute_time_t buzzer_end_time;

//Especificações em relação à lista de tarefas
#define MAX_NUM_TASKS 10
#define MAX_CHARS_PER_LINE 16
#define MAX_LINES 5

//O modelo de uma tarefa
typedef struct 
{
    char name[80];
    bool completed;
} Task;

//Vetor de tasks, com, no máximo, 10 tasks
Task tasks[MAX_NUM_TASKS];
int task_count = 0;

#define FLASH_MAGIC_NUMBER 0xDEADBEEF
#define FLASH_TARGET_OFFSET (1024*1024)

typedef struct {
    uint32_t magic_number; //número mágico que indica inicialização
    Task tasks[MAX_NUM_TASKS]; //Lista de tarefas
    int task_count;        //Número de tarefas
} FlashData;

//Especificações para o funcionamento da digitação em código de morse
#define ponto_morse 300
#define barra_morse 700
#define timeout_morse 2000
#define timeout_palavra_morse 5000

//Tabela de conversão de código morse para caractere
const char *morse_table[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", // A-I
    ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", //J-R
    "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.." // S-Z
};

//Função que mapeia um código morse para caractere
char morse_to_char(const char *morse) 
{
    for (int i = 0; i < 26; i++) {
        if (strcmp(morse, morse_table[i]) == 0) {
            return 'A' + i;
        }
    }
    return '?'; // Código Morse desconhecido
}

//Início da sequência de funções responsáveis pelo menu principal (Tarefas, Pomodoro e Lembretes, respectivamente)
uint main_menu_tarefas(struct render_area *frame_area)
{
    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "   Tarefas   ",
        "      Pomodoro   ",
        "      Lembretes   "};

    //Renderiza no display OLED o texto desejado
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 1;
}

uint main_menu_pomodoro(struct render_area *frame_area)
{
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    char *text[] = {
        "      Tarefas   ",
        "   Pomodoro   ",
        "      Lembretes   "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 2;
}

uint main_menu_lembretes(struct render_area *frame_area)
{
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    char *text[] = {
        "      Tarefas   ",
        "      Pomodoro   ",
        "   Lembretes   "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 3;
}
//Fim da sequência


//Início da sequência de funções responsáveis pelo menu de tarefas (Lista de Tarefas, Adicionar Tarefas, Limpar Lista, respectivamente)
uint menu_de_tarefas_1(struct render_area *frame_area)
{
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    char *text[] = {
        "   Tarefas:   ",
        "   Lista   ",
        "     Adicionar   ",
        "     Limpar   "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 1;
}

uint menu_de_tarefas_2(struct render_area *frame_area)
{
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    char *text[] = {
        "   Tarefas:   ",
        "     Lista   ",
        "   Adicionar   ",
        "     Limpar   "};
    
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 2;
}

uint menu_de_tarefas_3(struct render_area *frame_area)
{
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    char *text[] = {
        "   Tarefas:   ",
        "     Lista   ",
        "     Adicionar   ",
        "   Limpar   "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 3;
}
//Fim da sequência

//Função para quebrar o texto, caso ele ultrapasse 16 caracteres
void wrap_text(const char *input, char output[][17], int *num_lines) 
{
    int input_len = strlen(input);
    int line_start = 0;
    int line_end = 0;
    *num_lines = 0;

    while (line_start < input_len && *num_lines < 5) {
        // Encontra o último espaço dentro do limite da linha (16 char)
        line_end = line_start + 16;
        if (line_end > input_len) {
            line_end = input_len;
        } else {
            while (line_end > line_start && input[line_end] != ' ') {
                line_end--;
            }
            if (line_end == line_start) {
                line_end = line_start + 16; // Quebra a linha à força caso nenhum espaço seja encontrado
            }
        }

        // Copia a linha pra saída
        strncpy(output[*num_lines], &input[line_start], line_end - line_start);
        output[*num_lines][line_end - line_start] = '\0'; // Término nulo para a linha

        (*num_lines)++;
        line_start = line_end + 1; // Segue para a próxima linha
    }
}

//Função para renderizar no OLED o texto formatado
void render_wrapped_text(uint8_t *ssd, const char *text, struct render_area *frame_area, int index_scroll) 
{
    char lines[5][17]; // Buffer para guardar as linhas formatadas (até 5 linhas, 16 chars cada)
    int num_lines = 0;

    // Formata o texto nas linhas
    wrap_text(text, lines, &num_lines);

    // Limpa o display
    memset(ssd, 0, ssd1306_buffer_length);

    // Renderiza a primeira linha, vazia
    ssd1306_draw_string(ssd, 0, 0, "");

    // Renderiza o texto da tarefa
    int y = 8; // inicia na segunda linha
    for (int i = 0; i < num_lines && i < 3; i++) {
        ssd1306_draw_string(ssd, 0, y, lines[i]);
        y += 8; // Move para a próxima linha
    }

    y+=8;
    // Renderiza a última linha, vazia também
    char buffer[10]; // Buffer para guardar a string formarada
    sprintf(buffer, "%d de %d", (index_scroll + 1), task_count); // String de index de task (ex: 2 de 8)
    ssd1306_draw_string(ssd, 50, y, buffer);

    // Coloca o L e R na última linha, caso haja mais de uma tarefa, para indicar que o usuário pode navegar para os lados
    if(task_count > 1)
    {
        ssd1306_draw_string(ssd, 0, y + 8, "L");
        ssd1306_draw_string(ssd, 120, y + 8, "R");
    }

    // Atualiza o display OLED
    render_on_display(ssd, frame_area);
}

//Início de Sequência das funções responsáveis pelo display no pomodoro, nesse caso, dos intervalos a serem selecionados.
uint pomodoro_1(struct render_area *frame_area)
{
    //Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "  Escolha o  ",
        "   periodo ",
        " 20s pra 5s    ",
        "   30 pra 5    ",
        "   60 pra 10   ",
        "   60 pra 15   ",
        "   120 pra 20  ",
        "   120 pra 35  "
    };
    
    //Renderiza e exibe o conteúdo desejado
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 1;
}

uint pomodoro_2(struct render_area *frame_area)
{
    //Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "  Escolha o  ",
        "   periodo ",
        "   20s pra 5s  ",
        " 30 pra 5      ",
        "   60 pra 10   ",
        "   60 pra 15   ",
        "   120 pra 20  ",
        "   120 pra 35  "
    };
    
    //Renderiza e exibe o conteúdo desejado
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 2;
}

uint pomodoro_3(struct render_area *frame_area)
{
    //Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "  Escolha o  ",
        "   periodo ",
        "   20s pra 5s  ",
        "   30 pra 5    ",
        " 60 pra 10     ",
        "   60 pra 15   ",
        "   120 pra 20  ",
        "   120 pra 35  "
    };
    
    //Renderiza e exibe o conteúdo desejado
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 3;
}

uint pomodoro_4(struct render_area *frame_area)
{
    //Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "  Escolha o  ",
        "   periodo ",
        "   20s pra 5s  ",
        "   30 pra 5    ",
        "   60 pra 10   ",
        " 60 pra 15     ",
        "   120 pra 20  ",
        "   120 pra 35  "
    };

    //Renderiza e exibe o conteúdo desejado
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 4;
}

uint pomodoro_5(struct render_area *frame_area)
{
    //Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "  Escolha o  ",
        "   periodo ",
        "   20s pra 5s  ",
        "   30 pra 5    ",
        "   60 pra 10   ",
        "   60 pra 15   ",
        " 120 pra 20    ",
        "   120 pra 35  "
    };

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 5;
}

uint pomodoro_6(struct render_area *frame_area)
{
    //Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    //Texto a ser exibido
    char *text[] = {
        "  Escolha o  ",
        "   periodo ",
        "   20s pra 5s  ",
        "   30 pra 5    ",
        "   60 pra 10   ",
        "   60 pra 15   ",
        "   120 pra 20  ",
        " 120 pra 35    "
    };
    
    //Renderiza e exibe o conteúdo desejado
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, frame_area);

    return 6;
}
//Fim da Sequência

//Função para fazer o buzzer ter múltiplos beeps
void buzzer_multiple_beeps(int count, int duration_ms) 
{
    for (int i = 0; i < count; i++) {
        gpio_put(pin_buzzer, 1); // Liga o buzzer
        sleep_ms(duration_ms);  
        gpio_put(pin_buzzer, 0); //Desliga o buzzer
        sleep_ms(200);
    }
}

//Função a ser chamada no alarme do lembrete
int64_t reminder_alarm_callback(alarm_id_t id, void *user_data) {
    //Soa o alarme
    gpio_put(pin_buzzer, true); // Liga o buzzer
    reminder_active = true;

    buzzer_end_time = delayed_by_ms(get_absolute_time(), 5000);

    return 0; //Sem repetições
}

// Check and update the buzzer state
void check_buzzer_state() {
    if (reminder_active && absolute_time_diff_us(buzzer_end_time, get_absolute_time()) <= 0) {
        // Turn off the buzzer
        gpio_put(pin_buzzer, false);
        reminder_active = false;
    }
}

//Função que insere no display a seleção de tempo pro alarme
void set_reminder_time(int *minutes, int *seconds, struct render_area *frame_area) 
{
    *minutes = 0;
    *seconds = 0;

    sleep_ms(600);
    while (true) {
        //Coloca o tempo atual selecionado no display OLED
        char buffer[17];
        snprintf(buffer, sizeof(buffer), "Tempo: %02d:%02d", *minutes, *seconds);
        
        //Limpa o display
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);

        //Texto a ser exibido
        char *text[] = {
            "               ",
            buffer};

        //Exibe mensagem desejada no display
        int y = 0;
        for (uint i = 0; i < count_of(text); i++)
        {
            ssd1306_draw_string(ssd, 5, y, text[i]);
            y += 8;
        }
        render_on_display(ssd, frame_area);

        if (!gpio_get(btna)) {
            *minutes += 1; //Aumenta o tempo em 1 minuto
            sleep_ms(200);
        }
        if (!gpio_get(btnb)) {
            *seconds += 5;//Aumenta o tempo em 5 segundos
            sleep_ms(200);
        }

        bool sw_val = gpio_get(sw) == 0;
        sleep_ms(250);

        //Saída
        if (sw_val) {
            break;
        }

        //Cuidados de overflow
        if (*seconds >= 60) {
            *minutes += 1;
            *seconds -= 60;
        }
    }
}

//Função responsável pela contagem e exibição do tempo do pomodoro
void pomodoro_timer(uint8_t *ssd, struct render_area *frame_area, uint work_dur, uint rest_dur) 
{
    int work_time = work_dur;
    int rest_time = rest_dur;
    bool is_working = true; // O tempo começará com trabalho

    while (true) {
        // Limpa o display
        memset(ssd, 0, ssd1306_buffer_length);

        //adicionar condição de parada
        if(gpio_get(btna) == 0 && gpio_get(btnb) == 0)
        {
            gpio_put(pin_azul, false);
            gpio_put(pin_vermelho, false);
            return;
        }

        if (is_working) {
            // Display do tempo de trabalho
            char buffer[17];
            snprintf(buffer, sizeof(buffer), "Trabalho: %02d:%02d", work_time / 60, work_time % 60);

            char lines[2][17]; // Buffer para guardar as linhas formatadas (até 5 linhas, 16 chars cada)
            int num_lines = 0;
            wrap_text(buffer, lines, &num_lines);// Formata o texto nas linhas

            // Renderiza a primeira linha, vazia
            ssd1306_draw_string(ssd, 0, 0, "");
            
            // Renderiza o tempo atual do pomodoro
            int y = 8; // inicia na segunda linha
            for (int i = 0; i < num_lines && i < 3; i++) {
                ssd1306_draw_string(ssd, 0, y, lines[i]);
                y += 8; // Move para a próxima linha
            }

            gpio_put(pin_vermelho, true);

            work_time--;
            if (work_time < 0) {
                is_working = false; // Muda para o intervalo de descanso
                work_time = work_dur; // Reseta o tempo de trabalho

                buzzer_multiple_beeps(3, 200);
                gpio_put(pin_vermelho, false); // Apaga o LED vermelho
            }
        } else {
            // Display do tempo de descanso
            char buffer[17];
            snprintf(buffer, sizeof(buffer), "Descanso: %02d:%02d", rest_time / 60, rest_time % 60);
            
            char lines[2][17]; // Buffer para guardar as linhas formatadas (até 5 linhas, 16 chars cada)
            int num_lines = 0;
            wrap_text(buffer, lines, &num_lines);// Formata o texto nas linhas

            // Renderiza a primeira linha, vazia
            ssd1306_draw_string(ssd, 0, 0, "");
            
            // Renderiza o tempo atual do pomodoro
            int y = 8; // inicia na segunda linha
            for (int i = 0; i < num_lines && i < 3; i++) {
                ssd1306_draw_string(ssd, 0, y, lines[i]);
                y += 8; // Move para a próxima linha
            }

            gpio_put(pin_azul, true);

            rest_time--;
            if (rest_time < 0) {
                is_working = true; // Muda para o intervalo de trabalho
                rest_time = rest_dur; // Reseta o tempo de descanso

                buzzer_multiple_beeps(3, 200);
                gpio_put(pin_azul, false); // Apaga o LED azul
            }
        }

        // atualiza no display
        render_on_display(ssd, frame_area);

        sleep_ms(1000);
    }
}

//Função que lê da Memória Flash utilizando DMA
void dma_read_flash(uint8_t *dest, const uint8_t *src, size_t size) 
{
    int dma_channel = dma_claim_unused_channel(true); //Reserva um canal de DMA
    dma_channel_config config = dma_channel_get_default_config(dma_channel);

    //Configura o canal de DMA para ler da memória Flash
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8); //A transferência é feita a cada 8 bits
    channel_config_set_read_increment(&config, true); //Incrementa o endereço de leitura
    channel_config_set_write_increment(&config, true); // Incrementa o endereço para escrever

    dma_channel_configure(dma_channel, &config,
                          dest,    //Destino, que nesse caso, é o buffer do RAM
                          src,     //Fonte, a flash memory
                          size,    //Número de transferências
                          true);   //Começar de imediato

    dma_channel_wait_for_finish_blocking(dma_channel); //Espera  a transferência ser completada
    dma_channel_unclaim(dma_channel); //Tira a reserva no canal
}

//Função que escreve na Memória Flash utilizando DMA
void dma_write_flash(const uint8_t *src, uint8_t *dest, size_t size) 
{
    int dma_channel = dma_claim_unused_channel(true); //Faz a reserva de um canal do DMA
    dma_channel_config config = dma_channel_get_default_config(dma_channel);

    //Configura o DMA para escrever na Flash Memory
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8); //Transferência de 8 bits por vez
    channel_config_set_read_increment(&config, true); //Incrementa o endereço de leitura
    channel_config_set_write_increment(&config, true); //Incrementa o endereço para escrever

    dma_channel_configure(dma_channel, &config,
                          dest,    //Destinação (Memória Flash)
                          src,     //Fonte: buffer da RAM
                          size,    //Número de transferências
                          true);   //Começo imediato

    dma_channel_wait_for_finish_blocking(dma_channel); //Espera o fim da transferência
    dma_channel_unclaim(dma_channel); //Libera o canal de DMA
}

//Função com os padrões de inicialização para a Memória flash
void initialize_flash_with_defaults() 
{
    FlashData flash_data;
    flash_data.magic_number = FLASH_MAGIC_NUMBER; //Coloca o número mágico
    flash_data.task_count = 0; //Inicializa com zero tarefas

    //Salva os dados padrões à flash
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, (uint8_t *)&flash_data, sizeof(FlashData));
    restore_interrupts(ints);
}

//Função que carrega as tarefas da Flash Memory
void load_tasks(Task *tasks) 
{
    FlashData flash_data;

    //Lê da flash utilizando DMA
    dma_read_flash((uint8_t *)&flash_data, (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET), sizeof(FlashData));

    //Checa se a memória flash foi inicializada
    if (flash_data.magic_number != FLASH_MAGIC_NUMBER) {
        //Caso não tenha sido inicializada; ela é inicializada com os padrões desejados
        initialize_flash_with_defaults();

        //Recarrega os dados
        dma_read_flash((uint8_t *)&flash_data, (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET), sizeof(FlashData));
    }

    //Copia as tarefas e o número de tarefas
    memcpy(tasks, flash_data.tasks, sizeof(flash_data.tasks));
    task_count = flash_data.task_count;
}

//Função que salva as tarefas na Flash memory
void save_tasks(Task *tasks) 
{
    FlashData flash_data;
    flash_data.magic_number = FLASH_MAGIC_NUMBER; //Coloca o número mágico
    memcpy(flash_data.tasks, tasks, sizeof(flash_data.tasks)); //Copia as tarefas
    flash_data.task_count = task_count; //Guarda a contagem de tarefas

    //Salva na flash
    uint32_t ints = save_and_disable_interrupts(); //Desativa interrupções
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE); //Apaga o setor
    flash_range_program(FLASH_TARGET_OFFSET, (uint8_t *)&flash_data, sizeof(FlashData)); //Escreve os dados
    restore_interrupts(ints); //Reativa interrupções
}

//Função para adicionar uma tarefa à lista
bool add_task(const char* name)
{
    if(task_count >= MAX_NUM_TASKS) return false;

    if (name == NULL || strlen(name) == 0) {
        return false; // Pra não adicionar tarefas vazias
    }

    //Copia o nome da tarefa
    strncpy(tasks[task_count].name, name, 79);
    tasks[task_count].name[79] = '\0'; // Se assegura que tem fim nulo

    tasks[task_count].completed = false; //Tarefa não completa por default

    task_count++;

    save_tasks(tasks); //Salva na memória flash a tarefa

    return true;
}

//Função para completar uma tarefa na lista
bool complete_task(int index) 
{
    if (index < 0 || index >= task_count) {
        return false; // Index inválido
    }

    tasks[index].completed = true;
    return true;
}

int main()
{
    stdio_init_all(); 

    adc_init();

    //Configura os pinos 26 e 27 como leituras analógicas do ADC, sendo respectivamente responsáveis pelos eixos x e y. (JOYSTICK)
    adc_gpio_init(vrx);
    adc_gpio_init(vry);

    //Configura o pino 22 como botão, com pull-up interno. (JOYSTICK)
    gpio_init(sw);
    gpio_set_dir(sw, GPIO_IN);
    gpio_pull_up(sw);

    //Inicializa o Botão A (GPIO 5)
    gpio_init(btna);
    gpio_set_dir(btna, GPIO_IN);
    gpio_pull_up(btna);

    //Inicializa o Botão B (GPIO 6)
    gpio_init(btnb);
    gpio_set_dir(btnb, GPIO_IN);
    gpio_pull_up(btnb);

    //Inicializa o Led Vermelho
    gpio_init(pin_vermelho);
    gpio_set_dir(pin_vermelho, GPIO_OUT);

    //Inicializa o Led Verde
    gpio_init(pin_verde);
    gpio_set_dir(pin_verde, GPIO_OUT);

    //Inicializa o Led Azul
    gpio_init(pin_azul);
    gpio_set_dir(pin_azul, GPIO_OUT);

    //Inicializa o Buzzer
    gpio_init(pin_buzzer);
    gpio_set_dir(pin_buzzer, GPIO_OUT);
    gpio_put(pin_buzzer, 0);

    //Carrega as tasks da memória
    load_tasks(tasks);

    // Inicializa o i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    //Inicializa o OLED SSD1306
    ssd1306_init();

    // Prepara a área de renderização para o display
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

restart:

    //Gera o texto que estará disponível no display OLED
    char *text[] = {
        "  Gerente de   ",
        "   Tarefas   ",
        "          ",
        "  Clique em   ",
        "  Algum botao   ",
        "  para Entrar!   "}; 


    //Renderiza o conteúdo desejado no display OLED
    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);

    while(true) 
    {
        check_buzzer_state();
        if(gpio_get(btna) == 0 || gpio_get(btnb) == 0)
        {
            atual = main_menu_tarefas(&frame_area);
            while(true)
            {
                check_buzzer_state();
                //Canal 0 para o ADC (GP26)
                adc_select_input(0);
                uint16_t vrx_val = adc_read();

                //Canal 1 para o ADC (GP27)
                adc_select_input(1);
                uint16_t vry_val = adc_read();

                bool sw_val = gpio_get(sw) == 0;
                sleep_ms(250);

                //Cenário em que o Joystick seja movido para cima
                if(vrx_val > 3500)
                {
                    if(atual == 1){
                        atual = main_menu_lembretes(&frame_area);
                    } else if(atual == 2){
                        atual = main_menu_tarefas(&frame_area);
                    } else if(atual == 3){
                        atual = main_menu_pomodoro(&frame_area);
                    }
                }

                //Cenário em que o Joystick seja movido para baixo
                else if(vrx_val < 600)
                {
                    if(atual == 1){
                        atual = main_menu_pomodoro(&frame_area);
                    } else if(atual == 2){
                        atual = main_menu_lembretes(&frame_area);
                    } else if(atual == 3){
                        atual = main_menu_tarefas(&frame_area);
                    }
                }

                //Sequência caso o usuário clique em alguma das opções do menu
                if(gpio_get(btna) == 0 || gpio_get(btnb) == 0)
                {
                    //Tarefas
                    if(atual == 1)
                    {
                        uint atual_tarefas = menu_de_tarefas_1(&frame_area);

                        while(true)
                        {
                            check_buzzer_state();
                            //Sair do menu de opções de Tarefas e voltar ao menu principal
                            sleep_ms(300);
                            if(gpio_get(btna) == 0 && gpio_get(btnb) == 0)
                            {
                                atual = main_menu_tarefas(&frame_area);
                                break;
                            }

                            //Canal 0 para o ADC (GP26)
                            adc_select_input(0);
                            vrx_val = adc_read();

                            //Canal 1 para o ADC (GP27)
                            adc_select_input(1);
                            vry_val = adc_read();

                            sw_val = gpio_get(sw) == 0;
                            sleep_ms(250);

                            //Cenário em que o Joystick seja movido para cima
                            if(vrx_val > 3500)
                            {
                                if(atual_tarefas == 1){
                                    atual_tarefas = menu_de_tarefas_3(&frame_area);
                                } else if(atual_tarefas == 2){
                                    atual_tarefas = menu_de_tarefas_1(&frame_area);
                                } else if(atual_tarefas == 3){
                                    atual_tarefas = menu_de_tarefas_2(&frame_area);
                                }
                            }

                            //Cenário em que o Joystick seja movido para baixo
                            else if(vrx_val < 600)
                            {
                                if(atual_tarefas == 1){
                                    atual_tarefas = menu_de_tarefas_2(&frame_area);
                                } else if(atual_tarefas == 2){
                                    atual_tarefas = menu_de_tarefas_3(&frame_area);
                                } else if(atual_tarefas == 3){
                                    atual_tarefas = menu_de_tarefas_1(&frame_area);
                                }
                            }

                            //Implementa cada função de tarefas: lista, add, limpar.
                            if(gpio_get(btna) == 0 || gpio_get(btnb) == 0)
                            {
                                //Lista de Tarefas
                                if(atual_tarefas == 1)
                                {
                                    if(gpio_get(btna) == 0 && gpio_get(btnb) == 0)
                                    {
                                        sleep_ms(300);
                                        atual_tarefas = menu_de_tarefas_1(&frame_area);
                                        continue;
                                    }

                                    //Limpa o display
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);
                                    render_on_display(ssd, &frame_area); 

                                    if(!task_count)
                                    {
                                        char *text[] = {
                                            "       ",
                                            "    Nada por    ",
                                            "      aqui      "};

                                        //Insere no display texto desejado
                                        int y = 0;
                                        for (uint i = 0; i < count_of(text); i++)
                                        {
                                            ssd1306_draw_string(ssd, 5, y, text[i]);
                                            y += 8;
                                        }
                                        render_on_display(ssd, &frame_area);
                                    }

                                    else
                                    {
                                        uint8_t ssd[ssd1306_buffer_length];
                                        memset(ssd, 0, ssd1306_buffer_length);

                                        //Texto a ser exibido
                                        const char *task_text = tasks[0].name;

                                        // Renderiza o texto formatado
                                        render_wrapped_text(ssd, task_text, &frame_area, 0);

                                        //Atualiza o display
                                        render_on_display(ssd, &frame_area);

                                        int index_scroll = 0, count = 0;
                                        while(true)
                                        {
                                            check_buzzer_state();
                                            //Cor do LED, de acordo com estado da tarefa(Verde - Feita, Vermelho - Não Feita)
                                            if(tasks[index_scroll].completed) {gpio_put(pin_vermelho, false); gpio_put(pin_verde, true);}
                                            else {gpio_put(pin_verde, false); gpio_put(pin_vermelho, true);}

                                            //Saída da Lista de Tarefas e voltar ao menu de opções para Tarefas
                                            if(gpio_get(btna) == 0 && gpio_get(btnb) == 0)
                                            {
                                                gpio_put(pin_vermelho, false);
                                                gpio_put(pin_verde, false);

                                                atual_tarefas = menu_de_tarefas_1(&frame_area);

                                                break;
                                            }

                                            if(index_scroll+1 <= task_count)
                                            {
                                                //Canal 0 para o ADC (GP26)
                                                adc_select_input(0);
                                                vrx_val = adc_read();

                                                //Canal 1 para o ADC (GP27)
                                                adc_select_input(1);
                                                vry_val = adc_read();

                                                sw_val = gpio_get(sw) == 0;
                                                sleep_ms(250);
                                                
                                                
                                                //Cenário em que o Joystick seja movido para direita
                                                if(vry_val > 3500)
                                                {
                                                    count = 0;
                                                    index_scroll++;
                                                    if(index_scroll >= task_count) index_scroll = 0;

                                                    uint8_t ssd[ssd1306_buffer_length];
                                                    memset(ssd, 0, ssd1306_buffer_length);

                                                    // Texto a ser exibido
                                                    const char *task_text = tasks[index_scroll].name;

                                                    // Renderiza o Texto formatado
                                                    render_wrapped_text(ssd, task_text, &frame_area, index_scroll);

                                                    // Atualiza o display
                                                    render_on_display(ssd, &frame_area);
                                                }

                                                //Cenário em que o Joystick seja movido para esquerda
                                                else if(vry_val < 600)
                                                {
                                                    count = 0;
                                                    index_scroll--;
                                                    if(index_scroll < 0) index_scroll = task_count - 1;

                                                    uint8_t ssd[ssd1306_buffer_length];
                                                    memset(ssd, 0, ssd1306_buffer_length);

                                                    // Texto a ser exibido
                                                    const char *task_text = tasks[index_scroll].name;

                                                    // Renderiza o Texto formatado
                                                    render_wrapped_text(ssd, task_text, &frame_area, index_scroll);

                                                    // Atualiza o display
                                                    render_on_display(ssd, &frame_area);
                                                }
                                            }
                                            else index_scroll = 0;
                                            
                                            //Se apertar um dos botões duaz vezes, a tarefa é marcada como feita, pode apertar mais duas vezes para desmarcar
                                            if(gpio_get(btna) == 0 || gpio_get(btnb) == 0)
                                            {
                                                count++;
                                                if(count >= 2)
                                                {
                                                    tasks[index_scroll].completed = !tasks[index_scroll].completed;
                                                    count = 0;
                                                }
                                            }
                                        }
                                    }
                                }

                                //Adicionar Tarefas
                                if(atual_tarefas == 2)
                                {
                                    //Limpa o display
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);
                                    render_on_display(ssd, &frame_area);

                                    char *text[] = {
                                        "   Adicione    ",
                                        "   a Tarefa   ",
                                        "       ",
                                        "       ",
                                        "  Clique em   ",
                                        " ambos botoes ",
                                        " para Terminar "};

                                    //Insere no display texto desejado
                                    int y = 0;
                                    for (uint i = 0; i < count_of(text); i++)
                                    {
                                        ssd1306_draw_string(ssd, 5, y, text[i]);
                                        y += 8;
                                    }
                                    render_on_display(ssd, &frame_area);

                                    // Armazena a sequência do código morse atual
                                    char morse_buffer[10] = ""; 
                                    int morse_index = 0;
                                    
                                    uint32_t press_start_time = 0;
                                    uint32_t last_press_time = 0;

                                    // Buffer para a palavra decodificada
                                    char word_buffer[80] = "";
                                    int word_index = 0;

                                    char session_buffer[80] = "";
                                    int session_index = 0;

                                    sleep_ms(3000);

                                    while(true)
                                    {
                                        check_buzzer_state();
                                        //Sair do Adicionar Tarefa
                                        if(gpio_get(btna) == 0 && gpio_get(btnb) == 0)
                                        {
                                            atual_tarefas = menu_de_tarefas_1(&frame_area);
                                            break;
                                        }

                                        if(gpio_get(btna) == 0 || gpio_get(btnb) == 0)
                                        {
                                            if(press_start_time == 0)
                                            {
                                                press_start_time = to_ms_since_boot(get_absolute_time());
                                            }
                                        }
                                        
                                        else {
                                            if (press_start_time != 0) {
                                                // Quando o botão for solto, isso calculará o tempo em que ele ficou pressionado
                                                uint32_t press_duration = to_ms_since_boot(get_absolute_time()) - press_start_time;
                                                press_start_time = 0;
                                
                                                // Determina se é ponto ou barra
                                                if (press_duration < ponto_morse) {
                                                    morse_buffer[morse_index++] = '.';
                                                } else {
                                                    morse_buffer[morse_index++] = '-';
                                                }
                                                morse_buffer[morse_index] = '\0'; // Termina o buffer com nulo

                                                last_press_time = to_ms_since_boot(get_absolute_time()); // Record last press time
                                            }
                                        }
                                
                                        // Checa se teve timeout de sequência (2000ms)
                                        if (morse_index > 0 && (to_ms_since_boot(get_absolute_time()) - last_press_time > timeout_morse)) {
                                            // Decoda a sequência de código Morse
                                            char decoded_char = morse_to_char(morse_buffer);

                                            word_buffer[word_index++] = decoded_char;
                                            word_buffer[word_index] = '\0'; // Terminação Nula da palavra

                                            //Limpa o display
                                            uint8_t ssd[ssd1306_buffer_length];
                                            memset(ssd, 0, ssd1306_buffer_length);
                                            render_on_display(ssd, &frame_area);

                                            char *text[] = {
                                                "   Adicione   ",
                                                "   a Tarefa   ",
                                                "       ",
                                                word_buffer};

                                            //Renderiza o conteúdo desejado no display OLED
                                            y = 0;
                                            for (uint i = 0; i < count_of(text); i++)
                                            {
                                                ssd1306_draw_string(ssd, 5, y, text[i]);
                                                y += 8;
                                            }
                                            render_on_display(ssd, &frame_area);
                                
                                            // Limpa o buffer do código morse
                                            morse_index = 0;
                                            morse_buffer[0] = '\0';
                                        }

                                        // Checa se teve timeout de palavra (5000ms)
                                        if (word_index > 0 && (to_ms_since_boot(get_absolute_time()) - last_press_time > timeout_palavra_morse)) 
                                        {
                                            // Fim da palavra

                                            if (session_index + strlen(word_buffer) + 1 < 40) {
                                                if (session_index > 0) {
                                                    session_buffer[session_index++] = ' '; // Add a space between words
                                                }
                                                strcat(session_buffer, word_buffer); // Append the word to the session buffer
                                                session_index += strlen(word_buffer);
                                            } 
                                            
                                            else {
                                                //Limpa o display
                                                uint8_t ssd[ssd1306_buffer_length];
                                                memset(ssd, 0, ssd1306_buffer_length);
                                                render_on_display(ssd, &frame_area);

                                                char *text[] = {
                                                    "       ",
                                                    "Tamanho Maximo",
                                                    "   Atingido    "};

                                                //Renderiza o conteúdo desejado no display OLED
                                                y = 0;
                                                for (uint i = 0; i < count_of(text); i++)
                                                {
                                                    ssd1306_draw_string(ssd, 5, y, text[i]);
                                                    y += 8;
                                                }
                                                render_on_display(ssd, &frame_area);
                                            }

                                            // Limpa o buffer de palavra em preparação para a próxima palavra
                                            word_index = 0;
                                            word_buffer[0] = '\0';

                                            //Limpa o display
                                            uint8_t ssd[ssd1306_buffer_length];
                                            memset(ssd, 0, ssd1306_buffer_length);
                                            render_on_display(ssd, &frame_area);

                                            char *text[] = {
                                                "       ",
                                                "   Adicione   ",
                                                "      a       ",
                                                "    Tarefa    "};

                                            //Renderiza o conteúdo desejado no display OLED
                                            y = 0;
                                            for (uint i = 0; i < count_of(text); i++)
                                            {
                                                ssd1306_draw_string(ssd, 5, y, text[i]);
                                                y += 8;
                                            }
                                            render_on_display(ssd, &frame_area);
                                        }
                                
                                        sleep_ms(10); 
                                    }

                                    //Adiciona a tarefa criada à lista de tarefas
                                    if(add_task(session_buffer))
                                    {
                                        //Limpa o display
                                        uint8_t ssd[ssd1306_buffer_length];
                                        memset(ssd, 0, ssd1306_buffer_length);
                                        render_on_display(ssd, &frame_area);

                                        char *text[] = {
                                            "       ",
                                            "    Tarefa    ",
                                            "Adicionada com",
                                            "    Sucesso!   "};

                                        //Renderiza o conteúdo desejado no display OLED
                                        y = 0;
                                        for (uint i = 0; i < count_of(text); i++)
                                        {
                                            ssd1306_draw_string(ssd, 5, y, text[i]);
                                            y += 8;
                                        }
                                        render_on_display(ssd, &frame_area);

                                        printf("%s", session_buffer);
                                    }

                                    //Caso tente adicionar uma tarefa à lista e o máximo de tarefas já foi atingido, o código abaixo avisa sobre a situação
                                    else
                                    {
                                        //Limpa o display
                                        uint8_t ssd[ssd1306_buffer_length];
                                        memset(ssd, 0, ssd1306_buffer_length);
                                        render_on_display(ssd, &frame_area); 

                                        char *text[] = {
                                            "       ",
                                            "    Tarefa    ",
                                            "    Nao foi   ",
                                            "  Adicionada  "};

                                        //Renderiza o conteúdo desejado no display OLED
                                        y = 0;
                                        for (uint i = 0; i < count_of(text); i++)
                                        {
                                            ssd1306_draw_string(ssd, 5, y, text[i]);
                                            y += 8;
                                        }
                                        render_on_display(ssd, &frame_area);
                                    }

                                    //Volta ao menu de opções para Tarefas
                                    sleep_ms(2000);
                                    atual_tarefas = menu_de_tarefas_1(&frame_area);
                                    continue;
                                }

                                //Limpar tarefas
                                if(atual_tarefas == 3)
                                {
                                    //Limpa a lista de tarefas da memória
                                    memset(tasks, 0, sizeof(Task) * MAX_NUM_TASKS);
                                    task_count = 0;

                                    //Salva a lista vazia na flash
                                    save_tasks(tasks);

                                    // zera o display
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);
                                    render_on_display(ssd, &frame_area);

                                    char *text[] = {
                                        "               ",
                                        "  A Lista foi  ",
                                        "  reiniciada ",
                                        "  com sucesso! "}; 

                                    int y = 0;
                                    for (uint i = 0; i < count_of(text); i++)
                                    {
                                        ssd1306_draw_string(ssd, 5, y, text[i]);
                                        y += 8;
                                    }
                                    render_on_display(ssd, &frame_area);

                                    sleep_ms(3000);

                                    atual_tarefas = menu_de_tarefas_1(&frame_area);
                                }
                            }
                        }
                    }

                    //Pomodoro
                    else if(atual == 2)
                    {
                        uint atual_pomodoro = pomodoro_1(&frame_area);

                        while(true)
                        {
                            //Sair do menu de opções de intervalo do Pomodoro e voltar ao menu principal
                            sleep_ms(300);
                            if(gpio_get(btna) == 0 && gpio_get(btnb) == 0)
                            {
                                atual = main_menu_tarefas(&frame_area);
                                break;
                            }

                            //Canal 0 para o ADC (GP26)
                            adc_select_input(0);
                            vrx_val = adc_read();

                            //Canal 1 para o ADC (GP27)
                            adc_select_input(1);
                            vry_val = adc_read();

                            sw_val = gpio_get(sw) == 0;
                            sleep_ms(250);

                            //Cenário em que o Joystick seja movido para cima
                            if(vrx_val > 3500)
                            {
                                if(atual_pomodoro == 1){
                                    atual_pomodoro = pomodoro_6(&frame_area);
                                } else if(atual_pomodoro == 2){
                                    atual_pomodoro = pomodoro_1(&frame_area);
                                } else if(atual_pomodoro == 3){
                                    atual_pomodoro = pomodoro_2(&frame_area);
                                } else if(atual_pomodoro == 4){
                                    atual_pomodoro = pomodoro_3(&frame_area);
                                } else if(atual_pomodoro == 5){
                                    atual_pomodoro = pomodoro_4(&frame_area);
                                } else if(atual_pomodoro == 6){
                                    atual_pomodoro = pomodoro_5(&frame_area);
                                } 
                            }

                            //Cenário em que o Joystick seja movido para baixo
                            else if(vrx_val < 600)
                            {
                                if(atual_pomodoro == 1){
                                    atual_pomodoro = pomodoro_2(&frame_area);
                                } else if(atual_pomodoro == 2){
                                    atual_pomodoro = pomodoro_3(&frame_area);
                                } else if(atual_pomodoro == 3){
                                    atual_pomodoro = pomodoro_4(&frame_area);
                                } else if(atual_pomodoro == 4){
                                    atual_pomodoro = pomodoro_5(&frame_area);
                                } else if(atual_pomodoro == 5){
                                    atual_pomodoro = pomodoro_6(&frame_area);
                                } else if(atual_pomodoro == 6){
                                    atual_pomodoro = pomodoro_1(&frame_area);
                                } 
                            }

                            //Implementa o pomodoro em si, o contador de tempo
                            if(gpio_get(btna) == 0 || gpio_get(btnb) == 0)
                            {
                                if(atual_pomodoro == 1)
                                {
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);

                                    pomodoro_timer(ssd, &frame_area, 20, 5);
                                }

                                else if(atual_pomodoro == 2)
                                {
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);

                                    pomodoro_timer(ssd, &frame_area, 30*60, 5*60);
                                }

                                else if(atual_pomodoro == 3)
                                {
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);

                                    pomodoro_timer(ssd, &frame_area, 60*60, 10*60);
                                }

                                else if(atual_pomodoro == 4)
                                {
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);

                                    pomodoro_timer(ssd, &frame_area, 60*60, 15*60);
                                }

                                else if(atual_pomodoro == 5)
                                {
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);

                                    pomodoro_timer(ssd, &frame_area, 120*60, 20*60);
                                }

                                else if(atual_pomodoro == 6)
                                {
                                    uint8_t ssd[ssd1306_buffer_length];
                                    memset(ssd, 0, ssd1306_buffer_length);

                                    pomodoro_timer(ssd, &frame_area, 120*60, 35*60);
                                }

                                atual_pomodoro = pomodoro_1(&frame_area);
                            }
                        }
                    }

                    //Lembretes
                    else if(atual == 3)
                    {
                        // Initialize the reminder timer
                        //initialize_reminder_timer();

                        // Set a new reminder
                        int minutes = 0, seconds = 0;
                        
                        // Set the reminder time
                        set_reminder_time(&minutes, &seconds, &frame_area);

                        current_reminder.time_seconds = (minutes * 60) + seconds;
                        reminder_alarm_id = add_alarm_in_ms(current_reminder.time_seconds * 1000, reminder_alarm_callback, NULL, false);

                        // zera o display
                        uint8_t ssd[ssd1306_buffer_length];
                        memset(ssd, 0, ssd1306_buffer_length);
                        render_on_display(ssd, &frame_area);

                        char buffer[17];
                        sprintf(buffer, "Toca em %02d:%02d\n", minutes, seconds); // Replace with LCD display code
                            
                        y = 16;
                        ssd1306_draw_string(ssd, 5, y, buffer);

                        render_on_display(ssd, &frame_area);
                        sleep_ms(2000);

                        atual = main_menu_tarefas(&frame_area);
                    }
                }
            }
        }
        sleep_ms(100);
    }

    return 0;
}